#include "nexusenroll/business/cqrs/commands/finalize_grades_command.hpp"
#include "nexusenroll/business/cqrs/commands/submit_course_change_request_command.hpp"
#include "nexusenroll/business/cqrs/commands/submit_grades_command.hpp"
#include "nexusenroll/business/cqrs/queries/faculty_queries.hpp"
#include "nexusenroll/business/cqrs/queries/student_queries.hpp"

#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace nexusenroll;
using namespace business::cqrs::commands;
using namespace business::cqrs::queries;
using namespace business::domain;
using namespace common;
using namespace data::contracts;
using namespace std;

void require(bool condition, const string& message) {
    if (!condition) throw runtime_error(message);
}

template <typename T, typename Id>
optional<T> byId(const vector<T>& values, const Id& id) {
    const auto found = find_if(values.begin(), values.end(), [&](const T& value) {
        return value.id == id;
    });
    return found == values.end() ? optional<T>{} : optional<T>{*found};
}

class FakeFacultyContext final : public IUserStore,
                                 public ICourseStore,
                                 public IEnrollmentStore,
                                 public IProgramStore,
                                 public IGradeStore,
                                 public IChangeRequestStore,
                                 public ITransactionBoundary {
public:
    Result<optional<User>> findUser(UserId id) const override {
        if (readError) return failed<optional<User>>();
        return Result<optional<User>>::success(byId(users_, id));
    }
    Result<optional<Student>> findStudent(StudentId id) const override {
        if (readError) return failed<optional<Student>>();
        return Result<optional<Student>>::success(byId(students_, id));
    }
    Result<optional<Faculty>> findFaculty(FacultyId id) const override {
        if (readError) return failed<optional<Faculty>>();
        return Result<optional<Faculty>>::success(byId(faculty_, id));
    }
    Result<optional<Student>> findStudentByUserId(UserId id) const override {
        const auto found = find_if(students_.begin(), students_.end(), [&](const Student& value) {
            return value.userId == id;
        });
        return Result<optional<Student>>::success(
            found == students_.end() ? optional<Student>{} : optional<Student>{*found});
    }
    Result<optional<Faculty>> findFacultyByUserId(UserId id) const override {
        const auto found = find_if(faculty_.begin(), faculty_.end(), [&](const Faculty& value) {
            return value.userId == id;
        });
        return Result<optional<Faculty>>::success(
            found == faculty_.end() ? optional<Faculty>{} : optional<Faculty>{*found});
    }
    Result<vector<User>> users() const override { return Result<vector<User>>::success(users_); }
    Result<vector<Student>> students() const override {
        return Result<vector<Student>>::success(students_);
    }
    Result<vector<Faculty>> facultyMembers() const override {
        return Result<vector<Faculty>>::success(faculty_);
    }
    Result<void> saveUser(User value) override { return save(users_, move(value)); }
    Result<void> saveStudent(Student value) override { return save(students_, move(value)); }
    Result<void> saveFaculty(Faculty value) override { return save(faculty_, move(value)); }

    Result<optional<Course>> findCourse(CourseId id) const override {
        if (readError) return failed<optional<Course>>();
        return Result<optional<Course>>::success(byId(courses_, id));
    }
    Result<optional<CourseOffering>> findOffering(OfferingId id) const override {
        if (readError) return failed<optional<CourseOffering>>();
        auto offering = byId(offerings_, id);
        if (offering) {
            offering->enrolledCount = static_cast<size_t>(count_if(
                enrollments_.begin(), enrollments_.end(), [&](const Enrollment& enrollment) {
                    return enrollment.offeringId == id &&
                           enrollment.status == EnrollmentStatus::Active;
                }));
        }
        return Result<optional<CourseOffering>>::success(move(offering));
    }
    Result<vector<Course>> courses() const override {
        return Result<vector<Course>>::success(courses_);
    }
    Result<vector<CourseOffering>> offerings() const override {
        return Result<vector<CourseOffering>>::success(offerings_);
    }
    Result<vector<CatalogueItem>> browseCatalogue(const CatalogueFilter&) const override {
        return Result<vector<CatalogueItem>>::success({});
    }
    Result<vector<FacultyOfferingItem>> assignedOfferings(FacultyId facultyId) const override {
        if (readError) return failed<vector<FacultyOfferingItem>>();
        vector<FacultyOfferingItem> values;
        for (const auto& offering : offerings_) {
            if (offering.instructorId != facultyId) continue;
            auto connected = findOffering(offering.id);
            auto course = byId(courses_, offering.courseId);
            if (connected && connected.value() && course) {
                values.push_back({*course, *connected.value()});
            }
        }
        sort(values.begin(), values.end(), [](const auto& left, const auto& right) {
            return pair{left.offering.semester, left.course.code} <
                   pair{right.offering.semester, right.course.code};
        });
        return Result<vector<FacultyOfferingItem>>::success(move(values));
    }
    Result<bool> facultyTeachesCourse(FacultyId facultyId, CourseId courseId) const override {
        if (readError) return failed<bool>();
        return Result<bool>::success(any_of(
            offerings_.begin(), offerings_.end(), [&](const CourseOffering& offering) {
                return offering.instructorId == facultyId && offering.courseId == courseId;
            }));
    }
    Result<void> saveCourse(Course value) override { return save(courses_, move(value)); }
    Result<void> saveOffering(CourseOffering value) override {
        return save(offerings_, move(value));
    }

    Result<optional<Enrollment>> findEnrollment(EnrollmentId id) const override {
        return Result<optional<Enrollment>>::success(byId(enrollments_, id));
    }
    Result<optional<Enrollment>> findStudentEnrollment(
        StudentId studentId, OfferingId offeringId) const override {
        if (readError) return failed<optional<Enrollment>>();
        const auto found = find_if(enrollments_.begin(), enrollments_.end(), [&](const auto& value) {
            return value.studentId == studentId && value.offeringId == offeringId;
        });
        return Result<optional<Enrollment>>::success(
            found == enrollments_.end() ? optional<Enrollment>{} : optional<Enrollment>{*found});
    }
    Result<vector<Enrollment>> enrollments() const override {
        return Result<vector<Enrollment>>::success(enrollments_);
    }
    Result<vector<Enrollment>> activeEnrollmentsForStudent(StudentId studentId) const override {
        vector<Enrollment> values;
        copy_if(enrollments_.begin(), enrollments_.end(), back_inserter(values), [&](const auto& value) {
            return value.studentId == studentId && value.status == EnrollmentStatus::Active;
        });
        return Result<vector<Enrollment>>::success(move(values));
    }
    Result<vector<Enrollment>> scheduleEnrollmentsForStudent(
        StudentId studentId, const string&) const override {
        return activeEnrollmentsForStudent(studentId);
    }
    Result<vector<FacultyRosterEntry>> activeRosterForOffering(
        OfferingId offeringId) const override {
        if (readError) return failed<vector<FacultyRosterEntry>>();
        vector<FacultyRosterEntry> values;
        for (const auto& enrollment : enrollments_) {
            if (enrollment.offeringId != offeringId ||
                enrollment.status != EnrollmentStatus::Active) continue;
            const auto student = byId(students_, enrollment.studentId);
            const auto user = student ? byId(users_, student->userId) : optional<User>{};
            if (user) values.push_back({enrollment, user->name, user->email});
        }
        sort(values.begin(), values.end(), [](const auto& left, const auto& right) {
            return left.enrollment.studentId < right.enrollment.studentId;
        });
        return Result<vector<FacultyRosterEntry>>::success(move(values));
    }
    Result<void> saveEnrollment(Enrollment value) override {
        return save(enrollments_, move(value));
    }
    Result<void> removeEnrollment(EnrollmentId) override {
        return Result<void>::failure("UNUSED", "Unused in Faculty tests.");
    }

    Result<optional<DegreeProgram>> findProgram(ProgramId id) const override {
        return Result<optional<DegreeProgram>>::success(byId(programs_, id));
    }
    Result<vector<DegreeProgram>> programs() const override {
        return Result<vector<DegreeProgram>>::success(programs_);
    }
    Result<void> saveProgram(DegreeProgram value) override { return save(programs_, move(value)); }

    Result<optional<GradeRecord>> findGradeRecord(GradeRecordId id) const override {
        return Result<optional<GradeRecord>>::success(byId(grades_, id));
    }
    Result<vector<GradeRecord>> gradeRecords() const override {
        return Result<vector<GradeRecord>>::success(grades_);
    }
    Result<vector<GradeRecord>> submittedGradesForStudent(StudentId studentId) const override {
        vector<GradeRecord> values;
        copy_if(grades_.begin(), grades_.end(), back_inserter(values), [&](const auto& value) {
            auto enrollment = findStudentEnrollment(value.studentId, value.offeringId);
            return value.studentId == studentId && value.lifecycle == GradeLifecycle::Submitted &&
                   enrollment && enrollment.value() &&
                   enrollment.value()->status == EnrollmentStatus::Completed;
        });
        return Result<vector<GradeRecord>>::success(move(values));
    }
    Result<optional<GradeRecord>> findStudentGradeRecord(
        StudentId studentId, OfferingId offeringId) const override {
        if (readError) return failed<optional<GradeRecord>>();
        const auto found = find_if(grades_.begin(), grades_.end(), [&](const auto& value) {
            return value.studentId == studentId && value.offeringId == offeringId;
        });
        return Result<optional<GradeRecord>>::success(
            found == grades_.end() ? optional<GradeRecord>{} : optional<GradeRecord>{*found});
    }
    Result<vector<FacultyGradeStateEntry>> gradeStateForOffering(
        OfferingId offeringId) const override {
        if (readError) return failed<vector<FacultyGradeStateEntry>>();
        vector<FacultyGradeStateEntry> values;
        for (const auto& enrollment : enrollments_) {
            if (enrollment.offeringId == offeringId &&
                enrollment.status != EnrollmentStatus::Dropped) {
                auto grade = findStudentGradeRecord(enrollment.studentId, offeringId);
                values.push_back({enrollment, grade.value()});
            }
        }
        sort(values.begin(), values.end(), [](const auto& left, const auto& right) {
            return left.enrollment.studentId < right.enrollment.studentId;
        });
        return Result<vector<FacultyGradeStateEntry>>::success(move(values));
    }
    Result<vector<GradeRecord>> pendingGradesForOffering(OfferingId offeringId) const override {
        if (readError) return failed<vector<GradeRecord>>();
        vector<GradeRecord> values;
        copy_if(grades_.begin(), grades_.end(), back_inserter(values), [&](const auto& value) {
            return value.offeringId == offeringId && value.lifecycle == GradeLifecycle::Pending;
        });
        return Result<vector<GradeRecord>>::success(move(values));
    }
    Result<void> createGradeRecord(GradeRecord value) override {
        if (byId(grades_, value.id) || findStudentGradeRecord(value.studentId, value.offeringId).value()) {
            return Result<void>::failure("PERSISTENCE_ID_CONFLICT", "Grade collision.");
        }
        return save(grades_, move(value));
    }
    Result<void> saveGradeRecord(GradeRecord value) override {
        auto existing = byId(grades_, value.id);
        if (existing && existing->lifecycle == GradeLifecycle::Submitted &&
            (existing->grade != value.grade || value.lifecycle != GradeLifecycle::Submitted)) {
            return Result<void>::failure("PERSISTENCE_ID_CONFLICT", "Submitted grade is final.");
        }
        return save(grades_, move(value));
    }

    Result<optional<CourseChangeRequest>> findChangeRequest(ChangeRequestId id) const override {
        return Result<optional<CourseChangeRequest>>::success(byId(changes_, id));
    }
    Result<vector<CourseChangeRequest>> changeRequests() const override {
        return Result<vector<CourseChangeRequest>>::success(changes_);
    }
    Result<vector<CourseChangeRequest>> changeRequestsForFaculty(FacultyId id) const override {
        if (readError) return failed<vector<CourseChangeRequest>>();
        vector<CourseChangeRequest> values;
        copy_if(changes_.begin(), changes_.end(), back_inserter(values), [&](const auto& value) {
            return value.facultyId == id;
        });
        sort(values.begin(), values.end(), [](const auto& left, const auto& right) {
            return left.id < right.id;
        });
        return Result<vector<CourseChangeRequest>>::success(move(values));
    }
    Result<void> createChangeRequest(CourseChangeRequest value) override {
        if (failChangeCreate) return Result<void>::failure("TEST_CHANGE_FAILED", "Create failed.");
        if (byId(changes_, value.id)) {
            return Result<void>::failure("PERSISTENCE_ID_CONFLICT", "Change collision.");
        }
        return save(changes_, move(value));
    }
    Result<void> saveChangeRequest(CourseChangeRequest value) override {
        return save(changes_, move(value));
    }

    Result<void> executeTransaction(const Operation& operation) override {
        ++transactionCount;
        const auto enrollmentSnapshot = enrollments_;
        const auto gradeSnapshot = grades_;
        auto result = operation();
        if (!result || failCommit) {
            enrollments_ = enrollmentSnapshot;
            grades_ = gradeSnapshot;
            if (result && failCommit)
                return Result<void>::failure("TRANSACTION_COMMIT_FAILED", "Commit failed.");
            return result;
        }
        return Result<void>::success();
    }

    vector<User> users_;
    vector<Student> students_;
    vector<Faculty> faculty_;
    vector<Course> courses_;
    vector<CourseOffering> offerings_;
    vector<Enrollment> enrollments_;
    vector<DegreeProgram> programs_;
    vector<GradeRecord> grades_;
    vector<CourseChangeRequest> changes_;
    optional<Error> readError;
    size_t writeCount{0};
    size_t transactionCount{0};
    size_t failWriteNumber{0};
    bool failCommit{false};
    bool failChangeCreate{false};

private:
    template <typename T>
    Result<T> failed() const {
        return Result<T>::failure(readError->code, readError->message);
    }

    template <typename T>
    Result<void> save(vector<T>& values, T value) {
        ++writeCount;
        if (failWriteNumber != 0 && writeCount == failWriteNumber) {
            return Result<void>::failure("TEST_WRITE_FAILED", "The configured write failed.");
        }
        auto found = find_if(values.begin(), values.end(), [&](const T& current) {
            return current.id == value.id;
        });
        if (found == values.end()) values.push_back(move(value));
        else *found = move(value);
        return Result<void>::success();
    }
};

