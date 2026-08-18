#include "nexusenroll/presentation/api/faculty_routes.hpp"

#include "nexusenroll/business/cqrs/commands/finalize_grades_command.hpp"
#include "nexusenroll/business/cqrs/commands/submit_course_change_request_command.hpp"
#include "nexusenroll/business/cqrs/commands/submit_grades_command.hpp"
#include "nexusenroll/business/cqrs/queries/faculty_queries.hpp"

#include <cstdint>
#include <limits>
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
    return errorResponse(400, {"INVALID_FACULTY_REQUEST", message});
}

int statusFor(const Error& error) {
    if (error.code == "FACULTY_INACTIVE") {
        return 403;
    }
    if (error.code == "FACULTY_NOT_FOUND" || error.code == "OFFERING_NOT_FOUND" ||
        error.code == "COURSE_NOT_FOUND" || error.code == "PREREQUISITE_NOT_FOUND" ||
        error.code == "GRADABLE_ENROLLMENT_NOT_FOUND") {
        return 404;
    }
    if (error.code == "FACULTY_OFFERING_MISMATCH" ||
        error.code == "FACULTY_COURSE_MISMATCH" ||
        error.code == "OFFERING_COURSE_MISMATCH" ||
        error.code == "DUPLICATE_STUDENT" ||
        error.code == "GRADE_ALREADY_SUBMITTED" ||
        error.code == "NO_PENDING_GRADES") {
        return 409;
    }
    if (error.code == "INVALID_GRADE" || error.code == "INVALID_CHANGE_VALUE" ||
        error.code == "INVALID_CHANGE_TYPE" ||
        error.code == "INVALID_PREREQUISITE" || error.code == "SELF_PREREQUISITE" ||
        error.code == "DUPLICATE_PREREQUISITE") {
        return 422;
    }
    if (error.code == "EMPTY_GRADE_BATCH" || error.code == "GRADE_BATCH_TOO_LARGE") {
        return 400;
    }
    return 500;
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

const char* gradeLifecycleName(GradeLifecycle lifecycle) {
    return lifecycle == GradeLifecycle::Pending ? "PENDING" : "SUBMITTED";
}

const char* changeTypeName(CourseChangeType type) {
    switch (type) {
    case CourseChangeType::Description:
        return "DESCRIPTION";
    case CourseChangeType::Prerequisites:
        return "PREREQUISITES";
    case CourseChangeType::Capacity:
        return "CAPACITY";
    }
    return "UNKNOWN";
}

const char* changeStatusName(CourseChangeStatus status) {
    switch (status) {
    case CourseChangeStatus::Pending:
        return "PENDING";
    case CourseChangeStatus::Approved:
        return "APPROVED";
    case CourseChangeStatus::Rejected:
        return "REJECTED";
    }
    return "UNKNOWN";
}

const char* dayName(DayOfWeek day) {
    static constexpr const char* names[]{
        "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY",
        "FRIDAY", "SATURDAY", "SUNDAY"};
    const auto index = static_cast<size_t>(day);
    return index < 7 ? names[index] : "UNKNOWN";
}

string timeText(int minutes) {
    const int hours = minutes / 60;
    const int remainder = minutes % 60;
    string value;
    value.push_back(static_cast<char>('0' + hours / 10));
    value.push_back(static_cast<char>('0' + hours % 10));
    value.push_back(':');
    value.push_back(static_cast<char>('0' + remainder / 10));
    value.push_back(static_cast<char>('0' + remainder % 10));
    return value;
}

crow::json::wvalue offeringValue(const FacultyOfferingItem& item) {
    crow::json::wvalue value;
    value["offeringId"] = item.offering.id.value();
    value["courseId"] = item.course.id.value();
    value["courseCode"] = item.course.code;
    value["courseName"] = item.course.name;
    value["semester"] = item.offering.semester;
    value["capacity"] = static_cast<uint64_t>(item.offering.capacity);
    value["activeOccupancy"] = static_cast<uint64_t>(item.offering.enrolledCount);
    crow::json::wvalue::list schedule;
    for (const auto& slot : item.offering.schedule) {
        crow::json::wvalue slotValue;
        slotValue["day"] = dayName(slot.day());
        slotValue["start"] = timeText(slot.startMinutes());
        slotValue["end"] = timeText(slot.endMinutes());
        slotValue["location"] = slot.location();
        schedule.push_back(move(slotValue));
    }
    value["schedule"] = crow::json::wvalue(schedule);
    return value;
}

crow::json::wvalue changeRequestValue(const CourseChangeRequest& request) {
    crow::json::wvalue value;
    value["requestId"] = request.id.value();
    value["courseId"] = request.courseId.value();
    if (request.offeringId) {
        value["offeringId"] = request.offeringId->value();
    }
    value["changeType"] = changeTypeName(request.type);
    value["requestedValue"] = request.requestedValue;
    value["status"] = changeStatusName(request.status);
    return value;
}

Result<vector<GradeInput>> gradeInputs(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body || body.t() != crow::json::type::Object || !body.has("grades") ||
        body["grades"].t() != crow::json::type::List) {
        return Result<vector<GradeInput>>::failure(
            "INVALID_FACULTY_REQUEST", "A grades JSON array is required.");
    }
    vector<GradeInput> inputs;
    for (const auto& value : body["grades"]) {
        if (value.t() != crow::json::type::Object || !value.has("studentId") ||
            !value.has("grade") || value["studentId"].t() != crow::json::type::String ||
            value["grade"].t() != crow::json::type::String) {
            return Result<vector<GradeInput>>::failure(
                "INVALID_FACULTY_REQUEST",
                "Every grade entry requires string studentId and grade values.");
        }
        string studentId = value["studentId"].s();
        string grade = value["grade"].s();
        if (studentId.empty()) {
            return Result<vector<GradeInput>>::failure(
                "INVALID_FACULTY_REQUEST", "Grade entry Student IDs must not be empty.");
        }
        inputs.push_back({StudentId{move(studentId)}, move(grade)});
    }
    return Result<vector<GradeInput>>::success(move(inputs));
}

