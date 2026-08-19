#include "nexusenroll/business/cqrs/commands/administrator_commands.hpp"

#include "nexusenroll/business/cqrs/persistent_id.hpp"
#include "nexusenroll/business/cqrs/student_validation.hpp"
#include "nexusenroll/business/domain/schedule.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace nexusenroll::business::cqrs::commands {

using namespace common;
using namespace data::contracts;
using namespace domain;
using namespace std;

namespace {

constexpr int64_t maximumSmallUnsigned = 65535;

CommandResult failure(const Error& error) {
    return CommandResult::failure(error.code, error.message);
}

bool boundedText(const string& value, size_t maximum) {
    return !value.empty() && value.size() <= maximum &&
           any_of(value.begin(), value.end(), [](unsigned char character) {
               return !isspace(character);
           });
}

CommandResult persistenceConflict(
    const Result<void>& result,
    const char* code,
    const char* message) {
    if (!result && result.error().code == "PERSISTENCE_ID_CONFLICT") {
        return CommandResult::failure(code, message);
    }
    return result;
}

bool pathReaches(
    const CourseId& current,
    const CourseId& target,
    const map<CourseId, vector<CourseId>>& graph,
    set<CourseId>& visited) {
    if (current == target) {
        return true;
    }
    if (!visited.insert(current).second) {
        return false;
    }
    const auto found = graph.find(current);
    if (found == graph.end()) {
        return false;
    }
    for (const auto& next : found->second) {
        if (pathReaches(next, target, graph, visited)) {
            return true;
        }
    }
    return false;
}

CommandResult validatePrerequisites(
    CourseId courseId,
    const vector<CourseId>& prerequisites,
    const ICourseStore& courseStore) {
    set<CourseId> unique;
    for (const auto& prerequisite : prerequisites) {
        if (prerequisite.empty() || prerequisite.value().size() > 32) {
            return CommandResult::failure(
                "INVALID_PREREQUISITE", "A prerequisite requires a bounded Course ID.");
        }
        if (prerequisite == courseId) {
            return CommandResult::failure(
                "SELF_PREREQUISITE", "A Course cannot be its own prerequisite.");
        }
        if (!unique.insert(prerequisite).second) {
            return CommandResult::failure(
                "DUPLICATE_PREREQUISITE", "A prerequisite may appear only once.");
        }
    }

    auto courses = courseStore.courses();
    if (!courses) {
        return failure(courses.error());
    }
    map<CourseId, vector<CourseId>> graph;
    set<CourseId> existing;
    for (const auto& course : courses.value()) {
        existing.insert(course.id);
        graph[course.id] = course.prerequisiteCourseIds;
    }
    for (const auto& prerequisite : prerequisites) {
        if (existing.count(prerequisite) == 0) {
            return CommandResult::failure(
                "PREREQUISITE_NOT_FOUND", "A proposed prerequisite Course does not exist.");
        }
    }
    graph[courseId] = prerequisites;
    for (const auto& prerequisite : prerequisites) {
        set<CourseId> visited;
        if (pathReaches(prerequisite, courseId, graph, visited)) {
            return CommandResult::failure(
                "PREREQUISITE_CYCLE", "The proposed prerequisites introduce a cycle.");
        }
    }
    return CommandResult::success();
}

CommandResult validateCourse(const Course& course, const ICourseStore& courseStore) {
    if (!boundedText(course.id.value(), 32) || !boundedText(course.code, 24) ||
        !boundedText(course.department, 120) || !boundedText(course.courseNumber, 20) ||
        !boundedText(course.name, 160) || !boundedText(course.description, 4000) ||
        course.credits == 0 || course.credits > maximumSmallUnsigned) {
        return CommandResult::failure(
            "INVALID_COURSE", "Course fields are missing or exceed their supported bounds.");
    }
    auto department = courseStore.departmentExists(course.department);
    if (!department) {
        return failure(department.error());
    }
    if (!department.value()) {
        return CommandResult::failure(
            "DEPARTMENT_NOT_FOUND", "The selected Course department does not exist.");
    }
    return validatePrerequisites(course.id, course.prerequisiteCourseIds, courseStore);
}

CommandResult validateRequiredCourses(
    const vector<CourseId>& courseIds,
    const ICourseStore& courseStore) {
    set<CourseId> unique;
    for (const auto& courseId : courseIds) {
        if (courseId.empty() || courseId.value().size() > 32) {
            return CommandResult::failure(
                "INVALID_REQUIRED_COURSE", "A requirement needs a bounded Course ID.");
        }
        if (!unique.insert(courseId).second) {
            return CommandResult::failure(
                "DUPLICATE_REQUIRED_COURSE", "A required Course may appear only once.");
        }
        auto course = courseStore.findCourse(courseId);
        if (!course) {
            return failure(course.error());
        }
        if (!course.value()) {
            return CommandResult::failure(
                "REQUIRED_COURSE_NOT_FOUND", "A required Course does not exist.");
        }
    }
    return CommandResult::success();
}

CommandResult validateProgram(
    const DegreeProgram& program,
    const ICourseStore& courseStore) {
    if (!boundedText(program.id.value(), 32) || !boundedText(program.name, 160) ||
        !boundedText(program.department, 120) || program.requiredCredits == 0 ||
        program.requiredCredits > maximumSmallUnsigned) {
        return CommandResult::failure(
            "INVALID_PROGRAM", "Programme fields are missing or exceed their supported bounds.");
    }
    auto department = courseStore.departmentExists(program.department);
    if (!department) {
        return failure(department.error());
    }
    if (!department.value()) {
        return CommandResult::failure(
            "DEPARTMENT_NOT_FOUND", "The selected Programme department does not exist.");
    }
    return validateRequiredCourses(program.requiredCourseIds, courseStore);
}

Result<User> activeAdministrator(UserId userId, const IUserStore& userStore) {
    auto user = userStore.findUser(userId);
    if (!user) {
        return Result<User>::failure(user.error().code, user.error().message);
    }
    if (!user.value()) {
        return Result<User>::failure(
            "ADMINISTRATOR_NOT_FOUND", "The override Administrator User does not exist.");
    }
    if (user.value()->role != UserRole::Administrator) {
        return Result<User>::failure(
            "ADMINISTRATOR_ROLE_REQUIRED", "The override actor is not an Administrator.");
    }
    if (user.value()->status != UserStatus::Active) {
        return Result<User>::failure(
            "ADMINISTRATOR_INACTIVE", "The override Administrator is inactive.");
    }
    return Result<User>::success(*user.value());
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

Result<vector<CourseId>> parsePrerequisiteValue(const string& value) {
    vector<CourseId> values;
    if (value.empty()) {
        return Result<vector<CourseId>>::success(move(values));
    }
    istringstream stream(value);
    string item;
    while (getline(stream, item, ',')) {
        if (item.empty()) {
            return Result<vector<CourseId>>::failure(
                "INVALID_CHANGE_VALUE", "The stored prerequisite value is malformed.");
        }
        values.emplace_back(move(item));
    }
    if (!value.empty() && value.back() == ',') {
        return Result<vector<CourseId>>::failure(
            "INVALID_CHANGE_VALUE", "The stored prerequisite value is malformed.");
    }
    return Result<vector<CourseId>>::success(move(values));
}

Result<size_t> parseCapacityValue(const string& value) {
    if (value.empty() || !all_of(value.begin(), value.end(), [](unsigned char character) {
            return isdigit(character);
        })) {
        return Result<size_t>::failure(
            "INVALID_CHANGE_VALUE", "The stored capacity value is not a positive integer.");
    }
    try {
        const unsigned long long parsed = stoull(value);
        if (parsed == 0 || parsed > static_cast<unsigned long long>(maximumSmallUnsigned)) {
            throw out_of_range("capacity");
        }
        return Result<size_t>::success(static_cast<size_t>(parsed));
    } catch (const exception&) {
        return Result<size_t>::failure(
            "INVALID_CHANGE_VALUE", "The stored capacity value is outside supported bounds.");
    }
}

CommandResult pendingRequest(
    ChangeRequestId id,
    IChangeRequestStore& store,
    CourseChangeRequest& value) {
    auto request = store.findChangeRequest(id);
    if (!request) {
        return failure(request.error());
    }
    if (!request.value()) {
        return CommandResult::failure(
            "CHANGE_REQUEST_NOT_FOUND", "The Course-change request does not exist.");
    }
    if (request.value()->status != CourseChangeStatus::Pending) {
        return CommandResult::failure(
            "CHANGE_REQUEST_ALREADY_RESOLVED", "The Course-change request is already resolved.");
    }
    value = *request.value();
    return CommandResult::success();
}

}

CreateCourseCommand::CreateCourseCommand(
    CourseInput input,
    ICourseStore& courseStore,
    ITransactionBoundary& transactionBoundary)
    : input_(move(input)), courseStore_(courseStore), transactionBoundary_(transactionBoundary) {}

CommandResult CreateCourseCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        Course course{input_.id, input_.code, input_.department, input_.courseNumber,
                      input_.name, input_.description, 0, input_.prerequisiteCourseIds};
        if (input_.credits > 0 && input_.credits <= maximumSmallUnsigned) {
            course.credits = static_cast<unsigned int>(input_.credits);
        }
        auto valid = validateCourse(course, courseStore_);
        if (!valid) {
            return valid;
        }
        auto existing = courseStore_.findCourse(course.id);
        if (!existing) {
            return failure(existing.error());
        }
        if (existing.value()) {
            return CommandResult::failure(
                "COURSE_IDENTITY_CONFLICT", "The Course ID already exists.");
        }
        return persistenceConflict(
            courseStore_.createCourse(move(course)), "COURSE_IDENTITY_CONFLICT",
            "The Course ID, code, or department/course-number identity already exists.");
    });
}