FakeFacultyContext fixture() {
    FakeFacultyContext context;
    context.users_ = {
        {UserId{"U-F1"}, "Dr One", "one@nexus.edu", UserStatus::Active, UserRole::Faculty},
        {UserId{"U-F2"}, "Dr Inactive", "inactive@nexus.edu", UserStatus::Inactive, UserRole::Faculty},
        {UserId{"U-FX"}, "Dr Other", "other@nexus.edu", UserStatus::Active, UserRole::Faculty},
        {UserId{"U-S1"}, "Alice", "alice@nexus.edu", UserStatus::Active, UserRole::Student},
        {UserId{"U-S2"}, "Ben", "ben@nexus.edu", UserStatus::Active, UserRole::Student},
        {UserId{"U-S3"}, "Cara", "cara@nexus.edu", UserStatus::Active, UserRole::Student},
    };
    context.faculty_ = {
        {FacultyId{"FAC-1"}, UserId{"U-F1"}, "Computer Science"},
        {FacultyId{"FAC-2"}, UserId{"U-F2"}, "Computer Science"},
        {FacultyId{"FAC-X"}, UserId{"U-FX"}, "Business"},
    };
    context.students_ = {
        {StudentId{"STU-1"}, UserId{"U-S1"}, ProgramId{"P-CS"}},
        {StudentId{"STU-2"}, UserId{"U-S2"}, ProgramId{"P-CS"}},
        {StudentId{"STU-3"}, UserId{"U-S3"}, ProgramId{"P-CS"}},
    };
    context.courses_ = {
        {CourseId{"C-1"}, "CS201", "Computer Science", "201", "Structures", "Data", 4, {}},
        {CourseId{"C-2"}, "CS101", "Computer Science", "101", "Programming", "Intro", 4, {}},
    };
    context.offerings_ = {
        {OfferingId{"O-1"}, CourseId{"C-1"}, "2026S1", FacultyId{"FAC-1"}, 40, 0, {}},
        {OfferingId{"O-EMPTY"}, CourseId{"C-2"}, "2026S1", FacultyId{"FAC-1"}, 40, 0, {}},
        {OfferingId{"O-X"}, CourseId{"C-2"}, "2026S1", FacultyId{"FAC-X"}, 40, 0, {}},
    };
    context.enrollments_ = {
        {EnrollmentId{"E-1"}, StudentId{"STU-1"}, OfferingId{"O-1"}, EnrollmentStatus::Active},
        {EnrollmentId{"E-2"}, StudentId{"STU-2"}, OfferingId{"O-1"}, EnrollmentStatus::Active},
        {EnrollmentId{"E-3"}, StudentId{"STU-3"}, OfferingId{"O-1"}, EnrollmentStatus::Completed},
    };
    context.programs_ = {
        {ProgramId{"P-CS"}, "Computer Science", "Computer Science", {CourseId{"C-1"}}, 120},
    };
    context.grades_ = {
        {GradeRecordId{"G-2"}, StudentId{"STU-2"}, OfferingId{"O-1"}, CourseId{"C-1"},
         "B", GradeLifecycle::Pending},
        {GradeRecordId{"G-3"}, StudentId{"STU-3"}, OfferingId{"O-1"}, CourseId{"C-1"},
         "A", GradeLifecycle::Submitted},
    };
    context.changes_ = {
        {ChangeRequestId{"CH-1"}, FacultyId{"FAC-1"}, CourseId{"C-1"}, nullopt,
         CourseChangeType::Description, CourseChangeStatus::Pending, "New description"},
        {ChangeRequestId{"CH-X"}, FacultyId{"FAC-X"}, CourseId{"C-2"}, nullopt,
         CourseChangeType::Description, CourseChangeStatus::Pending, "Other description"},
    };
    return context;
}

