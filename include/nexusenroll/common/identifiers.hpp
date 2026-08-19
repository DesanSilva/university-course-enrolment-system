#pragma once

#include <ostream>
#include <string>
#include <utility>

namespace nexusenroll::common {

template <typename Tag>
class Identifier {
public:
    Identifier() = default;
    explicit Identifier(std::string value) : value_(std::move(value)) {}

    const std::string& value() const noexcept { return value_; }
    bool empty() const noexcept { return value_.empty(); }

    friend bool operator==(const Identifier& left, const Identifier& right) {
        return left.value_ == right.value_;
    }

    friend bool operator!=(const Identifier& left, const Identifier& right) {
        return !(left == right);
    }

    friend bool operator<(const Identifier& left, const Identifier& right) {
        return left.value_ < right.value_;
    }

private:
    std::string value_;
};

template <typename Tag>
std::ostream& operator<<(std::ostream& stream, const Identifier<Tag>& identifier) {
    return stream << identifier.value();
}

struct UserIdTag;
struct StudentIdTag;
struct FacultyIdTag;
struct CourseIdTag;
struct OfferingIdTag;
struct EnrollmentIdTag;
struct ProgramIdTag;
struct GradeRecordIdTag;
struct ChangeRequestIdTag;
struct WaitlistEntryIdTag;
struct EnrollmentOverrideIdTag;

using UserId = Identifier<UserIdTag>;
using StudentId = Identifier<StudentIdTag>;
using FacultyId = Identifier<FacultyIdTag>;
using CourseId = Identifier<CourseIdTag>;
using OfferingId = Identifier<OfferingIdTag>;
using EnrollmentId = Identifier<EnrollmentIdTag>;
using ProgramId = Identifier<ProgramIdTag>;
using GradeRecordId = Identifier<GradeRecordIdTag>;
using ChangeRequestId = Identifier<ChangeRequestIdTag>;
using WaitlistEntryId = Identifier<WaitlistEntryIdTag>;
using EnrollmentOverrideId = Identifier<EnrollmentOverrideIdTag>;

}
