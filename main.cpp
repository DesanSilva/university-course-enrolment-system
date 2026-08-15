#include "nexusenroll/data/mysql/mysql_data_context.hpp"
#include "nexusenroll/presentation/api/routes.hpp"

#include <iostream>

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

    crow::SimpleApp application;
    nexusenroll::presentation::api::registerRoutes(application);

    application.port(8080).multithreaded().run();
}
