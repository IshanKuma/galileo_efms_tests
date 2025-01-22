/**
 * @file DatabaseUtilities.hpp
 * @brief Thread-safe database utility class implementing singleton pattern with connection pooling
*/

#ifndef DATABASESERVICE_HPP
#define DATABASESERVICE_HPP

#include <pqxx/pqxx>
#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include <memory>
#include <string_view>
#include <optional>
#include <sstream>
#include <queue>

using namespace std;

class DatabaseUtilities {
public:
    // Connection parameters struct
    struct ConnectionParams {
        string host;
        string user;
        string password;
        string dbname;
        uint16_t port = 5432;

        // Convert parameters to connection string
        string toConnectionString() const {
            stringstream ss;
            ss << "host=" << host
                << " user=" << user
                << " password=" << password
                << " dbname=" << dbname
                << " port=" << port;
            return ss.str();
        }
    };

    /**
     * @brief Get singleton instance of DatabaseUtilities
     * @return Reference to DatabaseUtilities instance
     */
    static DatabaseUtilities& getInstance(const ConnectionParams& params);
    
    void initializeConnectionPool();
    shared_ptr<pqxx::connection> getConnection();
    void releaseConnection(shared_ptr<pqxx::connection> conn);

    /**
     * @brief Begin a new database transaction
     * @throws runtime_error if a transaction is already active
     */
    void beginTransaction();

    /**
     * @brief Commit the current transaction
     * @throws runtime_error if no transaction is active
     */
    void commitTransaction();

    /**
     * @brief Rollback the current transaction
     * @throws runtime_error if not transaction is active
     */
    void rollbackTransaction();

    /**
     * @brief Execute INSERT query and return last inserted ID
     * @param query SQL query with parameter placeholders
     * @param values Vector of parameter values
     * @return Last inserted ID
     * @throws std::runtime_error if operation fails
     */
    int executeInsert(const string_view query);

    int executeBatchInsert(const string_view query, bool passthrough);

    /**
     * @brief Execute SELECT query and return results
     * @param query SQL query with parameter placeholders
     * @param values Vector of parameter values
     * @return Vector of rows, each containing vector of column values
     * @throw std::runtime_error if operation fails
     */
    vector<vector<string>> executeSelect(const string_view query);

    /**
     * @brief Execute UPDATE query and return number of affected rows
     * @param query SQL query with parameter placeholders
     * @param values Vector of parameter values
     * @return Number of affected rows
     * @throws runtime_error if operation fails
     */
    int executeUpdate(const string_view query);

    /**
     * @brief Execute DELETE query and return number of affected rows
     * @param query SQL query with parameter placeholders
     * @param values Vector of parameter values
     * @return Number of affected rows
     * @throws std::runtime_error if operation fails
     */
    int executeDelete(const string_view query);

private:
    static constexpr int POOL_SIZE = 16;
    
    explicit DatabaseUtilities(const ConnectionParams& params);
    ~DatabaseUtilities();

    DatabaseUtilities(const DatabaseUtilities&) = delete;
    DatabaseUtilities& operator=(const DatabaseUtilities&) = delete;

    ConnectionParams connectionParams;
    queue<shared_ptr<pqxx::connection>> connectionPool; // Connection pool
    mutex poolMutex; // Mutex for thread safety

    thread_local static shared_ptr<pqxx::work> currentTransaction; // Active transaction object
    thread_local static shared_ptr<pqxx::connection> currentConnection;
};

#endif