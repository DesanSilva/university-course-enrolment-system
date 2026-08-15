#include "nexusenroll/business/domain/schedule.hpp"

#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace nexusenroll::business::domain;
using namespace std;

void require(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

ScheduleSlot slot(DayOfWeek day, int start, int end) {
    auto result = ScheduleSlot::create(day, start, end, "TEST-ROOM");
    require(static_cast<bool>(result), "Expected a valid schedule slot");
    return result.value();
}

void testScheduleOverlapSemantics() {
    const auto mondayMorning = slot(DayOfWeek::Monday, 540, 600);
    const auto mondayOverlap = slot(DayOfWeek::Monday, 570, 630);
    const auto tuesdaySameTime = slot(DayOfWeek::Tuesday, 540, 600);
    const auto mondayBackToBack = slot(DayOfWeek::Monday, 600, 660);

    require(schedulesOverlap(mondayMorning, mondayOverlap),
            "Intervals on the same day should overlap");
    require(!schedulesOverlap(mondayMorning, tuesdaySameTime),
            "Intervals on different days should not overlap");
    require(!schedulesOverlap(mondayMorning, mondayBackToBack),
            "Back-to-back intervals should not overlap");
}

void testInvalidScheduleRanges() {
    require(!ScheduleSlot::create(DayOfWeek::Monday, 600, 600, "ROOM"),
            "A zero-length interval should be invalid");
    require(!ScheduleSlot::create(DayOfWeek::Monday, 660, 600, "ROOM"),
            "A reversed interval should be invalid");
    require(!ScheduleSlot::create(DayOfWeek::Monday, -1, 60, "ROOM"),
            "A negative start should be invalid");
    require(!ScheduleSlot::create(DayOfWeek::Monday, 1380, 1441, "ROOM"),
            "An end after midnight should be invalid");
}

}

int main() {
    const vector<pair<string, function<void()>>> tests{
        {"schedule overlap semantics", testScheduleOverlapSemantics},
        {"invalid schedule ranges", testInvalidScheduleRanges},
    };

    size_t passed = 0;
    for (const auto& test : tests) {
        try {
            test.second();
            ++passed;
            cout << "PASS: " << test.first << '\n';
        } catch (const exception& exception) {
            cerr << "FAIL: " << test.first << ": " << exception.what() << '\n';
        }
    }

    cout << passed << '/' << tests.size() << " tests passed\n";
    return passed == tests.size() ? 0 : 1;
}
