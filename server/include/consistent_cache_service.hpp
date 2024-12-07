#ifndef CONSISTENT_CACHE_SERVICE_HPP
#define CONSISTENT_CACHE_SERVICE_HPP

#include <grpcpp/grpcpp.h>
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
using namespace std::chrono;

using grpc::ServerContext;
using grpc::Status;
using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

// Utility function declarations
void readFromDB(int key, string& value, sql::Connection* thread_con);
void writeToDB(int key, const string& timestamp, const string& value, sql::Connection* thread_con);
void handleRead(int key, sql::Connection* thread_con, string& value);
void handleWrite(int key, const string& timestamp, const string& value, sql::Connection* thread_con);

class ConsistentCacheServiceImpl final : public consistent_cache::ConsistentCache::Service {
public:
    ConsistentCacheServiceImpl(sql::mysql::MySQL_Driver* driver, const string& db_url, const string& db_user, const string& db_pass);
    ~ConsistentCacheServiceImpl(){
        server_->Shutdown();
        for (auto& cq : completion_queues_)
            cq->Shutdown();
    }

    void Run()
    {
        std::string server_address("0.0.0.0:50051");
        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&async_service_);

        for (int i = 0; i < NUM_SERVER_THREADS; ++i) {
            completion_queues_.emplace_back(builder.AddCompletionQueue());
        }
        server_ = builder.BuildAndStart();
        std::cout << "Async server listening on " << server_address << std::endl;
        std::cout << "Number of threads: " << NUM_SERVER_THREADS << std::endl;

        // Start multiple threads to handle RPCs
        for (auto& cq : completion_queues_) {
            threads_.emplace_back([this, cq = cq.get()] { HandleRpcs(cq); });
        }

        for (auto& thread : threads_) {
            thread.join();
        }
    }

private:
    sql::Connection* createConnection();
    sql::mysql::MySQL_Driver* driver_;
    string db_url_;
    string db_user_;
    string db_pass_;
    std::vector<std::unique_ptr<ServerCompletionQueue>> completion_queues_;
    std::vector<std::thread> threads_;
    class CallDataBase
    {
    public:
        virtual void Proceed(bool ok) = 0;
        virtual ~CallDataBase() {}
    };

    // Implementations for each RPC method
    template <typename ServiceType, typename RequestType, typename ResponseType>
    class CallData : public CallDataBase
    {
    public:
        CallData(ServiceType *service, ServerCompletionQueue *cq, ConsistentCacheServiceImpl *impl, sql::Connection* thread_con)
            : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE), impl_(impl), thread_con_(thread_con) {}

        void Proceed(bool ok) override
        {
            if (status_ == CREATE)
            {
                status_ = PROCESS;
                RequestRPC();
            }
            else if (status_ == PROCESS)
            {
                CreateNewInstance();
                ProcessRequest();
                status_ = FINISH;
                responder_.Finish(response_, Status::OK, this);
            }
            else
            {
                delete this;
            }
        }

    protected:
        virtual void RequestRPC() = 0;
        virtual void ProcessRequest() = 0;
        virtual void CreateNewInstance() = 0;

        typename std::remove_pointer<ServiceType>::type *service_;
        ServerCompletionQueue *cq_;
        ServerContext ctx_;
        RequestType request_;
        ResponseType response_;
        ServerAsyncResponseWriter<ResponseType> responder_;
        enum CallStatus
        {
            CREATE,
            PROCESS,
            FINISH
        };
        CallStatus status_;
        ConsistentCacheServiceImpl *impl_;
        sql::Connection* thread_con_;
    };

    class GetCallData : public CallData<consistent_cache::ConsistentCache::AsyncService, consistent_cache::ReadRequest, consistent_cache::ReadResponse> {
    public:
        GetCallData(consistent_cache::ConsistentCache::AsyncService* service, ServerCompletionQueue* cq, ConsistentCacheServiceImpl* impl, sql::Connection* thread_con)
            : CallData(service, cq, impl, thread_con) {}  // Pass nullptr since we'll use `getThreadConnection()`

        void RequestRPC() override {
            service_->RequestRead(&ctx_, &request_, &responder_, cq_, cq_, this);
        }

        void ProcessRequest() override {
            int key = request_.key();
            string value;
            try {
                handleRead(key, thread_con_, value);
                response_.set_value("KEY:" + std::to_string(key));
                query_count++;
            } catch (const sql::SQLException& e) {
                cerr << "Read error: " << e.what() << endl;
            }
        }

        void CreateNewInstance() override {
            auto call = new GetCallData(service_, cq_, impl_, thread_con_);
            call->Proceed(true);
        }
    };

    class SetCallData : public CallData<consistent_cache::ConsistentCache::AsyncService, consistent_cache::WriteRequest, consistent_cache::WriteResponse> {
    public:
        SetCallData(consistent_cache::ConsistentCache::AsyncService* service, ServerCompletionQueue* cq, ConsistentCacheServiceImpl* impl, sql::Connection* thread_con)
            : CallData(service, cq, impl, thread_con) {}  // Pass nullptr since we'll use `getThreadConnection()`

        void RequestRPC() override {
            service_->RequestWrite(&ctx_, &request_, &responder_, cq_, cq_, this);
        }

        void ProcessRequest() override {
            int key = request_.key();
            string timestamp = request_.timestamp();
            string value = request_.value();
            try {
                handleWrite(key, timestamp, value, thread_con_);
                response_.set_success(true);
                query_count++;
            } catch (const sql::SQLException& e) {
                cerr << "Write error: " << e.what() << endl;
                response_.set_success(false);
            }
        }

        void CreateNewInstance() override {
            auto call = new SetCallData(service_, cq_, impl_, thread_con_);
            call->Proceed(true);
        }
    };

    void HandleRpcs(ServerCompletionQueue* cq)
    {
        sql::Connection* get_connection = createConnection();
        auto *get_call = new GetCallData(&async_service_, cq, this, get_connection);
        get_call->Proceed(true);

        sql::Connection* set_connection = createConnection();
        auto *set_call = new SetCallData(&async_service_, cq, this, set_connection);
        set_call->Proceed(true);

        void* tag;
        bool ok;
        while (cq->Next(&tag, &ok)) {
            if (ok) {
                static_cast<CallDataBase*>(tag)->Proceed(true);
            } else {
                delete static_cast<CallDataBase*>(tag);
            }
        }
    }
    consistent_cache::ConsistentCache::AsyncService async_service_;
    std::unique_ptr<Server> server_;
};

#endif