void testFacultyQueries() {
    auto context = fixture();
    const size_t writes = context.writeCount;
    GetFacultyOfferingsQuery offerings(FacultyId{"FAC-1"}, context, context);
    auto offeringResult = offerings.execute();
    const auto assigned = offeringResult ? find_if(
        offeringResult.value().begin(), offeringResult.value().end(), [](const auto& value) {
            return value.offering.id == OfferingId{"O-1"};
        }) : vector<FacultyOfferingItem>::const_iterator{};
    require(offeringResult && offeringResult.value().size() == 2 &&
                assigned != offeringResult.value().end() && assigned->offering.enrolledCount == 2,
            "Assigned offerings should be connected and occupancy-derived");

    GetClassRosterQuery roster(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, context, context, context);
    auto rosterResult = roster.execute();
    require(rosterResult && rosterResult.value().size() == 2 &&
                rosterResult.value()[0].enrollment.studentId == StudentId{"STU-1"} &&
                rosterResult.value()[0].studentName == "Alice" &&
                rosterResult.value()[0].studentEmail == "alice@nexus.edu",
            "Roster should include only active Students with stable contact data");
    GetClassRosterQuery empty(
        FacultyId{"FAC-1"}, OfferingId{"O-EMPTY"}, context, context, context);
    const auto emptyResult = empty.execute();
    require(emptyResult && emptyResult.value().empty(),
            "An empty roster should remain successful and distinct from failure");

    GetGradeStateQuery grades(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, context, context, context);
    auto gradeResult = grades.execute();
    require(gradeResult && gradeResult.value().size() == 3 &&
                !gradeResult.value()[0].grade && gradeResult.value()[1].grade &&
                gradeResult.value()[1].grade->lifecycle == GradeLifecycle::Pending &&
                gradeResult.value()[2].grade->lifecycle == GradeLifecycle::Submitted,
            "Grade state should preserve ungraded, Pending, and Submitted entries");

    GetFacultyCourseChangeRequestsQuery changes(FacultyId{"FAC-1"}, context, context);
    auto changeResult = changes.execute();
    require(changeResult && changeResult.value().size() == 1 &&
                changeResult.value().front().id == ChangeRequestId{"CH-1"},
            "Faculty change queries should return only the selected submitter's requests");
    require(context.writeCount == writes && context.transactionCount == 0,
            "Faculty queries must perform no writes or mutation transactions");

    GetClassRosterQuery wrong(
        FacultyId{"FAC-X"}, OfferingId{"O-1"}, context, context, context);
    const auto wrongResult = wrong.execute();
    require(!wrongResult && wrongResult.error().code == "FACULTY_OFFERING_MISMATCH",
            "Offering ownership must be enforced");
    GetFacultyOfferingsQuery inactive(FacultyId{"FAC-2"}, context, context);
    GetFacultyOfferingsQuery missing(FacultyId{"FAC-MISSING"}, context, context);
    const auto inactiveResult = inactive.execute();
    const auto missingResult = missing.execute();
    require(!inactiveResult && inactiveResult.error().code == "FACULTY_INACTIVE" &&
                !missingResult && missingResult.error().code == "FACULTY_NOT_FOUND",
            "Inactive and missing Faculty identities should remain distinct");
    context.readError = Error{"TEST_READ_FAILED", "Read failed."};
    const auto failedRead = offerings.execute();
    require(!failedRead && failedRead.error().code == "TEST_READ_FAILED",
            "Storage errors must not become empty Faculty results");
}

