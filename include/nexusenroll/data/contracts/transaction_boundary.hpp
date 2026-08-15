#pragma once

#include "nexusenroll/common/result.hpp"

#include <functional>

namespace nexusenroll::data::contracts {

class ITransactionBoundary {
public:
    using Operation = std::function<common::Result<void>()>;

    virtual ~ITransactionBoundary() = default;
    virtual common::Result<void> executeTransaction(const Operation& operation) = 0;
};

}
