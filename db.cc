// Source
#include <iostream>
#include <thread>
#include <sstream>
// #include <sys/stat.h>
#include <sys/socket.h>
#include "./db.h"
#include "./socket.h"
#include "rocksdb/sst_file_reader.h"

using namespace rocksdb;


void AssertStatus(Status status, const std::string& flag_name) {
	if (!status.ok()) {
		// std::cout << status.ToString() << std::endl;
		std::cout << status.ToString() << "("  + flag_name + ")" << std::endl;
		exit(EXIT_FAILURE);
	}
}


void Flush_and_Compaction(DB* db) {
	std::cout << "Flush_and_Compaction is started" << std::endl;

	Status status;
	status = db->Flush(FlushOptions());
	AssertStatus(status, "Flush_and_Compaction");

	std::this_thread::sleep_for(std::chrono::seconds(10));
}



void GetSstFileData(DB* db, 
					const Options& options, 
					std::vector<SstFileData>& file_datas, 
					std::vector<std::string>& file_paths) {
	std::vector<LiveFileMetaData> live_file_metadata;
	db->GetLiveFilesMetaData(&live_file_metadata);

	for (const auto& meta : live_file_metadata) {
		SstFileData file_data;
		
		// on_disk
#ifdef ONLY_PMEM
		file_data.on_oisk = false;
#elif ONLY_DISK
		file_data.on_disk = true;
#else
		if (meta.db_path == options.db_paths[0].path)
			file_data.on_disk = false;
		else
			file_data.on_disk = true;
#endif
		// level
		file_data.level = meta.level;

		// number_of_entries
		file_data.number_of_entries = meta.num_entries;

		file_datas.push_back(file_data);
		
		// file_path
		const std::string file_path = meta.db_path + meta.name;
		file_paths.push_back(file_path);
	}
}


void SendSstFileData(const int& fd, const std::vector<SstFileData>& file_datas) {
	// シリアライズ
	std::ostringstream oss;
	{
		cereal::JSONOutputArchive archive(oss);
		archive(file_datas);
	}

	std::string serialized_data = oss.str();

	// std::cout << serialized_data << std::endl;

	size_t size = serialized_data.size();

	if (send(fd, &size, sizeof(size_t), 0) <= 0) {
		std::cerr << "send()" << std::endl;
		exit(EXIT_FAILURE);
	}

	RecvFlag(fd, "SendSstFileData");

	// 送信
	if (send(fd, serialized_data.c_str(), serialized_data.size(), 0) <= 0) {
		std::cerr << "send()" << std::endl;
		exit(EXIT_FAILURE);
	}

	RecvFlag(fd, "SendSstFileData");
}


void SendOptions(const int& fd, const SrcOptions& src_options) {
	if (send(fd, &src_options, sizeof(SrcOptions), 0) <= 0) {
		std::cerr << "send()" << std::endl;
		exit(EXIT_FAILURE);
	}
}