UpdateCourseCommand::UpdateCourseCommand(
    CourseId courseId,
    CoursePatch patch,
    ICourseStore& courseStore,
    ITransactionBoundary& transactionBoundary)
    : courseId_(move(courseId)), patch_(move(patch)), courseStore_(courseStore),
      transactionBoundary_(transactionBoundary) {}

CommandResult UpdateCourseCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        auto existing = courseStore_.findCourse(courseId_);
        if (!existing) return failure(existing.error());
        if (!existing.value()) {
            return CommandResult::failure("COURSE_NOT_FOUND", "The Course does not exist.");
        }
        Course course = *existing.value();
        if (patch_.code) course.code = *patch_.code;
        if (patch_.department) course.department = *patch_.department;
        if (patch_.courseNumber) course.courseNumber = *patch_.courseNumber;
        if (patch_.name) course.name = *patch_.name;
        if (patch_.description) course.description = *patch_.description;
        if (patch_.credits) {
            course.credits = *patch_.credits > 0 && *patch_.credits <= maximumSmallUnsigned
                                 ? static_cast<unsigned int>(*patch_.credits)
                                 : 0;
        }
        if (patch_.prerequisiteCourseIds) {
            course.prerequisiteCourseIds = *patch_.prerequisiteCourseIds;
        }
        auto valid = validateCourse(course, courseStore_);
        if (!valid) return valid;
        return persistenceConflict(
            courseStore_.saveCourse(move(course)), "COURSE_IDENTITY_CONFLICT",
            "The Course code or department/course-number identity already exists.");
    });
}

