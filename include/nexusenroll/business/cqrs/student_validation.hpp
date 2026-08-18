#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"

namespace nexusenroll::business::cqrs {

common::Result<domain::Student> validateActiveStudent(
    common::StudentId studentId,
    const data::contracts::IUserStore& userStore);

}
