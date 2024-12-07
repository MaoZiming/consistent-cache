#include "consistent_cache_service.hpp"
#include "globals.hpp"
#include <grpcpp/grpcpp.h>
#include <thread>
#include <chrono>
#include <iostream>
//#include <gperftools/profiler.h>
#include <signal.h>

using namespace std;
using grpc::Server;
using grpc::ServerBuilder;

// Declare a flag to safely stop the profiler
volatile bool profiling = true;
//
//// Signal handler to stop profiling
void handle_signal(int signal) {
    if (profiling) {
//        ProfilerStop();  // Stop profiling
        std::cout << "Profiler stopped due to signal " << signal << std::endl;
    }
    exit(0);  // Exit the program
}

int main() {

//    ProfilerStart("cpu_profile.prof");

    // Set up signal handler for SIGINT (Ctrl+C) and SIGTERM
//    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    try {
        cout << "Starting gRPC server..." << endl;
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        ConsistentCacheServiceImpl service(driver, "tcp://tidb-azure-westus-ziming.dev.databricks.com:4000", "ziming", "123456");

        thread qps_thread(log_qps);
        qps_thread.detach();

        service.Run();
        qps_thread.join();

    } catch (const sql::SQLException& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}
