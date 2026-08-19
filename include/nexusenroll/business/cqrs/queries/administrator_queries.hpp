#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"
#include "nexusenroll/data/contracts/change_request_store.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/program_store.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"

#include <optional>
#include <vector>

namespace nexusenroll::business::cqrs::queries {

class GetAdministratorCoursesQuery {
public:
    explicit GetAdministratorCoursesQuery(const data::contracts::ICourseStore& courseStore);
    common::Result<std::vector<domain::Course>> execute() const;

private:
    const data::contracts::ICourseStore& courseStore_;
};

class GetAdministratorProgramsQuery {
public:
    explicit GetAdministratorProgramsQuery(const data::contracts::IProgramStore& programStore);
    common::Result<std::vector<domain::DegreeProgram>> execute() const;

private:
    const data::contracts::IProgramStore& programStore_;
};

class GetAdministratorUsersQuery {
public:
    GetAdministratorUsersQuery(
        std::optional<domain::UserRole> role,
        const data::contracts::IUserStore& userStore);
    common::Result<std::vector<domain::User>> execute() const;

private:
    std::optional<domain::UserRole> role_;
    const data::contracts::IUserStore& userStore_;
};

class GetAdministratorCourseChangesQuery {
public:
    GetAdministratorCourseChangesQuery(
        std::optional<domain::CourseChangeStatus> status,
        const data::contracts::IChangeRequestStore& changeRequestStore);
    common::Result<std::vector<domain::CourseChangeRequest>> execute() const;

private:
    std::optional<domain::CourseChangeStatus> status_;
    const data::contracts::IChangeRequestStore& changeRequestStore_;
};

}
