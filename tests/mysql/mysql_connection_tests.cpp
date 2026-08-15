#include "nexusenroll/data/mysql/mysql_data_context.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace nexusenroll;
using namespace business::domain;
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

}

int main() {
    const vector<pair<string, function<void()>>> tests{
        {"seeded contracts", testSeededContracts},
        {"stable reads and referential integrity", testStableReadsAndReferentialIntegrity},
        {"transaction commit and rollback", testTransactionCommitAndRollback},
        {"concurrent capacity locking", testConcurrentCapacityLocking},
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
