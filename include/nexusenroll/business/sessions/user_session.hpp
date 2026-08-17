#pragma once

#include "nexusenroll/business/domain/model.hpp"

#include <string>

namespace nexusenroll::business::sessions {

class UserSession {
public:
    virtual ~UserSession() = default;

    const common::UserId& userId() const noexcept { return userId_; }
    const std::string& displayName() const noexcept { return displayName_; }
    domain::UserRole role() const noexcept { return role_; }

protected:
    UserSession(common::UserId userId, std::string displayName, domain::UserRole role);

private:
    common::UserId userId_;
    std::string displayName_;
    domain::UserRole role_;
};

class StudentSession final : public UserSession {
public:
    StudentSession(
        common::UserId userId,
        std::string displayName,
        common::StudentId studentId);

    const common::StudentId& studentId() const noexcept { return studentId_; }

private:
    common::StudentId studentId_;
};

class FacultySession final : public UserSession {
public:
    FacultySession(
        common::UserId userId,
        std::string displayName,
        common::FacultyId facultyId);

    const common::FacultyId& facultyId() const noexcept { return facultyId_; }

private:
    common::FacultyId facultyId_;
};

class AdministratorSession final : public UserSession {
public:
    AdministratorSession(common::UserId userId, std::string displayName);
};

}
