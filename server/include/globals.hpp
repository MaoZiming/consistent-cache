#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <unordered_map>
#include <mutex>
#include <atomic>
#include <string>
#include <iostream>
#include <ostream>
#include <thread>
#include <chrono>
#include <shared_mutex>

extern std::atomic<unsigned long long> query_count;
extern std::unordered_map<int, std::string> cache;
extern std::shared_mutex cache_mutex;
using namespace std;

#define USE_CACHE

const int NUM_KEYS = 100000;      // Number of keys
const int VALUE_SIZE = 1024;     // 10KB value size (10 * 1024 bytes)
const int NUM_CLIENT_THREADS = 8;
// having numcpu’s threads
// https://grpc.io/docs/guides/performance/
const int NUM_SERVER_THREADS = 4;

extern void log_qps();
#endif
