// Source
#include <iostream>
#include <sys/socket.h>
#include "./db.h"
#include "./workload.h"

using namespace rocksdb;


/*
static void InitValue(std::string* value) {
	char buf[VALUELEN];
	for (int i = 0; i < VALUELEN; ++i)
		buf[i] = 'A' + (i % 26);
	
	buf[VALUELEN-1] = '\0';
	*value = buf;
}


static std::string ReturnRandomKey(std::mt19937 mt, const int& max_key_number) {
	std::uniform_int_distribution<> KeyDist(0, max_key_number-1);

	int n = KeyDist(mt);
	char buf[KEYLEN];

	sprintf(buf, "%09d", n);
	const std::string key = buf;

	return key;
}
*/


static void OndemandPushValues(DB* db, const int& fd) {
	while (true) {
		char key[KEYLEN+1];

		ssize_t recv_size = recv(fd, key, KEYLEN, 0);
		if (recv_size < 0) {
			std::cerr << "recv(OndemandPushValues)" << std::endl;
			exit(EXIT_FAILURE);
		}
		else if (recv_size == 0)
			break;
		
		key[KEYLEN] = '\0';
		std::string value;
		Status status = db->Get(ReadOptions(), db->DefaultColumnFamily(), key, &value);
		AssertStatus(status, "OndemandPushValues(Get)");

		if (send(fd, value.c_str(), value.length(), 0) <= 0) {
			std::cerr << "send(OndemandPushValues)" << std::endl;
			exit(EXIT_FAILURE);
		}
	}
}


void RunWorkload(DB* db, const int& rate_of_workload, const int& fd) {
	std::cout << "TransferKVsThread is started." << std::endl;

	
	if (rate_of_workload == 100) {
		std::cout << "TransferKVsThread is ended." << std::endl;
		return;
	}

	OndemandPushValues(db, fd);


	std::cout << "TransferKVsThread is ended." << std::endl;
}
