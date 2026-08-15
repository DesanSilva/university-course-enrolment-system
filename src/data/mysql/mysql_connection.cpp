#include "nexusenroll/data/mysql/mysql_connection.hpp"

#include <mysql.h>

#include <cstdlib>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace nexusenroll::data::mysql {

using namespace common;
using namespace std;

namespace {

const char* environmentValue(const char* name) {
    const char* value = getenv(name);
    return value && *value ? value : nullptr;
}

}

Result<MySqlConfig> loadMySqlConfigFromEnvironment() {
    MySqlConfig config;
    const bool hostConfigured = environmentValue("NEXUSENROLL_DB_HOST") != nullptr;
    if (const char* host = environmentValue("NEXUSENROLL_DB_HOST")) {
        config.host = host;
    }
    if (const char* database = environmentValue("NEXUSENROLL_DB_NAME")) {
        config.database = database;
    }
    if (const char* user = environmentValue("NEXUSENROLL_DB_USER")) {
        config.user = user;
    }
    if (const char* password = getenv("NEXUSENROLL_DB_PASSWORD")) {
        config.password = password;
    }
    if (const char* socket = environmentValue("NEXUSENROLL_DB_SOCKET")) {
        config.unixSocket = socket;
        if (!hostConfigured) {
            config.host = "localhost";
        }
    }
    if (const char* port = environmentValue("NEXUSENROLL_DB_PORT")) {
        try {
            const unsigned long parsed = stoul(port);
            if (parsed == 0 || parsed > 65535) {
                throw out_of_range("database port");
            }
            config.port = static_cast<unsigned int>(parsed);
        } catch (const exception&) {
            return Result<MySqlConfig>::failure(
                "MYSQL_CONFIG_INVALID", "NEXUSENROLL_DB_PORT must be a valid TCP port.");
        }
    }
    if (config.user.empty()) {
        return Result<MySqlConfig>::failure(
            "MYSQL_CONFIG_INVALID", "NEXUSENROLL_DB_USER is required.");
    }
    return Result<MySqlConfig>::success(move(config));
}

class MySqlConnection::Implementation {
public:
    explicit Implementation(MySqlConfig config) : config_(move(config)) {}

    ~Implementation() {
        if (connection_) {
            mysql_close(connection_);
        }
    }

    Result<void> ensureConnected() {
        if (connection_) {
            return Result<void>::success();
        }
        if (config_.user.empty() || config_.database.empty()) {
            return Result<void>::failure(
                "MYSQL_CONFIG_INVALID", "MySQL user and database must be configured.");
        }

        MYSQL* candidate = mysql_init(nullptr);
        if (!candidate) {
            return Result<void>::failure(
                "MYSQL_INITIALIZATION_FAILED", "The MySQL client could not be initialized.");
        }
        mysql_options(candidate, MYSQL_SET_CHARSET_NAME, "utf8mb4");
        unsigned int connectTimeoutSeconds = 5;
        mysql_options(candidate, MYSQL_OPT_CONNECT_TIMEOUT, &connectTimeoutSeconds);
        const char* socket = config_.unixSocket.empty() ? nullptr : config_.unixSocket.c_str();
        connection_ = mysql_real_connect(
            candidate,
            config_.host.c_str(),
            config_.user.c_str(),
            config_.password.c_str(),
            config_.database.c_str(),
            config_.port,
            socket,
            0);
        if (!connection_) {
            const string message = mysql_error(candidate);
            mysql_close(candidate);
            return Result<void>::failure("MYSQL_CONNECTION_FAILED", message);
        }
        return Result<void>::success();
    }