DeleteCourseCommand::DeleteCourseCommand(
    CourseId courseId,
    ICourseStore& courseStore,
    ITransactionBoundary& transactionBoundary)
    : courseId_(move(courseId)), courseStore_(courseStore),
      transactionBoundary_(transactionBoundary) {}

CommandResult DeleteCourseCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        auto course = courseStore_.findCourse(courseId_);
        if (!course) return failure(course.error());
        if (!course.value()) {
            return CommandResult::failure("COURSE_NOT_FOUND", "The Course does not exist.");
        }
        auto referenced = courseStore_.courseHasReferences(courseId_);
        if (!referenced) return failure(referenced.error());
        if (referenced.value()) {
            return CommandResult::failure(
                "COURSE_IN_USE", "The Course is referenced by persisted university data.");
        }
        return courseStore_.deleteCourse(courseId_);
    });
}

CreateProgramCommand::CreateProgramCommand(
    ProgramInput input,
    IProgramStore& programStore,
    const ICourseStore& courseStore,
    ITransactionBoundary& transactionBoundary)
    : input_(move(input)), programStore_(programStore), courseStore_(courseStore),
      transactionBoundary_(transactionBoundary) {}

CommandResult CreateProgramCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        DegreeProgram program{input_.id, input_.name, input_.department,
                              input_.requiredCourseIds, 0};
        if (input_.requiredCredits > 0 && input_.requiredCredits <= maximumSmallUnsigned) {
            program.requiredCredits = static_cast<unsigned int>(input_.requiredCredits);
        }
        auto valid = validateProgram(program, courseStore_);
        if (!valid) return valid;
        auto existing = programStore_.findProgram(program.id);
        if (!existing) return failure(existing.error());
        if (existing.value()) {
            return CommandResult::failure(
                "PROGRAM_IDENTITY_CONFLICT", "The Programme ID already exists.");
        }
        return persistenceConflict(
            programStore_.createProgram(move(program)), "PROGRAM_IDENTITY_CONFLICT",
            "The Programme ID or name already exists.");
    });
}

