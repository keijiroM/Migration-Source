// Source
#include <iostream>
#include <thread>
#include <sys/socket.h> // send()
// #include "./db.h"
#include "./sst.h"
#include "./debug.h"
#include "rocksdb/sst_file_reader.h"
#include <unistd.h>	// for debug

std::mutex transfer_file_mutex;
RunTime export_time, send_keys_time, send_values_time;

void SstFile::ExportKVPairs(const Options& options, 
							const std::string& file_path, 
							std::queue<std::string>& key_que, 
							std::queue<std::string>& value_que, 
							const int& thread_number /* for debug */) {
	SstFileReader sstfile_reader(options);
	Status status = sstfile_reader.Open(file_path);
	AssertStatus(status, "EXportKVPairs(Open)");

	Iterator* iter = sstfile_reader.NewIterator(ReadOptions());

	{
		// for debug
		if (thread_number == 0)
			export_time.start = std::chrono::system_clock::now();
	}

	for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
        const std::string key   = iter->key().ToString();
        const std::string value = iter->value().ToString();
		
		{
			std::lock_guard<std::mutex> lock(mtx);
        	key_que.push(key);
			value_que.push(value);
		}
		cv.notify_all();
    }

	{
		// for debug
		if (thread_number == 0) {
			export_time.end = std::chrono::system_clock::now();
			export_time.time = ReturnRunTime(export_time.start, export_time.end);
		}
	}

    delete iter;
}



void SstFile::SendKVPairs(const int& fd, 
						  std::queue<std::string>& que, 
						  const uint64_t& number_of_entries, 
						  const int& thread_number /* for debug */,
						  const int& func_id /* for debug  */, 
						  const int& file_fd /* for debug */) {
	for (uint64_t i = 0; i < number_of_entries; ++i) {
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [&]{ return !que.empty(); });

		{
			//for debug
			if (thread_number == 0) {
				if (func_id == 0)
					send_keys_time.start = std::chrono::system_clock::now();
			 	else if (func_id == 1)
					send_values_time.start = std::chrono::system_clock::now();
			}
		}

		const std::string data = que.front();
		que.pop();
		
		if (func_id == 0) {
			// std::cout << thread_number << ": " << data << std::endl;
			std::string str = std::to_string(thread_number) + ": " + data + "\n";
			write(file_fd, str.c_str(), str.size());
		}

		ssize_t send_ret = send(fd, data.c_str(), data.size(), 0);
		if (send_ret <= 0) {
			std::cerr << "send(SendKVPairs)" << std::endl;
			exit(EXIT_FAILURE);
		}

		{
			//for debug
			if (thread_number == 0) {
				if (func_id == 0) {
					send_keys_time.end = std::chrono::system_clock::now();
					send_keys_time.time += ReturnRunTime(send_keys_time.start, send_keys_time.end);
				}
				else if (func_id == 1) {
					send_values_time.end = std::chrono::system_clock::now();
					send_values_time.time += ReturnRunTime(send_values_time.start, send_values_time.end);
				}
			}
		}
	}
}


static int file_count = 0;
static void SendSstFiles(TransferSstFilesArgs args, const int& thread_number, const int& file_fd) {
	std::vector<int> fd = args.fd[thread_number];
	while (file_count < args.number_of_files) {
		int file_number;
		{
			std::lock_guard<std::mutex> lock(transfer_file_mutex);
			file_number = file_count++;
		}
		
		SstFile sst_file;
		std::queue<std::string> key_que;
		std::queue<std::string> value_que;

		std::thread ExportKVPairsThread([&]{ sst_file.ExportKVPairs(args.options, 
																	args.file_paths[file_number], 
																	std::ref(key_que), 
																	std::ref(value_que), 
																	thread_number /* for debug */); });
		std::thread SendKeysThread([&]{ sst_file.SendKVPairs(fd[0], 
															 std::ref(key_que), 
															 args.file_datas[file_number].number_of_entries, 
															 thread_number /* for debug */, 
															 0 /* for debug */, 
															 file_fd /* for debug */); });
		std::thread SendValuesThread([&]{ sst_file.SendKVPairs(fd[1], 
															   std::ref(value_que), 
															   args.file_datas[file_number].number_of_entries, 
															   thread_number /* for debug */, 
															   1 /* for debug */, 
															   file_fd /* for debug */); });

		ExportKVPairsThread.join();
		SendKeysThread.join();
		SendValuesThread.join();
	}
}



void TransferSstFiles(TransferSstFilesArgs args, const int& number_of_threads) {
	std::cout << "TransferSstFilesThread is started." << std::endl;


	std::thread *threads[number_of_threads];
	int file_fds[number_of_threads];

	for (int i = 0; i < number_of_threads; ++i) {
		const std::string file_path = std::to_string(i) + ".dat";

		file_fds[i] = FileOpen(file_path);

		threads[i] = new std::thread(SendSstFiles, args, i, file_fds[i] /* for debug */);
	}
	
	for (int i = 0; i < number_of_threads; ++i) {
		threads[i]->join();
		close(file_fds[i]);  // for debug
	}
	
	
	std::cout << "ExportKVPiars: " << export_time.time / 1000000 << " sec" << std::endl;
	std::cout << "SendKeys     : " << send_keys_time.time / 1000000 << " sec" << std::endl;
	std::cout << "SendValues   : " << send_values_time.time / 1000000 << " sec" << std::endl;

	std::cout << "TransferSstFilesThread is ended." << std::endl;
}
