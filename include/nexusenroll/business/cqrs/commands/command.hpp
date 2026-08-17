#pragma once

#include "nexusenroll/common/result.hpp"

namespace nexusenroll::business::cqrs::commands {

using CommandResult = common::Result<void>;

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual CommandResult execute() = 0;
};

}
