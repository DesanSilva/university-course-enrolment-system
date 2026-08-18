#include "nexusenroll/presentation/api/routes.hpp"

#include "nexusenroll/business/cqrs/commands/drop_course_command.hpp"
#include "nexusenroll/business/cqrs/commands/enroll_student_command.hpp"
#include "nexusenroll/business/cqrs/commands/join_waitlist_command.hpp"
#include "nexusenroll/business/cqrs/queries/student_queries.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace nexusenroll::presentation::api {

using namespace business::cqrs::commands;
using namespace business::cqrs::queries;
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

crow::response invalidRequest(const string& code, const string& message) {
    return errorResponse(400, {code, message});
}

int statusFor(const Error& error) {
    if (error.code == "USER_INACTIVE" || error.code == "STUDENT_INACTIVE") {
        return 403;
    }
    if (error.code == "SESSION_USER_NOT_FOUND" ||
        error.code == "SESSION_PROFILE_NOT_FOUND" ||
        error.code == "STUDENT_NOT_FOUND" ||
        error.code == "OFFERING_NOT_FOUND" ||
        error.code == "ACTIVE_ENROLLMENT_NOT_FOUND") {
        return 404;
    }
    if (error.code == "PREREQUISITE_NOT_MET") {
        return 422;
    }
    if (error.code == "ALREADY_ENROLLED" ||
        error.code == "ALREADY_COMPLETED" ||
        error.code == "CAPACITY_FULL" ||
        error.code == "TIME_CONFLICT" ||
        error.code == "ALREADY_WAITLISTED" ||
        error.code == "SEAT_AVAILABLE") {
        return 409;
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

const char* enrollmentStatusName(EnrollmentStatus status) {
    switch (status) {
    case EnrollmentStatus::Active:
        return "ACTIVE";
    case EnrollmentStatus::Dropped:
        return "DROPPED";
    case EnrollmentStatus::Completed:
        return "COMPLETED";
    }
    return "UNKNOWN";
}

const char* waitlistStatusName(WaitlistStatus status) {
    switch (status) {
    case WaitlistStatus::Waiting:
        return "WAITING";
    case WaitlistStatus::Offered:
        return "OFFERED";
    case WaitlistStatus::Removed:
        return "REMOVED";
    }
    return "UNKNOWN";
}

const char* dayName(DayOfWeek day) {
    switch (day) {
    case DayOfWeek::Monday:
        return "MONDAY";
    case DayOfWeek::Tuesday:
        return "TUESDAY";
    case DayOfWeek::Wednesday:
        return "WEDNESDAY";
    case DayOfWeek::Thursday:
        return "THURSDAY";
    case DayOfWeek::Friday:
        return "FRIDAY";
    case DayOfWeek::Saturday:
        return "SATURDAY";
    case DayOfWeek::Sunday:
        return "SUNDAY";
    }
    return "UNKNOWN";
}

string timeText(int minutes) {
    const int hours = minutes / 60;
    const int remainingMinutes = minutes % 60;
    string value;
    value.push_back(static_cast<char>('0' + hours / 10));
    value.push_back(static_cast<char>('0' + hours % 10));
    value.push_back(':');
    value.push_back(static_cast<char>('0' + remainingMinutes / 10));
    value.push_back(static_cast<char>('0' + remainingMinutes % 10));
    return value;
}

crow::json::wvalue scheduleJson(const vector<ScheduleSlot>& schedule) {
    crow::json::wvalue::list values;
    for (const auto& slot : schedule) {
        crow::json::wvalue value;
        value["day"] = dayName(slot.day());
        value["start"] = timeText(slot.startMinutes());
        value["end"] = timeText(slot.endMinutes());
        value["location"] = slot.location();
        values.push_back(move(value));
    }
    return crow::json::wvalue(values);
}

crow::json::wvalue prerequisitesJson(const Course& course) {
    crow::json::wvalue::list values;
    for (const auto& prerequisite : course.prerequisiteCourseIds) {
        values.emplace_back(prerequisite.value());
    }
    return crow::json::wvalue(values);
}

crow::json::wvalue courseJson(const Course& course) {
    crow::json::wvalue value;
    value["courseId"] = course.id.value();
    value["code"] = course.code;
    value["department"] = course.department;
    value["courseNumber"] = course.courseNumber;
    value["name"] = course.name;
    value["description"] = course.description;
    value["credits"] = static_cast<uint64_t>(course.credits);
    value["prerequisites"] = prerequisitesJson(course);
    return value;
}

crow::json::wvalue offeringJson(const CourseOffering& offering) {
    crow::json::wvalue value;
    value["offeringId"] = offering.id.value();
    value["semester"] = offering.semester;
    value["instructorId"] = offering.instructorId.value();
    value["availableSeats"] = static_cast<uint64_t>(
        offering.capacity > offering.enrolledCount
            ? offering.capacity - offering.enrolledCount
            : 0);
    value["totalSeats"] = static_cast<uint64_t>(offering.capacity);
    value["schedule"] = scheduleJson(offering.schedule);
    return value;
}

crow::response commandResponse(int successStatus, const string& action, CommandResult result) {
    if (!result) {
        return errorResponse(statusFor(result.error()), result.error());
    }
    crow::json::wvalue body;
    body["ok"] = true;
    body["data"]["action"] = action;
    return crow::response(successStatus, move(body));
}

Result<string> offeringIdFromBody(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body || body.t() != crow::json::type::Object || !body.has("offeringId") ||
        body["offeringId"].t() != crow::json::type::String) {
        return Result<string>::failure(
            "INVALID_STUDENT_REQUEST", "A string offeringId is required.");
    }
    string offeringId = body["offeringId"].s();
    if (offeringId.empty()) {
        return Result<string>::failure(
            "INVALID_STUDENT_REQUEST", "offeringId must not be empty.");
    }
    return Result<string>::success(move(offeringId));
}

string queryParameter(const crow::request& request, const char* name) {
    const char* value = request.url_params.get(name);
    return value ? value : "";
}

}

