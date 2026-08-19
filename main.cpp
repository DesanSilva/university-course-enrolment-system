#include "nexusenroll/business/notifications/seat_notification.hpp"
#include "nexusenroll/business/sessions/demonstration_session_service.hpp"
#include "nexusenroll/data/mysql/mysql_data_context.hpp"
#include "nexusenroll/presentation/api/administrator_routes.hpp"
#include "nexusenroll/presentation/api/faculty_routes.hpp"
#include "nexusenroll/presentation/api/routes.hpp"

#include <iostream>
#include <memory>

using namespace nexusenroll::data::mysql;
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

    nexusenroll::business::sessions::DemonstrationSessionService sessionService(dataContext);
    nexusenroll::business::notifications::NotificationPublisher notificationPublisher;
    auto waitlistObserver =
        std::make_shared<nexusenroll::business::notifications::WaitlistNotificationObserver>();
    notificationPublisher.subscribe(waitlistObserver);
    crow::SimpleApp application;
    nexusenroll::presentation::api::registerRoutes(
        application,
        sessionService,
        {dataContext, dataContext, dataContext, dataContext, dataContext,
         dataContext, dataContext, notificationPublisher});
    nexusenroll::presentation::api::registerFacultyRoutes(
        application,
        {dataContext, dataContext, dataContext, dataContext, dataContext, dataContext});
    nexusenroll::presentation::api::registerAdministratorRoutes(
        application,
        {dataContext, dataContext, dataContext, dataContext, dataContext,
         dataContext, dataContext, dataContext});

    application.port(8080).multithreaded().run();
}
