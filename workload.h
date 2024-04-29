// Source
#include <string>
#include <random>
#include "rocksdb/db.h"

using namespace rocksdb;


void RunWorkload(DB* db, const int& fd, const int& rate);
