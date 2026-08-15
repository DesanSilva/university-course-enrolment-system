#pragma once

#include "nexusenroll/common/result.hpp"

#include <string>

namespace nexusenroll::business::domain {

enum class DayOfWeek {
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday
};

class ScheduleSlot {
public:
    static common::Result<ScheduleSlot> create(
        DayOfWeek day,
        int startMinutes,
        int endMinutes,
        std::string location);

    DayOfWeek day() const noexcept { return day_; }
    int startMinutes() const noexcept { return startMinutes_; }
    int endMinutes() const noexcept { return endMinutes_; }
    const std::string& location() const noexcept { return location_; }

private:
    ScheduleSlot(DayOfWeek day, int startMinutes, int endMinutes, std::string location);

    DayOfWeek day_;
    int startMinutes_;
    int endMinutes_;
    std::string location_;
};

bool schedulesOverlap(const ScheduleSlot& left, const ScheduleSlot& right) noexcept;

}
