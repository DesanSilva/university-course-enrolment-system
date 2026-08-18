#pragma once

#include "nexusenroll/business/cqrs/commands/command.hpp"
#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/identifiers.hpp"
#include "nexusenroll/data/contracts/change_request_store.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nexusenroll::business::cqrs::commands {

struct CourseChangeInput {
    common::CourseId courseId;
    domain::CourseChangeType type;
    std::string description;
    std::vector<common::CourseId> prerequisiteCourseIds;
    std::optional<std::int64_t> capacity;
    std::optional<common::OfferingId> offeringId;
};

class SubmitCourseChangeRequestCommand final : public ICommand {
public:
    SubmitCourseChangeRequestCommand(
        common::FacultyId facultyId,
        CourseChangeInput input,
        const data::contracts::IUserStore& userStore,
        const data::contracts::ICourseStore& courseStore,
        data::contracts::IChangeRequestStore& changeRequestStore);

    CommandResult execute() override;
    const common::ChangeRequestId& requestId() const noexcept { return requestId_; }

private:
    common::FacultyId facultyId_;
    CourseChangeInput input_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::ICourseStore& courseStore_;
    data::contracts::IChangeRequestStore& changeRequestStore_;
    common::ChangeRequestId requestId_;
};

}
