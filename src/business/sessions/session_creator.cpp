#include "nexusenroll/business/sessions/session_creator.hpp"

#include <utility>

namespace nexusenroll::business::sessions {

using namespace std;

StudentSessionCreator::StudentSessionCreator(common::StudentId studentId)
    : studentId_(move(studentId)) {}

unique_ptr<UserSession> StudentSessionCreator::createSession(
    common::UserId userId,
    string displayName) const {
    return make_unique<StudentSession>(move(userId), move(displayName), studentId_);
}

FacultySessionCreator::FacultySessionCreator(common::FacultyId facultyId)
    : facultyId_(move(facultyId)) {}

unique_ptr<UserSession> FacultySessionCreator::createSession(
    common::UserId userId,
    string displayName) const {
    return make_unique<FacultySession>(move(userId), move(displayName), facultyId_);
}

unique_ptr<UserSession> AdministratorSessionCreator::createSession(
    common::UserId userId,
    string displayName) const {
    return make_unique<AdministratorSession>(move(userId), move(displayName));
}

}
