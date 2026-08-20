#include "nexusenroll/presentation/api/route_helpers.hpp"

#include <utility>

namespace nexusenroll::presentation::api {

using namespace business::domain;
using namespace common;
using namespace std;

crow::response makeErrorResponse(int status, const Error& error) {
    crow::json::wvalue body;
    body["ok"] = false;
    body["error"]["code"] = error.code;
    body["error"]["message"] = error.message;
    return crow::response(status, move(body));
}

crow::response makeInvalidRequest(const string& code, const string& message) {
    return makeErrorResponse(400, {code, message});
}

const char* formatRoleName(UserRole role) {
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

const char* formatEnrollmentStatusName(EnrollmentStatus status) {
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

const char* formatWaitlistStatusName(WaitlistStatus status) {
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

const char* formatChangeTypeName(CourseChangeType type) {
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

const char* formatChangeStatusName(CourseChangeStatus status) {
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

const char* formatDayName(DayOfWeek day) {
    static constexpr const char* names[]{
        "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY",
        "FRIDAY", "SATURDAY", "SUNDAY"};
    const auto index = static_cast<size_t>(day);
    return index < 7 ? names[index] : "UNKNOWN";
}

string formatTimeText(int minutes) {
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

crow::json::wvalue formatScheduleJson(const vector<ScheduleSlot>& schedule) {
    crow::json::wvalue::list values;
    for (const auto& slot : schedule) {
        crow::json::wvalue value;
        value["day"] = formatDayName(slot.day());
        value["start"] = formatTimeText(slot.startMinutes());
        value["end"] = formatTimeText(slot.endMinutes());
        value["location"] = slot.location();
        values.push_back(move(value));
    }
    return crow::json::wvalue(values);
}

} // namespace nexusenroll::presentation::api
