// Source
#include <iostream>
#include <thread>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
// #include "./db.h"
#include "./sst.h"
#include "./workload.h"
#include "./socket.h"

using namespace rocksdb;


static void StartInstance(const std::string& id, const int& rate_of_workload, const int& number_of_threads) {
	std::cout << "Source Instance is started." << std::endl;
	
	// ソケットをオープン
	int port_number = 20000 + (atoi(id.c_str()) * 100);
	
	int socket_main_fd = ListenSocket(port_number++);
#ifndef IDLE
    int socket_kv_fd = ListenSocket(port_number++);
#endif
	std::vector<std::vector<int>> socket_sst_fd;
	std::vector<std::vector<int>> sst_fd;
	for (int i = 0; i < number_of_threads; ++i) {
		std::vector<int> socket_fd_vec;
		for (int j = 0; j < 2; ++j) {
			int socket_fd = ListenSocket(port_number++);
			socket_fd_vec.push_back(socket_fd);
		}
		socket_sst_fd.push_back(socket_fd_vec);
	}

	int main_fd = AcceptSocket(socket_main_fd);
#ifndef IDLE
	int kv_fd = AcceptSocket(socket_kv_fd);
#endif
	for (int i = 0; i < number_of_threads; ++i) {
		std::vector<int> fd_vec;
		for (int j = 0; j < 2; ++j) {
			int fd = AcceptSocket(socket_sst_fd[i][j]);
			fd_vec.push_back(fd);
		}
		sst_fd.push_back(fd_vec);
	}

	
	// DBのパスを指定
	const std::string db_path = "/mig_inst" + id;
#ifdef ONLY_PMEM
	const std::string pmem_db_path = pmem_db_dir + db_path;
	const std::vector<std::string> dbname = {pmem_db_path};
#elif ONLY_DISK
	const std::string disk_db_path = disk_db_dir + db_path;
	const std::vector<std::string> dbname = {disk_db_path};
#else
	const std::string pmem_db_path = pmem_db_dir + db_path;
	const std::string disk_db_path = disk_db_dir + db_path;
	const std::vector<std::string> dbname = {pmem_db_path, disk_db_path};
#endif


	// Optionsを設定
	Options options;
	options.IncreaseParallelism();
	options.create_if_missing = true;
	// options.write_buffer_size = 64 << 20;	
#ifdef MIXING
	options.db_paths = {{dbname[0], PMEMSIZE}, {dbname[1], DISKSIZE}};
#endif
	
	// DBを開く
	DB* db;
	Status status = DB::Open(options, dbname[0], &db);
	AssertStatus(status, "DB::Open");
	std::cout << "dbname: " << dbname[0] << std::endl;


/*-------------------------------------- Load KVs --------------------------------------*/

	
	// RunWorkloadBeforeMigration(db, id, rate_of_workload);
	// Flush_and_Compaction(db);


/*-------------------------------------- Phase 1 --------------------------------------*/
	

	// SSTableメタデータを抽出
    std::vector<SstFileData> file_datas;
	std::vector<std::string> file_paths;
    GetSstFileData(db, options, file_datas, file_paths);
	SendSstFileData(main_fd, file_datas);
    int number_of_files = file_datas.size();

	// OptionsをDSTに送信して，SRCでのDB作成完了の通知を受信
	SrcOptions src_options;
	src_options.write_buffer_size = options.write_buffer_size;
	src_options.number_of_files = number_of_files;
	SendOptions(main_fd, src_options);

	// DSTのDB作成完了を待機
	RecvFlag(main_fd, "db_creation");
	std::cout << "DB creation is completed on the destination." << std::endl;


/*-------------------------------------- Phase 2 --------------------------------------*/


#ifndef IDLE
	std::thread RunWorkloadThread([&]{ RunWorkload(db, kv_fd, rate_of_workload); });
#endif
	// SSTables転送スレッドに渡す引数を初期化
	TransferSstFilesArgs args(options, file_datas, file_paths, number_of_files, sst_fd, id);

	// SSTables転送スレッドを実行
	std::thread TransferSstFilesThread([&]{ TransferSstFiles(args, number_of_threads); });
	TransferSstFilesThread.join();
#ifndef IDLE
	RunWorkloadThread.join();
#endif
	

/*-------------------------------------- Phase 3 --------------------------------------*/


	// ソケットを閉じる
	close(socket_main_fd);
	close(main_fd);
#ifndef IDLE
	close(socket_kv_fd);
	close(kv_fd);
#endif
	for (int i = 0; i < number_of_threads; ++i) {
		for (int j = 0; j < 2; ++j) {
			close(socket_sst_fd[i][j]);
			close(sst_fd[i][j]);
		}
	}

	delete db;
}


int main(int argc, char* argv[]) {
	if (argc != 4) {
		std::cerr << "argument error" << std::endl;
		return -1;
	}
	
	const std::string id = argv[1];
	int rate_of_workload = atoi(argv[2]);
	int number_of_threads = atoi(argv[3]);

	StartInstance(id, rate_of_workload, number_of_threads);

	return 0;
}
