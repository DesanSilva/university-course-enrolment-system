#pragma once

#include "nexusenroll/data/contracts/change_request_store.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/enrollment_store.hpp"
#include "nexusenroll/data/contracts/grade_store.hpp"
#include "nexusenroll/data/contracts/program_store.hpp"
#include "nexusenroll/data/contracts/transaction_boundary.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"
#include "nexusenroll/data/contracts/waitlist_store.hpp"
#include "nexusenroll/data/mysql/mysql_connection.hpp"

#include <cstddef>
#include <memory>

namespace nexusenroll::data::mysql {

class MySqlDataContext final : public contracts::IUserStore,
                               public contracts::ICourseStore,
                               public contracts::IEnrollmentStore,
                               public contracts::IProgramStore,
                               public contracts::IGradeStore,
                               public contracts::IChangeRequestStore,
                               public contracts::IWaitlistStore,
                               public contracts::ITransactionBoundary {
public:
    explicit MySqlDataContext(MySqlConfig config, std::size_t poolSize = 4);
    ~MySqlDataContext();

    MySqlDataContext(const MySqlDataContext&) = delete;
    MySqlDataContext& operator=(const MySqlDataContext&) = delete;

    common::Result<void> verifyConnections();

    common::Result<std::optional<business::domain::User>> findUser(common::UserId id) const override;
    common::Result<std::optional<business::domain::Student>> findStudent(common::StudentId id) const override;
    common::Result<std::optional<business::domain::Faculty>> findFaculty(common::FacultyId id) const override;
    common::Result<std::optional<business::domain::Student>> findStudentByUserId(
        common::UserId userId) const override;
    common::Result<std::optional<business::domain::Faculty>> findFacultyByUserId(
        common::UserId userId) const override;
    common::Result<std::vector<business::domain::User>> users() const override;
    common::Result<std::vector<business::domain::Student>> students() const override;
    common::Result<std::vector<business::domain::Faculty>> facultyMembers() const override;
    common::Result<void> saveUser(business::domain::User user) override;
    common::Result<void> saveStudent(business::domain::Student student) override;
    common::Result<void> saveFaculty(business::domain::Faculty faculty) override;

    common::Result<std::optional<business::domain::Course>> findCourse(common::CourseId id) const override;
    common::Result<std::optional<business::domain::CourseOffering>> findOffering(
        common::OfferingId id) const override;
    common::Result<std::vector<business::domain::Course>> courses() const override;
    common::Result<std::vector<business::domain::CourseOffering>> offerings() const override;
    common::Result<std::vector<business::domain::CatalogueItem>> browseCatalogue(
        const business::domain::CatalogueFilter& filter) const override;
    common::Result<std::vector<business::domain::FacultyOfferingItem>> assignedOfferings(
        common::FacultyId facultyId) const override;
    common::Result<bool> facultyTeachesCourse(
        common::FacultyId facultyId,
        common::CourseId courseId) const override;
    common::Result<void> saveCourse(business::domain::Course course) override;
    common::Result<void> saveOffering(business::domain::CourseOffering offering) override;

    common::Result<std::optional<business::domain::Enrollment>> findEnrollment(
        common::EnrollmentId id) const override;
    common::Result<std::optional<business::domain::Enrollment>> findStudentEnrollment(
        common::StudentId studentId,
        common::OfferingId offeringId) const override;
    common::Result<std::vector<business::domain::Enrollment>> enrollments() const override;
    common::Result<std::vector<business::domain::Enrollment>> activeEnrollmentsForStudent(
        common::StudentId studentId) const override;
    common::Result<std::vector<business::domain::Enrollment>> scheduleEnrollmentsForStudent(
        common::StudentId studentId,
        const std::string& semester) const override;
    common::Result<std::vector<business::domain::FacultyRosterEntry>> activeRosterForOffering(
        common::OfferingId offeringId) const override;
    common::Result<void> saveEnrollment(business::domain::Enrollment enrollment) override;
    common::Result<void> removeEnrollment(common::EnrollmentId id) override;

    common::Result<std::optional<business::domain::DegreeProgram>> findProgram(
        common::ProgramId id) const override;
    common::Result<std::vector<business::domain::DegreeProgram>> programs() const override;
    common::Result<void> saveProgram(business::domain::DegreeProgram program) override;

    common::Result<std::optional<business::domain::GradeRecord>> findGradeRecord(
        common::GradeRecordId id) const override;
    common::Result<std::vector<business::domain::GradeRecord>> gradeRecords() const override;
    common::Result<std::vector<business::domain::GradeRecord>> submittedGradesForStudent(
        common::StudentId studentId) const override;
    common::Result<std::optional<business::domain::GradeRecord>> findStudentGradeRecord(
        common::StudentId studentId,
        common::OfferingId offeringId) const override;
    common::Result<std::vector<business::domain::FacultyGradeStateEntry>> gradeStateForOffering(
        common::OfferingId offeringId) const override;
    common::Result<std::vector<business::domain::GradeRecord>> pendingGradesForOffering(
        common::OfferingId offeringId) const override;
    common::Result<void> createGradeRecord(
        business::domain::GradeRecord record) override;
    common::Result<void> saveGradeRecord(business::domain::GradeRecord record) override;

    common::Result<std::optional<business::domain::CourseChangeRequest>> findChangeRequest(
        common::ChangeRequestId id) const override;
    common::Result<std::vector<business::domain::CourseChangeRequest>> changeRequests() const override;
    common::Result<std::vector<business::domain::CourseChangeRequest>> changeRequestsForFaculty(
        common::FacultyId facultyId) const override;
    common::Result<void> createChangeRequest(
        business::domain::CourseChangeRequest request) override;
    common::Result<void> saveChangeRequest(
        business::domain::CourseChangeRequest request) override;

    common::Result<std::optional<business::domain::WaitlistEntry>> findWaitlistEntry(
        common::WaitlistEntryId id) const override;
    common::Result<std::optional<business::domain::WaitlistEntry>> findStudentWaitlistEntry(
        common::StudentId studentId,
        common::OfferingId offeringId) const override;
    common::Result<std::vector<business::domain::WaitlistEntry>> waitlistEntries() const override;
    common::Result<std::vector<business::domain::WaitlistEntry>> waitlistEntriesForStudent(
        common::StudentId studentId) const override;
    common::Result<std::vector<business::domain::WaitlistEntry>> waitingEntriesForOffering(
        common::OfferingId offeringId) const override;
    common::Result<std::size_t> nextWaitlistPosition(
        common::OfferingId offeringId) const override;
    common::Result<void> saveWaitlistEntry(business::domain::WaitlistEntry entry) override;
    common::Result<void> removeWaitlistEntry(common::WaitlistEntryId id) override;

    common::Result<void> executeTransaction(const Operation& operation) override;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}