Result<CourseChangeInput> courseChangeInput(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body || body.t() != crow::json::type::Object || !body.has("courseId") ||
        !body.has("changeType") || !body.has("requestedValue") ||
        body["courseId"].t() != crow::json::type::String ||
        body["changeType"].t() != crow::json::type::String) {
        return Result<CourseChangeInput>::failure(
            "INVALID_FACULTY_REQUEST",
            "String courseId and changeType plus requestedValue are required.");
    }
    string courseId = body["courseId"].s();
    string type = body["changeType"].s();
    if (courseId.empty() || type.empty()) {
        return Result<CourseChangeInput>::failure(
            "INVALID_FACULTY_REQUEST", "courseId and changeType must not be empty.");
    }

    CourseChangeInput input{CourseId{move(courseId)}, CourseChangeType::Description,
                            {}, {}, nullopt, nullopt};
    const auto& requested = body["requestedValue"];
    if (type == "DESCRIPTION") {
        if (requested.t() != crow::json::type::String) {
            return Result<CourseChangeInput>::failure(
                "INVALID_FACULTY_REQUEST", "DESCRIPTION requestedValue must be a string.");
        }
        input.description = requested.s();
    } else if (type == "PREREQUISITES") {
        input.type = CourseChangeType::Prerequisites;
        if (requested.t() != crow::json::type::List) {
            return Result<CourseChangeInput>::failure(
                "INVALID_FACULTY_REQUEST", "PREREQUISITES requestedValue must be an array.");
        }
        for (const auto& value : requested) {
            if (value.t() != crow::json::type::String || string(value.s()).empty()) {
                return Result<CourseChangeInput>::failure(
                    "INVALID_FACULTY_REQUEST",
                    "Every prerequisite must be a non-empty Course ID string.");
            }
            input.prerequisiteCourseIds.emplace_back(string(value.s()));
        }
    } else if (type == "CAPACITY") {
        input.type = CourseChangeType::Capacity;
        if (requested.t() != crow::json::type::Number ||
            (requested.nt() != crow::json::num_type::Signed_integer &&
             requested.nt() != crow::json::num_type::Unsigned_integer) ||
            !body.has("offeringId") ||
            body["offeringId"].t() != crow::json::type::String ||
            string(body["offeringId"].s()).empty()) {
            return Result<CourseChangeInput>::failure(
                "INVALID_FACULTY_REQUEST",
                "CAPACITY requires an integer requestedValue and non-empty string offeringId.");
        }
        if (requested.nt() == crow::json::num_type::Signed_integer) {
            const int64_t capacity = requested.i();
            input.capacity = capacity;
        } else {
            const uint64_t capacity = requested.u();
            if (capacity > static_cast<uint64_t>(numeric_limits<int64_t>::max())) {
                return Result<CourseChangeInput>::failure(
                    "INVALID_FACULTY_REQUEST", "CAPACITY requestedValue is out of range.");
            }
            input.capacity = static_cast<int64_t>(capacity);
        }
        input.offeringId = OfferingId{string(body["offeringId"].s())};
    } else {
        return Result<CourseChangeInput>::failure(
            "INVALID_FACULTY_REQUEST", "changeType is not supported.");
    }
    return Result<CourseChangeInput>::success(move(input));
}

}

