#include "nexusenroll/presentation/api/administrator_routes.hpp"

#include "nexusenroll/business/cqrs/commands/administrator_commands.hpp"
#include "nexusenroll/business/cqrs/queries/administrator_queries.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nexusenroll::presentation::api {

using namespace business::cqrs::commands;
using namespace business::cqrs::queries;
using namespace business::domain;
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
    return errorResponse(400, {"INVALID_ADMINISTRATOR_REQUEST", message});
}

int statusFor(const Error& error) {
    if (error.code == "ADMINISTRATOR_INACTIVE" ||
        error.code == "ADMINISTRATOR_ROLE_REQUIRED") return 403;
    if (error.code == "COURSE_NOT_FOUND" || error.code == "PROGRAM_NOT_FOUND" ||
        error.code == "USER_NOT_FOUND" || error.code == "STUDENT_NOT_FOUND" ||
        error.code == "OFFERING_NOT_FOUND" || error.code == "DEPARTMENT_NOT_FOUND" ||
        error.code == "PREREQUISITE_NOT_FOUND" ||
        error.code == "REQUIRED_COURSE_NOT_FOUND" ||
        error.code == "CHANGE_REQUEST_NOT_FOUND" ||
        error.code == "ADMINISTRATOR_NOT_FOUND") return 404;
    if (error.code == "COURSE_IDENTITY_CONFLICT" ||
        error.code == "PROGRAM_IDENTITY_CONFLICT" ||
        error.code == "ACCOUNT_IDENTITY_CONFLICT" || error.code == "COURSE_IN_USE" ||
        error.code == "ACCOUNT_PROFILE_MISMATCH" ||
        error.code == "ACCOUNT_ROLE_IMMUTABLE" ||
        error.code == "UNSUPPORTED_ACCOUNT_ROLE" ||
        error.code == "USER_ALREADY_INACTIVE" || error.code == "ALREADY_ENROLLED" ||
        error.code == "ALREADY_COMPLETED" || error.code == "OVERRIDE_NOT_REQUIRED" ||
        error.code == "PREREQUISITE_NOT_MET" || error.code == "CAPACITY_FULL" ||
        error.code == "TIME_CONFLICT" || error.code == "CAPACITY_BELOW_OCCUPANCY" ||
        error.code == "OFFERING_COURSE_MISMATCH" ||
        error.code == "CHANGE_REQUEST_ALREADY_RESOLVED") return 409;
    if (error.code == "INVALID_COURSE" || error.code == "INVALID_PROGRAM" ||
        error.code == "INVALID_ACCOUNT" || error.code == "INVALID_PREREQUISITE" ||
        error.code == "SELF_PREREQUISITE" || error.code == "DUPLICATE_PREREQUISITE" ||
        error.code == "PREREQUISITE_CYCLE" || error.code == "INVALID_REQUIRED_COURSE" ||
        error.code == "DUPLICATE_REQUIRED_COURSE" ||
        error.code == "INVALID_OVERRIDE_RULE" || error.code == "INVALID_OVERRIDE_REASON" ||
        error.code == "INVALID_CHANGE_VALUE") return 422;
    return 500;
}

const char* roleName(UserRole role) {
    switch (role) {
    case UserRole::Student: return "STUDENT";
    case UserRole::Faculty: return "FACULTY";
    case UserRole::Administrator: return "ADMINISTRATOR";
    }
    return "UNKNOWN";
}

const char* userStatusName(UserStatus status) {
    return status == UserStatus::Active ? "ACTIVE" : "INACTIVE";
}

const char* changeTypeName(CourseChangeType type) {
    switch (type) {
    case CourseChangeType::Description: return "DESCRIPTION";
    case CourseChangeType::Prerequisites: return "PREREQUISITES";
    case CourseChangeType::Capacity: return "CAPACITY";
    }
    return "UNKNOWN";
}

const char* changeStatusName(CourseChangeStatus status) {
    switch (status) {
    case CourseChangeStatus::Pending: return "PENDING";
    case CourseChangeStatus::Approved: return "APPROVED";
    case CourseChangeStatus::Rejected: return "REJECTED";
    }
    return "UNKNOWN";
}