void testGradeBatch() {
    auto context = fixture();
    context.grades_.clear();
    vector<GradeInput> inputs{
        {StudentId{"STU-1"}, "A"}, {StudentId{"STU-2"}, "INVALID"},
        {StudentId{"STU-X"}, "B"}, {StudentId{"STU-1"}, "C"}};
    auto concrete = make_unique<SubmitGradesCommand>(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, inputs, context, context,
        context, context, context);
    SubmitGradesCommand* observed = concrete.get();
    unique_ptr<ICommand> command = move(concrete);
    auto result = command->execute();
    require(result && observed->outcome().accepted.size() == 1 &&
                observed->outcome().rejected.size() == 3 && context.grades_.size() == 1 &&
                context.grades_.front().lifecycle == GradeLifecycle::Pending,
            "A polymorphic mixed batch should retain valid grades and report each rejection");

    auto pending = fixture();
    SubmitGradesCommand update(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, {{StudentId{"STU-2"}, "C"}},
        pending, pending, pending, pending, pending);
    require(update.execute() && pending.grades_[0].id == GradeRecordId{"G-2"} &&
                pending.grades_[0].grade == "C",
            "Updating Pending grade text should retain its stable identity");

    auto submitted = fixture();
    submitted.enrollments_[2].status = EnrollmentStatus::Active;
    SubmitGradesCommand finalGrade(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, {{StudentId{"STU-3"}, "F"}},
        submitted, submitted, submitted, submitted, submitted);
    require(finalGrade.execute() && finalGrade.outcome().accepted.empty() &&
                finalGrade.outcome().rejected.front().error.code == "GRADE_ALREADY_SUBMITTED" &&
                submitted.grades_[1].grade == "A",
            "A Submitted grade must not be overwritten");

    auto invalid = fixture();
    const size_t beforeTransactions = invalid.transactionCount;
    SubmitGradesCommand allInvalid(
        FacultyId{"FAC-1"}, OfferingId{"O-1"},
        {{StudentId{"STU-1"}, "invalid"}, {StudentId{}, "A"}},
        invalid, invalid, invalid, invalid, invalid);
    require(allInvalid.execute() && allInvalid.outcome().accepted.empty() &&
                invalid.transactionCount == beforeTransactions && invalid.writeCount == 0,
            "A purely invalid batch should need no transaction or write");

    auto duplicateAfterInvalid = fixture();
    duplicateAfterInvalid.grades_.clear();
    SubmitGradesCommand duplicateInput(
        FacultyId{"FAC-1"}, OfferingId{"O-1"},
        {{StudentId{"STU-1"}, "INVALID"}, {StudentId{"STU-1"}, "A"}},
        duplicateAfterInvalid, duplicateAfterInvalid, duplicateAfterInvalid,
        duplicateAfterInvalid, duplicateAfterInvalid);
    const auto duplicateInputResult = duplicateInput.execute();
    require(duplicateInputResult && duplicateInput.outcome().accepted.empty() &&
                duplicateInput.outcome().rejected.size() == 2 &&
                duplicateInput.outcome().rejected[0].error.code == "INVALID_GRADE" &&
                duplicateInput.outcome().rejected[1].error.code == "DUPLICATE_STUDENT" &&
                duplicateAfterInvalid.grades_.empty(),
            "Duplicate detection should remain independent of another entry's grade validity");

    auto rollback = fixture();
    rollback.grades_.clear();
    rollback.failWriteNumber = 2;
    SubmitGradesCommand failed(
        FacultyId{"FAC-1"}, OfferingId{"O-1"},
        {{StudentId{"STU-1"}, "A"}, {StudentId{"STU-2"}, "B"}},
        rollback, rollback, rollback, rollback, rollback);
    require(!failed.execute() && rollback.grades_.empty() && failed.outcome().accepted.empty(),
            "A grade write failure should roll back every accepted entry");

    auto commitRollback = fixture();
    commitRollback.grades_.clear();
    commitRollback.failCommit = true;
    SubmitGradesCommand commitFailed(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, {{StudentId{"STU-1"}, "A"}},
        commitRollback, commitRollback, commitRollback, commitRollback, commitRollback);
    const auto commitResult = commitFailed.execute();
    require(!commitResult && commitResult.error().code == "TRANSACTION_COMMIT_FAILED" &&
                commitRollback.grades_.empty() && commitFailed.outcome().accepted.empty(),
            "A grade commit failure should roll back every accepted entry");

    auto wrong = fixture();
    SubmitGradesCommand wrongFaculty(
        FacultyId{"FAC-X"}, OfferingId{"O-1"}, {{StudentId{"STU-1"}, "A"}},
        wrong, wrong, wrong, wrong, wrong);
    const auto wrongFacultyResult = wrongFaculty.execute();
    require(!wrongFacultyResult &&
                wrongFacultyResult.error().code == "FACULTY_OFFERING_MISMATCH",
            "Grade entry must enforce offering ownership");
}

