#pragma once

#include "crow_all.h"
#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"

#include <string>
#include <vector>

namespace nexusenroll::presentation::api {

// Helper / Utility functions for API formatting and enum conversions across presentation routes.
crow::response makeErrorResponse(int status, const common::Error& error);
crow::response makeInvalidRequest(const std::string& code, const std::string& message);

const char* formatRoleName(business::domain::UserRole role);
const char* formatEnrollmentStatusName(business::domain::EnrollmentStatus status);
const char* formatWaitlistStatusName(business::domain::WaitlistStatus status);
const char* formatChangeTypeName(business::domain::CourseChangeType type);
const char* formatChangeStatusName(business::domain::CourseChangeStatus status);
const char* formatDayName(business::domain::DayOfWeek day);

std::string formatTimeText(int minutes);
crow::json::wvalue formatScheduleJson(const std::vector<business::domain::ScheduleSlot>& schedule);

} // namespace nexusenroll::presentation::api
