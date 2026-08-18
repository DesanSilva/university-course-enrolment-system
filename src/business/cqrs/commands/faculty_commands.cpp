#include "nexusenroll/business/cqrs/commands/finalize_grades_command.hpp"
#include "nexusenroll/business/cqrs/commands/submit_course_change_request_command.hpp"
#include "nexusenroll/business/cqrs/commands/submit_grades_command.hpp"

#include "nexusenroll/business/cqrs/faculty_validation.hpp"
#include "nexusenroll/business/cqrs/persistent_id.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

namespace nexusenroll::business::cqrs::commands {

using namespace common;
using namespace data::contracts;
using namespace domain;
using namespace std;

namespace {

constexpr size_t maximumGradeBatchSize = 200;
constexpr int64_t maximumOfferingCapacity = 65535;

CommandResult failure(const Error& error) {
    return CommandResult::failure(error.code, error.message);
}

bool validGrade(const string& grade) {
    static const set<string> gradeScale{"A", "B", "C", "D", "F"};
    return gradeScale.count(grade) != 0;
}

CommandResult validateOwnedOffering(
    FacultyId facultyId,
    OfferingId offeringId,
    const IUserStore& userStore,
    const ICourseStore& courseStore,
    optional<CourseOffering>& offeringValue) {
    auto faculty = validateActiveFaculty(facultyId, userStore);
    if (!faculty) {
        return failure(faculty.error());
    }
    auto offering = courseStore.findOffering(offeringId);
    if (!offering) {
        return failure(offering.error());
    }
    if (!offering.value()) {
        return CommandResult::failure(
            "OFFERING_NOT_FOUND", "The requested course offering does not exist.");
    }
    if (offering.value()->instructorId != facultyId) {
        return CommandResult::failure(
            "FACULTY_OFFERING_MISMATCH",
            "The selected Faculty member is not assigned to this offering.");
    }
    offeringValue = *offering.value();
    return CommandResult::success();
}

string prerequisiteValue(vector<CourseId> prerequisites) {
    sort(prerequisites.begin(), prerequisites.end());
    ostringstream stream;
    for (size_t index = 0; index < prerequisites.size(); ++index) {
        if (index != 0) {
            stream << ',';
        }
        stream << prerequisites[index].value();
    }
    return stream.str();
}

}

SubmitGradesCommand::SubmitGradesCommand(
    FacultyId facultyId,
    OfferingId offeringId,
    vector<GradeInput> grades,
    const IUserStore& userStore,
    const ICourseStore& courseStore,
    const IEnrollmentStore& enrollmentStore,
    IGradeStore& gradeStore,
    ITransactionBoundary& transactionBoundary)
    : facultyId_(move(facultyId)),
      offeringId_(move(offeringId)),
      grades_(move(grades)),
      userStore_(userStore),
      courseStore_(courseStore),
      enrollmentStore_(enrollmentStore),
      gradeStore_(gradeStore),
      transactionBoundary_(transactionBoundary) {}

CommandResult SubmitGradesCommand::execute() {
    outcome_ = {};
    if (grades_.empty()) {
        return CommandResult::failure(
            "EMPTY_GRADE_BATCH", "At least one grade entry is required.");
    }
    if (grades_.size() > maximumGradeBatchSize) {
        return CommandResult::failure(
            "GRADE_BATCH_TOO_LARGE", "The grade batch exceeds the supported limit.");
    }

    vector<GradeInput> candidates;
    set<StudentId> seen;
    for (const auto& input : grades_) {
        if (input.studentId.empty()) {
            outcome_.rejected.push_back(
                {input, {"INVALID_STUDENT_ID", "A grade entry requires a Student ID."}});
        } else if (!seen.insert(input.studentId).second) {
            outcome_.rejected.push_back(
                {input, {"DUPLICATE_STUDENT", "A Student may appear only once in a grade batch."}});
        } else if (input.grade.empty() || !validGrade(input.grade)) {
            outcome_.rejected.push_back(
                {input, {"INVALID_GRADE", "The grade value is not supported."}});
        } else {
            candidates.push_back(input);
        }
    }
    if (candidates.empty()) {
        return CommandResult::success();
    }

    auto transaction = transactionBoundary_.executeTransaction([this, &candidates] {
        optional<CourseOffering> offering;
        auto ownership = validateOwnedOffering(
            facultyId_, offeringId_, userStore_, courseStore_, offering);
        if (!ownership) {
            return ownership;
        }

        for (const auto& input : candidates) {
            auto enrollment = enrollmentStore_.findStudentEnrollment(input.studentId, offeringId_);
            if (!enrollment) {
                return failure(enrollment.error());
            }
            if (!enrollment.value() ||
                enrollment.value()->status != EnrollmentStatus::Active) {
                outcome_.rejected.push_back(
                    {input,
                     {"GRADABLE_ENROLLMENT_NOT_FOUND",
                      "The Student has no active gradable enrolment in this offering."}});
                continue;
            }

            auto existing = gradeStore_.findStudentGradeRecord(input.studentId, offeringId_);
            if (!existing) {
                return failure(existing.error());
            }
            if (existing.value() &&
                existing.value()->lifecycle == GradeLifecycle::Submitted) {
                outcome_.rejected.push_back(
                    {input,
                     {"GRADE_ALREADY_SUBMITTED",
                      "A Submitted grade is final and cannot be overwritten."}});
                continue;
            }

            GradeRecord grade{
                existing.value() ? existing.value()->id
                                 : GradeRecordId{newPersistentId("GRD-")},
                input.studentId,
                offeringId_,
                offering->courseId,
                input.grade,
                GradeLifecycle::Pending};
            auto saved = existing.value() ? gradeStore_.saveGradeRecord(move(grade))
                                          : gradeStore_.createGradeRecord(move(grade));
            if (!saved) {
                return saved;
            }
            outcome_.accepted.push_back(input);
        }
        return CommandResult::success();
    });
    if (!transaction) {
        outcome_.accepted.clear();
    }
    return transaction;
}

FinalizeGradesCommand::FinalizeGradesCommand(
    FacultyId facultyId,
    OfferingId offeringId,
    const IUserStore& userStore,
    const ICourseStore& courseStore,
    IEnrollmentStore& enrollmentStore,
    IGradeStore& gradeStore,
    ITransactionBoundary& transactionBoundary)
    : facultyId_(move(facultyId)),
      offeringId_(move(offeringId)),
      userStore_(userStore),
      courseStore_(courseStore),
      enrollmentStore_(enrollmentStore),
      gradeStore_(gradeStore),
      transactionBoundary_(transactionBoundary) {}

CommandResult FinalizeGradesCommand::execute() {
    finalizedCount_ = 0;
    size_t completed = 0;
    auto transaction = transactionBoundary_.executeTransaction([this, &completed] {
        optional<CourseOffering> offering;
        auto ownership = validateOwnedOffering(
            facultyId_, offeringId_, userStore_, courseStore_, offering);
        if (!ownership) {
            return ownership;
        }
        auto grades = gradeStore_.pendingGradesForOffering(offeringId_);
        if (!grades) {
            return failure(grades.error());
        }
        if (grades.value().empty()) {
            return CommandResult::failure(
                "NO_PENDING_GRADES", "The offering has no Pending grades to finalise.");
        }

        for (auto grade : grades.value()) {
            auto enrollment = enrollmentStore_.findStudentEnrollment(
                grade.studentId, offeringId_);
            if (!enrollment) {
                return failure(enrollment.error());
            }
            if (!enrollment.value() || enrollment.value()->id.empty() ||
                enrollment.value()->studentId != grade.studentId ||
                enrollment.value()->offeringId != offeringId_ ||
                enrollment.value()->status != EnrollmentStatus::Active) {
                return CommandResult::failure(
                    "GRADE_ENROLLMENT_INTEGRITY_ERROR",
                    "A Pending grade is not linked to its expected active enrolment.");
            }

            grade.lifecycle = GradeLifecycle::Submitted;
            auto savedGrade = gradeStore_.saveGradeRecord(move(grade));
            if (!savedGrade) {
                return savedGrade;
            }
            Enrollment completedEnrollment = *enrollment.value();
            completedEnrollment.status = EnrollmentStatus::Completed;
            auto savedEnrollment = enrollmentStore_.saveEnrollment(move(completedEnrollment));
            if (!savedEnrollment) {
                return savedEnrollment;
            }
            ++completed;
        }
        return CommandResult::success();
    });
    if (transaction) {
        finalizedCount_ = completed;
    }
    return transaction;
}

SubmitCourseChangeRequestCommand::SubmitCourseChangeRequestCommand(
    FacultyId facultyId,
    CourseChangeInput input,
    const IUserStore& userStore,
    const ICourseStore& courseStore,
    IChangeRequestStore& changeRequestStore)
    : facultyId_(move(facultyId)),
      input_(move(input)),
      userStore_(userStore),
      courseStore_(courseStore),
      changeRequestStore_(changeRequestStore) {}

CommandResult SubmitCourseChangeRequestCommand::execute() {
    requestId_ = ChangeRequestId{};
    auto faculty = validateActiveFaculty(facultyId_, userStore_);
    if (!faculty) {
        return failure(faculty.error());
    }
    auto course = courseStore_.findCourse(input_.courseId);
    if (!course) {
        return failure(course.error());
    }
    if (!course.value()) {
        return CommandResult::failure(
            "COURSE_NOT_FOUND", "The requested course does not exist.");
    }

    string requestedValue;
    optional<OfferingId> offeringId;
    if (input_.type != CourseChangeType::Description &&
        input_.type != CourseChangeType::Prerequisites &&
        input_.type != CourseChangeType::Capacity) {
        return CommandResult::failure(
            "INVALID_CHANGE_TYPE", "The course-change type is not supported.");
    }
    if (input_.type == CourseChangeType::Capacity) {
        if (!input_.offeringId) {
            return CommandResult::failure(
                "INVALID_CHANGE_VALUE",
                "A capacity request requires an offering ID.");
        }
        auto offering = courseStore_.findOffering(*input_.offeringId);
        if (!offering) {
            return failure(offering.error());
        }
        if (!offering.value()) {
            return CommandResult::failure(
                "OFFERING_NOT_FOUND", "The requested course offering does not exist.");
        }
        if (offering.value()->courseId != input_.courseId) {
            return CommandResult::failure(
                "OFFERING_COURSE_MISMATCH", "The offering does not belong to the requested course.");
        }
        if (offering.value()->instructorId != facultyId_) {
            return CommandResult::failure(
                "FACULTY_OFFERING_MISMATCH",
                "The selected Faculty member is not assigned to this offering.");
        }
        if (!input_.capacity || *input_.capacity <= 0 ||
            *input_.capacity > maximumOfferingCapacity ||
            !input_.description.empty() || !input_.prerequisiteCourseIds.empty()) {
            return CommandResult::failure(
                "INVALID_CHANGE_VALUE",
                "A capacity request requires only a positive representable capacity and offering ID.");
        }
        offeringId = input_.offeringId;
        requestedValue = to_string(*input_.capacity);
    } else {
        auto teachesCourse = courseStore_.facultyTeachesCourse(facultyId_, input_.courseId);
        if (!teachesCourse) {
            return failure(teachesCourse.error());
        }
        if (!teachesCourse.value()) {
            return CommandResult::failure(
                "FACULTY_COURSE_MISMATCH",
                "The selected Faculty member is not assigned to this course.");
        }
        if (input_.offeringId || input_.capacity) {
            return CommandResult::failure(
                "INVALID_CHANGE_VALUE", "This course-level request has incompatible values.");
        }
        if (input_.type == CourseChangeType::Description) {
            if (input_.description.empty() || !input_.prerequisiteCourseIds.empty()) {
                return CommandResult::failure(
                    "INVALID_CHANGE_VALUE", "A description request requires one non-empty string.");
            }
            requestedValue = input_.description;
        } else {
            if (!input_.description.empty()) {
                return CommandResult::failure(
                    "INVALID_CHANGE_VALUE", "A prerequisite request requires a Course ID list.");
            }
            set<CourseId> unique;
            for (const auto& prerequisiteId : input_.prerequisiteCourseIds) {
                if (prerequisiteId.empty()) {
                    return CommandResult::failure(
                        "INVALID_PREREQUISITE", "A prerequisite Course ID must not be empty.");
                }
                if (prerequisiteId == input_.courseId) {
                    return CommandResult::failure(
                        "SELF_PREREQUISITE", "A course cannot be its own prerequisite.");
                }
                if (!unique.insert(prerequisiteId).second) {
                    return CommandResult::failure(
                        "DUPLICATE_PREREQUISITE", "A prerequisite may appear only once.");
                }
                auto prerequisite = courseStore_.findCourse(prerequisiteId);
                if (!prerequisite) {
                    return failure(prerequisite.error());
                }
                if (!prerequisite.value()) {
                    return CommandResult::failure(
                        "PREREQUISITE_NOT_FOUND", "A proposed prerequisite does not exist.");
                }
            }
            requestedValue = prerequisiteValue(input_.prerequisiteCourseIds);
        }
    }

    requestId_ = ChangeRequestId{newPersistentId("CHG-")};
    auto saved = changeRequestStore_.createChangeRequest(
        {requestId_, facultyId_, input_.courseId, move(offeringId), input_.type,
         CourseChangeStatus::Pending, move(requestedValue)});
    if (!saved) {
        requestId_ = ChangeRequestId{};
    }
    return saved;
}

}
