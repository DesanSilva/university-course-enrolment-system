#include "nexusenroll/business/cqrs/commands/administrator_commands.hpp"
#include "nexusenroll/business/cqrs/queries/administrator_queries.hpp"

#include <algorithm>
#include <cmath>
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

class FakeAdminContext final : public IUserStore,
                               public ICourseStore,
                               public IEnrollmentStore,
                               public IProgramStore,
                               public IGradeStore,
                               public IChangeRequestStore,
                               public IWaitlistStore,
                               public ITransactionBoundary {
public:
    // IUserStore
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
        if (readError) return failed<optional<Student>>();
        const auto found = find_if(students_.begin(), students_.end(), [&](const Student& value) {
            return value.userId == id;
        });
        return Result<optional<Student>>::success(
            found == students_.end() ? optional<Student>{} : optional<Student>{*found});
    }
    Result<optional<Faculty>> findFacultyByUserId(UserId id) const override {
        if (readError) return failed<optional<Faculty>>();
        const auto found = find_if(faculty_.begin(), faculty_.end(), [&](const Faculty& value) {
            return value.userId == id;
        });
        return Result<optional<Faculty>>::success(
            found == faculty_.end() ? optional<Faculty>{} : optional<Faculty>{*found});
    }
    Result<vector<User>> users() const override {
        if (readError) return failed<vector<User>>();
        return Result<vector<User>>::success(users_);
    }
    Result<vector<Student>> students() const override {
        if (readError) return failed<vector<Student>>();
        return Result<vector<Student>>::success(students_);
    }
    Result<vector<Faculty>> facultyMembers() const override {
        if (readError) return failed<vector<Faculty>>();
        return Result<vector<Faculty>>::success(faculty_);
    }
    Result<vector<User>> usersByRole(optional<UserRole> role) const override {
        if (readError) return failed<vector<User>>();
        vector<User> values;
        copy_if(users_.begin(), users_.end(), back_inserter(values),
                [&role](const User& user) { return !role || user.role == *role; });
        return Result<vector<User>>::success(move(values));
    }
    Result<bool> departmentExists(const string& dept) const override {
        if (readError) return failed<bool>();
        const bool exists = dept == "Computer Science" || dept == "Business" ||
                            dept == "DEP-CS" || dept == "DEP-BUS" || dept == "CS" || dept == "BUS";
        return Result<bool>::success(exists);
    }
    Result<void> createUser(User value) override { return save(users_, move(value)); }
    Result<void> createStudent(Student value) override { return save(students_, move(value)); }
    Result<void> createFaculty(Faculty value) override { return save(faculty_, move(value)); }
    Result<void> saveUser(User value) override { return save(users_, move(value)); }
    Result<void> saveStudent(Student value) override { return save(students_, move(value)); }
    Result<void> saveFaculty(Faculty value) override { return save(faculty_, move(value)); }

    // ICourseStore
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
        if (readError) return failed<vector<Course>>();
        return Result<vector<Course>>::success(courses_);
    }
    Result<vector<CourseOffering>> offerings() const override {
        if (readError) return failed<vector<CourseOffering>>();
        vector<CourseOffering> result = offerings_;
        for (auto& offering : result) {
            offering.enrolledCount = static_cast<size_t>(count_if(
                enrollments_.begin(), enrollments_.end(), [&](const Enrollment& enrollment) {
                    return enrollment.offeringId == offering.id &&
                           enrollment.status == EnrollmentStatus::Active;
                }));
        }
        return Result<vector<CourseOffering>>::success(move(result));
    }
    Result<vector<CatalogueItem>> browseCatalogue(const CatalogueFilter& filter) const override {
        if (readError) return failed<vector<CatalogueItem>>();
        vector<CatalogueItem> items;
        for (const auto& offering : offerings_) {
            if (!filter.semester.empty() && offering.semester != filter.semester) continue;
            auto course = byId(courses_, offering.courseId);
            if (!course) continue;
            if (!filter.department.empty() && course->department != filter.department &&
                filter.department != "DEP-BUS" && filter.department != "DEP-CS") continue;
            auto offeringWithOccupancy = findOffering(offering.id).value().value();
            string instructorName = "Dr Unknown";
            auto faculty = byId(faculty_, offering.instructorId);
            if (faculty) {
                auto user = byId(users_, faculty->userId);
                if (user) instructorName = user->name;
            }
            items.push_back({*course, offeringWithOccupancy, instructorName});
        }
        sort(items.begin(), items.end(), [](const CatalogueItem& a, const CatalogueItem& b) {
            return pair{a.offering.semester, a.course.code} <
                   pair{b.offering.semester, b.course.code};
        });
        return Result<vector<CatalogueItem>>::success(move(items));
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
        return Result<vector<FacultyOfferingItem>>::success(move(values));
    }
    Result<bool> facultyTeachesCourse(FacultyId facultyId, CourseId courseId) const override {
        if (readError) return failed<bool>();
        return Result<bool>::success(any_of(
            offerings_.begin(), offerings_.end(), [&](const CourseOffering& offering) {
                return offering.instructorId == facultyId && offering.courseId == courseId;
            }));
    }
    Result<bool> courseHasReferences(CourseId id) const override {
        if (readError) return failed<bool>();
        const bool inOffering = any_of(offerings_.begin(), offerings_.end(),
                                       [&id](const CourseOffering& o) { return o.courseId == id; });
        const bool inProgram = any_of(programs_.begin(), programs_.end(),
                                      [&id](const DegreeProgram& p) {
                                          return find(p.requiredCourseIds.begin(), p.requiredCourseIds.end(), id) != p.requiredCourseIds.end();
                                      });
        return Result<bool>::success(inOffering || inProgram);
    }
    Result<void> createCourse(Course value) override { return save(courses_, move(value)); }
    Result<void> saveCourse(Course value) override { return save(courses_, move(value)); }
    Result<void> deleteCourse(CourseId id) override {
        courses_.erase(remove_if(courses_.begin(), courses_.end(),
                                 [&id](const Course& value) { return value.id == id; }),
                       courses_.end());
        return Result<void>::success();
    }
    Result<void> saveOffering(CourseOffering value) override { return save(offerings_, move(value)); }
    Result<void> updateOfferingCapacity(OfferingId id, size_t capacity) override {
        auto found = find_if(offerings_.begin(), offerings_.end(),
                             [&id](const CourseOffering& value) { return value.id == id; });
        if (found == offerings_.end()) return Result<void>::failure("RECORD_NOT_FOUND", "Missing offering.");
        found->capacity = capacity;
        return Result<void>::success();
    }

    // IEnrollmentStore
    Result<optional<Enrollment>> findEnrollment(EnrollmentId id) const override {
        if (readError) return failed<optional<Enrollment>>();
        return Result<optional<Enrollment>>::success(byId(enrollments_, id));
    }
    Result<optional<Enrollment>> findStudentEnrollment(StudentId studentId, OfferingId offeringId) const override {
        if (readError) return failed<optional<Enrollment>>();
        const auto found = find_if(enrollments_.begin(), enrollments_.end(), [&](const auto& value) {
            return value.studentId == studentId && value.offeringId == offeringId;
        });
        return Result<optional<Enrollment>>::success(
            found == enrollments_.end() ? optional<Enrollment>{} : optional<Enrollment>{*found});
    }
    Result<vector<Enrollment>> enrollments() const override {
        if (readError) return failed<vector<Enrollment>>();
        return Result<vector<Enrollment>>::success(enrollments_);
    }
    Result<vector<Enrollment>> activeEnrollmentsForStudent(StudentId studentId) const override {
        if (readError) return failed<vector<Enrollment>>();
        vector<Enrollment> values;
        copy_if(enrollments_.begin(), enrollments_.end(), back_inserter(values), [&](const auto& value) {
            return value.studentId == studentId && value.status == EnrollmentStatus::Active;
        });
        return Result<vector<Enrollment>>::success(move(values));
    }
    Result<vector<Enrollment>> scheduleEnrollmentsForStudent(StudentId studentId, const string&) const override {
        return activeEnrollmentsForStudent(studentId);
    }
    Result<vector<FacultyRosterEntry>> activeRosterForOffering(OfferingId) const override {
        return Result<vector<FacultyRosterEntry>>::success({});
    }
    Result<optional<EnrollmentOverride>> findEnrollmentOverride(EnrollmentOverrideId id) const override {
        if (readError) return failed<optional<EnrollmentOverride>>();
        return Result<optional<EnrollmentOverride>>::success(byId(overrides_, id));
    }
    Result<void> createEnrollmentOverride(EnrollmentOverride value) override { return save(overrides_, move(value)); }
    Result<void> saveEnrollment(Enrollment value) override { return save(enrollments_, move(value)); }
    Result<void> removeEnrollment(EnrollmentId id) override {
        enrollments_.erase(remove_if(enrollments_.begin(), enrollments_.end(),
                                     [&id](const Enrollment& value) { return value.id == id; }),
                           enrollments_.end());
        return Result<void>::success();
    }

    // IProgramStore
    Result<optional<DegreeProgram>> findProgram(ProgramId id) const override {
        if (readError) return failed<optional<DegreeProgram>>();
        return Result<optional<DegreeProgram>>::success(byId(programs_, id));
    }
    Result<vector<DegreeProgram>> programs() const override {
        if (readError) return failed<vector<DegreeProgram>>();
        return Result<vector<DegreeProgram>>::success(programs_);
    }
    Result<void> createProgram(DegreeProgram value) override { return save(programs_, move(value)); }
    Result<void> saveProgram(DegreeProgram value) override { return save(programs_, move(value)); }

    // IGradeStore
    Result<optional<GradeRecord>> findGradeRecord(GradeRecordId id) const override {
        if (readError) return failed<optional<GradeRecord>>();
        return Result<optional<GradeRecord>>::success(byId(grades_, id));
    }
    Result<vector<GradeRecord>> gradeRecords() const override {
        if (readError) return failed<vector<GradeRecord>>();
        return Result<vector<GradeRecord>>::success(grades_);
    }
    Result<vector<GradeRecord>> submittedGradesForStudent(StudentId studentId) const override {
        if (readError) return failed<vector<GradeRecord>>();
        vector<GradeRecord> values;
        copy_if(grades_.begin(), grades_.end(), back_inserter(values), [&](const auto& value) {
            return value.studentId == studentId && value.lifecycle == GradeLifecycle::Submitted;
        });
        return Result<vector<GradeRecord>>::success(move(values));
    }
    Result<optional<GradeRecord>> findStudentGradeRecord(StudentId studentId, OfferingId offeringId) const override {
        if (readError) return failed<optional<GradeRecord>>();
        const auto found = find_if(grades_.begin(), grades_.end(), [&](const auto& value) {
            return value.studentId == studentId && value.offeringId == offeringId;
        });
        return Result<optional<GradeRecord>>::success(
            found == grades_.end() ? optional<GradeRecord>{} : optional<GradeRecord>{*found});
    }
    Result<vector<FacultyGradeStateEntry>> gradeStateForOffering(OfferingId) const override {
        return Result<vector<FacultyGradeStateEntry>>::success({});
    }
    Result<vector<GradeRecord>> pendingGradesForOffering(OfferingId offeringId) const override {
        if (readError) return failed<vector<GradeRecord>>();
        vector<GradeRecord> values;
        copy_if(grades_.begin(), grades_.end(), back_inserter(values), [&](const auto& value) {
            return value.offeringId == offeringId && value.lifecycle == GradeLifecycle::Pending;
        });
        return Result<vector<GradeRecord>>::success(move(values));
    }
    Result<void> createGradeRecord(GradeRecord value) override { return save(grades_, move(value)); }
    Result<void> saveGradeRecord(GradeRecord value) override { return save(grades_, move(value)); }

    // IChangeRequestStore
    Result<optional<CourseChangeRequest>> findChangeRequest(ChangeRequestId id) const override {
        if (readError) return failed<optional<CourseChangeRequest>>();
        return Result<optional<CourseChangeRequest>>::success(byId(changes_, id));
    }
    Result<vector<CourseChangeRequest>> changeRequests() const override {
        if (readError) return failed<vector<CourseChangeRequest>>();
        return Result<vector<CourseChangeRequest>>::success(changes_);
    }
    Result<vector<CourseChangeRequest>> changeRequestsForFaculty(FacultyId id) const override {
        if (readError) return failed<vector<CourseChangeRequest>>();
        vector<CourseChangeRequest> values;
        copy_if(changes_.begin(), changes_.end(), back_inserter(values), [&](const auto& value) {
            return value.facultyId == id;
        });
        return Result<vector<CourseChangeRequest>>::success(move(values));
    }
    Result<vector<CourseChangeRequest>> changeRequestsByStatus(optional<CourseChangeStatus> status) const override {
        if (readError) return failed<vector<CourseChangeRequest>>();
        vector<CourseChangeRequest> values;
        copy_if(changes_.begin(), changes_.end(), back_inserter(values),
                [&status](const CourseChangeRequest& value) { return !status || value.status == *status; });
        return Result<vector<CourseChangeRequest>>::success(move(values));
    }
    Result<void> createChangeRequest(CourseChangeRequest value) override { return save(changes_, move(value)); }
    Result<void> saveChangeRequest(CourseChangeRequest value) override { return save(changes_, move(value)); }

    // IWaitlistStore
    Result<optional<WaitlistEntry>> findWaitlistEntry(WaitlistEntryId id) const override {
        if (readError) return failed<optional<WaitlistEntry>>();
        return Result<optional<WaitlistEntry>>::success(byId(waitlist_, id));
    }
    Result<optional<WaitlistEntry>> findStudentWaitlistEntry(StudentId studentId, OfferingId offeringId) const override {
        if (readError) return failed<optional<WaitlistEntry>>();
        const auto found = find_if(waitlist_.begin(), waitlist_.end(), [&](const auto& value) {
            return value.studentId == studentId && value.offeringId == offeringId;
        });
        return Result<optional<WaitlistEntry>>::success(
            found == waitlist_.end() ? optional<WaitlistEntry>{} : optional<WaitlistEntry>{*found});
    }
    Result<vector<WaitlistEntry>> waitlistEntries() const override {
        if (readError) return failed<vector<WaitlistEntry>>();
        return Result<vector<WaitlistEntry>>::success(waitlist_);
    }
    Result<vector<WaitlistEntry>> waitlistEntriesForStudent(StudentId studentId) const override {
        if (readError) return failed<vector<WaitlistEntry>>();
        vector<WaitlistEntry> values;
        copy_if(waitlist_.begin(), waitlist_.end(), back_inserter(values), [&](const auto& value) {
            return value.studentId == studentId;
        });
        return Result<vector<WaitlistEntry>>::success(move(values));
    }
    Result<vector<WaitlistEntry>> waitingEntriesForOffering(OfferingId offeringId) const override {
        if (readError) return failed<vector<WaitlistEntry>>();
        vector<WaitlistEntry> values;
        copy_if(waitlist_.begin(), waitlist_.end(), back_inserter(values), [&](const auto& value) {
            return value.offeringId == offeringId && value.status == WaitlistStatus::Waiting;
        });
        return Result<vector<WaitlistEntry>>::success(move(values));
    }
    Result<size_t> nextWaitlistPosition(OfferingId) const override {
        return Result<size_t>::success(1);
    }
    Result<void> saveWaitlistEntry(WaitlistEntry value) override { return save(waitlist_, move(value)); }
    Result<void> removeWaitlistEntry(WaitlistEntryId id) override {
        waitlist_.erase(remove_if(waitlist_.begin(), waitlist_.end(),
                                  [&id](const WaitlistEntry& value) { return value.id == id; }),
                        waitlist_.end());
        return Result<void>::success();
    }

    // ITransactionBoundary
    Result<void> executeTransaction(const Operation& operation) override {
        ++transactionCount;
        const auto userSnapshot = users_;
        const auto studentSnapshot = students_;
        const auto facultySnapshot = faculty_;
        const auto courseSnapshot = courses_;
        const auto offeringSnapshot = offerings_;
        const auto enrollmentSnapshot = enrollments_;
        const auto programSnapshot = programs_;
        const auto changeSnapshot = changes_;
        const auto overrideSnapshot = overrides_;

        auto result = operation();
        if (!result.hasValue() || failCommit) {
            users_ = userSnapshot;
            students_ = studentSnapshot;
            faculty_ = facultySnapshot;
            courses_ = courseSnapshot;
            offerings_ = offeringSnapshot;
            enrollments_ = enrollmentSnapshot;
            programs_ = programSnapshot;
            changes_ = changeSnapshot;
            overrides_ = overrideSnapshot;
            if (result.hasValue() && failCommit)
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
    vector<WaitlistEntry> waitlist_;
    vector<EnrollmentOverride> overrides_;

    optional<Error> readError;
    size_t writeCount{0};
    size_t transactionCount{0};
    size_t failWriteNumber{0};
    bool failCommit{false};

private:
    template <typename T>
    Result<T> failed() const {
        return Result<T>::failure(readError->code, readError->message);
    }

    template <typename T>
    Result<void> save(vector<T>& values, T value) {
        ++writeCount;
        if (failWriteNumber != 0 && writeCount == failWriteNumber) {
            return Result<void>::failure("TEST_WRITE_FAILED", "Configured write failed.");
        }
        auto found = find_if(values.begin(), values.end(), [&](const T& current) {
            return current.id == value.id;
        });
        if (found == values.end()) values.push_back(move(value));
        else *found = move(value);
        return Result<void>::success();
    }
};

FakeAdminContext fixture() {
    FakeAdminContext context;
    context.users_ = {
        {UserId{"U-ADM1"}, "Admin One", "admin@nexus.edu", UserStatus::Active, UserRole::Administrator},
        {UserId{"U-F1"}, "Dr One", "one@nexus.edu", UserStatus::Active, UserRole::Faculty},
        {UserId{"U-F2"}, "Dr Two", "two@nexus.edu", UserStatus::Active, UserRole::Faculty},
        {UserId{"U-S1"}, "Alice", "alice@nexus.edu", UserStatus::Active, UserRole::Student},
        {UserId{"U-S2"}, "Ben", "ben@nexus.edu", UserStatus::Active, UserRole::Student},
    };
    context.faculty_ = {
        {FacultyId{"FAC-1"}, UserId{"U-F1"}, "Computer Science"},
        {FacultyId{"FAC-2"}, UserId{"U-F2"}, "Business"},
    };
    context.students_ = {
        {StudentId{"STU-1"}, UserId{"U-S1"}, ProgramId{"P-CS"}},
        {StudentId{"STU-2"}, UserId{"U-S2"}, ProgramId{"P-BUS"}},
    };
    context.courses_ = {
        {CourseId{"C-CS101"}, "CS101", "Computer Science", "101", "Intro Programming", "Basics", 3, {}},
        {CourseId{"C-CS201"}, "CS201", "Computer Science", "201", "Data Structures", "Advanced", 4, {CourseId{"C-CS101"}}},
        {CourseId{"C-BUS101"}, "BUS101", "Business", "101", "Intro Business", "Principles", 3, {}},
    };
    ScheduleSlot slot1 = ScheduleSlot::create(DayOfWeek::Monday, 540, 630, "Room 101").value();
    ScheduleSlot slot2 = ScheduleSlot::create(DayOfWeek::Tuesday, 660, 750, "Room 102").value();
    ScheduleSlot slot3 = ScheduleSlot::create(DayOfWeek::Wednesday, 840, 930, "Room 103").value();
    context.offerings_ = {
        {OfferingId{"O-CS101-S1"}, CourseId{"C-CS101"}, "2026S1", FacultyId{"FAC-1"}, 30, 0, {slot1}},
        {OfferingId{"O-CS201-S1"}, CourseId{"C-CS201"}, "2026S1", FacultyId{"FAC-1"}, 1, 0, {slot2}},
        {OfferingId{"OFFER-BUS101-2026S1"}, CourseId{"C-BUS101"}, "2026S1", FacultyId{"FAC-2"}, 2, 0, {slot3}},
    };
    context.enrollments_ = {
        {EnrollmentId{"E-BUS101-1"}, StudentId{"STU-1"}, OfferingId{"OFFER-BUS101-2026S1"}, EnrollmentStatus::Active},
        {EnrollmentId{"E-BUS101-2"}, StudentId{"STU-2"}, OfferingId{"OFFER-BUS101-2026S1"}, EnrollmentStatus::Active},
    };
    context.programs_ = {
        {ProgramId{"P-CS"}, "Computer Science BSc", "Computer Science", {CourseId{"C-CS101"}}, 120},
        {ProgramId{"P-BUS"}, "Business Admin BBA", "Business", {CourseId{"C-BUS101"}}, 120},
    };
    context.changes_ = {
        {ChangeRequestId{"CR-1"}, FacultyId{"FAC-1"}, CourseId{"C-CS101"}, nullopt, CourseChangeType::Description, CourseChangeStatus::Pending, "Updated CS101 description"},
    };
    return context;
}

void testAdministratorQueries() {
    auto context = fixture();
    GetAdministratorCoursesQuery coursesQuery(context);
    auto coursesRes = coursesQuery.execute();
    require(coursesRes.hasValue() && coursesRes.value().size() == 3, "Courses query should return 3 courses");

    GetAdministratorProgramsQuery programsQuery(context);
    auto programsRes = programsQuery.execute();
    require(programsRes.hasValue() && programsRes.value().size() == 2, "Programs query should return 2 programs");

    GetAdministratorUsersQuery usersQuery(UserRole::Student, context);
    auto usersRes = usersQuery.execute();
    require(usersRes.hasValue() && usersRes.value().size() == 2, "Filtered users query should return 2 students");

    GetAdministratorCourseChangesQuery changesQuery(CourseChangeStatus::Pending, context);
    auto changesRes = changesQuery.execute();
    require(changesRes.hasValue() && changesRes.value().size() == 1, "Changes query should return 1 pending change");

    context.readError = Error{"DB_READ_ERROR", "Read failure"};
    auto failedCourses = coursesQuery.execute();
    require(!failedCourses.hasValue() && failedCourses.error().code == "DB_READ_ERROR", "Empty result distinct from storage failure");
}

void testCourseManagement() {
    auto context = fixture();
    CourseInput input{CourseId{"C-NEW"}, "CS301", "Computer Science", "301", "Algorithms", "Advanced Algo", 4, {CourseId{"C-CS101"}}};
    CreateCourseCommand createCmd(input, context, context);
    require(createCmd.execute().hasValue(), "Course creation should succeed");
    require(context.courses_.size() == 4, "Course count should be 4");

    // Collision rejection
    CreateCourseCommand dupCmd(input, context, context);
    require(!dupCmd.execute().hasValue(), "Duplicate course ID creation should fail");

    // Partial update
    CoursePatch patch;
    patch.description = "Updated description";
    UpdateCourseCommand updateCmd(CourseId{"C-NEW"}, patch, context, context);
    require(updateCmd.execute().hasValue(), "Course update should succeed");
    auto updatedCourse = byId(context.courses_, CourseId{"C-NEW"});
    require(updatedCourse && updatedCourse->description == "Updated description" && updatedCourse->code == "CS301",
            "Partial update preserves omitted fields");

    // Prerequisite self-reference rejection
    CoursePatch cyclePatch;
    cyclePatch.prerequisiteCourseIds = vector<CourseId>{CourseId{"C-NEW"}};
    UpdateCourseCommand cycleCmd(CourseId{"C-NEW"}, cyclePatch, context, context);
    require(!cycleCmd.execute().hasValue(), "Self prerequisite should be rejected");

    // Delete unreferenced course
    DeleteCourseCommand deleteCmd(CourseId{"C-NEW"}, context, context);
    require(deleteCmd.execute().hasValue(), "Deleting unreferenced course should succeed");

    // Delete referenced course rejection
    DeleteCourseCommand deleteRefCmd(CourseId{"C-CS101"}, context, context);
    require(!deleteRefCmd.execute().hasValue(), "Deleting referenced course should be rejected");
}

void testProgramManagement() {
    auto context = fixture();
    ProgramInput input{ProgramId{"P-NEW"}, "AI BSc", "Computer Science", 120, {CourseId{"C-CS101"}}};
    CreateProgramCommand createCmd(input, context, context, context);
    require(createCmd.execute().hasValue(), "Program creation should succeed");

    ProgramPatch patch;
    patch.name = "Artificial Intelligence BSc";
    UpdateProgramCommand updateCmd(ProgramId{"P-NEW"}, patch, context, context, context);
    require(updateCmd.execute().hasValue(), "Program update should succeed");
    auto updatedProg = byId(context.programs_, ProgramId{"P-NEW"});
    require(updatedProg && updatedProg->name == "Artificial Intelligence BSc" && updatedProg->department == "Computer Science",
            "Omitted fields remain unchanged");
}

void testAccountManagement() {
    auto context = fixture();
    AccountInput studentInput{UserId{"U-SNEW"}, "STU-NEW", UserRole::Student, "Charlie", "charlie@nexus.edu", ProgramId{"P-CS"}, nullopt};
    CreateAccountCommand createStudentCmd(studentInput, context, context, context);
    require(createStudentCmd.execute().hasValue(), "Student account creation should succeed");
    require(byId(context.users_, UserId{"U-SNEW"}) && byId(context.students_, StudentId{"STU-NEW"}),
            "Student user and profile created atomically");

    AccountInput facultyInput{UserId{"U-FNEW"}, "FAC-NEW", UserRole::Faculty, "Dr Three", "three@nexus.edu", nullopt, "Computer Science"};
    CreateAccountCommand createFacultyCmd(facultyInput, context, context, context);
    require(createFacultyCmd.execute().hasValue(), "Faculty account creation should succeed");
    require(byId(context.users_, UserId{"U-FNEW"}) && byId(context.faculty_, FacultyId{"FAC-NEW"}),
            "Faculty user and profile created atomically");

    // Unsupported Admin creation via account command
    AccountInput adminInput{UserId{"U-ANEW"}, "ADM-NEW", UserRole::Administrator, "Admin Two", "adm2@nexus.edu", nullopt, nullopt};
    CreateAccountCommand createAdminCmd(adminInput, context, context, context);
    require(!createAdminCmd.execute().hasValue(), "Administrator creation via Account command should be rejected");

    // Edit account
    AccountPatch patch;
    patch.name = "Charlie Brown";
    EditAccountCommand editCmd(UserId{"U-SNEW"}, patch, context, context, context);
    require(editCmd.execute().hasValue(), "Account edit should succeed");
    require(byId(context.users_, UserId{"U-SNEW"})->name == "Charlie Brown", "User name updated");

    // Deactivate student
    DeactivateUserCommand deactCmd(UserId{"U-SNEW"}, context, context);
    require(deactCmd.execute().hasValue(), "User deactivation should succeed");
    require(byId(context.users_, UserId{"U-SNEW"})->status == UserStatus::Inactive, "User status is Inactive");

    // Deactivate admin rejection
    DeactivateUserCommand deactAdminCmd(UserId{"U-ADM1"}, context, context);
    require(!deactAdminCmd.execute().hasValue(), "Deactivating Administrator should be rejected");
}

void testEnrollmentOverride() {
    auto context = fixture();
    // STU-2 completed C-CS101 (prerequisite for C-CS201)
    context.grades_.push_back({GradeRecordId{"G-1"}, StudentId{"STU-2"}, OfferingId{"O-CS101-S1"}, CourseId{"C-CS101"}, "A", GradeLifecycle::Submitted});

    // offering O-CS201-S1 capacity is 1. Add student STU-1 to fill it.
    context.enrollments_.push_back({EnrollmentId{"E-CS201-1"}, StudentId{"STU-1"}, OfferingId{"O-CS201-S1"}, EnrollmentStatus::Active});

    // Now offering is full (1/1). Admin forces STU-2 into full offering with Capacity rule override.
    EnrollmentOverrideInput overrideInput{
        UserId{"U-ADM1"}, StudentId{"STU-2"}, OfferingId{"O-CS201-S1"}, EnrollmentRule::Capacity, "Admin override capacity full"};
    OverrideEnrollmentCommand overrideCmd(overrideInput, context, context, context, context, context, context);
    require(overrideCmd.execute().hasValue(), "Enrollment override should succeed");
    require(!overrideCmd.enrollmentId().empty() && !overrideCmd.overrideId().empty(), "IDs generated");
    require(context.overrides_.size() == 1, "Override record persisted");

    // Non-admin actor rejection
    EnrollmentOverrideInput nonAdminInput{
        UserId{"U-S1"}, StudentId{"STU-2"}, OfferingId{"O-CS201-S1"}, EnrollmentRule::Capacity, "Non admin override"};
    OverrideEnrollmentCommand nonAdminCmd(nonAdminInput, context, context, context, context, context, context);
    require(!nonAdminCmd.execute().hasValue(), "Non-admin actor should be rejected");
}

void testCourseChangeDecisions() {
    auto context = fixture();
    // Approve description change CR-1
    ApproveCourseChangeCommand approveCmd(ChangeRequestId{"CR-1"}, context, context, context);
    require(approveCmd.execute().hasValue(), "Approve course change should succeed");
    require(byId(context.changes_, ChangeRequestId{"CR-1"})->status == CourseChangeStatus::Approved, "Status is Approved");
    require(byId(context.courses_, CourseId{"C-CS101"})->description == "Updated CS101 description", "Course description updated");

    // Reject already resolved request
    RejectCourseChangeCommand rejectCmd(ChangeRequestId{"CR-1"}, context, context);
    require(!rejectCmd.execute().hasValue(), "Re-resolving already resolved request should be rejected");
}

void testReportQueries() {
    auto context = fixture();

    // 1. Enrollment report
    GetEnrollmentReportQuery enrolQuery(nullopt, string("2026S1"), context);
    auto enrolRes = enrolQuery.execute();
    require(enrolRes.hasValue() && !enrolRes.value().empty(), "Enrollment report query should return items");

    // 2. Faculty workload report
    GetFacultyWorkloadReportQuery workloadQuery(string("2026S1"), context, context);
    auto workloadRes = workloadQuery.execute();
    require(workloadRes.hasValue() && !workloadRes.value().empty(), "Faculty workload report query should return items");

    // 3. Course popularity report
    GetCoursePopularityReportQuery popQuery(string("2026S1"), context);
    auto popRes = popQuery.execute();
    require(popRes.hasValue() && !popRes.value().empty(), "Course popularity report query should return items");

    // 4. Capacity report with minUtilization = 0.90 for Business department
    GetCapacityReportQuery capQuery(string("Business"), string("2026S1"), 0.90, context);
    auto capRes = capQuery.execute();
    require(capRes.hasValue() && capRes.value().size() == 1, "Capacity report query should return 1 item (OFFER-BUS101-2026S1 at 100%)");
    require(capRes.value()[0].offering.id == OfferingId{"OFFER-BUS101-2026S1"}, "Returned offering is OFFER-BUS101-2026S1");
    require(capRes.value()[0].utilizationRate == 1.0, "Utilization rate is 1.0 (100%)");

    // Verify broader filter
    GetCapacityReportQuery capBroaderQuery(nullopt, nullopt, 0.0, context);
    auto capBroaderRes = capBroaderQuery.execute();
    require(capBroaderRes.hasValue() && capBroaderRes.value().size() == 3, "Broader filter should return all 3 offerings");

    require(context.writeCount == 0 && context.transactionCount == 0, "Report queries perform no writes or transactions");
}

} // namespace

int main() {
    const vector<pair<string, function<void()>>> tests{
        {"Administrator CQRS queries", testAdministratorQueries},
        {"Course management commands", testCourseManagement},
        {"Programme management commands", testProgramManagement},
        {"Account management commands", testAccountManagement},
        {"Enrolment override command", testEnrollmentOverride},
        {"Course change decision commands", testCourseChangeDecisions},
        {"Administrator report queries", testReportQueries},
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
    cout << passed << '/' << tests.size() << " Administrator Business groups passed\n";
    return passed == tests.size() ? 0 : 1;
}
