#include "nexusenroll/business/sessions/demonstration_session_service.hpp"

#include "nexusenroll/business/sessions/session_creator.hpp"

#include <memory>
#include <utility>

namespace nexusenroll::business::sessions {

using namespace common;
using namespace data::contracts;
using namespace domain;
using namespace std;

namespace {

template <typename T>
Result<T> failure(const Error& error) {
    return Result<T>::failure(error.code, error.message);
}

Result<unique_ptr<UserSession>> missingProfile(const char* role) {
    return Result<unique_ptr<UserSession>>::failure(
        "SESSION_PROFILE_NOT_FOUND",
        string("The selected ") + role + " user has no matching role profile.");
}

Result<unique_ptr<UserSession>> mismatchedProfile(const char* role) {
    return Result<unique_ptr<UserSession>>::failure(
        "SESSION_PROFILE_MISMATCH",
        string("The selected ") + role + " profile belongs to another user.");
}

}

DemonstrationSessionService::DemonstrationSessionService(const IUserStore& userStore)
    : userStore_(userStore) {}

Result<unique_ptr<UserSession>> DemonstrationSessionService::create(UserId userId) const {
    auto userResult = userStore_.findUser(userId);
    if (!userResult) {
        return failure<unique_ptr<UserSession>>(userResult.error());
    }
    if (!userResult.value()) {
        return Result<unique_ptr<UserSession>>::failure(
            "SESSION_USER_NOT_FOUND", "The selected demonstration user does not exist.");
    }

    const User& user = *userResult.value();
    if (user.status != UserStatus::Active) {
        return Result<unique_ptr<UserSession>>::failure(
            "USER_INACTIVE", "The selected demonstration user is inactive.");
    }

    unique_ptr<SessionCreator> creator;
    switch (user.role) {
    case UserRole::Student: {
        auto profile = userStore_.findStudentByUserId(user.id);
        if (!profile) {
            return failure<unique_ptr<UserSession>>(profile.error());
        }
        if (!profile.value()) {
            return missingProfile("Student");
        }
        if (profile.value()->userId != user.id) {
            return mismatchedProfile("Student");
        }
        creator = make_unique<StudentSessionCreator>(profile.value()->id);
        break;
    }
    case UserRole::Faculty: {
        auto profile = userStore_.findFacultyByUserId(user.id);
        if (!profile) {
            return failure<unique_ptr<UserSession>>(profile.error());
        }
        if (!profile.value()) {
            return missingProfile("Faculty");
        }
        if (profile.value()->userId != user.id) {
            return mismatchedProfile("Faculty");
        }
        creator = make_unique<FacultySessionCreator>(profile.value()->id);
        break;
    }
    case UserRole::Administrator:
        creator = make_unique<AdministratorSessionCreator>();
        break;
    }

    return Result<unique_ptr<UserSession>>::success(
        creator->createSession(user.id, user.name));
}

}
