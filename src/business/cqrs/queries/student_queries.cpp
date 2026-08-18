#include "nexusenroll/business/cqrs/queries/student_queries.hpp"

#include "nexusenroll/business/cqrs/student_validation.hpp"

#include <set>
#include <utility>

namespace nexusenroll::business::cqrs::queries {

using namespace common;
using namespace data::contracts;
using namespace domain;
using namespace std;

namespace {

template <typename T>
Result<T> failure(const Error& error) {
    return Result<T>::failure(error.code, error.message);
}

template <typename T>
Result<T> missingRelated(const string& description) {
    return Result<T>::failure(
        "STUDENT_DATA_INTEGRITY_ERROR", description + " is missing from persisted Student data.");
}

}

BrowseCourseCatalogueQuery::BrowseCourseCatalogueQuery(
    StudentId studentId,
    CatalogueFilter filter,
    const IUserStore& userStore,
    const ICourseStore& courseStore)
    : studentId_(move(studentId)),
      filter_(move(filter)),
      userStore_(userStore),
      courseStore_(courseStore) {}

Result<vector<CatalogueItem>> BrowseCourseCatalogueQuery::execute() const {
    auto student = validateActiveStudent(studentId_, userStore_);
    if (!student) {
        return failure<vector<CatalogueItem>>(student.error());
    }
    return courseStore_.browseCatalogue(filter_);
}

GetStudentScheduleQuery::GetStudentScheduleQuery(
    StudentId studentId,
    string semester,
    const IUserStore& userStore,
    const IEnrollmentStore& enrollmentStore,
    const ICourseStore& courseStore)
    : studentId_(move(studentId)),
      semester_(move(semester)),
      userStore_(userStore),
      enrollmentStore_(enrollmentStore),
      courseStore_(courseStore) {}

Result<vector<StudentScheduleItem>> GetStudentScheduleQuery::execute() const {
    auto student = validateActiveStudent(studentId_, userStore_);
    if (!student) {
        return failure<vector<StudentScheduleItem>>(student.error());
    }
    auto enrollments = enrollmentStore_.scheduleEnrollmentsForStudent(studentId_, semester_);
    if (!enrollments) {
        return failure<vector<StudentScheduleItem>>(enrollments.error());
    }

    vector<StudentScheduleItem> schedule;
    for (const auto& enrollment : enrollments.value()) {
        auto offering = courseStore_.findOffering(enrollment.offeringId);
        if (!offering) {
            return failure<vector<StudentScheduleItem>>(offering.error());
        }
        if (!offering.value()) {
            return missingRelated<vector<StudentScheduleItem>>("A scheduled offering");
        }
        auto course = courseStore_.findCourse(offering.value()->courseId);
        if (!course) {
            return failure<vector<StudentScheduleItem>>(course.error());
        }
        if (!course.value()) {
            return missingRelated<vector<StudentScheduleItem>>("A scheduled course");
        }
        schedule.push_back({enrollment, *offering.value(), *course.value()});
    }
    return Result<vector<StudentScheduleItem>>::success(move(schedule));
}

GetAcademicProgressQuery::GetAcademicProgressQuery(
    StudentId studentId,
    const IUserStore& userStore,
    const IGradeStore& gradeStore,
    const IProgramStore& programStore,
    const ICourseStore& courseStore)
    : studentId_(move(studentId)),
      userStore_(userStore),
      gradeStore_(gradeStore),
      programStore_(programStore),
      courseStore_(courseStore) {}

Result<AcademicProgress> GetAcademicProgressQuery::execute() const {
    auto student = validateActiveStudent(studentId_, userStore_);
    if (!student) {
        return failure<AcademicProgress>(student.error());
    }
    auto program = programStore_.findProgram(student.value().programId);
    if (!program) {
        return failure<AcademicProgress>(program.error());
    }
    if (!program.value()) {
        return missingRelated<AcademicProgress>("The Student degree programme");
    }
    auto grades = gradeStore_.submittedGradesForStudent(studentId_);
    if (!grades) {
        return failure<AcademicProgress>(grades.error());
    }

    AcademicProgress progress{*program.value(), {}, {}};
    set<CourseId> completedIds;
    for (const auto& grade : grades.value()) {
        auto course = courseStore_.findCourse(grade.courseId);
        if (!course) {
            return failure<AcademicProgress>(course.error());
        }
        if (!course.value()) {
            return missingRelated<AcademicProgress>("A completed course");
        }
        completedIds.insert(grade.courseId);
        progress.completedCourses.push_back({*course.value(), grade.grade});
    }
    for (const auto& courseId : progress.program.requiredCourseIds) {
        if (completedIds.count(courseId) != 0) {
            continue;
        }
        auto course = courseStore_.findCourse(courseId);
        if (!course) {
            return failure<AcademicProgress>(course.error());
        }
        if (!course.value()) {
            return missingRelated<AcademicProgress>("A programme requirement");
        }
        progress.remainingRequiredCourses.push_back(*course.value());
    }
    return Result<AcademicProgress>::success(move(progress));
}

GetStudentWaitlistQuery::GetStudentWaitlistQuery(
    StudentId studentId,
    const IUserStore& userStore,
    const IWaitlistStore& waitlistStore,
    const ICourseStore& courseStore)
    : studentId_(move(studentId)),
      userStore_(userStore),
      waitlistStore_(waitlistStore),
      courseStore_(courseStore) {}

Result<vector<StudentWaitlistItem>> GetStudentWaitlistQuery::execute() const {
    auto student = validateActiveStudent(studentId_, userStore_);
    if (!student) {
        return failure<vector<StudentWaitlistItem>>(student.error());
    }
    auto entries = waitlistStore_.waitlistEntriesForStudent(studentId_);
    if (!entries) {
        return failure<vector<StudentWaitlistItem>>(entries.error());
    }

    vector<StudentWaitlistItem> waitlist;
    for (const auto& entry : entries.value()) {
        auto offering = courseStore_.findOffering(entry.offeringId);
        if (!offering) {
            return failure<vector<StudentWaitlistItem>>(offering.error());
        }
        if (!offering.value()) {
            return missingRelated<vector<StudentWaitlistItem>>("A waitlisted offering");
        }
        auto course = courseStore_.findCourse(offering.value()->courseId);
        if (!course) {
            return failure<vector<StudentWaitlistItem>>(course.error());
        }
        if (!course.value()) {
            return missingRelated<vector<StudentWaitlistItem>>("A waitlisted course");
        }
        waitlist.push_back({entry, *offering.value(), *course.value()});
    }
    return Result<vector<StudentWaitlistItem>>::success(move(waitlist));
}

}