    Result<void> executeUnlocked(const string& sql) {
        auto connected = ensureConnected();
        if (!connected) {
            return connected;
        }
        if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
            return Result<void>::failure("MYSQL_STATEMENT_FAILED", mysql_error(connection_));
        }
        MYSQL_RES* result = mysql_store_result(connection_);
        if (result) {
            mysql_free_result(result);
        } else if (mysql_field_count(connection_) != 0) {
            return Result<void>::failure("MYSQL_RESULT_FAILED", mysql_error(connection_));
        }
        return Result<void>::success();
    }

    Result<Rows> queryUnlocked(const string& sql) {
        auto connected = ensureConnected();
        if (!connected) {
            return Result<Rows>::failure(connected.error().code, connected.error().message);
        }
        if (mysql_real_query(connection_, sql.data(), sql.size()) != 0) {
            return Result<Rows>::failure("MYSQL_STATEMENT_FAILED", mysql_error(connection_));
        }

        MYSQL_RES* result = mysql_store_result(connection_);
        if (!result) {
            return Result<Rows>::failure("MYSQL_RESULT_FAILED", mysql_error(connection_));
        }
        const unsigned int fieldCount = mysql_num_fields(result);
        Rows rows;
        while (MYSQL_ROW mysqlRow = mysql_fetch_row(result)) {
            const unsigned long* lengths = mysql_fetch_lengths(result);
            Row row;
            row.reserve(fieldCount);
            for (unsigned int index = 0; index < fieldCount; ++index) {
                row.emplace_back(mysqlRow[index] ? string(mysqlRow[index], lengths[index]) : "");
            }
            rows.push_back(move(row));
        }
        mysql_free_result(result);
        return Result<Rows>::success(move(rows));
    }

    MySqlConfig config_;
    MYSQL* connection_{nullptr};
    mutable recursive_mutex mutex_;
};

MySqlConnection::MySqlConnection(MySqlConfig config)
    : implementation_(make_unique<Implementation>(move(config))) {}

MySqlConnection::~MySqlConnection() = default;
MySqlConnection::MySqlConnection(MySqlConnection&&) noexcept = default;
MySqlConnection& MySqlConnection::operator=(MySqlConnection&&) noexcept = default;

bool MySqlConnection::isConnected() const noexcept {
    if (!implementation_) {
        return false;
    }
    lock_guard<recursive_mutex> lock(implementation_->mutex_);
    return implementation_->connection_ != nullptr;
}

Result<void> MySqlConnection::execute(const string& sql) {
    lock_guard<recursive_mutex> lock(implementation_->mutex_);
    return implementation_->executeUnlocked(sql);
}

Result<Rows> MySqlConnection::query(const string& sql) {
    lock_guard<recursive_mutex> lock(implementation_->mutex_);
    return implementation_->queryUnlocked(sql);
}

Result<void> MySqlConnection::transaction(const TransactionOperation& operation) {
    lock_guard<recursive_mutex> lock(implementation_->mutex_);
    if (!operation) {
        return Result<void>::failure(
            "INVALID_TRANSACTION", "A transaction requires an operation.");
    }
    auto started = implementation_->executeUnlocked("START TRANSACTION");
    if (!started) {
        return started;
    }
    try {
        auto result = operation(*this);
        if (!result) {
            implementation_->executeUnlocked("ROLLBACK");
            return result;
        }
        auto committed = implementation_->executeUnlocked("COMMIT");
        if (!committed) {
            implementation_->executeUnlocked("ROLLBACK");
        }
        return committed;
    } catch (const exception& exception) {
        implementation_->executeUnlocked("ROLLBACK");
        return Result<void>::failure("TRANSACTION_EXCEPTION", exception.what());
    } catch (...) {
        implementation_->executeUnlocked("ROLLBACK");
        return Result<void>::failure(
            "TRANSACTION_EXCEPTION", "The transaction failed with an unknown exception.");
    }
}

string MySqlConnection::escape(const string& value) {
    lock_guard<recursive_mutex> lock(implementation_->mutex_);
    auto connected = implementation_->ensureConnected();
    if (!connected) {
        throw runtime_error(connected.error().message);
    }
    string escaped(value.size() * 2 + 1, '\0');
    const auto length = mysql_real_escape_string(
        implementation_->connection_, escaped.data(), value.data(), value.size());
    escaped.resize(length);
    return escaped;
}

}