void registerRoutes(
    crow::SimpleApp& application,
    const DemonstrationSessionService& sessionService,
    StudentRouteDependencies dependencies) {
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
                return invalidRequest(
                    "INVALID_SESSION_REQUEST", "The request body must be a JSON object.");
            }
            if (!requestBody.has("userId") ||
                requestBody["userId"].t() != crow::json::type::String) {
                return invalidRequest("INVALID_SESSION_REQUEST", "A string userId is required.");
            }

            const string userId = requestBody["userId"].s();
            if (userId.empty()) {
                return invalidRequest("INVALID_SESSION_REQUEST", "userId must not be empty.");
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

    CROW_ROUTE(application, "/api/v1/students/<string>/catalogue")
        ([dependencies](const crow::request& request, string studentId) {
            if (studentId.empty()) {
                return invalidRequest("INVALID_STUDENT_REQUEST", "studentId must not be empty.");
            }
            CatalogueFilter filter{
                queryParameter(request, "semester"), queryParameter(request, "department"),
                queryParameter(request, "courseNumber"), queryParameter(request, "keyword"),
                queryParameter(request, "instructor")};
            BrowseCourseCatalogueQuery query(
                StudentId{studentId}, move(filter), dependencies.userStore,
                dependencies.courseStore);
            auto result = query.execute();
            if (!result) {
                return errorResponse(statusFor(result.error()), result.error());
            }
            crow::json::wvalue::list items;
            for (const auto& item : result.value()) {
                auto value = offeringJson(item.offering);
                value["course"] = courseJson(item.course);
                value["instructorName"] = item.instructorName;
                items.push_back(move(value));
            }
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/students/<string>/schedule")
        ([dependencies](const crow::request& request, string studentId) {
            const string semester = queryParameter(request, "semester");
            if (studentId.empty() || semester.empty()) {
                return invalidRequest(
                    "INVALID_STUDENT_REQUEST", "Non-empty studentId and semester values are required.");
            }
            GetStudentScheduleQuery query(
                StudentId{studentId}, semester, dependencies.userStore,
                dependencies.enrollmentStore, dependencies.courseStore);
            auto result = query.execute();
            if (!result) {
                return errorResponse(statusFor(result.error()), result.error());
            }
            crow::json::wvalue::list items;
            for (const auto& item : result.value()) {
                auto value = offeringJson(item.offering);
                value["course"] = courseJson(item.course);
                value["enrollmentId"] = item.enrollment.id.value();
                value["status"] = enrollmentStatusName(item.enrollment.status);
                items.push_back(move(value));
            }
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["semester"] = semester;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/students/<string>/progress")
        ([dependencies](string studentId) {
            if (studentId.empty()) {
                return invalidRequest("INVALID_STUDENT_REQUEST", "studentId must not be empty.");
            }
            GetAcademicProgressQuery query(
                StudentId{studentId}, dependencies.userStore, dependencies.gradeStore,
                dependencies.programStore, dependencies.courseStore);
            auto result = query.execute();
            if (!result) {
                return errorResponse(statusFor(result.error()), result.error());
            }
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["program"]["programId"] = result.value().program.id.value();
            body["data"]["program"]["name"] = result.value().program.name;
            body["data"]["program"]["department"] = result.value().program.department;
            body["data"]["program"]["requiredCredits"] =
                static_cast<uint64_t>(result.value().program.requiredCredits);
            crow::json::wvalue::list completed;
            for (const auto& item : result.value().completedCourses) {
                auto value = courseJson(item.course);
                value["grade"] = item.grade;
                completed.push_back(move(value));
            }
            crow::json::wvalue::list remaining;
            for (const auto& course : result.value().remainingRequiredCourses) {
                remaining.push_back(courseJson(course));
            }
            body["data"]["completedCourses"] = crow::json::wvalue(completed);
            body["data"]["remainingRequirements"] = crow::json::wvalue(remaining);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/students/<string>/waitlist")
        .methods(crow::HTTPMethod::GET)([dependencies](string studentId) {
            if (studentId.empty()) {
                return invalidRequest("INVALID_STUDENT_REQUEST", "studentId must not be empty.");
            }
            GetStudentWaitlistQuery query(
                StudentId{studentId}, dependencies.userStore, dependencies.waitlistStore,
                dependencies.courseStore);
            auto result = query.execute();
            if (!result) {
                return errorResponse(statusFor(result.error()), result.error());
            }
            crow::json::wvalue::list items;
            for (const auto& item : result.value()) {
                auto value = offeringJson(item.offering);
                value["course"] = courseJson(item.course);
                value["waitlistEntryId"] = item.entry.id.value();
                value["position"] = static_cast<uint64_t>(item.entry.position);
                value["status"] = waitlistStatusName(item.entry.status);
                items.push_back(move(value));
            }
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/students/<string>/enrolments")
        .methods(crow::HTTPMethod::POST)([dependencies](const crow::request& request, string studentId) {
            if (studentId.empty()) {
                return invalidRequest("INVALID_STUDENT_REQUEST", "studentId must not be empty.");
            }
            auto offeringId = offeringIdFromBody(request);
            if (!offeringId) {
                return invalidRequest(offeringId.error().code, offeringId.error().message);
            }
            EnrollStudentCommand command(
                StudentId{studentId}, OfferingId{offeringId.value()}, dependencies.userStore,
                dependencies.courseStore, dependencies.enrollmentStore,
                dependencies.gradeStore, dependencies.waitlistStore,
                dependencies.transactionBoundary);
            return commandResponse(201, "ENROLLED", command.execute());
        });

    CROW_ROUTE(application, "/api/v1/students/<string>/enrolments/<string>")
        .methods(crow::HTTPMethod::DELETE)([dependencies](string studentId, string offeringId) {
            if (studentId.empty() || offeringId.empty()) {
                return invalidRequest(
                    "INVALID_STUDENT_REQUEST", "studentId and offeringId must not be empty.");
            }
            DropCourseCommand command(
                StudentId{studentId}, OfferingId{offeringId}, dependencies.userStore,
                dependencies.courseStore, dependencies.enrollmentStore,
                dependencies.waitlistStore, dependencies.transactionBoundary,
                dependencies.notificationPublisher);
            return commandResponse(200, "DROPPED", command.execute());
        });

    CROW_ROUTE(application, "/api/v1/students/<string>/waitlist")
        .methods(crow::HTTPMethod::POST)([dependencies](const crow::request& request, string studentId) {
            if (studentId.empty()) {
                return invalidRequest("INVALID_STUDENT_REQUEST", "studentId must not be empty.");
            }
            auto offeringId = offeringIdFromBody(request);
            if (!offeringId) {
                return invalidRequest(offeringId.error().code, offeringId.error().message);
            }
            JoinWaitlistCommand command(
                StudentId{studentId}, OfferingId{offeringId.value()}, dependencies.userStore,
                dependencies.courseStore, dependencies.enrollmentStore,
                dependencies.waitlistStore, dependencies.transactionBoundary);
            return commandResponse(201, "WAITLISTED", command.execute());
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
