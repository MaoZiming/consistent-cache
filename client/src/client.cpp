// client.cpp
#include "client.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include "globals.hpp"

using namespace std;

// Constructor: Initializes the gRPC client
ConsistentCacheClient::ConsistentCacheClient(std::shared_ptr<grpc::Channel> channel)
    : current_channel_index_(0) {
        stubs_.emplace_back(consistent_cache::ConsistentCache::NewStub(channel));
    }

ConsistentCacheClient::ConsistentCacheClient(const std::vector<std::shared_ptr<grpc::Channel>>& channels)
    : current_channel_index_(0) {
    // Initialize stubs and completion queues for each channel
    for (const auto& channel : channels) {
        stubs_.emplace_back(consistent_cache::ConsistentCache::NewStub(channel));
    }
}

consistent_cache::ConsistentCache::Stub* ConsistentCacheClient::get_stub() {
    current_channel_index_ = (current_channel_index_ + 1) % stubs_.size();
    return stubs_[current_channel_index_].get();
}

// Tag structure to identify and handle this specific RPC
//struct AsyncReadTag {
//    std::shared_ptr<consistent_cache::ReadResponse> response;
//    std::function<void(const std::string&)> callback;
//};
//
//// Tag structure to identify and handle this specific RPC
//struct AsyncWriteTag {
//    std::shared_ptr<consistent_cache::WriteResponse> response;
//    std::function<void(bool)> callback;
//};


// Read method: Sends a read request to the gRPC service
std::string ConsistentCacheClient::Read(int key) {
    consistent_cache::ReadRequest request;
    request.set_key(key);
    consistent_cache::ReadResponse response;
    grpc::ClientContext context;

    grpc::Status status = get_stub()->Read(&context, request, &response);
    if (status.ok()) {
        return response.value();
    } else {
        cerr << "RPC failed: " << status.error_message() << endl;
        return "";
    }
}

//void HandleReadCompletion(void* got_tag, bool ok) {
//    auto* tag = static_cast<AsyncReadTag*>(got_tag);
//    if (ok) {
//        tag->callback(tag->response->value());
//    } else {
//        std::cerr << "AsyncRead RPC failed!" << std::endl;
//        tag->callback("");
//    }
//    delete tag;
//}
//
//void HandleWriteCompletion(void* got_tag, bool ok) {
//    auto* tag = static_cast<AsyncWriteTag*>(got_tag);
//    if (ok) {
//        tag->callback(tag->response->success());
//    } else {
//        std::cerr << "AsyncWrite RPC failed!" << std::endl;
//        tag->callback(false);
//    }
//    delete tag;
//}


//void ConsistentCacheClient::StartCompletionQueue() {
//    std::thread([this]() {
//        void* got_tag;
//        bool ok = false;
//
//        while (true) {
//            if (cq_read_.Next(&got_tag, &ok)) {
//                HandleReadCompletion(got_tag, ok);
//            }
//            if (cq_write_.Next(&got_tag, &ok)) {
//                HandleWriteCompletion(got_tag, ok);
//            }
//        }
//    }).detach();
//}
//


// Asynchronous read method
//void ConsistentCacheClient::AsyncRead(int key, std::function<void(const std::string&)> callback) {
//    auto request = std::make_shared<consistent_cache::ReadRequest>();
//    auto response = std::make_shared<consistent_cache::ReadResponse>();
//    auto context = std::make_shared<grpc::ClientContext>();
//
//    request->set_key(key);
//
//    auto tag = new AsyncReadTag{response, callback};
//
//    // Initiate asynchronous read
////    std::cout << "AsyncRead sent for key " << key << std::endl;
//    auto rpc = get_stub()->AsyncRead(context.get(), *request, &cq_read_);
//    rpc->Finish(response.get(), nullptr, tag);  // Use 'tag' to identify the operation in the CQ
//}
//
//// Write method: Sends a write request to the gRPC service
bool ConsistentCacheClient::Write(int key, const std::string& timestamp, const std::string& value) {
    consistent_cache::WriteRequest request;
    request.set_key(key);
    request.set_timestamp(timestamp);
    request.set_value(value);
    consistent_cache::WriteResponse response;
    grpc::ClientContext context;


    grpc::Status status = get_stub()->Write(&context, request, &response);
    if (status.ok()) {
        return response.success();
    } else {
        cerr << "RPC failed: " << status.error_message() << endl;
        return false;
    }
}

// Asynchronous write method
//void ConsistentCacheClient::AsyncWrite(int key, const std::string& timestamp, const std::string& value, std::function<void(bool)> callback) {
//    auto request = std::make_shared<consistent_cache::WriteRequest>();
//    auto response = std::make_shared<consistent_cache::WriteResponse>();
//    auto context = std::make_shared<grpc::ClientContext>();
//
//    request->set_key(key);
//    request->set_timestamp(timestamp);
//    request->set_value(value);
//
//    auto tag = new AsyncWriteTag{response, callback};
//
//    // Initiate asynchronous write
////    std::cout << "AsyncWrite sent for key " << key << std::endl;
//    auto rpc = get_stub()->AsyncWrite(context.get(), *request, &cq_write_);
//    rpc->Finish(response.get(), nullptr, tag);  // Use 'tag' to identify this operation
//}

// Helper function to generate a random value
std::string generate_random_value(int key, const std::string& timestamp) {
    string value;
    value.resize(VALUE_SIZE - (timestamp.length() + to_string(key).length() + 2), 'A');  // Fill remaining with 'A' characters
    value = "KEY:" + to_string(key) + ";TIMESTAMP:" + timestamp + ";" + value;
    return value;
}

// Helper function to get the current timestamp
std::string get_current_timestamp() {
    auto now = chrono::system_clock::now();
    time_t now_time_t = chrono::system_clock::to_time_t(now);

    stringstream ss;
    ss << put_time(localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool checkResult(const std::string& result, int key) {
    // Check if result is not empty
    if (result.empty()) {
        return false; // result is empty
    }

    // Construct the expected prefix
    std::string expected_prefix = "KEY:" + std::to_string(key);

    // Check if the result starts with the expected prefix
    if (result.find(expected_prefix) == 0) {
        return true; // result starts with the expected prefix
    }

    return false; // result does not match the expected prefix
}
