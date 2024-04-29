// Source
#include <chrono>


struct OperationCount {
	int putCount = 0;
	int getCount = 0;
};


struct DebugRunTime {
	std::chrono::system_clock::time_point start, mid, end;
	double total_time = 0.0;
	double mid_time = 0.0;
	double prev_time = 0.0;
};


struct RunTime {
	std::chrono::system_clock::time_point start, end;
	double time = 0.0;
};


void PrintThroughputToFile(const int& fd, 
						   const double& mid_time, 
						   const double& prev_time, 
						   const int& put_count, 
						   const int& get_count);
double ReturnRunTime(const std::chrono::system_clock::time_point& start,
					 const std::chrono::system_clock::time_point& end);
int FileOpen(const std::string& file_path);
