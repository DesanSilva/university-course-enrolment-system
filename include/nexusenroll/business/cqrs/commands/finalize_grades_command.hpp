#pragma once

#include "nexusenroll/business/cqrs/commands/command.hpp"
#include "nexusenroll/common/identifiers.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/enrollment_store.hpp"
#include "nexusenroll/data/contracts/grade_store.hpp"
#include "nexusenroll/data/contracts/transaction_boundary.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"

#include <cstddef>

namespace nexusenroll::business::cqrs::commands {

class FinalizeGradesCommand final : public ICommand {
public:
    FinalizeGradesCommand(
        common::FacultyId facultyId,
        common::OfferingId offeringId,
        const data::contracts::IUserStore& userStore,
        const data::contracts::ICourseStore& courseStore,
        data::contracts::IEnrollmentStore& enrollmentStore,
        data::contracts::IGradeStore& gradeStore,
        data::contracts::ITransactionBoundary& transactionBoundary);

    CommandResult execute() override;
    std::size_t finalizedCount() const noexcept { return finalizedCount_; }

private:
    common::FacultyId facultyId_;
    common::OfferingId offeringId_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::ICourseStore& courseStore_;
    data::contracts::IEnrollmentStore& enrollmentStore_;
    data::contracts::IGradeStore& gradeStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
    std::size_t finalizedCount_{0};
};

}
