// main.cpp
#include "client.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include "globals.hpp"

using namespace std;

int main() {
    try {
        // Create a gRPC channel
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> key_dist(1, NUM_KEYS);  // Uniform distribution for keys
        uniform_int_distribution<> action_dist(0, 9);      // 90% reads, 10% writes

        auto query_thread = [&]() {
            cout << "Thread started!" << endl;
            std::vector<std::shared_ptr<grpc::Channel>> channels = {
                grpc::CreateChannel("10.11.152.207:50051", grpc::InsecureChannelCredentials()),
                grpc::CreateChannel("10.11.179.48:50051", grpc::InsecureChannelCredentials()),
                grpc::CreateChannel("10.11.179.228:50051", grpc::InsecureChannelCredentials()),
                grpc::CreateChannel("10.11.142.213:50051", grpc::InsecureChannelCredentials()),
                grpc::CreateChannel("10.11.231.224:50051", grpc::InsecureChannelCredentials())
            };

            // Pass the vector of channels to the client constructor
            ConsistentCacheClient client(channels);

            while (true) {
                int action = action_dist(gen);  // Decide if it's a read or write (10% write)
                int key = key_dist(gen);        // Random key for read or write
                string timestamp = get_current_timestamp(); // Example timestamp (use current time if needed)
                string value = generate_random_value(key, timestamp);  // Generate value with key and timestamp

                try {
                    if (action < 9) {  // 90% chance to read
                        string result = client.Read(key);
                        if (checkResult(result, key))
                            cerr << "Read failed for key" << key << endl;
                    } else {  // 10% chance to write
                        bool success = client.Write(key, timestamp, value);
                        if (!success) {
                            cerr << "Write failed for key " << key << endl;
                        }
                    }
                    query_count++;  // Increment the query count
                } catch (const exception& e) {
                    cerr << "Error during gRPC execution: " << e.what() << endl;
                }
            }
        };

        thread qps_thread(log_qps);
        qps_thread.detach(); // Detach to run independently

        vector<thread> threads;
        for (int i = 0; i < NUM_CLIENT_THREADS; ++i) {
            threads.push_back(thread(query_thread));
        }

        // Join threads
        for (auto& t : threads) {
            t.join();
        }

        qps_thread.join();

    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}

//int main_async() {
//    try {
//
//        random_device rd;
//        mt19937 gen(rd());
//        uniform_int_distribution<> key_dist(1, NUM_KEYS);  // Uniform distribution for keys
//        uniform_int_distribution<> action_dist(0, 9);      // 90% reads, 10% writes
//
//        int num_threads = 10;
//        auto query_thread = [&]() {
//            cout << "Thread started!" << endl;
//
//            // Create a gRPC channel
//            std::vector<std::shared_ptr<grpc::Channel>> channels = {
//                grpc::CreateChannel("10.11.213.24:50051", grpc::InsecureChannelCredentials()),
//                grpc::CreateChannel("10.11.191.243:50051", grpc::InsecureChannelCredentials()),
//                grpc::CreateChannel("10.11.191.163:50051", grpc::InsecureChannelCredentials()),
//                grpc::CreateChannel("10.11.222.61:50051", grpc::InsecureChannelCredentials()),
//                grpc::CreateChannel("10.11.135.35:50051", grpc::InsecureChannelCredentials())
//            };
//            ConsistentCacheClient client(channels);
//            client.StartCompletionQueue();  // Start the thread for writes
//            while (true) {
//                int action = action_dist(gen);  // Decide if it's a read or write (10% write)
//                int key = key_dist(gen);        // Random key for read or write
//                string timestamp = get_current_timestamp(); // Example timestamp
//                string value = generate_random_value(key, timestamp);  // Generate value with key and timestamp
//                try {
//                    if (action < 9) {  // 90% chance to read
//                        client.AsyncRead(key, [&](const string& result) {
//                            query_count++;
//                        });
//                    } else {  // 10% chance to write
//                        client.AsyncWrite(key, timestamp, value, [&](bool success) {
//                            if (success) {
//                                query_count++;  // Increment the query count for successful write
//                            } else {
//                                cerr << "AsyncWrite failed for key " << key << endl;
//                            }
//                        });
//                    }
//                } catch (const exception& e) {
//                    cerr << "Error during gRPC execution: " << e.what() << endl;
//                }
//                this_thread::sleep_for(chrono::milliseconds(10));  // Simulate a small delay to throttle requests
//            }
//        };
//
//        // Launch a separate thread to log QPS
//        thread qps_thread(log_qps);
//        qps_thread.detach();  // Detach to run independently
//
//        vector<thread> threads;
//        for (int i = 0; i < num_threads; ++i) {
//            threads.emplace_back(query_thread);
//        }
//
//        for (auto& t : threads) {
//            t.join();
//        }
//        qps_thread.join();
//    } catch (const std::exception& e) {
//        cerr << "Error: " << e.what() << endl;
//    }
//
//    return 0;
//}