UpdateProgramCommand::UpdateProgramCommand(
    ProgramId programId,
    ProgramPatch patch,
    IProgramStore& programStore,
    const ICourseStore& courseStore,
    ITransactionBoundary& transactionBoundary)
    : programId_(move(programId)), patch_(move(patch)), programStore_(programStore),
      courseStore_(courseStore), transactionBoundary_(transactionBoundary) {}

CommandResult UpdateProgramCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        auto existing = programStore_.findProgram(programId_);
        if (!existing) return failure(existing.error());
        if (!existing.value()) {
            return CommandResult::failure("PROGRAM_NOT_FOUND", "The Programme does not exist.");
        }
        DegreeProgram program = *existing.value();
        if (patch_.name) program.name = *patch_.name;
        if (patch_.department) program.department = *patch_.department;
        if (patch_.requiredCredits) {
            program.requiredCredits =
                *patch_.requiredCredits > 0 && *patch_.requiredCredits <= maximumSmallUnsigned
                    ? static_cast<unsigned int>(*patch_.requiredCredits)
                    : 0;
        }
        if (patch_.requiredCourseIds) program.requiredCourseIds = *patch_.requiredCourseIds;
        auto valid = validateProgram(program, courseStore_);
        if (!valid) return valid;
        return persistenceConflict(
            programStore_.saveProgram(move(program)), "PROGRAM_IDENTITY_CONFLICT",
            "The Programme name already exists.");
    });
}

CreateAccountCommand::CreateAccountCommand(
    AccountInput input,
    IUserStore& userStore,
    const IProgramStore& programStore,
    ITransactionBoundary& transactionBoundary)
    : input_(move(input)), userStore_(userStore), programStore_(programStore),
      transactionBoundary_(transactionBoundary) {}

CommandResult CreateAccountCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        if (!boundedText(input_.userId.value(), 32) || !boundedText(input_.profileId, 32) ||
            !boundedText(input_.name, 160) || !boundedText(input_.email, 254)) {
            return CommandResult::failure(
                "INVALID_ACCOUNT", "Account identifiers, name, and email must be bounded values.");
        }
        if (input_.role != UserRole::Student && input_.role != UserRole::Faculty) {
            return CommandResult::failure(
                "UNSUPPORTED_ACCOUNT_ROLE", "Only Student and Faculty accounts can be created.");
        }
        if (input_.role == UserRole::Student) {
            if (!input_.programId || input_.department) {
                return CommandResult::failure(
                    "ACCOUNT_PROFILE_MISMATCH",
                    "A Student requires only a Programme profile value.");
            }
            auto program = programStore_.findProgram(*input_.programId);
            if (!program) return failure(program.error());
            if (!program.value()) {
                return CommandResult::failure(
                    "PROGRAM_NOT_FOUND", "The Student Programme does not exist.");
            }
        } else {
            if (!input_.department || input_.programId) {
                return CommandResult::failure(
                    "ACCOUNT_PROFILE_MISMATCH",
                    "A Faculty member requires only a department profile value.");
            }
            auto department = userStore_.departmentExists(*input_.department);
            if (!department) return failure(department.error());
            if (!department.value()) {
                return CommandResult::failure(
                    "DEPARTMENT_NOT_FOUND", "The Faculty department does not exist.");
            }
        }
        auto existing = userStore_.findUser(input_.userId);
        if (!existing) return failure(existing.error());
        if (existing.value()) {
            return CommandResult::failure(
                "ACCOUNT_IDENTITY_CONFLICT", "The User ID already exists.");
        }
        auto userCreated = userStore_.createUser(
            {input_.userId, input_.name, input_.email, UserStatus::Active, input_.role});
        if (!userCreated) {
            return persistenceConflict(
                userCreated, "ACCOUNT_IDENTITY_CONFLICT", "The User ID or email already exists.");
        }
        Result<void> profile = input_.role == UserRole::Student
            ? userStore_.createStudent(
                  {StudentId{input_.profileId}, input_.userId, *input_.programId})
            : userStore_.createFaculty(
                  {FacultyId{input_.profileId}, input_.userId, *input_.department});
        return persistenceConflict(
            profile, "ACCOUNT_IDENTITY_CONFLICT",
            "The profile ID or User/profile relationship already exists.");
    });
}

