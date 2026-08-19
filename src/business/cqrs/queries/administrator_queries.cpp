#include "nexusenroll/business/cqrs/queries/administrator_queries.hpp"

#include <algorithm>
#include <tuple>
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

GetEnrollmentReportQuery::GetEnrollmentReportQuery(
    optional<string> department,
    optional<string> semester,
    const ICourseStore& courseStore)
    : department_(move(department)),
      semester_(move(semester)),
      courseStore_(courseStore) {}

Result<vector<EnrollmentReportItem>> GetEnrollmentReportQuery::execute() const {
    CatalogueFilter filter;
    if (department_) filter.department = *department_;
    if (semester_) filter.semester = *semester_;

    auto catalogue = courseStore_.browseCatalogue(filter);
    if (!catalogue) {
        return Result<vector<EnrollmentReportItem>>::failure(
            catalogue.error().code, catalogue.error().message);
    }

    vector<EnrollmentReportItem> items;
    items.reserve(catalogue.value().size());
    for (const auto& item : catalogue.value()) {
        const size_t enrolled = item.offering.enrolledCount;
        const size_t cap = item.offering.capacity;
        const double util = cap > 0 ? static_cast<double>(enrolled) / static_cast<double>(cap) : 0.0;
        items.push_back(EnrollmentReportItem{
            item.course,
            item.offering,
            item.instructorName,
            item.course.department,
            enrolled,
            cap,
            util});
    }

    sort(items.begin(), items.end(), [](const EnrollmentReportItem& a, const EnrollmentReportItem& b) {
        return tie(a.department, a.course.code, a.offering.semester, a.offering.id) <
               tie(b.department, b.course.code, b.offering.semester, b.offering.id);
    });

    return Result<vector<EnrollmentReportItem>>::success(move(items));
}

GetFacultyWorkloadReportQuery::GetFacultyWorkloadReportQuery(
    optional<string> semester,
    const IUserStore& userStore,
    const ICourseStore& courseStore)
    : semester_(move(semester)),
      userStore_(userStore),
      courseStore_(courseStore) {}

Result<vector<FacultyWorkloadReportItem>> GetFacultyWorkloadReportQuery::execute() const {
    auto facultyList = userStore_.facultyMembers();
    if (!facultyList) {
        return Result<vector<FacultyWorkloadReportItem>>::failure(
            facultyList.error().code, facultyList.error().message);
    }

    vector<FacultyWorkloadReportItem> reportItems;
    for (const auto& faculty : facultyList.value()) {
        auto userResult = userStore_.findUser(faculty.userId);
        if (!userResult) {
            return Result<vector<FacultyWorkloadReportItem>>::failure(
                userResult.error().code, userResult.error().message);
        }
        if (!userResult.value()) continue;

        auto assignedResult = courseStore_.assignedOfferings(faculty.id);
        if (!assignedResult) {
            return Result<vector<FacultyWorkloadReportItem>>::failure(
                assignedResult.error().code, assignedResult.error().message);
        }

        vector<FacultyWorkloadOfferingItem> offerings;
        size_t totalEnrolled = 0;
        for (const auto& assigned : assignedResult.value()) {
            if (semester_ && assigned.offering.semester != *semester_) {
                continue;
            }
            const size_t enrolled = assigned.offering.enrolledCount;
            totalEnrolled += enrolled;
            offerings.push_back(FacultyWorkloadOfferingItem{
                assigned.offering.id,
                assigned.course.id,
                assigned.course.code,
                assigned.course.name,
                enrolled,
                assigned.offering.capacity});
        }

        reportItems.push_back(FacultyWorkloadReportItem{
            faculty.id,
            userResult.value()->name,
            userResult.value()->email,
            faculty.department,
            semester_.value_or("ALL"),
            offerings.size(),
            totalEnrolled,
            move(offerings)});
    }

    sort(reportItems.begin(), reportItems.end(), [](const FacultyWorkloadReportItem& a, const FacultyWorkloadReportItem& b) {
        return tie(a.facultyName, a.facultyId) < tie(b.facultyName, b.facultyId);
    });

    return Result<vector<FacultyWorkloadReportItem>>::success(move(reportItems));
}

GetCoursePopularityReportQuery::GetCoursePopularityReportQuery(
    optional<string> semester,
    const ICourseStore& courseStore)
    : semester_(move(semester)),
      courseStore_(courseStore) {}

Result<vector<CoursePopularityReportItem>> GetCoursePopularityReportQuery::execute() const {
    CatalogueFilter filter;
    if (semester_) filter.semester = *semester_;

    auto catalogue = courseStore_.browseCatalogue(filter);
    if (!catalogue) {
        return Result<vector<CoursePopularityReportItem>>::failure(
            catalogue.error().code, catalogue.error().message);
    }

    vector<CoursePopularityReportItem> items;
    items.reserve(catalogue.value().size());
    for (const auto& item : catalogue.value()) {
        const size_t enrolled = item.offering.enrolledCount;
        const size_t cap = item.offering.capacity;
        const double util = cap > 0 ? static_cast<double>(enrolled) / static_cast<double>(cap) : 0.0;
        items.push_back(CoursePopularityReportItem{
            item.course,
            item.offering,
            item.offering.semester,
            enrolled,
            cap,
            util});
    }

    sort(items.begin(), items.end(), [](const CoursePopularityReportItem& a, const CoursePopularityReportItem& b) {
        if (a.enrolledCount != b.enrolledCount) {
            return a.enrolledCount > b.enrolledCount;
        }
        if (a.utilizationRate != b.utilizationRate) {
            return a.utilizationRate > b.utilizationRate;
        }
        return tie(a.course.code, a.offering.id) < tie(b.course.code, b.offering.id);
    });

    return Result<vector<CoursePopularityReportItem>>::success(move(items));
}

GetCapacityReportQuery::GetCapacityReportQuery(
    optional<string> department,
    optional<string> semester,
    double minUtilization,
    const ICourseStore& courseStore)
    : department_(move(department)),
      semester_(move(semester)),
      minUtilization_(minUtilization),
      courseStore_(courseStore) {}

Result<vector<CapacityReportItem>> GetCapacityReportQuery::execute() const {
    CatalogueFilter filter;
    if (department_) filter.department = *department_;
    if (semester_) filter.semester = *semester_;

    auto catalogue = courseStore_.browseCatalogue(filter);
    if (!catalogue) {
        return Result<vector<CapacityReportItem>>::failure(
            catalogue.error().code, catalogue.error().message);
    }

    vector<CapacityReportItem> items;
    for (const auto& item : catalogue.value()) {
        const size_t enrolled = item.offering.enrolledCount;
        const size_t cap = item.offering.capacity;
        const double util = cap > 0 ? static_cast<double>(enrolled) / static_cast<double>(cap) : 0.0;
        if (util >= minUtilization_) {
            items.push_back(CapacityReportItem{
                item.course,
                item.offering,
                item.instructorName,
                item.course.department,
                item.offering.semester,
                enrolled,
                cap,
                util});
        }
    }

    sort(items.begin(), items.end(), [](const CapacityReportItem& a, const CapacityReportItem& b) {
        if (a.utilizationRate != b.utilizationRate) {
            return a.utilizationRate > b.utilizationRate;
        }
        return tie(a.department, a.course.code, a.offering.id) <
               tie(b.department, b.course.code, b.offering.id);
    });

    return Result<vector<CapacityReportItem>>::success(move(items));
}

}

