#include "nexusenroll/business/cqrs/faculty_validation.hpp"

namespace nexusenroll::business::cqrs {

using namespace common;
using namespace data::contracts;
using namespace domain;

Result<Faculty> validateActiveFaculty(FacultyId facultyId, const IUserStore& userStore) {
    auto faculty = userStore.findFaculty(facultyId);
    if (!faculty) {
        return Result<Faculty>::failure(faculty.error().code, faculty.error().message);
    }
    if (!faculty.value()) {
        return Result<Faculty>::failure(
            "FACULTY_NOT_FOUND", "The selected Faculty profile does not exist.");
    }

    auto user = userStore.findUser(faculty.value()->userId);
    if (!user) {
        return Result<Faculty>::failure(user.error().code, user.error().message);
    }
    if (!user.value()) {
        return Result<Faculty>::failure(
            "FACULTY_USER_NOT_FOUND", "The Faculty profile has no linked User.");
    }
    if (user.value()->role != UserRole::Faculty) {
        return Result<Faculty>::failure(
            "FACULTY_ROLE_MISMATCH", "The Faculty profile is linked to a non-Faculty User.");
    }
    if (user.value()->status != UserStatus::Active) {
        return Result<Faculty>::failure(
            "FACULTY_INACTIVE", "The selected Faculty account is inactive.");
    }
    return Result<Faculty>::success(*faculty.value());
}

}
