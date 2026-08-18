#include "nexusenroll/business/cqrs/student_validation.hpp"

namespace nexusenroll::business::cqrs {

using namespace common;
using namespace data::contracts;
using namespace domain;

Result<Student> validateActiveStudent(StudentId studentId, const IUserStore& userStore) {
    auto student = userStore.findStudent(studentId);
    if (!student) {
        return Result<Student>::failure(student.error().code, student.error().message);
    }
    if (!student.value()) {
        return Result<Student>::failure(
            "STUDENT_NOT_FOUND", "The selected Student profile does not exist.");
    }

    auto user = userStore.findUser(student.value()->userId);
    if (!user) {
        return Result<Student>::failure(user.error().code, user.error().message);
    }
    if (!user.value()) {
        return Result<Student>::failure(
            "STUDENT_USER_NOT_FOUND", "The Student profile has no linked User.");
    }
    if (user.value()->role != UserRole::Student) {
        return Result<Student>::failure(
            "STUDENT_ROLE_MISMATCH", "The Student profile is linked to a non-Student User.");
    }
    if (user.value()->status != UserStatus::Active) {
        return Result<Student>::failure(
            "STUDENT_INACTIVE", "The selected Student account is inactive.");
    }
    return Result<Student>::success(*student.value());
}

}
