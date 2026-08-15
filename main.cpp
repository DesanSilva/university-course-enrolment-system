#include "nexusenroll/presentation/api/routes.hpp"

int main() {
    crow::SimpleApp application;
    nexusenroll::presentation::api::registerRoutes(application);

    application.port(8080).multithreaded().run();
}
