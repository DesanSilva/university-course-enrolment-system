#pragma once

#include "crow_all.h"
#include "nexusenroll/data/contracts/change_request_store.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/enrollment_store.hpp"
#include "nexusenroll/data/contracts/grade_store.hpp"
#include "nexusenroll/data/contracts/program_store.hpp"
#include "nexusenroll/data/contracts/transaction_boundary.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"
#include "nexusenroll/data/contracts/waitlist_store.hpp"

namespace nexusenroll::presentation::api {

struct AdministratorRouteDependencies {
    data::contracts::IUserStore& userStore;
    data::contracts::ICourseStore& courseStore;
    data::contracts::IEnrollmentStore& enrollmentStore;
    data::contracts::IProgramStore& programStore;
    const data::contracts::IGradeStore& gradeStore;
    data::contracts::IChangeRequestStore& changeRequestStore;
    data::contracts::IWaitlistStore& waitlistStore;
    data::contracts::ITransactionBoundary& transactionBoundary;
};

void registerAdministratorRoutes(
    crow::SimpleApp& application,
    AdministratorRouteDependencies dependencies);

}
