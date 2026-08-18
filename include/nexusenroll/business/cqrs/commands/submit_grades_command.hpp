#pragma once

#include "nexusenroll/business/cqrs/commands/command.hpp"
#include "nexusenroll/common/identifiers.hpp"
#include "nexusenroll/common/result.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/enrollment_store.hpp"
#include "nexusenroll/data/contracts/grade_store.hpp"
#include "nexusenroll/data/contracts/transaction_boundary.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"

#include <string>
#include <vector>

namespace nexusenroll::business::cqrs::commands {

struct GradeInput {
    common::StudentId studentId;
    std::string grade;
};

struct GradeRejection {
    GradeInput input;
    common::Error error;
};

struct GradeBatchOutcome {
    std::vector<GradeInput> accepted;
    std::vector<GradeRejection> rejected;
};

class SubmitGradesCommand final : public ICommand {
public:
    SubmitGradesCommand(
        common::FacultyId facultyId,
        common::OfferingId offeringId,
        std::vector<GradeInput> grades,
        const data::contracts::IUserStore& userStore,
        const data::contracts::ICourseStore& courseStore,
        const data::contracts::IEnrollmentStore& enrollmentStore,
        data::contracts::IGradeStore& gradeStore,
        data::contracts::ITransactionBoundary& transactionBoundary);

    CommandResult execute() override;
    const GradeBatchOutcome& outcome() const noexcept { return outcome_; }

private:
    common::FacultyId facultyId_;
    common::OfferingId offeringId_;
    std::vector<GradeInput> grades_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::ICourseStore& courseStore_;
    const data::contracts::IEnrollmentStore& enrollmentStore_;
    data::contracts::IGradeStore& gradeStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
    GradeBatchOutcome outcome_;
};

}
