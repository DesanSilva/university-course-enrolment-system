#include "nexusenroll/business/domain/schedule.hpp"

#include <algorithm>
#include <utility>

namespace nexusenroll::business::domain {

using namespace common;
using namespace std;

ScheduleSlot::ScheduleSlot(
    DayOfWeek day,
    int startMinutes,
    int endMinutes,
    string location)
    : day_(day),
      startMinutes_(startMinutes),
      endMinutes_(endMinutes),
      location_(move(location)) {}

Result<ScheduleSlot> ScheduleSlot::create(
    DayOfWeek day,
    int startMinutes,
    int endMinutes,
    string location) {
    constexpr int minutesPerDay = 24 * 60;
    if (startMinutes < 0 || endMinutes > minutesPerDay || startMinutes >= endMinutes) {
        return Result<ScheduleSlot>::failure(
            "INVALID_SCHEDULE_RANGE",
            "Schedule times must form a non-empty interval within one day.");
    }

    return Result<ScheduleSlot>::success(
        ScheduleSlot(day, startMinutes, endMinutes, move(location)));
}

bool schedulesOverlap(const ScheduleSlot& left, const ScheduleSlot& right) noexcept {
    if (left.day() != right.day()) {
        return false;
    }
    return max(left.startMinutes(), right.startMinutes()) <
           min(left.endMinutes(), right.endMinutes());
}

}
