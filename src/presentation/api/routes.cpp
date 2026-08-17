#include "nexusenroll/presentation/api/routes.hpp"

namespace nexusenroll::presentation::api {

using namespace business::domain;
using namespace business::sessions;
using namespace common;
using namespace std;

namespace {

crow::response errorResponse(int status, const Error& error) {
    crow::json::wvalue body;
    body["ok"] = false;
    body["error"]["code"] = error.code;
    body["error"]["message"] = error.message;
    return crow::response(status, move(body));
}

crow::response invalidRequest(const string& message) {
    return errorResponse(400, {"INVALID_SESSION_REQUEST", message});
}

int statusFor(const Error& error) {
    if (error.code == "USER_INACTIVE") {
        return 403;
    }
    if (error.code == "SESSION_USER_NOT_FOUND" ||
        error.code == "SESSION_PROFILE_NOT_FOUND") {
        return 404;
    }
    return 500;
}

const char* roleName(UserRole role) {
    switch (role) {
    case UserRole::Student:
        return "STUDENT";
    case UserRole::Faculty:
        return "FACULTY";
    case UserRole::Administrator:
        return "ADMINISTRATOR";
    }
    return "UNKNOWN";
}

}

void registerRoutes(
    crow::SimpleApp& application,
    const DemonstrationSessionService& sessionService) {
    CROW_ROUTE(application, "/api/v1/health")([] {
        crow::json::wvalue response;
        response["ok"] = true;
        response["data"]["service"] = "NexusEnroll";
        response["data"]["status"] = "running";
        return response;
    });

    CROW_ROUTE(application, "/api/v1/sessions")
        .methods(crow::HTTPMethod::POST)([&sessionService](const crow::request& request) {
            auto requestBody = crow::json::load(request.body);
            if (!requestBody || requestBody.t() != crow::json::type::Object) {
                return invalidRequest("The request body must be a JSON object.");
            }
            if (!requestBody.has("userId") ||
                requestBody["userId"].t() != crow::json::type::String) {
                return invalidRequest("A string userId is required.");
            }

            const string userId = requestBody["userId"].s();
            if (userId.empty()) {
                return invalidRequest("userId must not be empty.");
            }

            auto created = sessionService.create(UserId{userId});
            if (!created) {
                return errorResponse(statusFor(created.error()), created.error());
            }

            const UserSession& session = *created.value();
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["userId"] = session.userId().value();
            body["data"]["displayName"] = session.displayName();
            body["data"]["role"] = roleName(session.role());

            if (session.role() == UserRole::Student) {
                const auto* student = dynamic_cast<const StudentSession*>(&session);
                if (!student) {
                    return errorResponse(
                        500, {"SESSION_PRODUCT_INVALID", "The Student session product is invalid."});
                }
                body["data"]["profileId"] = student->studentId().value();
            } else if (session.role() == UserRole::Faculty) {
                const auto* faculty = dynamic_cast<const FacultySession*>(&session);
                if (!faculty) {
                    return errorResponse(
                        500, {"SESSION_PRODUCT_INVALID", "The Faculty session product is invalid."});
                }
                body["data"]["profileId"] = faculty->facultyId().value();
            }

            return crow::response(200, move(body));
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
