#include "nexusenroll/business/cqrs/commands/drop_course_command.hpp"
#include "nexusenroll/business/cqrs/commands/enroll_student_command.hpp"
#include "nexusenroll/business/cqrs/commands/join_waitlist_command.hpp"

#include "nexusenroll/business/cqrs/student_validation.hpp"
#include "nexusenroll/business/cqrs/persistent_id.hpp"
#include "nexusenroll/business/domain/schedule.hpp"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace nexusenroll::business::cqrs::commands {

using namespace common;
using namespace data::contracts;
using namespace domain;
using namespace notifications;
using namespace std;

namespace {

CommandResult failure(const Error& error) {
    return CommandResult::failure(error.code, error.message);
}

bool offeringsConflict(const CourseOffering& left, const CourseOffering& right) {
    if (left.semester != right.semester) {
        return false;
    }
    for (const auto& leftSlot : left.schedule) {
        for (const auto& rightSlot : right.schedule) {
            if (schedulesOverlap(leftSlot, rightSlot)) {
                return true;
            }
        }
    }
    return false;
}

}

EnrollStudentCommand::EnrollStudentCommand(
    StudentId studentId,
    OfferingId offeringId,
    const IUserStore& userStore,
    const ICourseStore& courseStore,
    IEnrollmentStore& enrollmentStore,
    const IGradeStore& gradeStore,
    IWaitlistStore& waitlistStore,
    ITransactionBoundary& transactionBoundary)
    : studentId_(move(studentId)),
      offeringId_(move(offeringId)),
      userStore_(userStore),
      courseStore_(courseStore),
      enrollmentStore_(enrollmentStore),
      gradeStore_(gradeStore),
      waitlistStore_(waitlistStore),
      transactionBoundary_(transactionBoundary) {}

CommandResult EnrollStudentCommand::execute() {
    // Begins the atomic enrolment transaction boundary using the Command Pattern
    return transactionBoundary_.executeTransaction([this] {
        auto student = validateActiveStudent(studentId_, userStore_);
        if (!student) {
            return failure(student.error());
        }

        auto offering = courseStore_.findOffering(offeringId_);
        if (!offering) {
            return failure(offering.error());
        }
        if (!offering.value()) {
            return CommandResult::failure(
                "OFFERING_NOT_FOUND", "The requested course offering does not exist.");
        }

        auto existing = enrollmentStore_.findStudentEnrollment(studentId_, offeringId_);
        if (!existing) {
            return failure(existing.error());
        }
        if (existing.value() && existing.value()->status == EnrollmentStatus::Active) {
            return CommandResult::failure(
                "ALREADY_ENROLLED", "The Student is already actively enrolled in this offering.");
        }
        if (existing.value() && existing.value()->status == EnrollmentStatus::Completed) {
            return CommandResult::failure(
                "ALREADY_COMPLETED", "The Student has already completed this offering.");
        }

        auto course = courseStore_.findCourse(offering.value()->courseId);
        if (!course) {
            return failure(course.error());
        }
        if (!course.value()) {
            return CommandResult::failure(
                "STUDENT_DATA_INTEGRITY_ERROR", "The offering course does not exist.");
        }
        auto grades = gradeStore_.submittedGradesForStudent(studentId_);
        if (!grades) {
            return failure(grades.error());
        }
        set<CourseId> completedCourses;
        for (const auto& grade : grades.value()) {
            completedCourses.insert(grade.courseId);
        }
        for (const auto& prerequisite : course.value()->prerequisiteCourseIds) {
            if (completedCourses.count(prerequisite) == 0) {
                return CommandResult::failure(
                    "PREREQUISITE_NOT_MET",
                    "The Student has not completed every prerequisite with a submitted result.");
            }
        }

        // Validates seat availability to enforce capacity rules
        if (offering.value()->enrolledCount >= offering.value()->capacity) {
            return CommandResult::failure(
                "CAPACITY_FULL", "The requested course offering has no available seats.");
        }

        auto activeEnrollments = enrollmentStore_.activeEnrollmentsForStudent(studentId_);
        if (!activeEnrollments) {
            return failure(activeEnrollments.error());
        }
        for (const auto& enrollment : activeEnrollments.value()) {
            auto enrolledOffering = courseStore_.findOffering(enrollment.offeringId);
            if (!enrolledOffering) {
                return failure(enrolledOffering.error());
            }
            if (!enrolledOffering.value()) {
                return CommandResult::failure(
                    "STUDENT_DATA_INTEGRITY_ERROR", "An active enrolment has no offering.");
            }
            if (offeringsConflict(*offering.value(), *enrolledOffering.value())) {
                return CommandResult::failure(
                    "TIME_CONFLICT", "The requested offering overlaps the Student's schedule.");
            }
        }

        auto waitlistEntry = waitlistStore_.findStudentWaitlistEntry(studentId_, offeringId_);
        if (!waitlistEntry) {
            return failure(waitlistEntry.error());
        }

        const EnrollmentId enrollmentId = existing.value()
                                                  ? existing.value()->id
                                                  : EnrollmentId{newPersistentId("ENR-")};
        auto saved = enrollmentStore_.saveEnrollment(
            {enrollmentId, studentId_, offeringId_, EnrollmentStatus::Active});
        if (!saved) {
            return saved;
        }
        if (waitlistEntry.value() &&
            waitlistEntry.value()->status != WaitlistStatus::Removed) {
            WaitlistEntry retired = *waitlistEntry.value();
            retired.status = WaitlistStatus::Removed;
            return waitlistStore_.saveWaitlistEntry(move(retired));
        }
        return CommandResult::success();
    });
}