EditAccountCommand::EditAccountCommand(
    UserId userId,
    AccountPatch patch,
    IUserStore& userStore,
    const IProgramStore& programStore,
    ITransactionBoundary& transactionBoundary)
    : userId_(move(userId)), patch_(move(patch)), userStore_(userStore),
      programStore_(programStore), transactionBoundary_(transactionBoundary) {}

CommandResult EditAccountCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        auto existing = userStore_.findUser(userId_);
        if (!existing) return failure(existing.error());
        if (!existing.value()) {
            return CommandResult::failure("USER_NOT_FOUND", "The User does not exist.");
        }
        User user = *existing.value();
        if (user.role != UserRole::Student && user.role != UserRole::Faculty) {
            return CommandResult::failure(
                "UNSUPPORTED_ACCOUNT_ROLE", "Administrator accounts cannot be edited here.");
        }
        if (patch_.role && *patch_.role != user.role) {
            return CommandResult::failure(
                "ACCOUNT_ROLE_IMMUTABLE", "An account role cannot be converted.");
        }
        if (patch_.name) user.name = *patch_.name;
        if (patch_.email) user.email = *patch_.email;
        if (!boundedText(user.name, 160) || !boundedText(user.email, 254)) {
            return CommandResult::failure(
                "INVALID_ACCOUNT", "Account name and email must be bounded values.");
        }

        if (user.role == UserRole::Student) {
            if (patch_.department) {
                return CommandResult::failure(
                    "ACCOUNT_PROFILE_MISMATCH", "A Student cannot receive a Faculty department.");
            }
            auto profile = userStore_.findStudentByUserId(user.id);
            if (!profile) return failure(profile.error());
            if (!profile.value() || profile.value()->userId != user.id) {
                return CommandResult::failure(
                    "ACCOUNT_PROFILE_MISMATCH", "The Student profile is missing or mismatched.");
            }
            Student student = *profile.value();
            if (patch_.programId) {
                auto program = programStore_.findProgram(*patch_.programId);
                if (!program) return failure(program.error());
                if (!program.value()) {
                    return CommandResult::failure(
                        "PROGRAM_NOT_FOUND", "The Student Programme does not exist.");
                }
                student.programId = *patch_.programId;
            }
            auto savedUser = userStore_.saveUser(user);
            if (!savedUser) {
                return persistenceConflict(
                    savedUser, "ACCOUNT_IDENTITY_CONFLICT", "The email already exists.");
            }
            return userStore_.saveStudent(move(student));
        }

        if (patch_.programId) {
            return CommandResult::failure(
                "ACCOUNT_PROFILE_MISMATCH", "A Faculty member cannot receive a Student Programme.");
        }
        auto profile = userStore_.findFacultyByUserId(user.id);
        if (!profile) return failure(profile.error());
        if (!profile.value() || profile.value()->userId != user.id) {
            return CommandResult::failure(
                "ACCOUNT_PROFILE_MISMATCH", "The Faculty profile is missing or mismatched.");
        }
        Faculty faculty = *profile.value();
        if (patch_.department) {
            auto department = userStore_.departmentExists(*patch_.department);
            if (!department) return failure(department.error());
            if (!department.value()) {
                return CommandResult::failure(
                    "DEPARTMENT_NOT_FOUND", "The Faculty department does not exist.");
            }
            faculty.department = *patch_.department;
        }
        auto savedUser = userStore_.saveUser(user);
        if (!savedUser) {
            return persistenceConflict(
                savedUser, "ACCOUNT_IDENTITY_CONFLICT", "The email already exists.");
        }
        return userStore_.saveFaculty(move(faculty));
    });
}