void testGradeFinalization() {
    auto context = fixture();
    context.grades_.erase(context.grades_.begin() + 1);
    FinalizeGradesCommand command(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, context, context, context, context, context);
    require(command.execute() && command.finalizedCount() == 1 &&
                context.grades_.front().lifecycle == GradeLifecycle::Submitted &&
                context.enrollments_[1].status == EnrollmentStatus::Completed,
            "Finalisation should atomically submit grades and complete enrolments");
    GetAcademicProgressQuery progress(
        StudentId{"STU-2"}, context, context, context, context);
    auto progressResult = progress.execute();
    require(progressResult && progressResult.value().completedCourses.size() == 1 &&
                progressResult.value().completedCourses.front().grade == "B",
            "Student progress should immediately see a newly Submitted completion");

    auto none = fixture();
    none.grades_.erase(none.grades_.begin());
    FinalizeGradesCommand noPending(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, none, none, none, none, none);
    const auto noPendingResult = noPending.execute();
    require(!noPendingResult && noPendingResult.error().code == "NO_PENDING_GRADES",
            "Finalisation should reject an offering without Pending grades");

    auto inconsistent = fixture();
    inconsistent.enrollments_[1].status = EnrollmentStatus::Dropped;
    FinalizeGradesCommand invalid(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, inconsistent, inconsistent,
        inconsistent, inconsistent, inconsistent);
    const auto invalidResult = invalid.execute();
    require(!invalidResult &&
                invalidResult.error().code == "GRADE_ENROLLMENT_INTEGRITY_ERROR",
            "A Pending grade with a non-active enrolment should fail as persisted integrity");

    auto rollback = fixture();
    rollback.grades_.erase(rollback.grades_.begin() + 1);
    rollback.failWriteNumber = 2;
    FinalizeGradesCommand failed(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, rollback, rollback,
        rollback, rollback, rollback);
    require(!failed.execute() && rollback.grades_.front().lifecycle == GradeLifecycle::Pending &&
                rollback.enrollments_[1].status == EnrollmentStatus::Active,
            "A finalisation update failure should roll back grade and enrolment state");

    auto commitRollback = fixture();
    commitRollback.grades_.erase(commitRollback.grades_.begin() + 1);
    commitRollback.failCommit = true;
    FinalizeGradesCommand commitFailed(
        FacultyId{"FAC-1"}, OfferingId{"O-1"}, commitRollback, commitRollback,
        commitRollback, commitRollback, commitRollback);
    const auto commitResult = commitFailed.execute();
    require(!commitResult && commitResult.error().code == "TRANSACTION_COMMIT_FAILED" &&
                commitRollback.grades_.front().lifecycle == GradeLifecycle::Pending &&
                commitRollback.enrollments_[1].status == EnrollmentStatus::Active,
            "A finalisation commit failure should restore every grade and enrolment");

    auto wrong = fixture();
    FinalizeGradesCommand wrongFaculty(
        FacultyId{"FAC-X"}, OfferingId{"O-1"}, wrong, wrong, wrong, wrong, wrong);
    const auto wrongResult = wrongFaculty.execute();
    require(!wrongResult && wrongResult.error().code == "FACULTY_OFFERING_MISMATCH",
            "Finalisation must enforce Faculty ownership");
}

