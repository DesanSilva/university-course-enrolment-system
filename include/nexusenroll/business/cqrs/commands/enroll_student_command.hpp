#pragma once

#include "nexusenroll/business/cqrs/commands/command.hpp"
#include "nexusenroll/common/identifiers.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/enrollment_store.hpp"
#include "nexusenroll/data/contracts/grade_store.hpp"
#include "nexusenroll/data/contracts/transaction_boundary.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"
#include "nexusenroll/data/contracts/waitlist_store.hpp"

namespace nexusenroll::business::cqrs::commands {

class EnrollStudentCommand final : public ICommand {
public:
    EnrollStudentCommand(
        common::StudentId studentId,
        common::OfferingId offeringId,
        const data::contracts::IUserStore& userStore,
        const data::contracts::ICourseStore& courseStore,
        data::contracts::IEnrollmentStore& enrollmentStore,
        const data::contracts::IGradeStore& gradeStore,
        data::contracts::IWaitlistStore& waitlistStore,
        data::contracts::ITransactionBoundary& transactionBoundary);

    CommandResult execute() override;

private:
    common::StudentId studentId_;
    common::OfferingId offeringId_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::ICourseStore& courseStore_;
    data::contracts::IEnrollmentStore& enrollmentStore_;
    const data::contracts::IGradeStore& gradeStore_;
    data::contracts::IWaitlistStore& waitlistStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

}
