#pragma once

#include "nexusenroll/business/domain/schedule.hpp"
#include "nexusenroll/common/identifiers.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nexusenroll::business::domain {

enum class UserRole { Student, Faculty, Administrator };
enum class UserStatus { Active, Inactive };
enum class EnrollmentStatus { Active, Dropped, Completed };
enum class GradeLifecycle { Pending, Submitted };
enum class CourseChangeStatus { Pending, Approved, Rejected };
enum class CourseChangeType { Description, Prerequisites, Capacity };
enum class WaitlistStatus { Waiting, Offered, Removed };

struct User {
    common::UserId id;
    std::string name;
    std::string email;
    UserStatus status;
    UserRole role;
};

struct Student {
    common::StudentId id;
    common::UserId userId;
    common::ProgramId programId;
};

struct Faculty {
    common::FacultyId id;
    common::UserId userId;
    std::string department;
};

struct Course {
    common::CourseId id;
    std::string code;
    std::string department;
    std::string courseNumber;
    std::string name;
    std::string description;
    unsigned int credits;
    std::vector<common::CourseId> prerequisiteCourseIds;
};

struct CourseOffering {
    common::OfferingId id;
    common::CourseId courseId;
    std::string semester;
    common::FacultyId instructorId;
    std::size_t capacity;
    std::size_t enrolledCount;
    std::vector<ScheduleSlot> schedule;
};

struct Enrollment {
    common::EnrollmentId id;
    common::StudentId studentId;
    common::OfferingId offeringId;
    EnrollmentStatus status;
};

struct DegreeProgram {
    common::ProgramId id;
    std::string name;
    std::string department;
    std::vector<common::CourseId> requiredCourseIds;
    unsigned int requiredCredits;
};

struct GradeRecord {
    common::GradeRecordId id;
    common::StudentId studentId;
    common::OfferingId offeringId;
    common::CourseId courseId;
    std::string grade;
    GradeLifecycle lifecycle;
};

struct CourseChangeRequest {
    common::ChangeRequestId id;
    common::FacultyId facultyId;
    common::CourseId courseId;
    std::optional<common::OfferingId> offeringId;
    CourseChangeType type;
    CourseChangeStatus status;
    std::string requestedValue;
};

struct WaitlistEntry {
    common::WaitlistEntryId id;
    common::StudentId studentId;
    common::OfferingId offeringId;
    std::size_t position;
    WaitlistStatus status;
};

struct CatalogueFilter {
    std::string semester;
    std::string department;
    std::string courseNumber;
    std::string keyword;
    std::string instructor;
};

struct CatalogueItem {
    Course course;
    CourseOffering offering;
    std::string instructorName;
};

}
