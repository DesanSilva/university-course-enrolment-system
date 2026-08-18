#include "nexusenroll/business/cqrs/commands/drop_course_command.hpp"
#include "nexusenroll/business/cqrs/commands/enroll_student_command.hpp"
#include "nexusenroll/business/cqrs/commands/join_waitlist_command.hpp"
#include "nexusenroll/business/cqrs/queries/student_queries.hpp"

#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace nexusenroll;
using namespace business::cqrs::commands;
using namespace business::cqrs::queries;
using namespace business::domain;
using namespace business::notifications;
using namespace common;
using namespace data::contracts;
using namespace std;

void require(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

ScheduleSlot slot(DayOfWeek day, int start, int end) {
    auto value = ScheduleSlot::create(day, start, end, "TEST-ROOM");
    require(static_cast<bool>(value), "Expected a valid fixture schedule");
    return value.value();
}

template <typename T, typename Id>
optional<T> byId(const vector<T>& values, const Id& id) {
    const auto found = find_if(values.begin(), values.end(), [&id](const T& value) {
        return value.id == id;
    });
    return found == values.end() ? optional<T>{} : optional<T>{*found};
}

class FakeStudentContext final : public IUserStore,
                                 public ICourseStore,
                                 public IEnrollmentStore,
                                 public IProgramStore,
                                 public IGradeStore,
                                 public IWaitlistStore,
                                 public ITransactionBoundary {
public:
    Result<optional<User>> findUser(UserId id) const override {
        if (readError) return failure<optional<User>>(*readError);
        return Result<optional<User>>::success(byId(userValues, id));
    }
    Result<optional<Student>> findStudent(StudentId id) const override {
        if (readError) return failure<optional<Student>>(*readError);
        return Result<optional<Student>>::success(byId(studentValues, id));
    }
    Result<optional<Faculty>> findFaculty(FacultyId id) const override {
        return Result<optional<Faculty>>::success(byId(facultyValues, id));
    }
    Result<optional<Student>> findStudentByUserId(UserId id) const override {
        const auto found = find_if(studentValues.begin(), studentValues.end(), [&id](const Student& value) {
            return value.userId == id;
        });
        return Result<optional<Student>>::success(
            found == studentValues.end() ? optional<Student>{} : optional<Student>{*found});
    }
    Result<optional<Faculty>> findFacultyByUserId(UserId id) const override {
        const auto found = find_if(facultyValues.begin(), facultyValues.end(), [&id](const Faculty& value) {
            return value.userId == id;
        });
        return Result<optional<Faculty>>::success(
            found == facultyValues.end() ? optional<Faculty>{} : optional<Faculty>{*found});
    }
    Result<vector<User>> users() const override {
        return Result<vector<User>>::success(userValues);
    }
    Result<vector<Student>> students() const override {
        return Result<vector<Student>>::success(studentValues);
    }
    Result<vector<Faculty>> facultyMembers() const override {
        return Result<vector<Faculty>>::success(facultyValues);
    }
    Result<void> saveUser(User value) override { return save(userValues, move(value)); }
    Result<void> saveStudent(Student value) override { return save(studentValues, move(value)); }
    Result<void> saveFaculty(Faculty value) override { return save(facultyValues, move(value)); }

    Result<optional<Course>> findCourse(CourseId id) const override {
        if (readError) return failure<optional<Course>>(*readError);
        return Result<optional<Course>>::success(byId(courseValues, id));
    }
    Result<optional<CourseOffering>> findOffering(OfferingId id) const override {
        if (readError) return failure<optional<CourseOffering>>(*readError);
        auto offering = byId(offeringValues, id);
        if (offering) {
            offering->enrolledCount = static_cast<size_t>(count_if(
                enrollmentValues.begin(), enrollmentValues.end(), [&id](const Enrollment& enrollment) {
                    return enrollment.offeringId == id &&
                           enrollment.status == EnrollmentStatus::Active;
                }));
        }
        return Result<optional<CourseOffering>>::success(move(offering));
    }
    Result<vector<Course>> courses() const override {
        return Result<vector<Course>>::success(courseValues);
    }
    Result<vector<CourseOffering>> offerings() const override {
        return Result<vector<CourseOffering>>::success(offeringValues);
    }
    Result<vector<CatalogueItem>> browseCatalogue(const CatalogueFilter& filter) const override {
        if (readError) return failure<vector<CatalogueItem>>(*readError);
        vector<CatalogueItem> matches;
        for (const auto& source : catalogue) {
            auto current = findOffering(source.offering.id);
            if (!current) return failure<vector<CatalogueItem>>(current.error());
            CatalogueItem item{source.course, *current.value(), source.instructorName};
            const bool keywordMatches = filter.keyword.empty() ||
                item.course.code.find(filter.keyword) != string::npos ||
                item.course.name.find(filter.keyword) != string::npos ||
                item.course.description.find(filter.keyword) != string::npos;
            if ((filter.semester.empty() || item.offering.semester == filter.semester) &&
                (filter.department.empty() || item.course.department == filter.department) &&
                (filter.courseNumber.empty() || item.course.courseNumber == filter.courseNumber) &&
                keywordMatches &&
                (filter.instructor.empty() ||
                 item.instructorName.find(filter.instructor) != string::npos)) {
                matches.push_back(move(item));
            }
        }
        return Result<vector<CatalogueItem>>::success(move(matches));
    }
    Result<vector<FacultyOfferingItem>> assignedOfferings(FacultyId) const override {
        return Result<vector<FacultyOfferingItem>>::success({});
    }
    Result<bool> facultyTeachesCourse(FacultyId, CourseId) const override {
        return Result<bool>::success(false);
    }
    Result<void> saveCourse(Course value) override { return save(courseValues, move(value)); }
    Result<void> saveOffering(CourseOffering value) override {
        return save(offeringValues, move(value));
    }

    Result<optional<Enrollment>> findEnrollment(EnrollmentId id) const override {
        return Result<optional<Enrollment>>::success(byId(enrollmentValues, id));
    }
    Result<optional<Enrollment>> findStudentEnrollment(
        StudentId studentId, OfferingId offeringId) const override {
        if (readError) return failure<optional<Enrollment>>(*readError);
        const auto found = find_if(
            enrollmentValues.begin(), enrollmentValues.end(), [&](const Enrollment& value) {
                return value.studentId == studentId && value.offeringId == offeringId;
            });
        return Result<optional<Enrollment>>::success(
            found == enrollmentValues.end() ? optional<Enrollment>{} : optional<Enrollment>{*found});
    }
    Result<vector<Enrollment>> enrollments() const override {
        return Result<vector<Enrollment>>::success(enrollmentValues);
    }
    Result<vector<Enrollment>> activeEnrollmentsForStudent(StudentId studentId) const override {
        if (readError) return failure<vector<Enrollment>>(*readError);
        return selectedEnrollments(studentId, "", true);
    }
    Result<vector<Enrollment>> scheduleEnrollmentsForStudent(
        StudentId studentId, const string& semester) const override {
        if (readError) return failure<vector<Enrollment>>(*readError);
        return selectedEnrollments(studentId, semester, false);
    }
    Result<vector<FacultyRosterEntry>> activeRosterForOffering(OfferingId) const override {
        return Result<vector<FacultyRosterEntry>>::success({});
    }
    Result<void> saveEnrollment(Enrollment value) override {
        return save(enrollmentValues, move(value));
    }
    Result<void> removeEnrollment(EnrollmentId id) override {
        ++writeCount;
        enrollmentValues.erase(remove_if(enrollmentValues.begin(), enrollmentValues.end(),
                                    [&id](const Enrollment& value) { return value.id == id; }),
                          enrollmentValues.end());
        return Result<void>::success();
    }

    Result<optional<DegreeProgram>> findProgram(ProgramId id) const override {
        if (readError) return failure<optional<DegreeProgram>>(*readError);
        return Result<optional<DegreeProgram>>::success(byId(programValues, id));
    }
    Result<vector<DegreeProgram>> programs() const override {
        return Result<vector<DegreeProgram>>::success(programValues);
    }
    Result<void> saveProgram(DegreeProgram value) override {
        return save(programValues, move(value));
    }

    Result<optional<GradeRecord>> findGradeRecord(GradeRecordId id) const override {
        return Result<optional<GradeRecord>>::success(byId(gradeValues, id));
    }
    Result<vector<GradeRecord>> gradeRecords() const override {
        return Result<vector<GradeRecord>>::success(gradeValues);
    }
    Result<vector<GradeRecord>> submittedGradesForStudent(StudentId studentId) const override {
        if (readError) return failure<vector<GradeRecord>>(*readError);
        vector<GradeRecord> values;
        copy_if(gradeValues.begin(), gradeValues.end(), back_inserter(values), [&](const GradeRecord& grade) {
            const auto enrollment = find_if(
                enrollmentValues.begin(), enrollmentValues.end(), [&](const Enrollment& value) {
                    return value.studentId == grade.studentId &&
                           value.offeringId == grade.offeringId;
                });
            return grade.studentId == studentId &&
                   grade.lifecycle == GradeLifecycle::Submitted &&
                   enrollment != enrollmentValues.end() &&
                   enrollment->status == EnrollmentStatus::Completed;
        });
        return Result<vector<GradeRecord>>::success(move(values));
    }
    Result<optional<GradeRecord>> findStudentGradeRecord(
        StudentId studentId, OfferingId offeringId) const override {
        const auto found = find_if(gradeValues.begin(), gradeValues.end(), [&](const GradeRecord& value) {
            return value.studentId == studentId && value.offeringId == offeringId;
        });
        return Result<optional<GradeRecord>>::success(
            found == gradeValues.end() ? optional<GradeRecord>{} : optional<GradeRecord>{*found});
    }
    Result<vector<FacultyGradeStateEntry>> gradeStateForOffering(OfferingId) const override {
        return Result<vector<FacultyGradeStateEntry>>::success({});
    }
    Result<vector<GradeRecord>> pendingGradesForOffering(OfferingId offeringId) const override {
        vector<GradeRecord> values;
        copy_if(gradeValues.begin(), gradeValues.end(), back_inserter(values), [&](const GradeRecord& value) {
            return value.offeringId == offeringId && value.lifecycle == GradeLifecycle::Pending;
        });
        return Result<vector<GradeRecord>>::success(move(values));
    }
    Result<void> createGradeRecord(GradeRecord value) override {
        return save(gradeValues, move(value));
    }
    Result<void> saveGradeRecord(GradeRecord value) override {
        return save(gradeValues, move(value));
    }

    Result<optional<WaitlistEntry>> findWaitlistEntry(WaitlistEntryId id) const override {
        return Result<optional<WaitlistEntry>>::success(byId(waitlistValues, id));
    }
    Result<optional<WaitlistEntry>> findStudentWaitlistEntry(
        StudentId studentId, OfferingId offeringId) const override {
        if (readError) return failure<optional<WaitlistEntry>>(*readError);
        const auto found = find_if(waitlistValues.begin(), waitlistValues.end(), [&](const WaitlistEntry& value) {
            return value.studentId == studentId && value.offeringId == offeringId;
        });
        return Result<optional<WaitlistEntry>>::success(
            found == waitlistValues.end() ? optional<WaitlistEntry>{} : optional<WaitlistEntry>{*found});
    }
    Result<vector<WaitlistEntry>> waitlistEntries() const override {
        return Result<vector<WaitlistEntry>>::success(waitlistValues);
    }
    Result<vector<WaitlistEntry>> waitlistEntriesForStudent(StudentId studentId) const override {
        if (readError) return failure<vector<WaitlistEntry>>(*readError);
        vector<WaitlistEntry> values;
        copy_if(waitlistValues.begin(), waitlistValues.end(), back_inserter(values), [&](const WaitlistEntry& value) {
            return value.studentId == studentId && value.status != WaitlistStatus::Removed;
        });
        sort(values.begin(), values.end(), [](const WaitlistEntry& left, const WaitlistEntry& right) {
            return pair{left.offeringId, left.position} < pair{right.offeringId, right.position};
        });
        return Result<vector<WaitlistEntry>>::success(move(values));
    }
    Result<vector<WaitlistEntry>> waitingEntriesForOffering(OfferingId offeringId) const override {
        if (readError) return failure<vector<WaitlistEntry>>(*readError);
        vector<WaitlistEntry> values;
        copy_if(waitlistValues.begin(), waitlistValues.end(), back_inserter(values), [&](const WaitlistEntry& value) {
            return value.offeringId == offeringId && value.status == WaitlistStatus::Waiting;
        });
        sort(values.begin(), values.end(), [](const WaitlistEntry& left, const WaitlistEntry& right) {
            return left.position < right.position;
        });
        return Result<vector<WaitlistEntry>>::success(move(values));
    }
    Result<size_t> nextWaitlistPosition(OfferingId offeringId) const override {
        size_t next = 1;
        for (const auto& value : waitlistValues) {
            if (value.offeringId == offeringId) next = max(next, value.position + 1);
        }
        return Result<size_t>::success(next);
    }
    Result<void> saveWaitlistEntry(WaitlistEntry value) override {
        if (failWaitlistWrites) {
            ++writeCount;
            return Result<void>::failure(
                "TEST_WAITLIST_WRITE_FAILED", "The test waitlist write failed.");
        }
        return save(waitlistValues, move(value));
    }
    Result<void> removeWaitlistEntry(WaitlistEntryId id) override {
        ++writeCount;
        waitlistValues.erase(remove_if(waitlistValues.begin(), waitlistValues.end(),
                                 [&id](const WaitlistEntry& value) { return value.id == id; }),
                       waitlistValues.end());
        return Result<void>::success();
    }

    Result<void> executeTransaction(const Operation& operation) override {
        lock_guard<mutex> lock(*transactionMutex);
        const auto enrollmentSnapshot = enrollmentValues;
        const auto waitlistSnapshot = waitlistValues;
        committed = false;
        auto result = operation();
        if (!result || failCommit) {
            enrollmentValues = enrollmentSnapshot;
            waitlistValues = waitlistSnapshot;
            if (result && failCommit) {
                return Result<void>::failure("TRANSACTION_COMMIT_FAILED", "Commit failed.");
            }
            return result;
        }
        committed = true;
        return Result<void>::success();
    }

    vector<User> userValues;
    vector<Student> studentValues;
    vector<Faculty> facultyValues;
    vector<Course> courseValues;
    vector<CourseOffering> offeringValues;
    vector<CatalogueItem> catalogue;
    vector<Enrollment> enrollmentValues;
    vector<DegreeProgram> programValues;
    vector<GradeRecord> gradeValues;
    vector<WaitlistEntry> waitlistValues;
    optional<Error> readError;
    bool failWrites{false};
    bool failWaitlistWrites{false};
    bool failCommit{false};
    bool committed{false};
    size_t writeCount{0};

private:
    template <typename T>
    Result<T> failure(const Error& error) const {
        return Result<T>::failure(error.code, error.message);
    }

    template <typename T>
    Result<void> save(vector<T>& values, T value) {
        ++writeCount;
        if (failWrites) {
            return Result<void>::failure("TEST_WRITE_FAILED", "The test write failed.");
        }
        const auto found = find_if(values.begin(), values.end(), [&](const T& current) {
            return current.id == value.id;
        });
        if (found == values.end()) values.push_back(move(value));
        else *found = move(value);
        return Result<void>::success();
    }

    Result<vector<Enrollment>> selectedEnrollments(
        StudentId studentId, const string& semester, bool activeOnly) const {
        vector<Enrollment> values;
        for (const auto& enrollment : enrollmentValues) {
            if (enrollment.studentId != studentId ||
                (activeOnly && enrollment.status != EnrollmentStatus::Active) ||
                (!activeOnly && enrollment.status == EnrollmentStatus::Dropped)) {
                continue;
            }
            const auto offering = byId(offeringValues, enrollment.offeringId);
            if (semester.empty() || (offering && offering->semester == semester)) {
                values.push_back(enrollment);
            }
        }
        sort(values.begin(), values.end(), [](const Enrollment& left, const Enrollment& right) {
            return left.offeringId < right.offeringId;
        });
        return Result<vector<Enrollment>>::success(move(values));
    }

    shared_ptr<mutex> transactionMutex{make_shared<mutex>()};
};

FakeStudentContext fixture() {
    FakeStudentContext context;
    context.userValues = {
        {UserId{"U-A"}, "Alice", "alice@nexus.edu", UserStatus::Active, UserRole::Student},
        {UserId{"U-B"}, "Ben", "ben@nexus.edu", UserStatus::Active, UserRole::Student},
        {UserId{"U-C"}, "Cara", "cara@nexus.edu", UserStatus::Inactive, UserRole::Student},
        {UserId{"U-D"}, "Dev", "dev@nexus.edu", UserStatus::Active, UserRole::Student},
    };
    context.studentValues = {
        {StudentId{"STU-A"}, UserId{"U-A"}, ProgramId{"PROGRAM-CS"}},
        {StudentId{"STU-B"}, UserId{"U-B"}, ProgramId{"PROGRAM-CS"}},
        {StudentId{"STU-C"}, UserId{"U-C"}, ProgramId{"PROGRAM-CS"}},
        {StudentId{"STU-D"}, UserId{"U-D"}, ProgramId{"PROGRAM-CS"}},
    };
    context.courseValues = {
        {CourseId{"COURSE-INTRO"}, "CS101", "Computer Science", "101", "Programming",
         "Programming foundations", 4, {}},
        {CourseId{"COURSE-ADV"}, "CS201", "Computer Science", "201", "Data Structures",
         "Data structures and algorithms", 4, {CourseId{"COURSE-INTRO"}}},
        {CourseId{"COURSE-OTHER"}, "CS220", "Computer Science", "220", "Architecture",
         "Software architecture", 4, {}},
    };
    context.offeringValues = {
        {OfferingId{"O-PAST"}, CourseId{"COURSE-INTRO"}, "2025S2", FacultyId{"FAC-1"},
         40, 0, {slot(DayOfWeek::Monday, 540, 600)}},
        {OfferingId{"O-BASE"}, CourseId{"COURSE-OTHER"}, "2026S1", FacultyId{"FAC-1"},
         40, 0, {slot(DayOfWeek::Monday, 540, 600)}},
        {OfferingId{"O-ADV"}, CourseId{"COURSE-ADV"}, "2026S1", FacultyId{"FAC-1"},
         3, 0, {slot(DayOfWeek::Tuesday, 540, 600)}},
        {OfferingId{"O-CONFLICT"}, CourseId{"COURSE-OTHER"}, "2026S1", FacultyId{"FAC-1"},
         3, 0, {slot(DayOfWeek::Monday, 570, 630)}},
        {OfferingId{"O-FULL"}, CourseId{"COURSE-OTHER"}, "2026S1", FacultyId{"FAC-1"},
         1, 0, {slot(DayOfWeek::Wednesday, 540, 600)}},
    };
    for (const auto& offering : context.offeringValues) {
        const auto course = byId(context.courseValues, offering.courseId);
        context.catalogue.push_back({*course, offering, "Dr Test"});
    }
    context.enrollmentValues = {
        {EnrollmentId{"ENR-PAST"}, StudentId{"STU-A"}, OfferingId{"O-PAST"},
         EnrollmentStatus::Completed},
        {EnrollmentId{"ENR-BASE"}, StudentId{"STU-A"}, OfferingId{"O-BASE"},
         EnrollmentStatus::Active},
        {EnrollmentId{"ENR-FULL"}, StudentId{"STU-B"}, OfferingId{"O-FULL"},
         EnrollmentStatus::Active},
    };
    context.programValues = {
        {ProgramId{"PROGRAM-CS"}, "Computer Science", "Computer Science",
         {CourseId{"COURSE-INTRO"}, CourseId{"COURSE-ADV"}}, 120},
    };
    context.gradeValues = {
        {GradeRecordId{"GRADE-1"}, StudentId{"STU-A"}, OfferingId{"O-PAST"},
         CourseId{"COURSE-INTRO"}, "A", GradeLifecycle::Submitted},
    };
    return context;
}

class CommitCheckingObserver final : public INotificationObserver {
public:
    explicit CommitCheckingObserver(const FakeStudentContext& context) : context_(context) {}
    void notify(const CourseSeatAvailable& event) override {
        observedAfterCommit = context_.committed;
        events.push_back(event);
    }
    const FakeStudentContext& context_;
    bool observedAfterCommit{false};
    vector<CourseSeatAvailable> events;
};

void testStudentQueriesAndNoWrites() {
    auto context = fixture();
    const size_t writesBefore = context.writeCount;
    for (const CatalogueFilter& filter : {
             CatalogueFilter{"2026S1", "", "", "", ""},
             CatalogueFilter{"", "Computer Science", "", "", ""},
             CatalogueFilter{"", "", "201", "", ""},
             CatalogueFilter{"", "", "", "algorithms", ""},
             CatalogueFilter{"", "", "", "", "Dr Test"}}) {
        BrowseCourseCatalogueQuery query(
            StudentId{"STU-A"}, filter, context, context);
        auto result = query.execute();
        require(result && !result.value().empty(), "Every supported catalogue filter should match");
    }
    BrowseCourseCatalogueQuery catalogueQuery(
        StudentId{"STU-A"}, {"2026S1", "", "201", "", ""}, context, context);
    auto catalogue = catalogueQuery.execute();
    require(catalogue && catalogue.value().size() == 1 &&
                catalogue.value().front().offering.enrolledCount == 0 &&
                catalogue.value().front().offering.capacity == 3,
            "Catalogue results should expose active-derived capacity");

    GetStudentScheduleQuery current(
        StudentId{"STU-A"}, "2026S1", context, context, context);
    GetStudentScheduleQuery past(
        StudentId{"STU-A"}, "2025S2", context, context, context);
    const auto currentResult = current.execute();
    require(currentResult && currentResult.value().size() == 1,
            "Current schedule should include the active current enrolment");
    auto pastResult = past.execute();
    require(pastResult && pastResult.value().size() == 1 &&
                pastResult.value().front().enrollment.status == EnrollmentStatus::Completed,
            "Past schedule should include the completed enrolment");

    GetAcademicProgressQuery progress(
        StudentId{"STU-A"}, context, context, context, context);
    auto progressResult = progress.execute();
    require(progressResult && progressResult.value().completedCourses.size() == 1 &&
                progressResult.value().completedCourses.front().grade == "A" &&
                progressResult.value().remainingRequiredCourses.size() == 1 &&
                progressResult.value().remainingRequiredCourses.front().id == CourseId{"COURSE-ADV"},
            "Progress should derive submitted grades and remaining requirements");

    context.waitlistValues.push_back(
        {WaitlistEntryId{"WAIT-1"}, StudentId{"STU-A"}, OfferingId{"O-FULL"},
         2, WaitlistStatus::Waiting});
    GetStudentWaitlistQuery waitlist(StudentId{"STU-A"}, context, context, context);
    auto waitlistResult = waitlist.execute();
    require(waitlistResult && waitlistResult.value().size() == 1 &&
                waitlistResult.value().front().entry.position == 2,
            "Waitlist query should return stable persisted entries");
    require(context.writeCount == writesBefore, "Student queries must not perform writes");
}

void testQueryValidationAndStorageErrors() {
    auto context = fixture();
    BrowseCourseCatalogueQuery missing(
        StudentId{"STU-MISSING"}, {}, context, context);
    const auto missingResult = missing.execute();
    require(!missingResult && missingResult.error().code == "STUDENT_NOT_FOUND",
            "A missing Student should remain distinct");
    BrowseCourseCatalogueQuery inactive(StudentId{"STU-C"}, {}, context, context);
    const auto inactiveResult = inactive.execute();
    require(!inactiveResult && inactiveResult.error().code == "STUDENT_INACTIVE",
            "An inactive linked User should be rejected");
    context.readError = Error{"TEST_READ_FAILED", "The test read failed."};
    BrowseCourseCatalogueQuery failing(StudentId{"STU-A"}, {}, context, context);
    auto result = failing.execute();
    require(!result && result.error().code == "TEST_READ_FAILED",
            "Storage errors must not become empty query results");
}

void testEnrollmentValidationAndRollback() {
    {
        auto context = fixture();
        context.waitlistValues.push_back(
            {WaitlistEntryId{"WAIT-ADV"}, StudentId{"STU-A"}, OfferingId{"O-ADV"},
             1, WaitlistStatus::Waiting});
        unique_ptr<ICommand> command = make_unique<EnrollStudentCommand>(
            StudentId{"STU-A"}, OfferingId{"O-ADV"}, context, context, context,
            context, context, context);
        auto result = command->execute();
        auto saved = context.findStudentEnrollment(StudentId{"STU-A"}, OfferingId{"O-ADV"});
        auto retired = context.findStudentWaitlistEntry(
            StudentId{"STU-A"}, OfferingId{"O-ADV"});
        require(result && saved && saved.value() &&
                    saved.value()->status == EnrollmentStatus::Active && retired &&
                    retired.value() && retired.value()->status == WaitlistStatus::Removed &&
                    context.committed,
                "Polymorphic enrolment should commit and retire its waitlist entry");
    }
    struct Case { StudentId student; OfferingId offering; const char* code; };
    const vector<Case> cases{
        {StudentId{"STU-X"}, OfferingId{"O-ADV"}, "STUDENT_NOT_FOUND"},
        {StudentId{"STU-C"}, OfferingId{"O-ADV"}, "STUDENT_INACTIVE"},
        {StudentId{"STU-A"}, OfferingId{"O-X"}, "OFFERING_NOT_FOUND"},
        {StudentId{"STU-A"}, OfferingId{"O-BASE"}, "ALREADY_ENROLLED"},
        {StudentId{"STU-B"}, OfferingId{"O-ADV"}, "PREREQUISITE_NOT_MET"},
        {StudentId{"STU-A"}, OfferingId{"O-FULL"}, "CAPACITY_FULL"},
        {StudentId{"STU-A"}, OfferingId{"O-CONFLICT"}, "TIME_CONFLICT"},
    };
    for (const auto& testCase : cases) {
        auto context = fixture();
        EnrollStudentCommand command(
            testCase.student, testCase.offering, context, context, context,
            context, context, context);
        auto result = command.execute();
        require(!result && result.error().code == testCase.code,
                string("Expected enrolment error ") + testCase.code);
    }

    auto context = fixture();
    context.failWrites = true;
    EnrollStudentCommand failed(
        StudentId{"STU-A"}, OfferingId{"O-ADV"}, context, context, context,
        context, context, context);
    auto result = failed.execute();
    auto absent = context.findStudentEnrollment(StudentId{"STU-A"}, OfferingId{"O-ADV"});
    require(!result && result.error().code == "TEST_WRITE_FAILED" && absent && !absent.value(),
            "A write failure should roll back without a partial enrolment");

    auto commitContext = fixture();
    commitContext.failCommit = true;
    EnrollStudentCommand commitFailed(
        StudentId{"STU-A"}, OfferingId{"O-ADV"}, commitContext, commitContext,
        commitContext, commitContext, commitContext, commitContext);
    auto commitResult = commitFailed.execute();
    auto commitAbsent = commitContext.findStudentEnrollment(
        StudentId{"STU-A"}, OfferingId{"O-ADV"});
    require(!commitResult && commitResult.error().code == "TRANSACTION_COMMIT_FAILED" &&
                commitAbsent && !commitAbsent.value(),
            "A commit failure should restore the previous enrolment state");

    auto waitlistFailureContext = fixture();
    waitlistFailureContext.waitlistValues.push_back(
        {WaitlistEntryId{"WAIT-ADV"}, StudentId{"STU-A"}, OfferingId{"O-ADV"},
         1, WaitlistStatus::Waiting});
    waitlistFailureContext.failWaitlistWrites = true;
    EnrollStudentCommand waitlistFailure(
        StudentId{"STU-A"}, OfferingId{"O-ADV"}, waitlistFailureContext,
        waitlistFailureContext, waitlistFailureContext, waitlistFailureContext,
        waitlistFailureContext, waitlistFailureContext);
    const auto waitlistFailureResult = waitlistFailure.execute();
    const auto rolledBackEnrollment = waitlistFailureContext.findStudentEnrollment(
        StudentId{"STU-A"}, OfferingId{"O-ADV"});
    const auto preservedWaitlist = waitlistFailureContext.findStudentWaitlistEntry(
        StudentId{"STU-A"}, OfferingId{"O-ADV"});
    require(!waitlistFailureResult &&
                waitlistFailureResult.error().code == "TEST_WAITLIST_WRITE_FAILED" &&
                rolledBackEnrollment && !rolledBackEnrollment.value() &&
                preservedWaitlist && preservedWaitlist.value() &&
                preservedWaitlist.value()->status == WaitlistStatus::Waiting,
            "A waitlist-retirement failure must roll back the new enrolment");
}

void testDropObserverAfterCommitOnly() {
    auto context = fixture();
    context.enrollmentValues.push_back(
        {EnrollmentId{"ENR-ADV"}, StudentId{"STU-A"}, OfferingId{"O-ADV"},
         EnrollmentStatus::Active});
    context.waitlistValues.push_back(
        {WaitlistEntryId{"WAIT-B"}, StudentId{"STU-B"}, OfferingId{"O-ADV"},
         1, WaitlistStatus::Waiting});
    NotificationPublisher publisher;
    auto observer = make_shared<CommitCheckingObserver>(context);
    publisher.subscribe(observer);
    DropCourseCommand command(
        StudentId{"STU-A"}, OfferingId{"O-ADV"}, context, context, context,
        context, context, publisher);
    auto result = command.execute();
    require(result && observer->events.size() == 1 && observer->observedAfterCommit &&
                observer->events.front().offeringId == OfferingId{"O-ADV"} &&
                observer->events.front().waitingStudentIds == vector<StudentId>{StudentId{"STU-B"}},
            "A successful drop should notify affected waiters after commit");

    auto failingContext = fixture();
    failingContext.enrollmentValues.push_back(
        {EnrollmentId{"ENR-ADV"}, StudentId{"STU-A"}, OfferingId{"O-ADV"},
         EnrollmentStatus::Active});
    failingContext.waitlistValues = context.waitlistValues;
    failingContext.failWrites = true;
    NotificationPublisher failingPublisher;
    auto recorder = make_shared<WaitlistNotificationObserver>();
    failingPublisher.subscribe(recorder);
    DropCourseCommand failing(
        StudentId{"STU-A"}, OfferingId{"O-ADV"}, failingContext, failingContext,
        failingContext, failingContext, failingContext, failingPublisher);
    require(!failing.execute() && recorder->notifications().empty(),
            "A rolled-back drop must produce no Observer reaction");

    auto overCapacityContext = fixture();
    overCapacityContext.offeringValues[2].capacity = 1;
    overCapacityContext.enrollmentValues.push_back(
        {EnrollmentId{"ENR-ADV-A"}, StudentId{"STU-A"}, OfferingId{"O-ADV"},
         EnrollmentStatus::Active});
    overCapacityContext.enrollmentValues.push_back(
        {EnrollmentId{"ENR-ADV-B"}, StudentId{"STU-B"}, OfferingId{"O-ADV"},
         EnrollmentStatus::Active});
    overCapacityContext.waitlistValues.push_back(
        {WaitlistEntryId{"WAIT-ADV"}, StudentId{"STU-D"}, OfferingId{"O-ADV"},
         1, WaitlistStatus::Waiting});
    NotificationPublisher overCapacityPublisher;
    auto overCapacityRecorder = make_shared<WaitlistNotificationObserver>();
    overCapacityPublisher.subscribe(overCapacityRecorder);
    DropCourseCommand overCapacityDrop(
        StudentId{"STU-A"}, OfferingId{"O-ADV"}, overCapacityContext,
        overCapacityContext, overCapacityContext, overCapacityContext,
        overCapacityContext, overCapacityPublisher);
    require(overCapacityDrop.execute() && overCapacityRecorder->notifications().empty(),
            "A drop that leaves an over-capacity offering full must not announce a seat");
}

void testWaitlistRulesRollbackAndConcurrentPositions() {
    auto context = fixture();
    JoinWaitlistCommand successful(
        StudentId{"STU-A"}, OfferingId{"O-FULL"}, context, context, context, context, context);
    require(static_cast<bool>(successful.execute()), "Joining a full offering should succeed");
    auto entry = context.findStudentWaitlistEntry(StudentId{"STU-A"}, OfferingId{"O-FULL"});
    require(entry && entry.value() && entry.value()->position == 1,
            "The first waitlist entry should receive position one");
    JoinWaitlistCommand duplicate(
        StudentId{"STU-A"}, OfferingId{"O-FULL"}, context, context, context, context, context);
    const auto duplicateResult = duplicate.execute();
    require(!duplicateResult && duplicateResult.error().code == "ALREADY_WAITLISTED",
            "A duplicate waiting entry should be rejected");

    auto activeContext = fixture();
    JoinWaitlistCommand active(
        StudentId{"STU-B"}, OfferingId{"O-FULL"}, activeContext, activeContext,
        activeContext, activeContext, activeContext);
    const auto activeResult = active.execute();
    require(!activeResult && activeResult.error().code == "ALREADY_ENROLLED",
            "An active enrolment should block waitlisting");
    JoinWaitlistCommand available(
        StudentId{"STU-A"}, OfferingId{"O-ADV"}, activeContext, activeContext,
        activeContext, activeContext, activeContext);
    const auto availableResult = available.execute();
    require(!availableResult && availableResult.error().code == "SEAT_AVAILABLE",
            "An available seat should block waitlisting");

    auto rollbackContext = fixture();
    rollbackContext.failWrites = true;
    JoinWaitlistCommand rollback(
        StudentId{"STU-A"}, OfferingId{"O-FULL"}, rollbackContext, rollbackContext,
        rollbackContext, rollbackContext, rollbackContext);
    require(!rollback.execute() && rollbackContext.waitlistValues.empty(),
            "A waitlist write failure should leave no partial entry");

    auto concurrentContext = fixture();
    optional<CommandResult> first;
    optional<CommandResult> second;
    thread left([&] {
        JoinWaitlistCommand command(
            StudentId{"STU-A"}, OfferingId{"O-FULL"}, concurrentContext, concurrentContext,
            concurrentContext, concurrentContext, concurrentContext);
        first = command.execute();
    });
    thread right([&] {
        JoinWaitlistCommand command(
            StudentId{"STU-D"}, OfferingId{"O-FULL"}, concurrentContext, concurrentContext,
            concurrentContext, concurrentContext, concurrentContext);
        second = command.execute();
    });
    left.join();
    right.join();
    auto entries = concurrentContext.waitingEntriesForOffering(OfferingId{"O-FULL"});
    require(first && *first && second && *second && entries && entries.value().size() == 2 &&
                entries.value()[0].position != entries.value()[1].position,
            "Concurrent waitlist joins must receive distinct positions");
}

}

int main() {
    const vector<pair<string, function<void()>>> tests{
        {"Student queries and no writes", testStudentQueriesAndNoWrites},
        {"query validation and storage errors", testQueryValidationAndStorageErrors},
        {"enrolment validation and rollback", testEnrollmentValidationAndRollback},
        {"drop Observer after commit only", testDropObserverAfterCommitOnly},
        {"waitlist rules, rollback, and concurrency", testWaitlistRulesRollbackAndConcurrentPositions},
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
    cout << passed << '/' << tests.size() << " Student Business groups passed\n";
    return passed == tests.size() ? 0 : 1;
}