Result<UserRole> parseRole(const string& value) {
    if (value == "STUDENT") return Result<UserRole>::success(UserRole::Student);
    if (value == "FACULTY") return Result<UserRole>::success(UserRole::Faculty);
    if (value == "ADMINISTRATOR") return Result<UserRole>::success(UserRole::Administrator);
    return Result<UserRole>::failure(
        "INVALID_ADMINISTRATOR_REQUEST", "role must be STUDENT, FACULTY, or ADMINISTRATOR.");
}

Result<CourseChangeStatus> parseChangeStatus(const string& value) {
    if (value == "PENDING") return Result<CourseChangeStatus>::success(CourseChangeStatus::Pending);
    if (value == "APPROVED") return Result<CourseChangeStatus>::success(CourseChangeStatus::Approved);
    if (value == "REJECTED") return Result<CourseChangeStatus>::success(CourseChangeStatus::Rejected);
    return Result<CourseChangeStatus>::failure(
        "INVALID_ADMINISTRATOR_REQUEST", "status must be PENDING, APPROVED, or REJECTED.");
}

Result<EnrollmentRule> parseRule(const string& value) {
    if (value == "PREREQUISITE") return Result<EnrollmentRule>::success(EnrollmentRule::Prerequisite);
    if (value == "CAPACITY") return Result<EnrollmentRule>::success(EnrollmentRule::Capacity);
    if (value == "TIME_CONFLICT") return Result<EnrollmentRule>::success(EnrollmentRule::TimeConflict);
    return Result<EnrollmentRule>::failure(
        "INVALID_ADMINISTRATOR_REQUEST",
        "bypassedRule must be PREREQUISITE, CAPACITY, or TIME_CONFLICT.");
}

Result<int64_t> integerValue(const crow::json::rvalue& value, const string& field) {
    if (value.t() != crow::json::type::Number ||
        (value.nt() != crow::json::num_type::Signed_integer &&
         value.nt() != crow::json::num_type::Unsigned_integer)) {
        return Result<int64_t>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", field + " must be an integer.");
    }
    if (value.nt() == crow::json::num_type::Signed_integer) {
        return Result<int64_t>::success(value.i());
    }
    const uint64_t unsignedValue = value.u();
    if (unsignedValue > static_cast<uint64_t>(numeric_limits<int64_t>::max())) {
        return Result<int64_t>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", field + " is out of range.");
    }
    return Result<int64_t>::success(static_cast<int64_t>(unsignedValue));
}

Result<vector<CourseId>> courseIds(const crow::json::rvalue& value, const string& field) {
    if (value.t() != crow::json::type::List) {
        return Result<vector<CourseId>>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", field + " must be an array.");
    }
    vector<CourseId> ids;
    for (const auto& item : value) {
        if (item.t() != crow::json::type::String) {
            return Result<vector<CourseId>>::failure(
                "INVALID_ADMINISTRATOR_REQUEST", field + " entries must be strings.");
        }
        ids.emplace_back(string(item.s()));
    }
    return Result<vector<CourseId>>::success(move(ids));
}

bool requiredString(const crow::json::rvalue& body, const char* name) {
    return body.has(name) && body[name].t() == crow::json::type::String;
}

Result<CourseInput> courseInput(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body || body.t() != crow::json::type::Object ||
        !requiredString(body, "courseId") || !requiredString(body, "code") ||
        !requiredString(body, "department") || !requiredString(body, "courseNumber") ||
        !requiredString(body, "name") || !requiredString(body, "description") ||
        !body.has("credits") || !body.has("prerequisites")) {
        return Result<CourseInput>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", "A complete Course JSON object is required.");
    }
    auto credits = integerValue(body["credits"], "credits");
    auto prerequisites = courseIds(body["prerequisites"], "prerequisites");
    if (!credits) return Result<CourseInput>::failure(credits.error().code, credits.error().message);
    if (!prerequisites) return Result<CourseInput>::failure(
        prerequisites.error().code, prerequisites.error().message);
    return Result<CourseInput>::success({
        CourseId{string(body["courseId"].s())}, string(body["code"].s()),
        string(body["department"].s()), string(body["courseNumber"].s()),
        string(body["name"].s()), string(body["description"].s()), credits.value(),
        move(prerequisites.value())});
}

