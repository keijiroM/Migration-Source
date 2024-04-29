// Source
// #include <string>
// #include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "./db.h"
#include "rocksdb/options.h"

using namespace rocksdb;


struct TransferSstFilesArgs {
	Options options;
	std::vector<SstFileData> file_datas;
	std::vector<std::string> file_paths;
	int number_of_files;
	std::vector<std::vector<int>> fd;
	std::string id;

	TransferSstFilesArgs(
				const Options& options_,
				const std::vector<SstFileData>& file_datas_, 
				const std::vector<std::string>& file_paths_,
				const int number_of_files_,  
				const std::vector<std::vector<int>>& fd_,
				const std::string& id_)
	{
		options = options_;
		file_datas = file_datas_;
		file_paths = file_paths_;
		number_of_files = number_of_files_;
		fd = fd_;
		id = id_;
	}
};


class SstFile {
private:
	std::mutex mtx;
	std::condition_variable cv;

public:
	void ExportKVPairs(const Options& options, 
					   const std::string& file_path, 
					   std::queue<std::string>& key_que, 
					   std::queue<std::string>& value_que, 
					   const int& thread_number /* for debug */);
	void SendKVPairs(const int& fd, 
					 std::queue<std::string>& que, 
					 const uint64_t& number_of_entries, 
					 const int& thread_number /* for debug */, 
					 const int& func_id /* for debug */, 
					 const int& file_fd /* for debug */);
};


void TransferSstFiles(TransferSstFilesArgs args, const int& number_of_threads);
