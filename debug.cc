// Source
#include <string>
#include <vector>
#include <unistd.h>
#include "./debug.h"
#include <iostream> // for debug
#include <fcntl.h> // for debug


template <typename ... Args>
static std::string format(const std::string& fmt, Args ... args ) {
	size_t len = std::snprintf( nullptr, 0, fmt.c_str(), args ... );
	std::vector<char> buf(len + 1);
	std::snprintf(&buf[0], len + 1, fmt.c_str(), args ... );

	return std::string(&buf[0], &buf[0] + len);
}


void PrintThroughputToFile(const int& fd, 
						   const double& mid_time, 
						   const double& prev_time, 
						   const int& put_count, 
						   const int& get_count) {
	if (mid_time > prev_time) {
		const std::string buf_time = format("%4.0lf", mid_time) + ",";
		const std::string buf_put_count = format("%9d", put_count) + ",";
		const std::string buf_get_count = format("%9d", get_count);
		const std::string buf = buf_time + buf_put_count + buf_get_count + "\n";

		write(fd, buf.c_str(), buf.length());
	}
}


double ReturnRunTime(const std::chrono::system_clock::time_point& start,
					 const std::chrono::system_clock::time_point& end) {
	return static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(end-start).count());
}


int FileOpen(const std::string& file_path) {
	int file_fd = open(file_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC , 00644);

	if (file_fd < 0) {
		std::cerr << "open()" << std::endl;
		exit(EXIT_FAILURE);
	}

	return file_fd;
}