Result<CoursePatch> coursePatch(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body || body.t() != crow::json::type::Object) {
        return Result<CoursePatch>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", "The Course PATCH body must be an object.");
    }
    CoursePatch patch;
    bool supplied = false;
    for (const char* field : {"code", "department", "courseNumber", "name", "description"}) {
        if (!body.has(field)) continue;
        if (body[field].t() != crow::json::type::String) {
            return Result<CoursePatch>::failure(
                "INVALID_ADMINISTRATOR_REQUEST", string(field) + " must be a string.");
        }
        const string value = body[field].s();
        if (string(field) == "code") patch.code = value;
        else if (string(field) == "department") patch.department = value;
        else if (string(field) == "courseNumber") patch.courseNumber = value;
        else if (string(field) == "name") patch.name = value;
        else patch.description = value;
        supplied = true;
    }
    if (body.has("credits")) {
        auto value = integerValue(body["credits"], "credits");
        if (!value) return Result<CoursePatch>::failure(value.error().code, value.error().message);
        patch.credits = value.value();
        supplied = true;
    }
    if (body.has("prerequisites")) {
        auto value = courseIds(body["prerequisites"], "prerequisites");
        if (!value) return Result<CoursePatch>::failure(value.error().code, value.error().message);
        patch.prerequisiteCourseIds = move(value.value());
        supplied = true;
    }
    if (!supplied || body.has("courseId")) {
        return Result<CoursePatch>::failure(
            "INVALID_ADMINISTRATOR_REQUEST",
            "A Course PATCH needs a mutable field and cannot change courseId.");
    }
    return Result<CoursePatch>::success(move(patch));
}

Result<ProgramInput> programInput(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body || body.t() != crow::json::type::Object ||
        !requiredString(body, "programId") || !requiredString(body, "name") ||
        !requiredString(body, "department") || !body.has("requiredCredits") ||
        !body.has("requiredCourses")) {
        return Result<ProgramInput>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", "A complete Programme JSON object is required.");
    }
    auto credits = integerValue(body["requiredCredits"], "requiredCredits");
    auto courses = courseIds(body["requiredCourses"], "requiredCourses");
    if (!credits) return Result<ProgramInput>::failure(credits.error().code, credits.error().message);
    if (!courses) return Result<ProgramInput>::failure(courses.error().code, courses.error().message);
    return Result<ProgramInput>::success({
        ProgramId{string(body["programId"].s())}, string(body["name"].s()),
        string(body["department"].s()), credits.value(), move(courses.value())});
}

Result<ProgramPatch> programPatch(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body || body.t() != crow::json::type::Object || body.has("programId")) {
        return Result<ProgramPatch>::failure(
            "INVALID_ADMINISTRATOR_REQUEST",
            "The Programme PATCH must be an object and cannot change programId.");
    }
    ProgramPatch patch;
    bool supplied = false;
    for (const char* field : {"name", "department"}) {
        if (!body.has(field)) continue;
        if (body[field].t() != crow::json::type::String) {
            return Result<ProgramPatch>::failure(
                "INVALID_ADMINISTRATOR_REQUEST", string(field) + " must be a string.");
        }
        if (string(field) == "name") patch.name = string(body[field].s());
        else patch.department = string(body[field].s());
        supplied = true;
    }
    if (body.has("requiredCredits")) {
        auto value = integerValue(body["requiredCredits"], "requiredCredits");
        if (!value) return Result<ProgramPatch>::failure(value.error().code, value.error().message);
        patch.requiredCredits = value.value();
        supplied = true;
    }
    if (body.has("requiredCourses")) {
        auto value = courseIds(body["requiredCourses"], "requiredCourses");
        if (!value) return Result<ProgramPatch>::failure(value.error().code, value.error().message);
        patch.requiredCourseIds = move(value.value());
        supplied = true;
    }
    if (!supplied) return Result<ProgramPatch>::failure(
        "INVALID_ADMINISTRATOR_REQUEST", "A Programme PATCH requires a mutable field.");
    return Result<ProgramPatch>::success(move(patch));
}

