#include "nexusenroll/business/sessions/user_session.hpp"

#include <utility>

namespace nexusenroll::business::sessions {

using namespace common;
using namespace domain;
using namespace std;

UserSession::UserSession(UserId userId, string displayName, UserRole role)
    : userId_(move(userId)), displayName_(move(displayName)), role_(role) {}

StudentSession::StudentSession(UserId userId, string displayName, StudentId studentId)
    : UserSession(move(userId), move(displayName), UserRole::Student),
      studentId_(move(studentId)) {}

FacultySession::FacultySession(UserId userId, string displayName, FacultyId facultyId)
    : UserSession(move(userId), move(displayName), UserRole::Faculty),
      facultyId_(move(facultyId)) {}

AdministratorSession::AdministratorSession(UserId userId, string displayName)
    : UserSession(move(userId), move(displayName), UserRole::Administrator) {}

}
