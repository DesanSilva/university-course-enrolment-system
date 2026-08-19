#include "nexusenroll/business/cqrs/queries/administrator_queries.hpp"

#include <utility>

namespace nexusenroll::business::cqrs::queries {

using namespace common;
using namespace data::contracts;
using namespace domain;
using namespace std;

GetAdministratorCoursesQuery::GetAdministratorCoursesQuery(
    const ICourseStore& courseStore)
    : courseStore_(courseStore) {}

Result<vector<Course>> GetAdministratorCoursesQuery::execute() const {
    return courseStore_.courses();
}

GetAdministratorProgramsQuery::GetAdministratorProgramsQuery(
    const IProgramStore& programStore)
    : programStore_(programStore) {}

Result<vector<DegreeProgram>> GetAdministratorProgramsQuery::execute() const {
    return programStore_.programs();
}

GetAdministratorUsersQuery::GetAdministratorUsersQuery(
    optional<UserRole> role,
    const IUserStore& userStore)
    : role_(move(role)), userStore_(userStore) {}

Result<vector<User>> GetAdministratorUsersQuery::execute() const {
    return userStore_.usersByRole(role_);
}

GetAdministratorCourseChangesQuery::GetAdministratorCourseChangesQuery(
    optional<CourseChangeStatus> status,
    const IChangeRequestStore& changeRequestStore)
    : status_(move(status)), changeRequestStore_(changeRequestStore) {}

Result<vector<CourseChangeRequest>> GetAdministratorCourseChangesQuery::execute() const {
    return changeRequestStore_.changeRequestsByStatus(status_);
}

}