Result<AccountInput> accountInput(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body || body.t() != crow::json::type::Object ||
        !requiredString(body, "userId") || !requiredString(body, "profileId") ||
        !requiredString(body, "role") || !requiredString(body, "name") ||
        !requiredString(body, "email")) {
        return Result<AccountInput>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", "Complete User and profile fields are required.");
    }
    auto role = parseRole(string(body["role"].s()));
    if (!role) return Result<AccountInput>::failure(role.error().code, role.error().message);
    optional<ProgramId> program;
    optional<string> department;
    if (body.has("programId")) {
        if (body["programId"].t() != crow::json::type::String) return Result<AccountInput>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", "programId must be a string.");
        program = ProgramId{string(body["programId"].s())};
    }
    if (body.has("department")) {
        if (body["department"].t() != crow::json::type::String) return Result<AccountInput>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", "department must be a string.");
        department = string(body["department"].s());
    }
    return Result<AccountInput>::success({
        UserId{string(body["userId"].s())}, string(body["profileId"].s()), role.value(),
        string(body["name"].s()), string(body["email"].s()), move(program), move(department)});
}

Result<AccountPatch> accountPatch(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body || body.t() != crow::json::type::Object ||
        body.has("userId") || body.has("profileId")) {
        return Result<AccountPatch>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", "Account identities cannot be changed.");
    }
    AccountPatch patch;
    bool supplied = false;
    for (const char* field : {"name", "email", "department"}) {
        if (!body.has(field)) continue;
        if (body[field].t() != crow::json::type::String) return Result<AccountPatch>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", string(field) + " must be a string.");
        if (string(field) == "name") patch.name = string(body[field].s());
        else if (string(field) == "email") patch.email = string(body[field].s());
        else patch.department = string(body[field].s());
        supplied = true;
    }
    if (body.has("role")) {
        if (body["role"].t() != crow::json::type::String) return Result<AccountPatch>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", "role must be a string.");
        auto role = parseRole(string(body["role"].s()));
        if (!role) return Result<AccountPatch>::failure(role.error().code, role.error().message);
        patch.role = role.value();
        supplied = true;
    }
    if (body.has("programId")) {
        if (body["programId"].t() != crow::json::type::String) return Result<AccountPatch>::failure(
            "INVALID_ADMINISTRATOR_REQUEST", "programId must be a string.");
        patch.programId = ProgramId{string(body["programId"].s())};
        supplied = true;
    }
    if (!supplied) return Result<AccountPatch>::failure(
        "INVALID_ADMINISTRATOR_REQUEST", "An Account PATCH requires a mutable field.");
    return Result<AccountPatch>::success(move(patch));
}

crow::json::wvalue courseValue(const Course& course) {
    crow::json::wvalue value;
    value["courseId"] = course.id.value();
    value["code"] = course.code;
    value["department"] = course.department;
    value["courseNumber"] = course.courseNumber;
    value["name"] = course.name;
    value["description"] = course.description;
    value["credits"] = static_cast<uint64_t>(course.credits);
    crow::json::wvalue::list prerequisites;
    for (const auto& id : course.prerequisiteCourseIds) prerequisites.emplace_back(id.value());
    value["prerequisites"] = crow::json::wvalue(prerequisites);
    return value;
}

crow::response actionResponse(
    int status,
    const string& action,
    const string& identityName,
    const string& identity,
    const CommandResult& result) {
    if (!result) return errorResponse(statusFor(result.error()), result.error());
    crow::json::wvalue body;
    body["ok"] = true;
    body["data"]["action"] = action;
    body["data"][identityName] = identity;
    return crow::response(status, move(body));
}

}

