#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"

#include <optional>
#include <vector>

namespace nexusenroll::data::contracts {

class IEnrollmentStore {
public:
    virtual ~IEnrollmentStore() = default;

    virtual common::Result<std::optional<business::domain::Enrollment>> findEnrollment(
        common::EnrollmentId id) const = 0;
    virtual common::Result<std::optional<business::domain::Enrollment>> findStudentEnrollment(
        common::StudentId studentId,
        common::OfferingId offeringId) const = 0;
    virtual common::Result<std::vector<business::domain::Enrollment>> enrollments() const = 0;
    virtual common::Result<std::vector<business::domain::Enrollment>> activeEnrollmentsForStudent(
        common::StudentId studentId) const = 0;
    virtual common::Result<std::vector<business::domain::Enrollment>> scheduleEnrollmentsForStudent(
        common::StudentId studentId,
        const std::string& semester) const = 0;
    virtual common::Result<std::vector<business::domain::FacultyRosterEntry>> activeRosterForOffering(
        common::OfferingId offeringId) const = 0;
    virtual common::Result<std::optional<business::domain::EnrollmentOverride>> findEnrollmentOverride(
        common::EnrollmentOverrideId id) const = 0;
    virtual common::Result<void> createEnrollmentOverride(
        business::domain::EnrollmentOverride enrollmentOverride) = 0;
    virtual common::Result<void> saveEnrollment(business::domain::Enrollment enrollment) = 0;
    virtual common::Result<void> removeEnrollment(common::EnrollmentId id) = 0;
};

}
