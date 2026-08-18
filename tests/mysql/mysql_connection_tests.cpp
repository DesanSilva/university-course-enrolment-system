#include "nexusenroll/data/mysql/mysql_data_context.hpp"
#include "nexusenroll/business/cqrs/commands/finalize_grades_command.hpp"
#include "nexusenroll/business/cqrs/commands/submit_grades_command.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace nexusenroll;
using namespace business::domain;
using namespace business::cqrs::commands;
using namespace common;
using namespace data::contracts;
using namespace data::mysql;
using namespace std;

void require(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

MySqlConfig testConfig() {
    auto config = loadMySqlConfigFromEnvironment();
    require(static_cast<bool>(config), config ? "" : config.error().message);
    return config.value();
}

void cleanConcurrencyFixtures(const MySqlConfig& config) {
    MySqlConnection connection(config);
    auto result = connection.transaction([](MySqlConnection& database) {
        for (const char* enrollmentId : {"ENR-IT-1", "ENR-IT-2", "ENR-IT-3"}) {
            auto removed = database.execute(
                string("DELETE FROM enrollments WHERE enrollment_id = '") +
                enrollmentId + "'");
            if (!removed) {
                return removed;
            }
        }
        auto student = database.execute("DELETE FROM students WHERE student_id = 'STU-IT'");
        if (!student) {
            return student;
        }
        return database.execute("DELETE FROM users WHERE user_id = 'U-IT'");
    });
    require(static_cast<bool>(result), result ? "" : result.error().message);
}

void cleanWaitlistConcurrencyFixtures(const MySqlConfig& config) {
    MySqlConnection connection(config);
    auto result = connection.execute(
        "DELETE FROM waitlist_entries WHERE waitlist_entry_id IN ('WAIT-IT-1', 'WAIT-IT-2')");
    require(static_cast<bool>(result), result ? "" : result.error().message);
}

void cleanFacultyFixtures(const MySqlConfig& config) {
    MySqlConnection connection(config);
    auto result = connection.transaction([](MySqlConnection& database) {
        auto grades = database.execute(
            "DELETE g FROM grade_records g JOIN enrollments e ON e.enrollment_id = g.enrollment_id "
            "WHERE e.enrollment_id IN ('ENR-FAC-IT-1', 'ENR-FAC-IT-2')");
        if (!grades) return grades;
        auto enrollments = database.execute(
            "DELETE FROM enrollments WHERE enrollment_id IN ('ENR-FAC-IT-1', 'ENR-FAC-IT-2')");
        if (!enrollments) return enrollments;
        return database.execute(
            "DELETE FROM course_change_requests WHERE change_request_id = 'CHANGE-FAC-IT'");
    });
    require(static_cast<bool>(result), result ? "" : result.error().message);
}

class FailingEnrollmentStore final : public IEnrollmentStore {
public:
    FailingEnrollmentStore(IEnrollmentStore& store, size_t failureNumber)
        : store_(store), failureNumber_(failureNumber) {}

    Result<optional<Enrollment>> findEnrollment(EnrollmentId id) const override {
        return store_.findEnrollment(move(id));
    }
    Result<optional<Enrollment>> findStudentEnrollment(
        StudentId studentId, OfferingId offeringId) const override {
        return store_.findStudentEnrollment(move(studentId), move(offeringId));
    }
    Result<vector<Enrollment>> enrollments() const override {
        return store_.enrollments();
    }
    Result<vector<Enrollment>> activeEnrollmentsForStudent(
        StudentId studentId) const override {
        return store_.activeEnrollmentsForStudent(move(studentId));
    }
    Result<vector<Enrollment>> scheduleEnrollmentsForStudent(
        StudentId studentId, const string& semester) const override {
        return store_.scheduleEnrollmentsForStudent(move(studentId), semester);
    }
    Result<vector<FacultyRosterEntry>> activeRosterForOffering(
        OfferingId offeringId) const override {
        return store_.activeRosterForOffering(move(offeringId));
    }
    Result<void> saveEnrollment(Enrollment enrollment) override {
        ++saveCount_;
        if (saveCount_ == failureNumber_) {
            return Result<void>::failure(
                "TEST_ENROLLMENT_WRITE_FAILED", "Force native finalisation rollback.");
        }
        return store_.saveEnrollment(move(enrollment));
    }
    Result<void> removeEnrollment(EnrollmentId id) override {
        return store_.removeEnrollment(move(id));
    }

private:
    IEnrollmentStore& store_;
    size_t failureNumber_;
    size_t saveCount_{0};
};

void testSeededContracts() {
    MySqlDataContext context(testConfig(), 1);
    require(static_cast<bool>(context.verifyConnections()),
            "Every pooled connection should connect");

    IUserStore& users = context;
    ICourseStore& courses = context;
    IEnrollmentStore& enrollments = context;
    IProgramStore& programs = context;
    IGradeStore& grades = context;
    IChangeRequestStore& changes = context;
    IWaitlistStore& waitlist = context;

    const auto allUsers = users.users();
    const auto allStudents = users.students();
    const auto allFaculty = users.facultyMembers();
    const auto allCourses = courses.courses();
    const auto allOfferings = courses.offerings();
    const auto allEnrollments = enrollments.enrollments();
    const auto allPrograms = programs.programs();
    const auto allGrades = grades.gradeRecords();
    const auto allChanges = changes.changeRequests();
    const auto allWaitlist = waitlist.waitlistEntries();

    require(allUsers && allUsers.value().size() == 8, "Expected eight seeded users");
    require(allStudents && allStudents.value().size() == 3, "Expected three seeded students");
    require(allFaculty && allFaculty.value().size() == 3,
            "Expected three seeded faculty profiles");
    const auto administratorCount = count_if(
        allUsers.value().begin(), allUsers.value().end(),
        [](const User& user) { return user.role == UserRole::Administrator; });
    require(administratorCount == 2, "Expected two seeded administrator users");
    require(allCourses && allCourses.value().size() == 5, "Expected five seeded courses");
    require(allOfferings && allOfferings.value().size() == 6,
            "Expected six seeded offerings");
    require(allEnrollments && allEnrollments.value().size() == 5,
            "Expected five seeded enrolments");
    require(allPrograms && allPrograms.value().size() == 2,
            "Expected two seeded programmes");
    require(allGrades && allGrades.value().size() == 1, "Expected one seeded grade");
    require(allChanges && allChanges.value().size() == 1,
            "Expected one seeded change request");
    require(allChanges.value().front().requestedValue == "60",
            "Expected the seeded capacity-change value");
    require(allWaitlist && allWaitlist.value().size() == 1,
            "Expected one seeded waitlist entry");

    const auto alice = users.findUser(UserId{"U-STU-001"});
    require(alice && alice.value() && alice.value()->status == UserStatus::Active,
            "Expected the active student fixture");
    const auto aliceProfile = users.findStudentByUserId(UserId{"U-STU-001"});
    require(aliceProfile && aliceProfile.value() &&
                aliceProfile.value()->id == StudentId{"STU-001"},
            "Expected the Student profile lookup by User ID");
    const auto mayaProfile = users.findFacultyByUserId(UserId{"U-FAC-001"});
    require(mayaProfile && mayaProfile.value() &&
                mayaProfile.value()->id == FacultyId{"FAC-001"},
            "Expected the Faculty profile lookup by User ID");
    const auto administratorStudentProfile =
        users.findStudentByUserId(UserId{"U-ADM-001"});
    const auto administratorFacultyProfile =
        users.findFacultyByUserId(UserId{"U-ADM-001"});
    require(administratorStudentProfile && !administratorStudentProfile.value() &&
                administratorFacultyProfile && !administratorFacultyProfile.value(),
            "An Administrator User should not have a duplicate role profile");
    const auto cs201 = courses.findCourse(CourseId{"COURSE-CS201"});
    require(cs201 && cs201.value() && cs201.value()->prerequisiteCourseIds.size() == 1,
            "Expected the connected prerequisite fixture");
    const auto business = courses.findOffering(OfferingId{"OFFER-BUS101-2026S1"});
    require(business && business.value() && business.value()->capacity == 2 &&
                business.value()->enrolledCount == 2,
            "Expected the fully represented Business capacity fixture");
}

void testStableReadsAndReferentialIntegrity() {
    MySqlDataContext context(testConfig(), 2);
    IUserStore& users = context;
    IEnrollmentStore& enrollments = context;

    auto copies = users.users();
    require(copies && !copies.value().empty(), "Expected user copies");
    copies.value().front().name = "Changed local copy";
    auto stored = users.findUser(copies.value().front().id);
    require(stored && stored.value() && stored.value()->name != "Changed local copy",
            "Read results must not expose mutable database state");

    const auto invalid = enrollments.saveEnrollment(
        {EnrollmentId{"ENR-INVALID"}, StudentId{"STU-404"},
         OfferingId{"OFFER-CS201-2026S1"}, EnrollmentStatus::Active});
    require(!invalid, "A missing foreign-key reference should fail");
    const auto absent = enrollments.findEnrollment(EnrollmentId{"ENR-INVALID"});
    require(absent && !absent.value(), "A failed write must not create an enrolment");

    const auto enrollmentCollision = enrollments.saveEnrollment(
        {EnrollmentId{"ENR-001"}, StudentId{"STU-002"},
         OfferingId{"OFFER-BUS301-2026S1"}, EnrollmentStatus::Active});
    const auto originalEnrollment = enrollments.findEnrollment(EnrollmentId{"ENR-001"});
    require(!enrollmentCollision &&
                enrollmentCollision.error().code == "PERSISTENCE_ID_CONFLICT" &&
                originalEnrollment && originalEnrollment.value() &&
                originalEnrollment.value()->studentId == StudentId{"STU-001"} &&
                originalEnrollment.value()->offeringId == OfferingId{"OFFER-CS201-2026S1"},
            "An enrolment ID collision must not overwrite another relationship");

    IWaitlistStore& waitlist = context;
    const auto waitlistCollision = waitlist.saveWaitlistEntry(
        {WaitlistEntryId{"WAIT-001"}, StudentId{"STU-001"},
         OfferingId{"OFFER-BUS301-2026S1"}, 1, WaitlistStatus::Waiting});
    const auto originalWaitlist = waitlist.findWaitlistEntry(WaitlistEntryId{"WAIT-001"});
    require(!waitlistCollision &&
                waitlistCollision.error().code == "PERSISTENCE_ID_CONFLICT" &&
                originalWaitlist && originalWaitlist.value() &&
                originalWaitlist.value()->studentId == StudentId{"STU-002"} &&
                originalWaitlist.value()->offeringId == OfferingId{"OFFER-CS220-2026S1"},
            "A waitlist ID collision must not overwrite another entry");
}

void testFailClosedUniqueKeyCollisions() {
    MySqlDataContext context(testConfig(), 2);
    IUserStore& users = context;
    ICourseStore& courses = context;
    IEnrollmentStore& enrollments = context;
    IProgramStore& programs = context;
    IGradeStore& grades = context;
    IChangeRequestStore& changes = context;
    IWaitlistStore& waitlist = context;
    ITransactionBoundary& transactions = context;

    const auto audited = transactions.executeTransaction([&] {
        const auto userCollision = users.saveUser(
            {UserId{"U-COLLIDE"}, "Wrong Name", "alice@nexus.edu",
             UserStatus::Inactive, UserRole::Administrator});
        const auto alice = users.findUser(UserId{"U-STU-001"});
        if (userCollision || userCollision.error().code != "PERSISTENCE_ID_CONFLICT" ||
            !alice || !alice.value() || alice.value()->name != "Alice Perera" ||
            alice.value()->role != UserRole::Student ||
            alice.value()->status != UserStatus::Active) {
            return Result<void>::failure(
                "COLLISION_REGRESSION", "A duplicate email changed an unrelated User.");
        }

        const auto studentCollision = users.saveStudent(
            {StudentId{"STU-COLLIDE"}, UserId{"U-STU-001"}, ProgramId{"PROGRAM-BBA"}});
        const auto aliceProfile = users.findStudent(StudentId{"STU-001"});
        if (studentCollision ||
            studentCollision.error().code != "PERSISTENCE_ID_CONFLICT" ||
            !aliceProfile || !aliceProfile.value() ||
            aliceProfile.value()->programId != ProgramId{"PROGRAM-CS"}) {
            return Result<void>::failure(
                "COLLISION_REGRESSION", "A duplicate profile User changed another Student.");
        }

        const auto facultyCollision = users.saveFaculty(
            {FacultyId{"FAC-COLLIDE"}, UserId{"U-FAC-001"}, "Business"});
        const auto mayaProfile = users.findFaculty(FacultyId{"FAC-001"});
        if (facultyCollision ||
            facultyCollision.error().code != "PERSISTENCE_ID_CONFLICT" ||
            !mayaProfile || !mayaProfile.value() ||
            mayaProfile.value()->department != "Computer Science") {
            return Result<void>::failure(
                "COLLISION_REGRESSION", "A duplicate profile User changed another Faculty member.");
        }

        const auto programCollision = programs.saveProgram(
            {ProgramId{"PROGRAM-COLLIDE"}, "Bachelor of Computer Science", "Business", {}, 1});
        const auto csProgram = programs.findProgram(ProgramId{"PROGRAM-CS"});
        if (programCollision ||
            programCollision.error().code != "PERSISTENCE_ID_CONFLICT" ||
            !csProgram || !csProgram.value() ||
            csProgram.value()->department != "Computer Science" ||
            csProgram.value()->requiredCredits != 120) {
            return Result<void>::failure(
                "COLLISION_REGRESSION", "A duplicate programme name changed another programme.");
        }

        const auto courseCollision = courses.saveCourse(
            {CourseId{"COURSE-COLLIDE"}, "CS201", "Computer Science", "999",
             "Wrong Course", "Wrong description", 1, {}});
        const auto cs201 = courses.findCourse(CourseId{"COURSE-CS201"});
        if (courseCollision ||
            courseCollision.error().code != "PERSISTENCE_ID_CONFLICT" ||
            !cs201 || !cs201.value() || cs201.value()->courseNumber != "201" ||
            cs201.value()->name != "Data Structures" ||
            cs201.value()->prerequisiteCourseIds.size() != 1) {
            return Result<void>::failure(
                "COLLISION_REGRESSION", "A duplicate course code changed another course.");
        }

        const auto enrollmentCollision = enrollments.saveEnrollment(
            {EnrollmentId{"ENR-COLLIDE"}, StudentId{"STU-001"},
             OfferingId{"OFFER-CS201-2026S1"}, EnrollmentStatus::Dropped});
        const auto originalEnrollment = enrollments.findEnrollment(EnrollmentId{"ENR-001"});
        if (enrollmentCollision ||
            enrollmentCollision.error().code != "PERSISTENCE_ID_CONFLICT" ||
            !originalEnrollment || !originalEnrollment.value() ||
            originalEnrollment.value()->status != EnrollmentStatus::Active) {
            return Result<void>::failure(
                "COLLISION_REGRESSION", "A duplicate Student/offering changed another enrolment.");
        }

        const auto gradeCollision = grades.saveGradeRecord(
            {GradeRecordId{"GRADE-COLLIDE"}, StudentId{"STU-001"},
             OfferingId{"OFFER-CS101-2025S2"}, CourseId{"COURSE-CS101"},
             "F", GradeLifecycle::Pending});
        const auto originalGrade = grades.findGradeRecord(GradeRecordId{"GRADE-001"});
        if (gradeCollision || gradeCollision.error().code != "PERSISTENCE_ID_CONFLICT" ||
            !originalGrade || !originalGrade.value() || originalGrade.value()->grade != "A" ||
            originalGrade.value()->lifecycle != GradeLifecycle::Submitted) {
            return Result<void>::failure(
                "COLLISION_REGRESSION", "A duplicate grade enrolment changed another grade.");
        }

        const auto waitlistCollision = waitlist.saveWaitlistEntry(
            {WaitlistEntryId{"WAIT-COLLIDE"}, StudentId{"STU-002"},
             OfferingId{"OFFER-CS220-2026S1"}, 2, WaitlistStatus::Offered});
        const auto originalWaitlist = waitlist.findWaitlistEntry(WaitlistEntryId{"WAIT-001"});
        if (waitlistCollision ||
            waitlistCollision.error().code != "PERSISTENCE_ID_CONFLICT" ||
            !originalWaitlist || !originalWaitlist.value() ||
            originalWaitlist.value()->position != 1 ||
            originalWaitlist.value()->status != WaitlistStatus::Waiting) {
            return Result<void>::failure(
                "COLLISION_REGRESSION", "A duplicate waitlist relationship changed another entry.");
        }

        const auto changeCollision = changes.saveChangeRequest(
            {ChangeRequestId{"CHANGE-001"}, FacultyId{"FAC-001"},
             CourseId{"COURSE-CS201"}, OfferingId{"OFFER-CS201-2026S1"},
             CourseChangeType::Description, CourseChangeStatus::Rejected, "Wrong value"});
        const auto originalChange = changes.findChangeRequest(ChangeRequestId{"CHANGE-001"});
        if (changeCollision ||
            changeCollision.error().code != "PERSISTENCE_ID_CONFLICT" ||
            !originalChange || !originalChange.value() ||
            originalChange.value()->facultyId != FacultyId{"FAC-002"} ||
            originalChange.value()->requestedValue != "60" ||
            originalChange.value()->status != CourseChangeStatus::Pending) {
            return Result<void>::failure(
                "COLLISION_REGRESSION", "A colliding request ID changed another request.");
        }

        return Result<void>::failure(
            "TEST_ROLLBACK", "Rollback the collision audit without changing seeded data.");
    });

    require(!audited && audited.error().code == "TEST_ROLLBACK",
            "Every alternate-key collision should fail closed before the audit rollback");
}

void testTargetedStudentReads() {
    MySqlDataContext context(testConfig(), 2);
    ICourseStore& courses = context;
    IEnrollmentStore& enrollments = context;
    IGradeStore& grades = context;
    IWaitlistStore& waitlist = context;

    const auto catalogue = courses.browseCatalogue(
        {"2026S1", "CS", "201", "Data", "Maya"});
    require(catalogue && catalogue.value().size() == 1 &&
                catalogue.value().front().course.id == CourseId{"COURSE-CS201"} &&
                catalogue.value().front().offering.capacity == 3 &&
                catalogue.value().front().offering.enrolledCount == 1 &&
                catalogue.value().front().instructorName == "Dr Maya Rao",
            "Targeted catalogue filters should return connected active-derived data");

    const auto current = enrollments.scheduleEnrollmentsForStudent(
        StudentId{"STU-001"}, "2026S1");
    const auto past = enrollments.scheduleEnrollmentsForStudent(
        StudentId{"STU-001"}, "2025S2");
    require(current && current.value().size() == 2 && past && past.value().size() == 1 &&
                past.value().front().status == EnrollmentStatus::Completed,
            "Targeted Student schedule reads should distinguish current and past semesters");

    const auto active = enrollments.activeEnrollmentsForStudent(StudentId{"STU-001"});
    const auto pair = enrollments.findStudentEnrollment(
        StudentId{"STU-001"}, OfferingId{"OFFER-CS201-2026S1"});
    require(active && active.value().size() == 2 && pair && pair.value() &&
                pair.value()->status == EnrollmentStatus::Active,
            "Targeted active and Student-offering enrolment reads should be deterministic");

    const auto completed = grades.submittedGradesForStudent(StudentId{"STU-001"});
    require(completed && completed.value().size() == 1 &&
                completed.value().front().courseId == CourseId{"COURSE-CS101"} &&
                completed.value().front().grade == "A",
            "Submitted completed grades should be targeted to one Student");

    const auto studentWaitlist = waitlist.waitlistEntriesForStudent(StudentId{"STU-002"});
    const auto offeringWaitlist = waitlist.waitingEntriesForOffering(
        OfferingId{"OFFER-CS220-2026S1"});
    const auto waitlistPair = waitlist.findStudentWaitlistEntry(
        StudentId{"STU-002"}, OfferingId{"OFFER-CS220-2026S1"});
    const auto nextPosition = waitlist.nextWaitlistPosition(
        OfferingId{"OFFER-CS220-2026S1"});
    require(studentWaitlist && studentWaitlist.value().size() == 1 &&
                offeringWaitlist && offeringWaitlist.value().size() == 1 &&
                waitlistPair && waitlistPair.value() && nextPosition && nextPosition.value() == 2,
            "Targeted waitlist reads should preserve stable order and next position");
}

void testTargetedFacultyReads() {
    MySqlDataContext context(testConfig(), 2);
    ICourseStore& courses = context;
    IEnrollmentStore& enrollments = context;
    IGradeStore& grades = context;
    IChangeRequestStore& changes = context;

    const auto offerings = courses.assignedOfferings(FacultyId{"FAC-001"});
    require(offerings && offerings.value().size() == 3 &&
                offerings.value().front().course.id == CourseId{"COURSE-CS101"} &&
                offerings.value().back().offering.id == OfferingId{"OFFER-CS201-2026S1"} &&
                offerings.value().back().offering.enrolledCount == 1,
            "Assigned-offering reads should be connected, stable, and occupancy-derived");
    const auto teaches = courses.facultyTeachesCourse(
        FacultyId{"FAC-001"}, CourseId{"COURSE-CS201"});
    const auto doesNotTeach = courses.facultyTeachesCourse(
        FacultyId{"FAC-002"}, CourseId{"COURSE-CS201"});
    require(teaches && teaches.value() && doesNotTeach && !doesNotTeach.value(),
            "Faculty-course ownership reads should be targeted");

    const auto roster = enrollments.activeRosterForOffering(
        OfferingId{"OFFER-CS201-2026S1"});
    const auto emptyRoster = enrollments.activeRosterForOffering(
        OfferingId{"OFFER-BUS301-2026S1"});
    require(roster && roster.value().size() == 1 &&
                roster.value().front().enrollment.studentId == StudentId{"STU-001"} &&
                roster.value().front().studentName == "Alice Perera" &&
                roster.value().front().studentEmail == "alice@nexus.edu" &&
                emptyRoster && emptyRoster.value().empty(),
            "Targeted roster reads should preserve active contact data and empty success");

    const auto ungraded = grades.gradeStateForOffering(
        OfferingId{"OFFER-CS201-2026S1"});
    const auto submitted = grades.gradeStateForOffering(
        OfferingId{"OFFER-CS101-2025S2"});
    require(ungraded && ungraded.value().size() == 1 && !ungraded.value().front().grade &&
                submitted && submitted.value().size() == 1 &&
                submitted.value().front().grade &&
                submitted.value().front().grade->lifecycle == GradeLifecycle::Submitted,
            "Grade-state reads should distinguish ungraded and persisted Submitted grades");

    const auto requests = changes.changeRequestsForFaculty(FacultyId{"FAC-002"});
    const auto otherRequests = changes.changeRequestsForFaculty(FacultyId{"FAC-001"});
    require(requests && requests.value().size() == 1 &&
                requests.value().front().id == ChangeRequestId{"CHANGE-001"} &&
                otherRequests && otherRequests.value().empty(),
            "Faculty change-request reads should be submitter-specific and stable");
}

void testFacultyWritesAndAtomicFinalization() {
    const MySqlConfig config = testConfig();
    cleanFacultyFixtures(config);
    try {
        MySqlDataContext context(config, 2);
        IEnrollmentStore& enrollments = context;
        IGradeStore& grades = context;
        IChangeRequestStore& changes = context;

        const auto firstEnrollmentSaved = enrollments.saveEnrollment(
            {EnrollmentId{"ENR-FAC-IT-1"}, StudentId{"STU-001"},
             OfferingId{"OFFER-BUS301-2026S1"}, EnrollmentStatus::Active});
        require(static_cast<bool>(firstEnrollmentSaved),
                "The first Faculty integration enrolment should be created");
        const auto secondEnrollmentSaved = enrollments.saveEnrollment(
            {EnrollmentId{"ENR-FAC-IT-2"}, StudentId{"STU-002"},
             OfferingId{"OFFER-BUS301-2026S1"}, EnrollmentStatus::Active});
        require(static_cast<bool>(secondEnrollmentSaved),
                "The second Faculty integration enrolment should be created");

        SubmitGradesCommand submit(
            FacultyId{"FAC-002"}, OfferingId{"OFFER-BUS301-2026S1"},
            {{StudentId{"STU-001"}, "A"}, {StudentId{"STU-002"}, "B"}},
            context, context, context, context, context);
        require(submit.execute() && submit.outcome().accepted.size() == 2,
                "The real MySQL grade batch should store both Pending grades");
        const auto pending = grades.pendingGradesForOffering(
            OfferingId{"OFFER-BUS301-2026S1"});
        require(pending && pending.value().size() == 2,
                "The targeted Pending-grade read should return the complete batch");

        FailingEnrollmentStore failingEnrollments(enrollments, 2);
        FinalizeGradesCommand failedFinalize(
            FacultyId{"FAC-002"}, OfferingId{"OFFER-BUS301-2026S1"},
            context, context, failingEnrollments, context, context);
        const auto failedFinalizeResult = failedFinalize.execute();
        const auto pendingAfterRollback = grades.pendingGradesForOffering(
            OfferingId{"OFFER-BUS301-2026S1"});
        const auto firstAfterRollback = enrollments.findEnrollment(
            EnrollmentId{"ENR-FAC-IT-1"});
        const auto secondAfterRollback = enrollments.findEnrollment(
            EnrollmentId{"ENR-FAC-IT-2"});
        require(!failedFinalizeResult &&
                    failedFinalizeResult.error().code == "TEST_ENROLLMENT_WRITE_FAILED" &&
                    failedFinalize.finalizedCount() == 0 &&
                    pendingAfterRollback && pendingAfterRollback.value().size() == 2 &&
                    firstAfterRollback && firstAfterRollback.value() &&
                    firstAfterRollback.value()->status == EnrollmentStatus::Active &&
                    secondAfterRollback && secondAfterRollback.value() &&
                    secondAfterRollback.value()->status == EnrollmentStatus::Active,
                "A real MySQL finalisation failure should roll back every grade and enrolment");

        FinalizeGradesCommand finalize(
            FacultyId{"FAC-002"}, OfferingId{"OFFER-BUS301-2026S1"},
            context, context, context, context, context);
        require(finalize.execute() && finalize.finalizedCount() == 2,
                "The real MySQL finalisation should commit the complete batch");
        const auto first = enrollments.findEnrollment(EnrollmentId{"ENR-FAC-IT-1"});
        const auto second = enrollments.findEnrollment(EnrollmentId{"ENR-FAC-IT-2"});
        require(first && first.value() && second && second.value() &&
                    first.value()->status == EnrollmentStatus::Completed &&
                    second.value()->status == EnrollmentStatus::Completed &&
                    grades.pendingGradesForOffering(OfferingId{"OFFER-BUS301-2026S1"}).value().empty(),
                "Finalisation should atomically complete every connected enrolment");

        const auto gradeCollision = grades.createGradeRecord(
            {GradeRecordId{"GRADE-001"}, StudentId{"STU-001"},
             OfferingId{"OFFER-CS201-2026S1"}, CourseId{"COURSE-CS201"},
             "F", GradeLifecycle::Pending});
        const auto originalGrade = grades.findGradeRecord(GradeRecordId{"GRADE-001"});
        require(!gradeCollision && gradeCollision.error().code == "PERSISTENCE_ID_CONFLICT" &&
                    originalGrade && originalGrade.value() && originalGrade.value()->grade == "A",
                "A create-only GradeRecord collision must not overwrite the seeded grade");

        const auto changeCreated = changes.createChangeRequest(
            {ChangeRequestId{"CHANGE-FAC-IT"}, FacultyId{"FAC-001"},
             CourseId{"COURSE-CS201"}, nullopt, CourseChangeType::Description,
             CourseChangeStatus::Pending, "Integration description"});
        require(static_cast<bool>(changeCreated),
                "A targeted Pending course-change request should be created");
        const auto changeCollision = changes.createChangeRequest(
            {ChangeRequestId{"CHANGE-001"}, FacultyId{"FAC-001"},
             CourseId{"COURSE-CS201"}, nullopt, CourseChangeType::Description,
             CourseChangeStatus::Pending, "Must not overwrite"});
        const auto originalChange = changes.findChangeRequest(ChangeRequestId{"CHANGE-001"});
        require(!changeCollision &&
                    changeCollision.error().code == "PERSISTENCE_ID_CONFLICT" &&
                    originalChange && originalChange.value() &&
                    originalChange.value()->requestedValue == "60",
                "A create-only ChangeRequest collision must not overwrite a persisted request");
    } catch (...) {
        cleanFacultyFixtures(config);
        throw;
    }
    cleanFacultyFixtures(config);
}

void testTransactionCommitAndRollback() {
    MySqlDataContext context(testConfig(), 2);
    IUserStore& users = context;
    ITransactionBoundary& transactions = context;

    const auto original = users.findUser(UserId{"U-STU-001"});
    require(original && original.value(), "Expected transaction fixture user");

    auto committed = transactions.executeTransaction([&users, original] {
        User updated = *original.value();
        updated.name = "Committed Name";
        return users.saveUser(move(updated));
    });
    require(static_cast<bool>(committed), "A successful transaction should commit");
    auto afterCommit = users.findUser(UserId{"U-STU-001"});
    require(afterCommit && afterCommit.value() && afterCommit.value()->name == "Committed Name",
            "Committed data should be visible");

    auto restored = users.saveUser(*original.value());
    require(static_cast<bool>(restored), "The committed fixture should be restored");

    auto rolledBack = transactions.executeTransaction([&users, original] {
        User changed = *original.value();
        changed.name = "Rolled Back Name";
        auto first = users.saveUser(move(changed));
        if (!first) {
            return first;
        }
        auto second = users.saveUser(
            {UserId{"U-ROLLBACK"}, "Rollback User", "rollback@nexus.edu",
             UserStatus::Active, UserRole::Student});
        if (!second) {
            return second;
        }
        return Result<void>::failure("DEMONSTRATED_FAILURE", "Force transaction rollback");
    });
    require(!rolledBack && rolledBack.error().code == "DEMONSTRATED_FAILURE",
            "The operation failure should be returned");
    auto afterRollback = users.findUser(UserId{"U-STU-001"});
    auto inserted = users.findUser(UserId{"U-ROLLBACK"});
    require(afterRollback && afterRollback.value() &&
                afterRollback.value()->name == original.value()->name,
            "Rollback should restore an updated record");
    require(inserted && !inserted.value(), "Rollback should remove an inserted record");
}

void testCrossContextLeaseRestoration() {
    const MySqlConfig config = testConfig();
    MySqlDataContext outerContext(config, 1);
    MySqlDataContext secondaryContext(config, 1);
    IUserStore& outerUsers = outerContext;
    const IUserStore& secondaryUsers = secondaryContext;
    ITransactionBoundary& transactions = outerContext;

    const auto original = outerUsers.findUser(UserId{"U-STU-001"});
    require(original && original.value(), "Expected the cross-context transaction fixture");

    const auto rolledBack = transactions.executeTransaction([&] {
        const auto secondaryRead = secondaryUsers.findUser(UserId{"U-STU-002"});
        if (!secondaryRead || !secondaryRead.value()) {
            return Result<void>::failure(
                "SECONDARY_READ_FAILED", "The secondary context read should succeed.");
        }
        User updated = *original.value();
        updated.name = "Must Roll Back";
        auto saved = outerUsers.saveUser(move(updated));
        if (!saved) {
            return saved;
        }
        return Result<void>::failure(
            "TEST_ROLLBACK", "Force rollback after the secondary context read.");
    });

    const auto afterRollback = outerUsers.findUser(UserId{"U-STU-001"});
    const bool restored = afterRollback && afterRollback.value() &&
                          afterRollback.value()->name == original.value()->name;
    if (!restored) {
        auto cleanup = outerUsers.saveUser(*original.value());
        require(static_cast<bool>(cleanup), "The cross-context fixture cleanup should succeed");
    }
    require(!rolledBack && rolledBack.error().code == "TEST_ROLLBACK" && restored,
            "A nested read through another context must not detach the outer transaction lease");
}

void testConcurrentCapacityLocking() {
    const MySqlConfig config = testConfig();
    cleanConcurrencyFixtures(config);
    MySqlDataContext context(config, 3);
    IUserStore& users = context;
    IEnrollmentStore& enrollments = context;
    ICourseStore& courses = context;
    ITransactionBoundary& transactions = context;

    auto userSaved = users.saveUser(
        {UserId{"U-IT"}, "Integration Student", "integration@nexus.edu",
         UserStatus::Active, UserRole::Student});
    require(static_cast<bool>(userSaved), "The concurrency user should be stored");
    auto studentSaved = users.saveStudent(
        {StudentId{"STU-IT"}, UserId{"U-IT"}, ProgramId{"PROGRAM-CS"}});
    require(static_cast<bool>(studentSaved), "The concurrency student should be stored");

    struct Attempt {
        EnrollmentId enrollmentId;
        StudentId studentId;
    };
    const vector<Attempt> attempts{
        {EnrollmentId{"ENR-IT-1"}, StudentId{"STU-002"}},
        {EnrollmentId{"ENR-IT-2"}, StudentId{"STU-003"}},
        {EnrollmentId{"ENR-IT-3"}, StudentId{"STU-IT"}},
    };

    atomic<bool> start{false};
    mutex resultsMutex;
    vector<Result<void>> results;
    vector<thread> workers;
    for (const auto& attempt : attempts) {
        workers.emplace_back([&, attempt] {
            while (!start.load()) {
                this_thread::yield();
            }
            auto result = transactions.executeTransaction([&, attempt] {
                auto offering = courses.findOffering(OfferingId{"OFFER-CS201-2026S1"});
                if (!offering) {
                    return Result<void>::failure(
                        offering.error().code, offering.error().message);
                }
                if (!offering.value() ||
                    offering.value()->enrolledCount >= offering.value()->capacity) {
                    return Result<void>::failure("CAPACITY_FULL", "The offering is full.");
                }
                this_thread::sleep_for(chrono::milliseconds(30));
                return enrollments.saveEnrollment(
                    {attempt.enrollmentId, attempt.studentId,
                     OfferingId{"OFFER-CS201-2026S1"}, EnrollmentStatus::Active});
            });
            lock_guard<mutex> lock(resultsMutex);
            results.push_back(move(result));
        });
    }
    start = true;
    for (auto& worker : workers) {
        worker.join();
    }

    size_t successes = 0;
    size_t capacityFailures = 0;
    for (const auto& result : results) {
        if (result) {
            ++successes;
        } else if (result.error().code == "CAPACITY_FULL") {
            ++capacityFailures;
        }
    }
    auto finalOffering = courses.findOffering(OfferingId{"OFFER-CS201-2026S1"});
    require(successes == 2 && capacityFailures == 1,
            "Row locking should admit two requests and reject the third");
    require(finalOffering && finalOffering.value() &&
                finalOffering.value()->enrolledCount == finalOffering.value()->capacity,
            "Concurrent requests must not overbook the offering");

    cleanConcurrencyFixtures(config);
}

void testConcurrentWaitlistPositionLocking() {
    const MySqlConfig config = testConfig();
    cleanWaitlistConcurrencyFixtures(config);
    MySqlDataContext context(config, 2);
    ICourseStore& courses = context;
    IWaitlistStore& waitlist = context;
    ITransactionBoundary& transactions = context;

    struct Attempt {
        WaitlistEntryId entryId;
        StudentId studentId;
    };
    const vector<Attempt> attempts{
        {WaitlistEntryId{"WAIT-IT-1"}, StudentId{"STU-001"}},
        {WaitlistEntryId{"WAIT-IT-2"}, StudentId{"STU-003"}},
    };
    atomic<bool> start{false};
    mutex resultsMutex;
    vector<Result<void>> results;
    vector<thread> workers;
    for (const auto& attempt : attempts) {
        workers.emplace_back([&, attempt] {
            while (!start.load()) {
                this_thread::yield();
            }
            auto result = transactions.executeTransaction([&, attempt] {
                auto offering = courses.findOffering(OfferingId{"OFFER-CS220-2026S1"});
                if (!offering) {
                    return Result<void>::failure(
                        offering.error().code, offering.error().message);
                }
                auto position = waitlist.nextWaitlistPosition(
                    OfferingId{"OFFER-CS220-2026S1"});
                if (!position) {
                    return Result<void>::failure(
                        position.error().code, position.error().message);
                }
                this_thread::sleep_for(chrono::milliseconds(30));
                return waitlist.saveWaitlistEntry(
                    {attempt.entryId, attempt.studentId,
                     OfferingId{"OFFER-CS220-2026S1"}, position.value(),
                     WaitlistStatus::Waiting});
            });
            lock_guard<mutex> lock(resultsMutex);
            results.push_back(move(result));
        });
    }
    start = true;
    for (auto& worker : workers) {
        worker.join();
    }

    const auto entries = waitlist.waitingEntriesForOffering(
        OfferingId{"OFFER-CS220-2026S1"});
    require(results.size() == 2 && results[0] && results[1] && entries &&
                entries.value().size() == 3 &&
                entries.value()[0].position == 1 &&
                entries.value()[1].position == 2 &&
                entries.value()[2].position == 3,
            "Offering locking should serialize unique waitlist positions");
    cleanWaitlistConcurrencyFixtures(config);
}

}

int main() {
    const vector<pair<string, function<void()>>> tests{
        {"seeded contracts", testSeededContracts},
        {"stable reads and referential integrity", testStableReadsAndReferentialIntegrity},
        {"fail-closed unique-key collisions", testFailClosedUniqueKeyCollisions},
        {"targeted Student reads", testTargetedStudentReads},
        {"targeted Faculty reads", testTargetedFacultyReads},
        {"Faculty writes and atomic finalisation", testFacultyWritesAndAtomicFinalization},
        {"transaction commit and rollback", testTransactionCommitAndRollback},
        {"cross-context lease restoration", testCrossContextLeaseRestoration},
        {"concurrent capacity locking", testConcurrentCapacityLocking},
        {"concurrent waitlist position locking", testConcurrentWaitlistPositionLocking},
    };

    size_t passed = 0;
    for (const auto& test : tests) {
        try {
            test.second();
            ++passed;
            cout << "PASS: " << test.first << '\n';
        } catch (const exception& exception) {
            cerr << "FAIL: " << test.first << ": " << exception.what() << '\n';
        }
    }

    cout << passed << '/' << tests.size() << " MySQL integration tests passed\n";
    return passed == tests.size() ? 0 : 1;
}
