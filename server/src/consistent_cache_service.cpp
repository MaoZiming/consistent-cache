#include "consistent_cache_service.hpp"
#include "globals.hpp"
#include <iostream>
#include <cppconn/exception.h>

using namespace std;

ConsistentCacheServiceImpl::ConsistentCacheServiceImpl(sql::mysql::MySQL_Driver* driver, const string& db_url, const string& db_user, const string& db_pass)
    : driver_(driver), db_url_(db_url), db_user_(db_user), db_pass_(db_pass) {
    sql::Connection* connection = createConnection();
    sql::Statement* stmt = connection->createStatement();
    stmt->execute(R"(
        CREATE TABLE IF NOT EXISTS `consistent_cache` (
            `db_key` VARBINARY(64) NOT NULL UNIQUE,
            `db_timestamp` BIGINT NOT NULL,
            `db_payload` LONGBLOB NOT NULL,
            PRIMARY KEY (`db_key`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8;
    )");
    delete stmt;
}

sql::Connection* ConsistentCacheServiceImpl::createConnection() {
    sql::Connection* con = driver_->connect(db_url_, db_user_, db_pass_);
    con->setClientOption("connect_timeout", "600");
    con->setClientOption("wait_timeout", "28800");

    sql::Statement* stmt = con->createStatement();
    stmt->execute("CREATE DATABASE IF NOT EXISTS consistent_cache");
    stmt->execute("USE consistent_cache");
    return con;
}

void readFromDB(int key, string& value, sql::Connection* thread_con) {
    sql::PreparedStatement *stmt_prep = thread_con->prepareStatement(
        "SELECT db_timestamp, db_payload FROM consistent_cache WHERE db_key = ?");
    stmt_prep->setString(1, to_string(key));
    sql::ResultSet* res = stmt_prep->executeQuery();
    if (res->next()) {
        long long db_timestamp = res->getInt64("db_timestamp");
        value = res->getString("db_payload");
    } else {
        value = "";
    }
    delete res;
    delete stmt_prep;
}

void writeToDB(int key, const string& timestamp, const string& value, sql::Connection* thread_con) {
    sql::PreparedStatement *stmt_prep = thread_con->prepareStatement(
        R"(
            INSERT INTO consistent_cache (db_key, db_timestamp, db_payload)
            VALUES (?, ?, ?)
            ON DUPLICATE KEY UPDATE
                db_payload = IF(db_timestamp <= ?, VALUES(db_payload), db_payload),
                db_timestamp = IF(db_timestamp <= ?, ?, db_timestamp)
        )");
    stmt_prep->setString(1, to_string(key));
    stmt_prep->setInt64(2, stoll(timestamp));
    stmt_prep->setString(3, value);
    stmt_prep->setInt64(4, stoll(timestamp));
    stmt_prep->setInt64(5, stoll(timestamp));
    stmt_prep->setInt64(6, stoll(timestamp));
    delete stmt_prep;
}

void handleRead(int key, sql::Connection* thread_con, string& value) {
#ifdef USE_CACHE
    {
        std::shared_lock<std::shared_mutex> read_lock(cache_mutex);
        if (cache.find(key) != cache.end()) {
            value = cache[key];
            return;
        }
    }
#endif
    readFromDB(key, value, thread_con);
}

void handleWrite(int key, const string& timestamp, const string& value, sql::Connection* thread_con) {
#ifdef USE_CACHE
    {
        std::unique_lock<std::shared_mutex> write_lock(cache_mutex);
        cache[key] = value;
    }
#endif
    writeToDB(key, timestamp, value, thread_con);
}
