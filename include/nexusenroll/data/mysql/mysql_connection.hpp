#pragma once

#include "nexusenroll/common/result.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nexusenroll::data::mysql {

struct MySqlConfig {
    std::string host{"127.0.0.1"};
    unsigned int port{3306};
    std::string user;
    std::string password;
    std::string database{"nexusenroll"};
    std::string unixSocket;
};

common::Result<MySqlConfig> loadMySqlConfigFromEnvironment();

using Row = std::vector<std::string>;
using Rows = std::vector<Row>;

class MySqlConnection final {
public:
    using TransactionOperation = std::function<common::Result<void>(MySqlConnection&)>;

    explicit MySqlConnection(MySqlConfig config);
    ~MySqlConnection();

    MySqlConnection(const MySqlConnection&) = delete;
    MySqlConnection& operator=(const MySqlConnection&) = delete;
    MySqlConnection(MySqlConnection&&) noexcept;
    MySqlConnection& operator=(MySqlConnection&&) noexcept;

    bool isConnected() const noexcept;
    common::Result<void> execute(const std::string& sql);
    common::Result<Rows> query(const std::string& sql);
    common::Result<void> transaction(const TransactionOperation& operation);
    std::string escape(const std::string& value);

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}
