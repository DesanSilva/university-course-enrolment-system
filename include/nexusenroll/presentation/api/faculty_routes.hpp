#pragma once

#include "crow_all.h"
#include "nexusenroll/data/contracts/change_request_store.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/enrollment_store.hpp"
#include "nexusenroll/data/contracts/grade_store.hpp"
#include "nexusenroll/data/contracts/transaction_boundary.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"

namespace nexusenroll::presentation::api {

struct FacultyRouteDependencies {
    const data::contracts::IUserStore& userStore;
    const data::contracts::ICourseStore& courseStore;
    data::contracts::IEnrollmentStore& enrollmentStore;
    data::contracts::IGradeStore& gradeStore;
    data::contracts::IChangeRequestStore& changeRequestStore;
    data::contracts::ITransactionBoundary& transactionBoundary;
};

void registerFacultyRoutes(
    crow::SimpleApp& application,
    FacultyRouteDependencies dependencies);

}
