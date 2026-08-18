#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/enrollment_store.hpp"
#include "nexusenroll/data/contracts/grade_store.hpp"
#include "nexusenroll/data/contracts/program_store.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"
#include "nexusenroll/data/contracts/waitlist_store.hpp"

#include <vector>

namespace nexusenroll::business::cqrs::queries {

struct StudentScheduleItem {
    domain::Enrollment enrollment;
    domain::CourseOffering offering;
    domain::Course course;
};

struct CompletedCourse {
    domain::Course course;
    std::string grade;
};

struct AcademicProgress {
    domain::DegreeProgram program;
    std::vector<CompletedCourse> completedCourses;
    std::vector<domain::Course> remainingRequiredCourses;
};

struct StudentWaitlistItem {
    domain::WaitlistEntry entry;
    domain::CourseOffering offering;
    domain::Course course;
};

class BrowseCourseCatalogueQuery {
public:
    BrowseCourseCatalogueQuery(
        common::StudentId studentId,
        domain::CatalogueFilter filter,
        const data::contracts::IUserStore& userStore,
        const data::contracts::ICourseStore& courseStore);

    common::Result<std::vector<domain::CatalogueItem>> execute() const;

private:
    common::StudentId studentId_;
    domain::CatalogueFilter filter_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::ICourseStore& courseStore_;
};

class GetStudentScheduleQuery {
public:
    GetStudentScheduleQuery(
        common::StudentId studentId,
        std::string semester,
        const data::contracts::IUserStore& userStore,
        const data::contracts::IEnrollmentStore& enrollmentStore,
        const data::contracts::ICourseStore& courseStore);

    common::Result<std::vector<StudentScheduleItem>> execute() const;

private:
    common::StudentId studentId_;
    std::string semester_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::IEnrollmentStore& enrollmentStore_;
    const data::contracts::ICourseStore& courseStore_;
};

class GetAcademicProgressQuery {
public:
    GetAcademicProgressQuery(
        common::StudentId studentId,
        const data::contracts::IUserStore& userStore,
        const data::contracts::IGradeStore& gradeStore,
        const data::contracts::IProgramStore& programStore,
        const data::contracts::ICourseStore& courseStore);

    common::Result<AcademicProgress> execute() const;

private:
    common::StudentId studentId_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::IGradeStore& gradeStore_;
    const data::contracts::IProgramStore& programStore_;
    const data::contracts::ICourseStore& courseStore_;
};

class GetStudentWaitlistQuery {
public:
    GetStudentWaitlistQuery(
        common::StudentId studentId,
        const data::contracts::IUserStore& userStore,
        const data::contracts::IWaitlistStore& waitlistStore,
        const data::contracts::ICourseStore& courseStore);

    common::Result<std::vector<StudentWaitlistItem>> execute() const;

private:
    common::StudentId studentId_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::IWaitlistStore& waitlistStore_;
    const data::contracts::ICourseStore& courseStore_;
};

}
