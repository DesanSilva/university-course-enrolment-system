#pragma once

#include "crow_all.h"
#include "nexusenroll/business/notifications/seat_notification.hpp"
#include "nexusenroll/business/sessions/demonstration_session_service.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/enrollment_store.hpp"
#include "nexusenroll/data/contracts/grade_store.hpp"
#include "nexusenroll/data/contracts/program_store.hpp"
#include "nexusenroll/data/contracts/transaction_boundary.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"
#include "nexusenroll/data/contracts/waitlist_store.hpp"

namespace nexusenroll::presentation::api {

struct StudentRouteDependencies {
    const data::contracts::IUserStore& userStore;
    const data::contracts::ICourseStore& courseStore;
    data::contracts::IEnrollmentStore& enrollmentStore;
    const data::contracts::IProgramStore& programStore;
    const data::contracts::IGradeStore& gradeStore;
    data::contracts::IWaitlistStore& waitlistStore;
    data::contracts::ITransactionBoundary& transactionBoundary;
    const business::notifications::NotificationPublisher& notificationPublisher;
};

void registerRoutes(
    crow::SimpleApp& application,
    const business::sessions::DemonstrationSessionService& sessionService,
    StudentRouteDependencies studentDependencies);

}