DeactivateUserCommand::DeactivateUserCommand(
    UserId userId,
    IUserStore& userStore,
    ITransactionBoundary& transactionBoundary)
    : userId_(move(userId)), userStore_(userStore), transactionBoundary_(transactionBoundary) {}

CommandResult DeactivateUserCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        auto existing = userStore_.findUser(userId_);
        if (!existing) return failure(existing.error());
        if (!existing.value()) {
            return CommandResult::failure("USER_NOT_FOUND", "The User does not exist.");
        }
        User user = *existing.value();
        if (user.role == UserRole::Administrator) {
            return CommandResult::failure(
                "UNSUPPORTED_ACCOUNT_ROLE", "Administrator accounts cannot be deactivated here.");
        }
        if (user.status == UserStatus::Inactive) {
            return CommandResult::failure(
                "USER_ALREADY_INACTIVE", "The User is already inactive.");
        }
        if (user.role == UserRole::Student) {
            auto profile = userStore_.findStudentByUserId(user.id);
            if (!profile) return failure(profile.error());
            if (!profile.value() || profile.value()->userId != user.id) {
                return CommandResult::failure(
                    "ACCOUNT_PROFILE_MISMATCH", "The Student profile is missing or mismatched.");
            }
        } else {
            auto profile = userStore_.findFacultyByUserId(user.id);
            if (!profile) return failure(profile.error());
            if (!profile.value() || profile.value()->userId != user.id) {
                return CommandResult::failure(
                    "ACCOUNT_PROFILE_MISMATCH", "The Faculty profile is missing or mismatched.");
            }
        }
        user.status = UserStatus::Inactive;
        return userStore_.saveUser(move(user));
    });
}

OverrideEnrollmentCommand::OverrideEnrollmentCommand(
    EnrollmentOverrideInput input,
    const IUserStore& userStore,
    const ICourseStore& courseStore,
    IEnrollmentStore& enrollmentStore,
    const IGradeStore& gradeStore,
    IWaitlistStore& waitlistStore,
    ITransactionBoundary& transactionBoundary)
    : input_(move(input)), userStore_(userStore), courseStore_(courseStore),
      enrollmentStore_(enrollmentStore), gradeStore_(gradeStore), waitlistStore_(waitlistStore),
      transactionBoundary_(transactionBoundary) {}

