#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <iostream>
#include <thread>  // for std::this_thread::sleep_for
#include <chrono>  // for std::chrono::milliseconds
#include <random>   // for random number generation
#include <vector>   // for vector to store keys
#include <string>
#include <sstream>
#include <iomanip>
#include <unordered_map> // for local hashmap
#include <mutex>    // for thread-safe access to hashmap
#include <atomic>   // For atomic counters
#include "globals.hpp"

using namespace std;
std::atomic<unsigned long long> query_count(0);  // Atomic counter to track executed queries



const int NUM_KEYS = 100000;      // Number of keys
const int VALUE_SIZE = 10240;     // 10KB value size (10 * 1024 bytes)
bool use_cache = false;            // Flag to indicate DB vs. cache mode

unordered_map<int, string> cache; // Local cache
mutex cache_mutex;                // Mutex for thread-safe access to cache

string generate_random_value(int key, const string& timestamp) {
    string value;
    value.resize(VALUE_SIZE - (timestamp.length() + to_string(key).length() + 2), 'A');  // Fill remaining with 'A' characters

    // Append the key and timestamp to the value
    value += "KEY:" + to_string(key) + ";TIMESTAMP:" + timestamp + ";";

    return value;
}

string get_current_timestamp() {
    // Get the current time from system_clock
    auto now = chrono::system_clock::now();
    time_t now_time_t = chrono::system_clock::to_time_t(now);

    // Convert to a string representation
    stringstream ss;
    ss << put_time(localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");

    return ss.str();
}

void readFromDB(int key, string& value, sql::Connection* thread_con) {
    sql::PreparedStatement *thread_selectStmt = thread_con->prepareStatement(
        R"(
            SELECT db_timestamp, db_payload
            FROM hello_store
            WHERE db_key = ?
        )");
    thread_selectStmt->setString(1, to_string(key));
    sql::ResultSet *res = thread_selectStmt->executeQuery();
    if (res->next()) {
        long long db_timestamp = res->getInt64("db_timestamp");
        value = res->getString("db_payload");
    } else {
        value = ""; // No value found
    }
    delete res;
    delete thread_selectStmt;
}

void writeToDB(int key, const string& timestamp, const string& value, sql::Connection* thread_con) {
    sql::PreparedStatement *thread_insertStmt = thread_con->prepareStatement(
        R"(
            INSERT INTO hello_store (db_key, db_timestamp, db_payload)
            VALUES (?, ?, ?)
            ON DUPLICATE KEY UPDATE
                db_payload = IF(db_timestamp <= ?, VALUES(db_payload), db_payload),
                db_timestamp = IF(db_timestamp <= ?, ?, db_timestamp)
        )");
    thread_insertStmt->setString(1, to_string(key));
    thread_insertStmt->setInt64(2, stoll(timestamp));
    thread_insertStmt->setString(3, value);
    thread_insertStmt->setInt64(4, stoll(timestamp));
    thread_insertStmt->setInt64(5, stoll(timestamp));
    thread_insertStmt->setInt64(6, stoll(timestamp));
    try {
        thread_insertStmt->executeUpdate();
    } catch (sql::SQLException &e) {
        cerr << "Insert error: " << e.what() << endl;
    }
    delete thread_insertStmt;
}

void handleRead(int key, sql::Connection* thread_con) {
    string value;

    if (use_cache) {
        {
            lock_guard<mutex> lock(cache_mutex);
            if (cache.find(key) != cache.end()) {
                // Cache hit
                value = cache[key];
                return;
            }
        }
        // Cache miss: Fetch from DB
        readFromDB(key, value, thread_con);

        // Update cache
        if (!value.empty()) {
            lock_guard<mutex> lock(cache_mutex);
            cache[key] = value;
        }
    } else {
        // Direct DB read
        readFromDB(key, value, thread_con);
    }
}

void handleWrite(int key, const string& timestamp, const string& value, sql::Connection* thread_con) {
    if (use_cache) {
        // Update cache
        {
            lock_guard<mutex> lock(cache_mutex);
            cache[key] = value;
        }
    }
    // Update DB
    writeToDB(key, timestamp, value, thread_con);
}

int main() {
    try {
        // Debug: Print before connecting
        cout << "Connecting to TiDB..." << endl;

        // Initialize the MySQL driver
        sql::mysql::MySQL_Driver *driver;
        sql::Connection *con;

        driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect("tcp://tidb-azure-westus-ziming.dev.databricks.com:4000", "ziming", "123456");
        con->setClientOption("connect_timeout", "600");  // Increase connection timeout (seconds)
        con->setClientOption("wait_timeout", "28800");  // Increase wait timeout (seconds)

        // Debug: Print after successful connection
        cout << "Connected to TiDB" << endl;

        // Create the database if it doesn't exist
        sql::Statement *stmt = con->createStatement();
        stmt->execute("CREATE DATABASE IF NOT EXISTS hello_world");

        // Select the database to use
        stmt->execute("USE hello_world");

        // Create the table with db_key, db_timestamp, and db_payload
        stmt->execute(R"(
            CREATE TABLE IF NOT EXISTS `hello_store` (
                `db_key` VARBINARY(64) NOT NULL UNIQUE,
                `db_timestamp` BIGINT NOT NULL,
                `db_payload` LONGBLOB NOT NULL,
                PRIMARY KEY (`db_key`)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8;
        )");

        // Random number generator for uniform load
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> key_dist(1, NUM_KEYS);  // Uniform distribution for keys
        uniform_int_distribution<> action_dist(0, 9);      // 90% reads, 10% writes
        int num_threads = 40;

        if (use_cache)
        {
            num_threads = 15; // Check QPS.
        } else {
            num_threads = 40;
        }
        int sleep_microseconds = 1'000'000 / (10000 / num_threads);

        // Thread for generating queries at 10k qps
        auto query_thread = [&]() {
            cout << "Thread started!" << endl;

            // Create a new connection for each thread
            sql::Connection *thread_con = driver->connect("tcp://tidb-azure-westus-ziming.dev.databricks.com:4000", "ziming", "123456");
            thread_con->setClientOption("connect_timeout", "600");
            thread_con->setClientOption("wait_timeout", "28800");

            // Select the database to use
            sql::Statement *stmt = thread_con->createStatement();
            stmt->execute("USE hello_world");

            while (true) {
                int action = action_dist(gen);  // Decide if it's a read or write (10% write)
                int key = key_dist(gen);        // Random key for read or write
                string timestamp = get_current_timestamp(); // Example timestamp (use current time if needed)
                string value = generate_random_value(key, timestamp);  // Generate value with key and timestamp

                try {
                    if (action < 9) {  // 90% chance to read
                        handleRead(key, thread_con);
                    } else {  // 10% chance to write
                        handleWrite(key, timestamp, value, thread_con);
                    }
                    query_count++;  // Increment the query count

                } catch (sql::SQLException &e) {
                    cerr << "Error during query execution: " << e.what() << endl;
                }

//                std::this_thread::sleep_for(std::chrono::microseconds(sleep_microseconds));
            }

            // Clean up after the thread finishes
            delete thread_con;
        };

        thread qps_thread(log_qps);
        qps_thread.detach(); // Detach to run independently

        // Launch multiple threads to handle the load
        vector<thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.push_back(thread(query_thread));
        }

        // Join threads
        for (auto &t : threads) {
            t.join();
        }

        qps_thread.join(); // Join the QPS thread

        // Clean up
        delete stmt;
        delete con;
    } catch (sql::SQLException &e) {
        // Debug: Print any connection error
        cout << "Error: " << e.what() << endl;
    }

    return 0;
}
