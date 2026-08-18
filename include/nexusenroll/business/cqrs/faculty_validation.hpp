#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"

namespace nexusenroll::business::cqrs {

common::Result<domain::Faculty> validateActiveFaculty(
    common::FacultyId facultyId,
    const data::contracts::IUserStore& userStore);

}