CommandResult OverrideEnrollmentCommand::execute() {
    enrollmentId_ = EnrollmentId{};
    overrideId_ = EnrollmentOverrideId{};
    EnrollmentId committedEnrollment;
    EnrollmentOverrideId committedOverride;
    auto result = transactionBoundary_.executeTransaction([this, &committedEnrollment,
                                                            &committedOverride] {
        if (input_.bypassedRule != EnrollmentRule::Prerequisite &&
            input_.bypassedRule != EnrollmentRule::Capacity &&
            input_.bypassedRule != EnrollmentRule::TimeConflict) {
            return CommandResult::failure(
                "INVALID_OVERRIDE_RULE", "The selected enrolment rule is not supported.");
        }
        if (!boundedText(input_.reason, 1000)) {
            return CommandResult::failure(
                "INVALID_OVERRIDE_REASON", "An override requires a bounded non-whitespace reason.");
        }
        auto administrator = activeAdministrator(input_.administratorUserId, userStore_);
        if (!administrator) return failure(administrator.error());
        auto student = validateActiveStudent(input_.studentId, userStore_);
        if (!student) return failure(student.error());
        auto offering = courseStore_.findOffering(input_.offeringId);
        if (!offering) return failure(offering.error());
        if (!offering.value()) {
            return CommandResult::failure("OFFERING_NOT_FOUND", "The offering does not exist.");
        }
        if (offering.value()->courseId.empty() || offering.value()->schedule.empty()) {
            return CommandResult::failure(
                "OFFERING_INTEGRITY_ERROR", "The offering has incomplete Course or schedule data.");
        }
        auto course = courseStore_.findCourse(offering.value()->courseId);
        if (!course) return failure(course.error());
        if (!course.value()) {
            return CommandResult::failure(
                "OFFERING_INTEGRITY_ERROR", "The offering Course does not exist.");
        }
        auto existing = enrollmentStore_.findStudentEnrollment(input_.studentId, input_.offeringId);
        if (!existing) return failure(existing.error());
        if (existing.value() && existing.value()->status == EnrollmentStatus::Active) {
            return CommandResult::failure(
                "ALREADY_ENROLLED", "The Student is already actively enrolled.");
        }
        if (existing.value() && existing.value()->status == EnrollmentStatus::Completed) {
            return CommandResult::failure(
                "ALREADY_COMPLETED", "The Student already completed this offering.");
        }

        auto grades = gradeStore_.submittedGradesForStudent(input_.studentId);
        if (!grades) return failure(grades.error());
        set<CourseId> completed;
        for (const auto& grade : grades.value()) completed.insert(grade.courseId);
        const bool prerequisiteViolation = any_of(
            course.value()->prerequisiteCourseIds.begin(),
            course.value()->prerequisiteCourseIds.end(),
            [&completed](const CourseId& id) { return completed.count(id) == 0; });
        const bool capacityViolation = offering.value()->enrolledCount >= offering.value()->capacity;

        auto active = enrollmentStore_.activeEnrollmentsForStudent(input_.studentId);
        if (!active) return failure(active.error());
        bool timeViolation = false;
        for (const auto& enrollment : active.value()) {
            auto current = courseStore_.findOffering(enrollment.offeringId);
            if (!current) return failure(current.error());
            if (!current.value()) {
                return CommandResult::failure(
                    "ENROLLMENT_INTEGRITY_ERROR", "An active enrolment has no offering.");
            }
            if (offeringsConflict(*offering.value(), *current.value())) {
                timeViolation = true;
                break;
            }
        }

        if (input_.bypassedRule != EnrollmentRule::Prerequisite && prerequisiteViolation) {
            return CommandResult::failure(
                "PREREQUISITE_NOT_MET", "A non-bypassed prerequisite rule failed.");
        }
        if (input_.bypassedRule != EnrollmentRule::Capacity && capacityViolation) {
            return CommandResult::failure("CAPACITY_FULL", "A non-bypassed capacity rule failed.");
        }
        if (input_.bypassedRule != EnrollmentRule::TimeConflict && timeViolation) {
            return CommandResult::failure("TIME_CONFLICT", "A non-bypassed time rule failed.");
        }
        const bool selectedViolation =
            (input_.bypassedRule == EnrollmentRule::Prerequisite && prerequisiteViolation) ||
            (input_.bypassedRule == EnrollmentRule::Capacity && capacityViolation) ||
            (input_.bypassedRule == EnrollmentRule::TimeConflict && timeViolation);
        if (!selectedViolation) {
            return CommandResult::failure(
                "OVERRIDE_NOT_REQUIRED", "The selected rule is not preventing enrolment.");
        }

        const EnrollmentId enrollmentId = existing.value()
            ? existing.value()->id
            : EnrollmentId{newPersistentId("ENR-")};
        auto saved = enrollmentStore_.saveEnrollment(
            {enrollmentId, input_.studentId, input_.offeringId, EnrollmentStatus::Active});
        if (!saved) return saved;
        auto waitlist = waitlistStore_.findStudentWaitlistEntry(
            input_.studentId, input_.offeringId);
        if (!waitlist) return failure(waitlist.error());
        if (waitlist.value() && waitlist.value()->status != WaitlistStatus::Removed) {
            WaitlistEntry retired = *waitlist.value();
            retired.status = WaitlistStatus::Removed;
            auto retiredResult = waitlistStore_.saveWaitlistEntry(move(retired));
            if (!retiredResult) return retiredResult;
        }
        const EnrollmentOverrideId overrideId{newPersistentId("OVR-")};
        auto recorded = enrollmentStore_.createEnrollmentOverride(
            {overrideId, input_.administratorUserId, enrollmentId,
             input_.bypassedRule, input_.reason});
        if (!recorded) return recorded;
        committedEnrollment = enrollmentId;
        committedOverride = overrideId;
        return CommandResult::success();
    });
    if (result) {
        enrollmentId_ = move(committedEnrollment);
        overrideId_ = move(committedOverride);
    }
    return result;
}

