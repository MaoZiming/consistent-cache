#include "globals.hpp"

// Definition of the globals
std::atomic<unsigned long long> query_count(0);
std::unordered_map<int, std::string> cache;
std::shared_mutex cache_mutex;

void log_qps() {
    cout << "log_qps thread started!" << endl;
    cout.flush();
    while (true) {
        this_thread::sleep_for(chrono::seconds(1));
        unsigned long long count = query_count.exchange(0);
        cout << "QPS: " << count << endl;
        cout.flush();
    }
}