void registerAdministratorRoutes(
    crow::SimpleApp& application,
    AdministratorRouteDependencies dependencies) {
    CROW_ROUTE(application, "/api/v1/admin/courses")
        .methods(crow::HTTPMethod::GET)([dependencies] {
            auto result = GetAdministratorCoursesQuery(dependencies.courseStore).execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue::list items;
            for (const auto& course : result.value()) items.push_back(courseValue(course));
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/admin/programs")
        .methods(crow::HTTPMethod::GET)([dependencies] {
            auto result = GetAdministratorProgramsQuery(dependencies.programStore).execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue::list items;
            for (const auto& program : result.value()) {
                crow::json::wvalue value;
                value["programId"] = program.id.value();
                value["name"] = program.name;
                value["department"] = program.department;
                value["requiredCredits"] = static_cast<uint64_t>(program.requiredCredits);
                crow::json::wvalue::list courses;
                for (const auto& id : program.requiredCourseIds) courses.emplace_back(id.value());
                value["requiredCourses"] = crow::json::wvalue(courses);
                items.push_back(move(value));
            }
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/admin/users")
        .methods(crow::HTTPMethod::GET)([dependencies](const crow::request& request) {
            optional<UserRole> role;
            if (const char* value = request.url_params.get("role")) {
                auto parsed = parseRole(value);
                if (!parsed) return errorResponse(400, parsed.error());
                role = parsed.value();
            }
            auto result = GetAdministratorUsersQuery(role, dependencies.userStore).execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue::list items;
            for (const auto& user : result.value()) {
                crow::json::wvalue value;
                value["userId"] = user.id.value();
                value["name"] = user.name;
                value["email"] = user.email;
                value["role"] = roleName(user.role);
                value["status"] = userStatusName(user.status);
                items.push_back(move(value));
            }
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/admin/course-change-requests")
        .methods(crow::HTTPMethod::GET)([dependencies](const crow::request& request) {
            optional<CourseChangeStatus> status;
            if (const char* value = request.url_params.get("status")) {
                auto parsed = parseChangeStatus(value);
                if (!parsed) return errorResponse(400, parsed.error());
                status = parsed.value();
            }
            auto result = GetAdministratorCourseChangesQuery(
                status, dependencies.changeRequestStore).execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue::list items;
            for (const auto& requestValue : result.value()) {
                crow::json::wvalue value;
                value["requestId"] = requestValue.id.value();
                value["facultyId"] = requestValue.facultyId.value();
                value["courseId"] = requestValue.courseId.value();
                if (requestValue.offeringId) value["offeringId"] = requestValue.offeringId->value();
                value["changeType"] = changeTypeName(requestValue.type);
                value["requestedValue"] = requestValue.requestedValue;
                value["status"] = changeStatusName(requestValue.status);
                items.push_back(move(value));
            }
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/admin/courses")
        .methods(crow::HTTPMethod::POST)([dependencies](const crow::request& request) {
            auto input = courseInput(request);
            if (!input) return errorResponse(400, input.error());
            const string id = input.value().id.value();
            CreateCourseCommand command(
                move(input.value()), dependencies.courseStore, dependencies.transactionBoundary);
            return actionResponse(201, "COURSE_CREATED", "courseId", id, command.execute());
        });

    CROW_ROUTE(application, "/api/v1/admin/courses/<string>")
        .methods(crow::HTTPMethod::PATCH)([dependencies](const crow::request& request, string id) {
            if (id.empty()) return invalidRequest("courseId must not be empty.");
            auto patch = coursePatch(request);
            if (!patch) return errorResponse(400, patch.error());
            UpdateCourseCommand command(
                CourseId{id}, move(patch.value()), dependencies.courseStore,
                dependencies.transactionBoundary);
            return actionResponse(200, "COURSE_UPDATED", "courseId", id, command.execute());
        });

    CROW_ROUTE(application, "/api/v1/admin/courses/<string>")
        .methods(crow::HTTPMethod::DELETE)([dependencies](string id) {
            if (id.empty()) return invalidRequest("courseId must not be empty.");
            DeleteCourseCommand command(
                CourseId{id}, dependencies.courseStore, dependencies.transactionBoundary);
            return actionResponse(200, "COURSE_DELETED", "courseId", id, command.execute());
        });

    CROW_ROUTE(application, "/api/v1/admin/programs")
        .methods(crow::HTTPMethod::POST)([dependencies](const crow::request& request) {
            auto input = programInput(request);
            if (!input) return errorResponse(400, input.error());
            const string id = input.value().id.value();
            CreateProgramCommand command(
                move(input.value()), dependencies.programStore, dependencies.courseStore,
                dependencies.transactionBoundary);
            return actionResponse(201, "PROGRAM_CREATED", "programId", id, command.execute());
        });

    CROW_ROUTE(application, "/api/v1/admin/programs/<string>")
        .methods(crow::HTTPMethod::PATCH)([dependencies](const crow::request& request, string id) {
            if (id.empty()) return invalidRequest("programId must not be empty.");
            auto patch = programPatch(request);
            if (!patch) return errorResponse(400, patch.error());
            UpdateProgramCommand command(
                ProgramId{id}, move(patch.value()), dependencies.programStore,
                dependencies.courseStore, dependencies.transactionBoundary);
            return actionResponse(200, "PROGRAM_UPDATED", "programId", id, command.execute());
        });

    CROW_ROUTE(application, "/api/v1/admin/users")
        .methods(crow::HTTPMethod::POST)([dependencies](const crow::request& request) {
            auto input = accountInput(request);
            if (!input) return errorResponse(400, input.error());
            const string id = input.value().userId.value();
            CreateAccountCommand command(
                move(input.value()), dependencies.userStore, dependencies.programStore,
                dependencies.transactionBoundary);
            return actionResponse(201, "ACCOUNT_CREATED", "userId", id, command.execute());
        });

    CROW_ROUTE(application, "/api/v1/admin/users/<string>")
        .methods(crow::HTTPMethod::PATCH)([dependencies](const crow::request& request, string id) {
            if (id.empty()) return invalidRequest("userId must not be empty.");
            auto patch = accountPatch(request);
            if (!patch) return errorResponse(400, patch.error());
            EditAccountCommand command(
                UserId{id}, move(patch.value()), dependencies.userStore,
                dependencies.programStore, dependencies.transactionBoundary);
            return actionResponse(200, "ACCOUNT_UPDATED", "userId", id, command.execute());
        });

    CROW_ROUTE(application, "/api/v1/admin/users/<string>/deactivate")
        .methods(crow::HTTPMethod::POST)([dependencies](string id) {
            if (id.empty()) return invalidRequest("userId must not be empty.");
            DeactivateUserCommand command(
                UserId{id}, dependencies.userStore, dependencies.transactionBoundary);
            return actionResponse(200, "ACCOUNT_DEACTIVATED", "userId", id, command.execute());
        });

    CROW_ROUTE(application, "/api/v1/admin/enrolment-overrides")
        .methods(crow::HTTPMethod::POST)([dependencies](const crow::request& request) {
            auto body = crow::json::load(request.body);
            if (!body || body.t() != crow::json::type::Object ||
                !requiredString(body, "administratorUserId") ||
                !requiredString(body, "studentId") || !requiredString(body, "offeringId") ||
                !requiredString(body, "bypassedRule") || !requiredString(body, "reason")) {
                return invalidRequest("Complete enrolment-override string fields are required.");
            }
            auto rule = parseRule(string(body["bypassedRule"].s()));
            if (!rule) return errorResponse(400, rule.error());
            OverrideEnrollmentCommand command(
                {UserId{string(body["administratorUserId"].s())},
                  StudentId{string(body["studentId"].s())},
                  OfferingId{string(body["offeringId"].s())}, rule.value(),
                  string(body["reason"].s())},
                dependencies.userStore, dependencies.courseStore, dependencies.enrollmentStore,
                dependencies.gradeStore, dependencies.waitlistStore,
                dependencies.transactionBoundary);
            auto result = command.execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue response;
            response["ok"] = true;
            response["data"]["action"] = "ENROLLMENT_OVERRIDDEN";
            response["data"]["enrollmentId"] = command.enrollmentId().value();
            response["data"]["overrideId"] = command.overrideId().value();
            return crow::response(200, move(response));
        });

    CROW_ROUTE(application, "/api/v1/admin/course-change-requests/<string>/approve")
        .methods(crow::HTTPMethod::POST)([dependencies](string id) {
            if (id.empty()) return invalidRequest("requestId must not be empty.");
            ApproveCourseChangeCommand command(
                ChangeRequestId{id}, dependencies.courseStore, dependencies.changeRequestStore,
                dependencies.transactionBoundary);
            return actionResponse(200, "COURSE_CHANGE_APPROVED", "requestId", id, command.execute());
        });

    CROW_ROUTE(application, "/api/v1/admin/course-change-requests/<string>/reject")
        .methods(crow::HTTPMethod::POST)([dependencies](string id) {
            if (id.empty()) return invalidRequest("requestId must not be empty.");
            RejectCourseChangeCommand command(
                ChangeRequestId{id}, dependencies.changeRequestStore,
                dependencies.transactionBoundary);
            return actionResponse(200, "COURSE_CHANGE_REJECTED", "requestId", id, command.execute());
        });
}

}
