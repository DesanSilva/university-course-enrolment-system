#include "nexusenroll/business/notifications/seat_notification.hpp"
#include "nexusenroll/business/sessions/demonstration_session_service.hpp"
#include "nexusenroll/data/mysql/mysql_data_context.hpp"
#include "nexusenroll/presentation/api/administrator_routes.hpp"
#include "nexusenroll/presentation/api/faculty_routes.hpp"
#include "nexusenroll/presentation/api/routes.hpp"

#include "crow_all.h"

#include <iostream>
#include <memory>

using namespace nexusenroll::data::mysql;
using namespace nexusenroll::business::notifications;
using namespace std;

int main() {
    auto databaseConfig = loadMySqlConfigFromEnvironment();
    if (!databaseConfig) {
        cerr << "Database configuration error: " << databaseConfig.error().message << '\n';
        return 1;
    }

    MySqlDataContext dataContext(databaseConfig.value());
    auto databaseReady = dataContext.verifyConnections();
    if (!databaseReady) {
        cerr << "Database connection error: " << databaseReady.error().message << '\n';
        return 1;
    }

    // Shared notification log – surfaced via GET /api/v1/notifications.
    NotificationLog notificationLog;

    // Observer pattern: three concrete observers subscribed to the publisher.
    NotificationPublisher notificationPublisher;
    auto waitlistObserver = make_shared<WaitlistNotificationObserver>(&notificationLog);
    auto advisorObserver  = make_shared<AdvisorNotificationObserver>(notificationLog);
    auto systemObserver   = make_shared<SystemAlertObserver>(notificationLog);
    notificationPublisher.subscribe(waitlistObserver);
    notificationPublisher.subscribe(advisorObserver);
    notificationPublisher.subscribe(systemObserver);

    nexusenroll::business::sessions::DemonstrationSessionService sessionService(dataContext);

    crow::SimpleApp application;

    // Student routes – named members make each dependency slot explicit.
    nexusenroll::presentation::api::registerRoutes(
        application,
        sessionService,
        nexusenroll::presentation::api::StudentRouteDependencies{
            .userStore            = dataContext,
            .courseStore          = dataContext,
            .enrollmentStore      = dataContext,
            .programStore         = dataContext,
            .gradeStore           = dataContext,
            .waitlistStore        = dataContext,
            .transactionBoundary  = dataContext,
            .notificationPublisher = notificationPublisher
        });

    // Faculty routes.
    nexusenroll::presentation::api::registerFacultyRoutes(
        application,
        nexusenroll::presentation::api::FacultyRouteDependencies{
            .userStore           = dataContext,
            .courseStore         = dataContext,
            .enrollmentStore     = dataContext,
            .gradeStore          = dataContext,
            .changeRequestStore  = dataContext,
            .transactionBoundary = dataContext
        });

    // Administrator routes.
    nexusenroll::presentation::api::registerAdministratorRoutes(
        application,
        nexusenroll::presentation::api::AdministratorRouteDependencies{
            .userStore           = dataContext,
            .courseStore         = dataContext,
            .enrollmentStore     = dataContext,
            .programStore        = dataContext,
            .gradeStore          = dataContext,
            .changeRequestStore  = dataContext,
            .waitlistStore       = dataContext,
            .transactionBoundary = dataContext
        });

    // Notification endpoint – returns all recent alerts to the SPA so the
    // Observer pattern outcome is visible without sending real emails.
    CROW_ROUTE(application, "/api/v1/notifications")
    ([&notificationLog]() {
        crow::json::wvalue result;
        result["ok"] = true;
        auto alerts = notificationLog.all();
        crow::json::wvalue::list items;
        items.reserve(alerts.size());
        for (const auto& alert : alerts) {
            crow::json::wvalue item;
            item["type"]        = alert.type;
            item["message"]     = alert.message;
            item["timestampMs"] = alert.timestampMs;
            items.push_back(move(item));
        }
        result["data"]["items"] = move(items);
        return crow::response(200, result);
    });

    application.port(8080).multithreaded().run();
}
