#include "nexusenroll/business/cqrs/queries/faculty_queries.hpp"

#include "nexusenroll/business/cqrs/faculty_validation.hpp"

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
Result<T> validateOwnedOffering(
    FacultyId facultyId,
    OfferingId offeringId,
    const IUserStore& userStore,
    const ICourseStore& courseStore) {
    auto faculty = validateActiveFaculty(facultyId, userStore);
    if (!faculty) {
        return failure<T>(faculty.error());
    }
    auto offering = courseStore.findOffering(offeringId);
    if (!offering) {
        return failure<T>(offering.error());
    }
    if (!offering.value()) {
        return Result<T>::failure(
            "OFFERING_NOT_FOUND", "The requested course offering does not exist.");
    }
    if (offering.value()->instructorId != facultyId) {
        return Result<T>::failure(
            "FACULTY_OFFERING_MISMATCH",
            "The selected Faculty member is not assigned to this offering.");
    }
    return Result<T>::success(T{});
}

}

GetFacultyOfferingsQuery::GetFacultyOfferingsQuery(
    FacultyId facultyId,
    const IUserStore& userStore,
    const ICourseStore& courseStore)
    : facultyId_(move(facultyId)), userStore_(userStore), courseStore_(courseStore) {}

Result<vector<FacultyOfferingItem>> GetFacultyOfferingsQuery::execute() const {
    auto faculty = validateActiveFaculty(facultyId_, userStore_);
    if (!faculty) {
        return failure<vector<FacultyOfferingItem>>(faculty.error());
    }
    return courseStore_.assignedOfferings(facultyId_);
}

GetClassRosterQuery::GetClassRosterQuery(
    FacultyId facultyId,
    OfferingId offeringId,
    const IUserStore& userStore,
    const ICourseStore& courseStore,
    const IEnrollmentStore& enrollmentStore)
    : facultyId_(move(facultyId)),
      offeringId_(move(offeringId)),
      userStore_(userStore),
      courseStore_(courseStore),
      enrollmentStore_(enrollmentStore) {}

Result<vector<FacultyRosterEntry>> GetClassRosterQuery::execute() const {
    auto ownership = validateOwnedOffering<vector<FacultyRosterEntry>>(
        facultyId_, offeringId_, userStore_, courseStore_);
    if (!ownership) {
        return ownership;
    }
    return enrollmentStore_.activeRosterForOffering(offeringId_);
}

GetGradeStateQuery::GetGradeStateQuery(
    FacultyId facultyId,
    OfferingId offeringId,
    const IUserStore& userStore,
    const ICourseStore& courseStore,
    const IGradeStore& gradeStore)
    : facultyId_(move(facultyId)),
      offeringId_(move(offeringId)),
      userStore_(userStore),
      courseStore_(courseStore),
      gradeStore_(gradeStore) {}

Result<vector<FacultyGradeStateEntry>> GetGradeStateQuery::execute() const {
    auto ownership = validateOwnedOffering<vector<FacultyGradeStateEntry>>(
        facultyId_, offeringId_, userStore_, courseStore_);
    if (!ownership) {
        return ownership;
    }
    return gradeStore_.gradeStateForOffering(offeringId_);
}

GetFacultyCourseChangeRequestsQuery::GetFacultyCourseChangeRequestsQuery(
    FacultyId facultyId,
    const IUserStore& userStore,
    const IChangeRequestStore& changeRequestStore)
    : facultyId_(move(facultyId)),
      userStore_(userStore),
      changeRequestStore_(changeRequestStore) {}

Result<vector<CourseChangeRequest>> GetFacultyCourseChangeRequestsQuery::execute() const {
    auto faculty = validateActiveFaculty(facultyId_, userStore_);
    if (!faculty) {
        return failure<vector<CourseChangeRequest>>(faculty.error());
    }
    return changeRequestStore_.changeRequestsForFaculty(facultyId_);
}

}
