#include "nexusenroll/business/cqrs/commands/command.hpp"
#include "nexusenroll/business/sessions/demonstration_session_service.hpp"
#include "nexusenroll/business/sessions/session_creator.hpp"

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
using namespace business::domain;
using namespace business::sessions;
using namespace common;
using namespace data::contracts;
using namespace std;

void require(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

class TestCommand final : public ICommand {
public:
    explicit TestCommand(bool succeeds) : succeeds_(succeeds) {}

    CommandResult execute() override {
        ++executions_;
        if (succeeds_) {
            return CommandResult::success();
        }
        return CommandResult::failure("TEST_COMMAND_FAILED", "The test command failed.");
    }

    int executions() const noexcept { return executions_; }

private:
    bool succeeds_;
    int executions_{0};
};

class FakeUserStore final : public IUserStore {
public:
    Result<optional<User>> findUser(UserId id) const override {
        if (readError_) {
            return Result<optional<User>>::failure(readError_->code, readError_->message);
        }
        const auto found = find_if(
            userValues.begin(), userValues.end(),
            [&id](const User& user) { return user.id == id; });
        return Result<optional<User>>::success(
            found == userValues.end() ? optional<User>{} : optional<User>{*found});
    }

    Result<optional<Student>> findStudent(StudentId id) const override {
        const auto found = find_if(
            studentValues.begin(), studentValues.end(),
            [&id](const Student& student) { return student.id == id; });
        return Result<optional<Student>>::success(
            found == studentValues.end() ? optional<Student>{} : optional<Student>{*found});
    }

    Result<optional<Faculty>> findFaculty(FacultyId id) const override {
        const auto found = find_if(
            facultyValues.begin(), facultyValues.end(),
            [&id](const Faculty& faculty) { return faculty.id == id; });
        return Result<optional<Faculty>>::success(
            found == facultyValues.end() ? optional<Faculty>{} : optional<Faculty>{*found});
    }

    Result<optional<Student>> findStudentByUserId(UserId userId) const override {
        if (readError_) {
            return Result<optional<Student>>::failure(readError_->code, readError_->message);
        }
        if (mismatchedStudent_ && !studentValues.empty()) {
            return Result<optional<Student>>::success(studentValues.front());
        }
        const auto found = find_if(
            studentValues.begin(), studentValues.end(),
            [&userId](const Student& student) { return student.userId == userId; });
        return Result<optional<Student>>::success(
            found == studentValues.end() ? optional<Student>{} : optional<Student>{*found});
    }

    Result<optional<Faculty>> findFacultyByUserId(UserId userId) const override {
        if (readError_) {
            return Result<optional<Faculty>>::failure(readError_->code, readError_->message);
        }
        const auto found = find_if(
            facultyValues.begin(), facultyValues.end(),
            [&userId](const Faculty& faculty) { return faculty.userId == userId; });
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

    Result<void> saveUser(User) override {
        ++writeCount;
        return Result<void>::success();
    }

    Result<void> saveStudent(Student) override {
        ++writeCount;
        return Result<void>::success();
    }

    Result<void> saveFaculty(Faculty) override {
        ++writeCount;
        return Result<void>::success();
    }

    vector<User> userValues;
    vector<Student> studentValues;
    vector<Faculty> facultyValues;
    optional<Error> readError_;
    bool mismatchedStudent_{false};
    size_t writeCount{0};
};

FakeUserStore seededStore() {
    FakeUserStore store;
    store.userValues = {
        {UserId{"U-STU"}, "Student Name", "student@nexus.edu", UserStatus::Active,
         UserRole::Student},
        {UserId{"U-FAC"}, "Faculty Name", "faculty@nexus.edu", UserStatus::Active,
         UserRole::Faculty},
        {UserId{"U-ADM"}, "Administrator Name", "admin@nexus.edu", UserStatus::Active,
         UserRole::Administrator},
        {UserId{"U-INACTIVE"}, "Inactive Name", "inactive@nexus.edu", UserStatus::Inactive,
         UserRole::Student},
    };
    store.studentValues = {
        {StudentId{"STU-001"}, UserId{"U-STU"}, ProgramId{"PROGRAM-CS"}},
        {StudentId{"STU-INACTIVE"}, UserId{"U-INACTIVE"}, ProgramId{"PROGRAM-CS"}},
    };
    store.facultyValues = {
        {FacultyId{"FAC-001"}, UserId{"U-FAC"}, "Computer Science"},
    };
    return store;
}

void testCommandPolymorphism() {
    unique_ptr<ICommand> success = make_unique<TestCommand>(true);
    auto successResult = success->execute();
    require(static_cast<bool>(successResult), "A successful command should propagate success");

    unique_ptr<ICommand> failure = make_unique<TestCommand>(false);
    auto failureResult = failure->execute();
    require(!failureResult && failureResult.error().code == "TEST_COMMAND_FAILED" &&
                failureResult.error().message == "The test command failed.",
            "A command error should propagate through ICommand");
}

void testFactoryMethodProducts() {
    vector<unique_ptr<SessionCreator>> creators;
    creators.push_back(make_unique<StudentSessionCreator>(StudentId{"STU-001"}));
    creators.push_back(make_unique<FacultySessionCreator>(FacultyId{"FAC-001"}));
    creators.push_back(make_unique<AdministratorSessionCreator>());

    auto studentProduct = creators[0]->createSession(UserId{"U-STU"}, "Student Name");
    auto facultyProduct = creators[1]->createSession(UserId{"U-FAC"}, "Faculty Name");
    auto administratorProduct = creators[2]->createSession(UserId{"U-ADM"}, "Administrator Name");

    const auto* student = dynamic_cast<const StudentSession*>(studentProduct.get());
    const auto* faculty = dynamic_cast<const FacultySession*>(facultyProduct.get());
    const auto* administrator = dynamic_cast<const AdministratorSession*>(administratorProduct.get());
    require(student && student->role() == UserRole::Student &&
                student->studentId() == StudentId{"STU-001"},
            "The Student creator should construct the Student product");
    require(faculty && faculty->role() == UserRole::Faculty &&
                faculty->facultyId() == FacultyId{"FAC-001"},
            "The Faculty creator should construct the Faculty product");
    require(administrator && administrator->role() == UserRole::Administrator &&
                administrator->userId() == UserId{"U-ADM"},
            "The Administrator creator should use the User as its complete identity");
}

void testActiveRoleSessionsAndNoWrites() {
    auto store = seededStore();
    DemonstrationSessionService service(store);

    auto studentResult = service.create(UserId{"U-STU"});
    auto facultyResult = service.create(UserId{"U-FAC"});
    auto administratorResult = service.create(UserId{"U-ADM"});

    require(studentResult && studentResult.value()->displayName() == "Student Name",
            "The Student session should retain stable User data");
    const auto* student = dynamic_cast<const StudentSession*>(studentResult.value().get());
    require(student && student->studentId() == StudentId{"STU-001"},
            "The Student session should retain its profile ID");
    const auto* faculty = dynamic_cast<const FacultySession*>(facultyResult.value().get());
    require(faculty && faculty->facultyId() == FacultyId{"FAC-001"},
            "The Faculty session should retain its profile ID");
    require(administratorResult &&
                dynamic_cast<const AdministratorSession*>(administratorResult.value().get()),
            "An active Administrator User should create an Administrator session");
    require(store.writeCount == 0, "Demonstration session creation must not write to the store");
}

void testInactiveAndMissingUsers() {
    auto store = seededStore();
    DemonstrationSessionService service(store);

    const auto inactive = service.create(UserId{"U-INACTIVE"});
    const auto missing = service.create(UserId{"U-MISSING"});
    require(!inactive && inactive.error().code == "USER_INACTIVE",
            "An inactive user should be rejected distinctly");
    require(!missing && missing.error().code == "SESSION_USER_NOT_FOUND",
            "A missing user should be rejected distinctly");
}

void testMissingAndMismatchedProfiles() {
    auto missingStore = seededStore();
    missingStore.studentValues.clear();
    DemonstrationSessionService missingService(missingStore);
    const auto missing = missingService.create(UserId{"U-STU"});
    require(!missing && missing.error().code == "SESSION_PROFILE_NOT_FOUND",
            "A missing role profile should be rejected");

    auto mismatchedStore = seededStore();
    mismatchedStore.studentValues.front().userId = UserId{"U-OTHER"};
    mismatchedStore.mismatchedStudent_ = true;
    DemonstrationSessionService mismatchedService(mismatchedStore);
    const auto mismatched = mismatchedService.create(UserId{"U-STU"});
    require(!mismatched && mismatched.error().code == "SESSION_PROFILE_MISMATCH",
            "A mismatched role profile should be rejected");
}

void testStorageErrorsRemainDistinct() {
    auto store = seededStore();
    store.readError_ = Error{"MYSQL_TEST_FAILURE", "Database unavailable"};
    DemonstrationSessionService service(store);

    const auto result = service.create(UserId{"U-STU"});
    require(!result && result.error().code == "MYSQL_TEST_FAILURE" &&
                result.error().message == "Database unavailable",
            "Storage failures must not be converted into normal absence");
}

}

int main() {
    const vector<pair<string, function<void()>>> tests{
        {"ICommand polymorphism", testCommandPolymorphism},
        {"Factory Method products", testFactoryMethodProducts},
        {"active role sessions and no writes", testActiveRoleSessionsAndNoWrites},
        {"inactive and missing users", testInactiveAndMissingUsers},
        {"missing and mismatched profiles", testMissingAndMismatchedProfiles},
        {"storage errors remain distinct", testStorageErrorsRemainDistinct},
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

    cout << passed << '/' << tests.size() << " Business tests passed\n";
    return passed == tests.size() ? 0 : 1;
}
