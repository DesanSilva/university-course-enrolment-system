#pragma once

#include "nexusenroll/business/sessions/user_session.hpp"

#include <memory>
#include <string>

namespace nexusenroll::business::sessions {

class SessionCreator {
public:
    virtual ~SessionCreator() = default;

    virtual std::unique_ptr<UserSession> createSession(
        common::UserId userId,
        std::string displayName) const = 0;
};

class StudentSessionCreator final : public SessionCreator {
public:
    explicit StudentSessionCreator(common::StudentId studentId);

    std::unique_ptr<UserSession> createSession(
        common::UserId userId,
        std::string displayName) const override;

private:
    common::StudentId studentId_;
};

class FacultySessionCreator final : public SessionCreator {
public:
    explicit FacultySessionCreator(common::FacultyId facultyId);

    std::unique_ptr<UserSession> createSession(
        common::UserId userId,
        std::string displayName) const override;

private:
    common::FacultyId facultyId_;
};

class AdministratorSessionCreator final : public SessionCreator {
public:
    std::unique_ptr<UserSession> createSession(
        common::UserId userId,
        std::string displayName) const override;
};

}