void registerFacultyRoutes(crow::SimpleApp& application, FacultyRouteDependencies dependencies) {
    CROW_ROUTE(application, "/api/v1/faculty/<string>/offerings")
        .methods(crow::HTTPMethod::GET)([dependencies](string facultyId) {
            if (facultyId.empty()) return invalidRequest("facultyId must not be empty.");
            GetFacultyOfferingsQuery query(
                FacultyId{facultyId}, dependencies.userStore, dependencies.courseStore);
            auto result = query.execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue::list items;
            for (const auto& item : result.value()) items.push_back(offeringValue(item));
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/faculty/<string>/offerings/<string>/roster")
        .methods(crow::HTTPMethod::GET)([dependencies](string facultyId, string offeringId) {
            if (facultyId.empty() || offeringId.empty())
                return invalidRequest("facultyId and offeringId must not be empty.");
            GetClassRosterQuery query(
                FacultyId{facultyId}, OfferingId{offeringId}, dependencies.userStore,
                dependencies.courseStore, dependencies.enrollmentStore);
            auto result = query.execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue::list items;
            for (const auto& item : result.value()) {
                crow::json::wvalue value;
                value["studentId"] = item.enrollment.studentId.value();
                value["name"] = item.studentName;
                value["email"] = item.studentEmail;
                items.push_back(move(value));
            }
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/faculty/<string>/offerings/<string>/grades")
        .methods(crow::HTTPMethod::GET)([dependencies](string facultyId, string offeringId) {
            if (facultyId.empty() || offeringId.empty())
                return invalidRequest("facultyId and offeringId must not be empty.");
            GetGradeStateQuery query(
                FacultyId{facultyId}, OfferingId{offeringId}, dependencies.userStore,
                dependencies.courseStore, dependencies.gradeStore);
            auto result = query.execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue::list items;
            for (const auto& item : result.value()) {
                crow::json::wvalue value;
                value["studentId"] = item.enrollment.studentId.value();
                value["enrollmentStatus"] = enrollmentStatusName(item.enrollment.status);
                value["hasGrade"] = item.grade.has_value();
                if (item.grade) {
                    value["grade"] = item.grade->grade;
                    value["lifecycle"] = gradeLifecycleName(item.grade->lifecycle);
                }
                items.push_back(move(value));
            }
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/faculty/<string>/course-change-requests")
        .methods(crow::HTTPMethod::GET)([dependencies](string facultyId) {
            if (facultyId.empty()) return invalidRequest("facultyId must not be empty.");
            GetFacultyCourseChangeRequestsQuery query(
                FacultyId{facultyId}, dependencies.userStore, dependencies.changeRequestStore);
            auto result = query.execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue::list items;
            for (const auto& request : result.value()) items.push_back(changeRequestValue(request));
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["items"] = crow::json::wvalue(items);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/faculty/<string>/offerings/<string>/grades")
        .methods(crow::HTTPMethod::POST)([dependencies](const crow::request& request,
                                                       string facultyId, string offeringId) {
            if (facultyId.empty() || offeringId.empty())
                return invalidRequest("facultyId and offeringId must not be empty.");
            auto inputs = gradeInputs(request);
            if (!inputs) return errorResponse(400, inputs.error());
            SubmitGradesCommand command(
                FacultyId{facultyId}, OfferingId{offeringId}, move(inputs.value()),
                dependencies.userStore, dependencies.courseStore, dependencies.enrollmentStore,
                dependencies.gradeStore, dependencies.transactionBoundary);
            auto result = command.execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue::list accepted;
            for (const auto& item : command.outcome().accepted) {
                crow::json::wvalue value;
                value["studentId"] = item.studentId.value();
                value["grade"] = item.grade;
                accepted.push_back(move(value));
            }
            crow::json::wvalue::list rejected;
            for (const auto& item : command.outcome().rejected) {
                crow::json::wvalue value;
                value["studentId"] = item.input.studentId.value();
                value["grade"] = item.input.grade;
                value["code"] = item.error.code;
                value["message"] = item.error.message;
                rejected.push_back(move(value));
            }
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["accepted"] = crow::json::wvalue(accepted);
            body["data"]["rejected"] = crow::json::wvalue(rejected);
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/faculty/<string>/offerings/<string>/grades/submit")
        .methods(crow::HTTPMethod::POST)([dependencies](string facultyId, string offeringId) {
            if (facultyId.empty() || offeringId.empty())
                return invalidRequest("facultyId and offeringId must not be empty.");
            FinalizeGradesCommand command(
                FacultyId{facultyId}, OfferingId{offeringId}, dependencies.userStore,
                dependencies.courseStore, dependencies.enrollmentStore,
                dependencies.gradeStore, dependencies.transactionBoundary);
            auto result = command.execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["action"] = "GRADES_SUBMITTED";
            body["data"]["count"] = static_cast<uint64_t>(command.finalizedCount());
            return crow::response(200, move(body));
        });

    CROW_ROUTE(application, "/api/v1/faculty/<string>/course-change-requests")
        .methods(crow::HTTPMethod::POST)([dependencies](const crow::request& request,
                                                       string facultyId) {
            if (facultyId.empty()) return invalidRequest("facultyId must not be empty.");
            auto input = courseChangeInput(request);
            if (!input) return errorResponse(400, input.error());
            SubmitCourseChangeRequestCommand command(
                FacultyId{facultyId}, move(input.value()), dependencies.userStore,
                dependencies.courseStore, dependencies.changeRequestStore);
            auto result = command.execute();
            if (!result) return errorResponse(statusFor(result.error()), result.error());
            crow::json::wvalue body;
            body["ok"] = true;
            body["data"]["action"] = "COURSE_CHANGE_REQUESTED";
            body["data"]["requestId"] = command.requestId().value();
            return crow::response(201, move(body));
        });
}

}
