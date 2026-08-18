#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"
#include "nexusenroll/data/contracts/change_request_store.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/enrollment_store.hpp"
#include "nexusenroll/data/contracts/grade_store.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"

#include <vector>

namespace nexusenroll::business::cqrs::queries {

class GetFacultyOfferingsQuery {
public:
    GetFacultyOfferingsQuery(
        common::FacultyId facultyId,
        const data::contracts::IUserStore& userStore,
        const data::contracts::ICourseStore& courseStore);

    common::Result<std::vector<domain::FacultyOfferingItem>> execute() const;

private:
    common::FacultyId facultyId_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::ICourseStore& courseStore_;
};

class GetClassRosterQuery {
public:
    GetClassRosterQuery(
        common::FacultyId facultyId,
        common::OfferingId offeringId,
        const data::contracts::IUserStore& userStore,
        const data::contracts::ICourseStore& courseStore,
        const data::contracts::IEnrollmentStore& enrollmentStore);

    common::Result<std::vector<domain::FacultyRosterEntry>> execute() const;

private:
    common::FacultyId facultyId_;
    common::OfferingId offeringId_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::ICourseStore& courseStore_;
    const data::contracts::IEnrollmentStore& enrollmentStore_;
};

class GetGradeStateQuery {
public:
    GetGradeStateQuery(
        common::FacultyId facultyId,
        common::OfferingId offeringId,
        const data::contracts::IUserStore& userStore,
        const data::contracts::ICourseStore& courseStore,
        const data::contracts::IGradeStore& gradeStore);

    common::Result<std::vector<domain::FacultyGradeStateEntry>> execute() const;

private:
    common::FacultyId facultyId_;
    common::OfferingId offeringId_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::ICourseStore& courseStore_;
    const data::contracts::IGradeStore& gradeStore_;
};

class GetFacultyCourseChangeRequestsQuery {
public:
    GetFacultyCourseChangeRequestsQuery(
        common::FacultyId facultyId,
        const data::contracts::IUserStore& userStore,
        const data::contracts::IChangeRequestStore& changeRequestStore);

    common::Result<std::vector<domain::CourseChangeRequest>> execute() const;

private:
    common::FacultyId facultyId_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::IChangeRequestStore& changeRequestStore_;
};

}
