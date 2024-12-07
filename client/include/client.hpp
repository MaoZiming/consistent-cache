// client.hpp
#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <grpcpp/grpcpp.h>
#include <myproto/consistent_cache.grpc.pb.h> // Generated from your .proto file
#include <string>
#include <memory>
#include <thread>

class ConsistentCacheClient {
public:
    // Constructor: Initializes the gRPC client
    ConsistentCacheClient(std::shared_ptr<grpc::Channel> channel);

    ConsistentCacheClient(const std::vector<std::shared_ptr<grpc::Channel>>& channels);

    // Method to perform a read operation
    std::string Read(int key);

    // Method to perform a write operation
    bool Write(int key, const std::string& timestamp, const std::string& value);

    // Asynchronous methods
//    void AsyncRead(int key, std::function<void(const std::string&)> callback);
//    void AsyncWrite(int key, const std::string& timestamp, const std::string& value, std::function<void(bool)> callback);

//    void StartCompletionQueue();

    consistent_cache::ConsistentCache::Stub* get_stub();

private:
    std::vector<std::unique_ptr<consistent_cache::ConsistentCache::Stub>> stubs_;
//    grpc::CompletionQueue cq_read_; // For handling asynchronous operations
//    grpc::CompletionQueue cq_write_; // For handling asynchronous operations
    size_t current_channel_index_;  // Index to select the next channel

};

// Utility functions to generate a random value and timestamp
std::string generate_random_value(int key, const std::string& timestamp);
std::string get_current_timestamp();
bool checkResult(const std::string& result, int key);

#endif // CLIENT_HPP