void testCourseChangeRequests() {
    auto description = fixture();
    SubmitCourseChangeRequestCommand descriptionCommand(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Description, "Updated", {}, nullopt, nullopt},
        description, description, description);
    require(descriptionCommand.execute() && !descriptionCommand.requestId().empty() &&
                description.changes_.back().requestedValue == "Updated" &&
                description.changes_.back().status == CourseChangeStatus::Pending,
            "A description request should be created as Pending");

    auto prerequisites = fixture();
    SubmitCourseChangeRequestCommand prerequisiteCommand(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Prerequisites, "",
         {CourseId{"C-2"}}, nullopt, nullopt},
        prerequisites, prerequisites, prerequisites);
    require(prerequisiteCommand.execute() &&
                prerequisites.changes_.back().requestedValue == "C-2",
            "Prerequisite requests should use a stable compact Course ID representation");

    auto emptyPrerequisites = fixture();
    SubmitCourseChangeRequestCommand emptyPrerequisiteCommand(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Prerequisites, "", {}, nullopt, nullopt},
        emptyPrerequisites, emptyPrerequisites, emptyPrerequisites);
    const auto emptyPrerequisiteResult = emptyPrerequisiteCommand.execute();
    require(emptyPrerequisiteResult && emptyPrerequisites.changes_.back().requestedValue.empty() &&
                emptyPrerequisites.changes_.back().status == CourseChangeStatus::Pending,
            "An empty prerequisite list should deliberately request removal of all prerequisites");

    auto capacity = fixture();
    SubmitCourseChangeRequestCommand capacityCommand(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Capacity, "", {}, 60, OfferingId{"O-1"}},
        capacity, capacity, capacity);
    require(capacityCommand.execute() && capacity.changes_.back().requestedValue == "60" &&
                capacity.changes_.back().offeringId == OfferingId{"O-1"},
            "Capacity requests should retain their affected offering and compact value");

    auto invalid = fixture();
    SubmitCourseChangeRequestCommand self(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Prerequisites, "", {CourseId{"C-1"}},
         nullopt, nullopt}, invalid, invalid, invalid);
    const auto selfResult = self.execute();
    require(!selfResult && selfResult.error().code == "SELF_PREREQUISITE",
            "A course must not request itself as a prerequisite");
    SubmitCourseChangeRequestCommand duplicate(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Prerequisites, "",
         {CourseId{"C-2"}, CourseId{"C-2"}}, nullopt, nullopt}, invalid, invalid, invalid);
    const auto duplicateResult = duplicate.execute();
    require(!duplicateResult && duplicateResult.error().code == "DUPLICATE_PREREQUISITE",
            "A prerequisite may not appear twice");
    SubmitCourseChangeRequestCommand excessiveCapacity(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Capacity, "", {}, 70000, OfferingId{"O-1"}},
        invalid, invalid, invalid);
    const auto capacityResult = excessiveCapacity.execute();
    require(!capacityResult && capacityResult.error().code == "INVALID_CHANGE_VALUE",
            "Capacity requests must fit the persisted column bounds");
    SubmitCourseChangeRequestCommand nonPositiveCapacity(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Capacity, "", {}, -1, OfferingId{"O-1"}},
        invalid, invalid, invalid);
    const auto nonPositiveResult = nonPositiveCapacity.execute();
    require(!nonPositiveResult && nonPositiveResult.error().code == "INVALID_CHANGE_VALUE",
            "Non-positive capacity must be rejected by Business Logic");
    SubmitCourseChangeRequestCommand emptyDescription(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Description, "", {}, nullopt, nullopt},
        invalid, invalid, invalid);
    const auto emptyDescriptionResult = emptyDescription.execute();
    require(!emptyDescriptionResult &&
                emptyDescriptionResult.error().code == "INVALID_CHANGE_VALUE",
            "An empty description must be rejected by Business Logic");
    SubmitCourseChangeRequestCommand invalidType(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, static_cast<CourseChangeType>(99), "", {}, nullopt, nullopt},
        invalid, invalid, invalid);
    const auto invalidTypeResult = invalidType.execute();
    require(!invalidTypeResult && invalidTypeResult.error().code == "INVALID_CHANGE_TYPE",
            "An unsupported domain enum value must fail closed");
    SubmitCourseChangeRequestCommand mismatchedValue(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Description, "Updated", {}, 10, nullopt},
        invalid, invalid, invalid);
    const auto mismatchedResult = mismatchedValue.execute();
    require(!mismatchedResult && mismatchedResult.error().code == "INVALID_CHANGE_VALUE",
            "Course-change value shape must agree with its type");
    SubmitCourseChangeRequestCommand missingCourse(
        FacultyId{"FAC-1"},
        {CourseId{"C-MISSING"}, CourseChangeType::Description, "Updated", {}, nullopt, nullopt},
        invalid, invalid, invalid);
    const auto missingCourseResult = missingCourse.execute();
    require(!missingCourseResult && missingCourseResult.error().code == "COURSE_NOT_FOUND",
            "A missing course should be rejected distinctly");
    SubmitCourseChangeRequestCommand missingOffering(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Capacity, "", {}, 60, OfferingId{"O-MISSING"}},
        invalid, invalid, invalid);
    const auto missingOfferingResult = missingOffering.execute();
    require(!missingOfferingResult && missingOfferingResult.error().code == "OFFERING_NOT_FOUND",
            "A missing capacity offering should be rejected distinctly");
    SubmitCourseChangeRequestCommand wrong(
        FacultyId{"FAC-X"},
        {CourseId{"C-1"}, CourseChangeType::Capacity, "", {}, 60, OfferingId{"O-1"}},
        invalid, invalid, invalid);
    const auto wrongResult = wrong.execute();
    require(!wrongResult && wrongResult.error().code == "FACULTY_OFFERING_MISMATCH",
            "Capacity changes must enforce offering ownership");

    auto failed = fixture();
    const auto existing = failed.changes_;
    failed.failChangeCreate = true;
    SubmitCourseChangeRequestCommand storageFailure(
        FacultyId{"FAC-1"},
        {CourseId{"C-1"}, CourseChangeType::Description, "Updated", {}, nullopt, nullopt},
        failed, failed, failed);
    require(!storageFailure.execute() && failed.changes_.size() == existing.size() &&
                failed.changes_.front().id == existing.front().id &&
                failed.changes_.front().requestedValue == existing.front().requestedValue,
            "A request creation failure must leave persisted requests unchanged");
}

}

int main() {
    const vector<pair<string, function<void()>>> tests{
        {"Faculty queries and ownership", testFacultyQueries},
        {"partial-valid grade batch", testGradeBatch},
        {"atomic grade finalisation", testGradeFinalization},
        {"Pending course-change requests", testCourseChangeRequests},
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
    cout << passed << '/' << tests.size() << " Faculty Business groups passed\n";
    return passed == tests.size() ? 0 : 1;
}