DropCourseCommand::DropCourseCommand(
    StudentId studentId,
    OfferingId offeringId,
    const IUserStore& userStore,
    const ICourseStore& courseStore,
    IEnrollmentStore& enrollmentStore,
    const IWaitlistStore& waitlistStore,
    ITransactionBoundary& transactionBoundary,
    const NotificationPublisher& notificationPublisher)
    : studentId_(move(studentId)),
      offeringId_(move(offeringId)),
      userStore_(userStore),
      courseStore_(courseStore),
      enrollmentStore_(enrollmentStore),
      waitlistStore_(waitlistStore),
      transactionBoundary_(transactionBoundary),
      notificationPublisher_(notificationPublisher) {}

CommandResult DropCourseCommand::execute() {
    vector<StudentId> waitingStudents;
    bool opensCapacity = false;
    auto result = transactionBoundary_.executeTransaction([this, &opensCapacity, &waitingStudents] {
        auto student = validateActiveStudent(studentId_, userStore_);
        if (!student) {
            return failure(student.error());
        }
        auto offering = courseStore_.findOffering(offeringId_);
        if (!offering) {
            return failure(offering.error());
        }
        if (!offering.value()) {
            return CommandResult::failure(
                "OFFERING_NOT_FOUND", "The requested course offering does not exist.");
        }
        auto enrollment = enrollmentStore_.findStudentEnrollment(studentId_, offeringId_);
        if (!enrollment) {
            return failure(enrollment.error());
        }
        if (!enrollment.value() || enrollment.value()->status != EnrollmentStatus::Active) {
            return CommandResult::failure(
                "ACTIVE_ENROLLMENT_NOT_FOUND",
                "The Student has no active enrolment in the requested offering.");
        }

        opensCapacity = offering.value()->enrolledCount <= offering.value()->capacity;

        Enrollment dropped = *enrollment.value();
        dropped.status = EnrollmentStatus::Dropped;
        auto saved = enrollmentStore_.saveEnrollment(move(dropped));
        if (!saved) {
            return saved;
        }
        auto waiters = waitlistStore_.waitingEntriesForOffering(offeringId_);
        if (!waiters) {
            return failure(waiters.error());
        }
        for (const auto& waiter : waiters.value()) {
            waitingStudents.push_back(waiter.studentId);
        }
        return CommandResult::success();
    });

    // Uses the Observer Pattern to publish events after the transaction successfully commits
    if (result && opensCapacity && !waitingStudents.empty()) {
        notificationPublisher_.publish({offeringId_, move(waitingStudents)});
    }
    if (result) {
        notificationPublisher_.publishDrop({studentId_, offeringId_});
    }
    return result;
}

JoinWaitlistCommand::JoinWaitlistCommand(
    StudentId studentId,
    OfferingId offeringId,
    const IUserStore& userStore,
    const ICourseStore& courseStore,
    const IEnrollmentStore& enrollmentStore,
    IWaitlistStore& waitlistStore,
    ITransactionBoundary& transactionBoundary)
    : studentId_(move(studentId)),
      offeringId_(move(offeringId)),
      userStore_(userStore),
      courseStore_(courseStore),
      enrollmentStore_(enrollmentStore),
      waitlistStore_(waitlistStore),
      transactionBoundary_(transactionBoundary) {}

CommandResult JoinWaitlistCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        auto student = validateActiveStudent(studentId_, userStore_);
        if (!student) {
            return failure(student.error());
        }
        auto offering = courseStore_.findOffering(offeringId_);
        if (!offering) {
            return failure(offering.error());
        }
        if (!offering.value()) {
            return CommandResult::failure(
                "OFFERING_NOT_FOUND", "The requested course offering does not exist.");
        }
        auto enrollment = enrollmentStore_.findStudentEnrollment(studentId_, offeringId_);
        if (!enrollment) {
            return failure(enrollment.error());
        }
        if (enrollment.value() && enrollment.value()->status == EnrollmentStatus::Active) {
            return CommandResult::failure(
                "ALREADY_ENROLLED", "An actively enrolled Student cannot join the waitlist.");
        }
        auto existing = waitlistStore_.findStudentWaitlistEntry(studentId_, offeringId_);
        if (!existing) {
            return failure(existing.error());
        }
        if (existing.value() && existing.value()->status != WaitlistStatus::Removed) {
            return CommandResult::failure(
                "ALREADY_WAITLISTED", "The Student is already waiting for this offering.");
        }
        if (offering.value()->enrolledCount < offering.value()->capacity) {
            return CommandResult::failure(
                "SEAT_AVAILABLE", "A seat is available; enrolment should be attempted directly.");
        }
        auto position = waitlistStore_.nextWaitlistPosition(offeringId_);
        if (!position) {
            return failure(position.error());
        }
        const WaitlistEntryId entryId = existing.value()
                                                    ? existing.value()->id
                                                    : WaitlistEntryId{newPersistentId("WAIT-")};
        return waitlistStore_.saveWaitlistEntry(
            {entryId, studentId_, offeringId_, position.value(), WaitlistStatus::Waiting});
    });
}

}
