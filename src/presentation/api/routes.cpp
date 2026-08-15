#include "nexusenroll/presentation/api/routes.hpp"

namespace nexusenroll::presentation::api {

void registerRoutes(crow::SimpleApp& application) {
    CROW_ROUTE(application, "/api/v1/health")([] {
        crow::json::wvalue response;
        response["ok"] = true;
        response["data"]["service"] = "NexusEnroll";
        response["data"]["status"] = "running";
        return response;
    });

    CROW_ROUTE(application, "/")([](crow::response& response) {
        response.set_static_file_info("frontend/index.html");
        response.end();
    });

    CROW_ROUTE(application, "/css/style.css")([](crow::response& response) {
        response.set_static_file_info("frontend/css/style.css");
        response.end();
    });

    CROW_ROUTE(application, "/js/app.js")([](crow::response& response) {
        response.set_static_file_info("frontend/js/app.js");
        response.end();
    });
}

}