ApproveCourseChangeCommand::ApproveCourseChangeCommand(
    ChangeRequestId requestId,
    ICourseStore& courseStore,
    IChangeRequestStore& changeRequestStore,
    ITransactionBoundary& transactionBoundary)
    : requestId_(move(requestId)), courseStore_(courseStore),
      changeRequestStore_(changeRequestStore), transactionBoundary_(transactionBoundary) {}

CommandResult ApproveCourseChangeCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        CourseChangeRequest request;
        auto pending = pendingRequest(requestId_, changeRequestStore_, request);
        if (!pending) return pending;
        auto course = courseStore_.findCourse(request.courseId);
        if (!course) return failure(course.error());
        if (!course.value()) {
            return CommandResult::failure("COURSE_NOT_FOUND", "The requested Course does not exist.");
        }

        if (request.type == CourseChangeType::Description) {
            if (!boundedText(request.requestedValue, 4000)) {
                return CommandResult::failure(
                    "INVALID_CHANGE_VALUE", "The stored description is empty or unsupported.");
            }
            Course updated = *course.value();
            updated.description = request.requestedValue;
            auto saved = courseStore_.saveCourse(move(updated));
            if (!saved) return saved;
        } else if (request.type == CourseChangeType::Prerequisites) {
            auto parsed = parsePrerequisiteValue(request.requestedValue);
            if (!parsed) return failure(parsed.error());
            auto valid = validatePrerequisites(request.courseId, parsed.value(), courseStore_);
            if (!valid) return valid;
            Course updated = *course.value();
            updated.prerequisiteCourseIds = move(parsed.value());
            auto saved = courseStore_.saveCourse(move(updated));
            if (!saved) return saved;
        } else if (request.type == CourseChangeType::Capacity) {
            if (!request.offeringId) {
                return CommandResult::failure(
                    "INVALID_CHANGE_VALUE", "A stored capacity request has no offering.");
            }
            auto capacity = parseCapacityValue(request.requestedValue);
            if (!capacity) return failure(capacity.error());
            auto offering = courseStore_.findOffering(*request.offeringId);
            if (!offering) return failure(offering.error());
            if (!offering.value()) {
                return CommandResult::failure("OFFERING_NOT_FOUND", "The offering does not exist.");
            }
            if (offering.value()->courseId != request.courseId) {
                return CommandResult::failure(
                    "OFFERING_COURSE_MISMATCH", "The offering does not belong to the stored Course.");
            }
            if (capacity.value() < offering.value()->enrolledCount) {
                return CommandResult::failure(
                    "CAPACITY_BELOW_OCCUPANCY", "Capacity cannot be below active occupancy.");
            }
            auto saved = courseStore_.updateOfferingCapacity(*request.offeringId, capacity.value());
            if (!saved) return saved;
        } else {
            return CommandResult::failure(
                "INVALID_CHANGE_VALUE", "The stored Course-change type is unsupported.");
        }
        request.status = CourseChangeStatus::Approved;
        return changeRequestStore_.saveChangeRequest(move(request));
    });
}

RejectCourseChangeCommand::RejectCourseChangeCommand(
    ChangeRequestId requestId,
    IChangeRequestStore& changeRequestStore,
    ITransactionBoundary& transactionBoundary)
    : requestId_(move(requestId)), changeRequestStore_(changeRequestStore),
      transactionBoundary_(transactionBoundary) {}

CommandResult RejectCourseChangeCommand::execute() {
    return transactionBoundary_.executeTransaction([this] {
        CourseChangeRequest request;
        auto pending = pendingRequest(requestId_, changeRequestStore_, request);
        if (!pending) return pending;
        request.status = CourseChangeStatus::Rejected;
        return changeRequestStore_.saveChangeRequest(move(request));
    });
}

}
