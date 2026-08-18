#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"

#include <optional>
#include <vector>

namespace nexusenroll::data::contracts {

class IChangeRequestStore {
public:
    virtual ~IChangeRequestStore() = default;

    virtual common::Result<std::optional<business::domain::CourseChangeRequest>> findChangeRequest(
        common::ChangeRequestId id) const = 0;
    virtual common::Result<std::vector<business::domain::CourseChangeRequest>> changeRequests() const = 0;
    virtual common::Result<std::vector<business::domain::CourseChangeRequest>>
    changeRequestsForFaculty(common::FacultyId facultyId) const = 0;
    virtual common::Result<void> createChangeRequest(
        business::domain::CourseChangeRequest request) = 0;
    virtual common::Result<void> saveChangeRequest(
        business::domain::CourseChangeRequest request) = 0;
};

}
