#ifndef CONSISTENT_CACHE_SERVICE_HPP
#define CONSISTENT_CACHE_SERVICE_HPP

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include <myproto/consistent_cache.grpc.pb.h>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>

#include "globals.hpp"

using namespace std;
using grpc::ServerBuilder;
using grpc::CallbackServerContext;
using grpc::Server;
using grpc::Status;

// Utility function declarations
void readFromDB(int key, string& value, sql::Connection* thread_con);
void writeToDB(int key, const string& timestamp, const string& value, sql::Connection* thread_con);
void handleRead(int key, sql::Connection* thread_con, string& value);
void handleWrite(int key, const string& timestamp, const string& value, sql::Connection* thread_con);

class ConsistentCacheServiceImpl final : public consistent_cache::ConsistentCache::CallbackService {
public:

    ConsistentCacheServiceImpl(sql::mysql::MySQL_Driver* driver, const string& db_url, const string& db_user, const string& db_pass);
    ~ConsistentCacheServiceImpl() {
        server_->Shutdown();
    }

    void Run() {
        std::string server_address("0.0.0.0:50051");
        ServerBuilder builder;

        // Set the number of threads in the server's thread pool
        int thread_pool_size = NUM_SERVER_THREADS; // Adjust this number as needed
        builder.SetSyncServerOption(ServerBuilder::SyncServerOption::NUM_CQS, thread_pool_size);
        builder.SetSyncServerOption(ServerBuilder::SyncServerOption::MIN_POLLERS, thread_pool_size);
        builder.SetSyncServerOption(ServerBuilder::SyncServerOption::MAX_POLLERS, thread_pool_size);


        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        server_ = builder.BuildAndStart();
        std::cout << "Callback server listening on " << server_address << std::endl;
        server_->Wait();
    }

    // Callback implementation for Read (Get)
    grpc::ServerUnaryReactor* Read(CallbackServerContext* context,
                                   const consistent_cache::ReadRequest* request,
                                   consistent_cache::ReadResponse* response) override {
        class ReadReactor : public grpc::ServerUnaryReactor {
         public:
          ReadReactor(const consistent_cache::ReadRequest* request,
                      consistent_cache::ReadResponse* response,
                      sql::Connection* connection)
              : response_(response), thread_con_(connection) {
            int key = request->key();
            try {
              string value;
              // Perform database read operation
              handleRead(key, thread_con_, value);
              response_->set_value("KEY:" + std::to_string(key));
              query_count++;
            } catch (const sql::SQLException& e) {
              cerr << "Read error: " << e.what() << endl;
              response_->set_value("ERROR");
            }
            Finish(Status::OK);
          }

         private:
          void OnDone() override {
            delete thread_con_;
            delete this;
          }

          void OnCancel() override {
            delete thread_con_;
            delete this;
          }

          consistent_cache::ReadResponse* response_;
          sql::Connection* thread_con_;
        };

        return new ReadReactor(request, response, createConnection());
    }

    // Callback implementation for Write (Set)
    grpc::ServerUnaryReactor* Write(CallbackServerContext* context,
                                    const consistent_cache::WriteRequest* request,
                                    consistent_cache::WriteResponse* response) override {
        class WriteReactor : public grpc::ServerUnaryReactor {
         public:
          WriteReactor(const consistent_cache::WriteRequest* request,
                       consistent_cache::WriteResponse* response,
                       sql::Connection* connection)
              : response_(response), thread_con_(connection) {
            int key = request->key();
            string timestamp = request->timestamp();
            string value = request->value();
            try {
              // Perform database write operation
//              handleWrite(key, timestamp, value, thread_con_);
              response_->set_success(true);
              query_count++;
            } catch (const sql::SQLException& e) {
              cerr << "Write error: " << e.what() << endl;
              response_->set_success(false);
            }
            Finish(Status::OK);
          }

         private:
          void OnDone() override {
            delete thread_con_;
            delete this;
          }

          void OnCancel() override {
            delete thread_con_;
            delete this;
          }

          consistent_cache::WriteResponse* response_;
          sql::Connection* thread_con_;
        };

        return new WriteReactor(request, response, createConnection());
    }
    sql::Connection* createConnection();

private:
    sql::mysql::MySQL_Driver* driver_;
    string db_url_;
    string db_user_;
    string db_pass_;
    std::unique_ptr<Server> server_;
};

#endif
