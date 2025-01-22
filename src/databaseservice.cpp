/**
 * @file DatabaseUtilities.cpp
 * @brief Implementation of DatabaseUtilities class for database operations
 * @details Provides thread-safe database operations with connection pooling
 */

#include "databaseservice.hpp"
#include <iostream>
#include <stdexcept>
#include <pqxx/transaction>
#include <pqxx/util>
#include <memory>
#include <thread>
#include <mutex>
#include <string_view>


using namespace std;

// Thread-local storage for transaction management
thread_local shared_ptr<pqxx::work> DatabaseUtilities::currentTransaction = nullptr;
thread_local shared_ptr<pqxx::connection> DatabaseUtilities::currentConnection = nullptr;

// Singleton instance getter
DatabaseUtilities& DatabaseUtilities::getInstance(const ConnectionParams& params) {
    static DatabaseUtilities instance(params);
    return instance;
}

DatabaseUtilities::DatabaseUtilities(const ConnectionParams& params) : connectionParams(params) {
    initializeConnectionPool();
}

DatabaseUtilities::~DatabaseUtilities() {
    lock_guard<mutex> lock(poolMutex);
    connectionPool = queue<shared_ptr<pqxx::connection>>();     // Clear the pool
}

// Initialize the connection pool with the specified size
void DatabaseUtilities::initializeConnectionPool() {
    lock_guard<mutex> lock(poolMutex);  // Thread-safe initialization

    // Clear existing connections if any
    while (!connectionPool.empty()) {
        connectionPool.pop();
    }

    string connStr = connectionParams.toConnectionString();

for (int i = 0; i < POOL_SIZE; ++i) {
        try {
            auto conn = make_shared<pqxx::connection>(connStr);
            if (!conn->is_open()) {
                throw runtime_error("Failed to open connection " + to_string(i + 1));
            }
            connectionPool.push(conn);
        } catch (const exception& e) {
            // Log error and continue trying to fill the pool
            cerr << "Failed to create connection " << i + 1 << ": " << e.what() << endl;
        }
    }
    
    if (connectionPool.empty()) {
        throw runtime_error("Failed to initialize any connections in the pool");
    }
}

shared_ptr<pqxx::connection> DatabaseUtilities::getConnection() {
    lock_guard<mutex> lock(poolMutex);
    if (connectionPool.empty()) {
        throw runtime_error("No available connections in the pool");
    }

    auto conn = connectionPool.front();
    connectionPool.pop();
    return conn;
}

void DatabaseUtilities::releaseConnection(shared_ptr<pqxx::connection> conn) {
    lock_guard<mutex> lock(poolMutex);
    connectionPool.push(conn);
}

// Begin a new database transaction
void DatabaseUtilities::beginTransaction() {
    if (currentTransaction) {
        throw runtime_error("Transaction already in progress");
    }

    currentConnection = getConnection();
    currentTransaction = make_shared<pqxx::work>(*currentConnection);    // Create new transaction
}

// Commit the current transaction
void DatabaseUtilities::commitTransaction() {
    if (!currentTransaction) {
        throw runtime_error("No active transaction to commit");
    }
    currentTransaction->commit();
    currentTransaction.reset();
    releaseConnection(currentConnection);
    currentConnection.reset();
}

// Rollback the current transaction
void DatabaseUtilities::rollbackTransaction() {
    if (!currentTransaction) {
        throw runtime_error("No active transaction to rollback");
    }

    currentTransaction->abort();
    currentTransaction.reset();
    releaseConnection(currentConnection);
    currentConnection.reset();
}

// In DatabaseUtilities.cpp

int DatabaseUtilities::executeInsert(const string_view query) {
    bool needsTransaction = !currentTransaction;
    
    if (needsTransaction) {
        beginTransaction();
    }

    try {
        pqxx::result result = currentTransaction->exec(string(query));
        int lastInsertId = result[0][0].as<int>();
        
        if (needsTransaction) {
            commitTransaction();
        }
        return lastInsertId;
    } catch (const exception& e) {
        if (needsTransaction) {
            rollbackTransaction();
        }
        throw runtime_error(string("Error during insert: ") + e.what());
    }
}

vector<vector<string>> DatabaseUtilities::executeSelect(const string_view query) {
    bool needsTransaction = !currentTransaction;
    
    if (needsTransaction) {
        beginTransaction();
    }

    try {
        pqxx::result result = currentTransaction->exec(string(query));
        vector<vector<string>> rows;
        rows.reserve(result.size());

        for (const auto& row : result) {
            vector<string> rowData;
            rowData.reserve(row.size());

            for (const auto& field : row) {
                rowData.push_back(field.as<string>());
            }
            rows.push_back(move(rowData));
        }

        if (needsTransaction) {
            commitTransaction();
        }
        return rows;
    } catch (const exception& e) {
        if (needsTransaction) {
            rollbackTransaction();
        }
        throw runtime_error(string("Error during select: ") + e.what());
    }
}

int DatabaseUtilities::executeUpdate(const string_view query) {
    bool needsTransaction = !currentTransaction;
    
    if (needsTransaction) {
        beginTransaction();
    }

    try {
        pqxx::result result = currentTransaction->exec(string(query));
        int affectedRows = result.affected_rows();
        
        if (needsTransaction) {
            commitTransaction();
        }
        return affectedRows;
    } catch (const exception& e) {
        if (needsTransaction) {
            rollbackTransaction();
        }
        throw runtime_error(string("Error during update: ") + e.what());
    }
}

int DatabaseUtilities::executeDelete(const string_view query) {
    bool needsTransaction = !currentTransaction;
    
    if (needsTransaction) {
        beginTransaction();
    }

    try {
        pqxx::result result = currentTransaction->exec(string(query));
        int affectedRows = result.affected_rows();
        
        if (needsTransaction) {
            commitTransaction();
        }
        return affectedRows;
    } catch (const exception& e) {
        if (needsTransaction) {
            rollbackTransaction();
        }
        throw runtime_error(string("Error during delete: ") + e.what());
    }
}

int DatabaseUtilities::executeBatchInsert(const string_view query, bool passthrough) {
    bool needsTransaction = !currentTransaction;
    
    if (needsTransaction) {
        beginTransaction();
    }

    try {
        pqxx::result result = currentTransaction->exec(string(query));
        int insertedRows = result.affected_rows();
        
        if (needsTransaction) {
            commitTransaction();
        }
        return insertedRows;
    } catch (const exception& e) {
        if (needsTransaction) {
            if (passthrough) {
                // passthrough = 1: Keep successful insertions
                try {
                    commitTransaction();
                    return 0;  // Return 0 to indicate partial failure
                } catch (const exception& e) {
                    rollbackTransaction();
                    throw runtime_error(string("Error committing partial batch: ") + e.what());
                }
            } else {
                // passthrough = 0: Rollback everything if any insert fails
                rollbackTransaction();
                return 0;  // Return 0 to indicate batch failure
            }
        }
        throw runtime_error(string("Error during batch insert: ") + e.what());
    }
}