#pragma once

#include "nexusenroll/business/cqrs/commands/command.hpp"
#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/data/contracts/change_request_store.hpp"
#include "nexusenroll/data/contracts/course_store.hpp"
#include "nexusenroll/data/contracts/enrollment_store.hpp"
#include "nexusenroll/data/contracts/grade_store.hpp"
#include "nexusenroll/data/contracts/program_store.hpp"
#include "nexusenroll/data/contracts/transaction_boundary.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"
#include "nexusenroll/data/contracts/waitlist_store.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nexusenroll::business::cqrs::commands {

struct CourseInput {
    common::CourseId id;
    std::string code;
    std::string department;
    std::string courseNumber;
    std::string name;
    std::string description;
    std::int64_t credits;
    std::vector<common::CourseId> prerequisiteCourseIds;
};

struct CoursePatch {
    std::optional<std::string> code;
    std::optional<std::string> department;
    std::optional<std::string> courseNumber;
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<std::int64_t> credits;
    std::optional<std::vector<common::CourseId>> prerequisiteCourseIds;
};

class CreateCourseCommand final : public ICommand {
public:
    CreateCourseCommand(CourseInput input, data::contracts::ICourseStore& courseStore,
                        data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;

private:
    CourseInput input_;
    data::contracts::ICourseStore& courseStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

class UpdateCourseCommand final : public ICommand {
public:
    UpdateCourseCommand(common::CourseId courseId, CoursePatch patch,
                        data::contracts::ICourseStore& courseStore,
                        data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;

private:
    common::CourseId courseId_;
    CoursePatch patch_;
    data::contracts::ICourseStore& courseStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

class DeleteCourseCommand final : public ICommand {
public:
    DeleteCourseCommand(common::CourseId courseId, data::contracts::ICourseStore& courseStore,
                        data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;

private:
    common::CourseId courseId_;
    data::contracts::ICourseStore& courseStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

struct ProgramInput {
    common::ProgramId id;
    std::string name;
    std::string department;
    std::int64_t requiredCredits;
    std::vector<common::CourseId> requiredCourseIds;
};

struct ProgramPatch {
    std::optional<std::string> name;
    std::optional<std::string> department;
    std::optional<std::int64_t> requiredCredits;
    std::optional<std::vector<common::CourseId>> requiredCourseIds;
};

class CreateProgramCommand final : public ICommand {
public:
    CreateProgramCommand(ProgramInput input, data::contracts::IProgramStore& programStore,
                         const data::contracts::ICourseStore& courseStore,
                         data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;

private:
    ProgramInput input_;
    data::contracts::IProgramStore& programStore_;
    const data::contracts::ICourseStore& courseStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

class UpdateProgramCommand final : public ICommand {
public:
    UpdateProgramCommand(common::ProgramId programId, ProgramPatch patch,
                         data::contracts::IProgramStore& programStore,
                         const data::contracts::ICourseStore& courseStore,
                         data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;

private:
    common::ProgramId programId_;
    ProgramPatch patch_;
    data::contracts::IProgramStore& programStore_;
    const data::contracts::ICourseStore& courseStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

struct AccountInput {
    common::UserId userId;
    std::string profileId;
    domain::UserRole role;
    std::string name;
    std::string email;
    std::optional<common::ProgramId> programId;
    std::optional<std::string> department;
};

struct AccountPatch {
    std::optional<std::string> name;
    std::optional<std::string> email;
    std::optional<domain::UserRole> role;
    std::optional<common::ProgramId> programId;
    std::optional<std::string> department;
};

class CreateAccountCommand final : public ICommand {
public:
    CreateAccountCommand(AccountInput input, data::contracts::IUserStore& userStore,
                         const data::contracts::IProgramStore& programStore,
                         data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;

private:
    AccountInput input_;
    data::contracts::IUserStore& userStore_;
    const data::contracts::IProgramStore& programStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

class EditAccountCommand final : public ICommand {
public:
    EditAccountCommand(common::UserId userId, AccountPatch patch,
                       data::contracts::IUserStore& userStore,
                       const data::contracts::IProgramStore& programStore,
                       data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;

private:
    common::UserId userId_;
    AccountPatch patch_;
    data::contracts::IUserStore& userStore_;
    const data::contracts::IProgramStore& programStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

class DeactivateUserCommand final : public ICommand {
public:
    DeactivateUserCommand(common::UserId userId, data::contracts::IUserStore& userStore,
                          data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;

private:
    common::UserId userId_;
    data::contracts::IUserStore& userStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

struct EnrollmentOverrideInput {
    common::UserId administratorUserId;
    common::StudentId studentId;
    common::OfferingId offeringId;
    domain::EnrollmentRule bypassedRule;
    std::string reason;
};

class OverrideEnrollmentCommand final : public ICommand {
public:
    OverrideEnrollmentCommand(
        EnrollmentOverrideInput input, const data::contracts::IUserStore& userStore,
        const data::contracts::ICourseStore& courseStore,
        data::contracts::IEnrollmentStore& enrollmentStore,
        const data::contracts::IGradeStore& gradeStore,
        data::contracts::IWaitlistStore& waitlistStore,
        data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;
    const common::EnrollmentId& enrollmentId() const noexcept { return enrollmentId_; }
    const common::EnrollmentOverrideId& overrideId() const noexcept { return overrideId_; }

private:
    EnrollmentOverrideInput input_;
    const data::contracts::IUserStore& userStore_;
    const data::contracts::ICourseStore& courseStore_;
    data::contracts::IEnrollmentStore& enrollmentStore_;
    const data::contracts::IGradeStore& gradeStore_;
    data::contracts::IWaitlistStore& waitlistStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
    common::EnrollmentId enrollmentId_;
    common::EnrollmentOverrideId overrideId_;
};

class ApproveCourseChangeCommand final : public ICommand {
public:
    ApproveCourseChangeCommand(
        common::ChangeRequestId requestId, data::contracts::ICourseStore& courseStore,
        data::contracts::IChangeRequestStore& changeRequestStore,
        data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;

private:
    common::ChangeRequestId requestId_;
    data::contracts::ICourseStore& courseStore_;
    data::contracts::IChangeRequestStore& changeRequestStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

class RejectCourseChangeCommand final : public ICommand {
public:
    RejectCourseChangeCommand(
        common::ChangeRequestId requestId,
        data::contracts::IChangeRequestStore& changeRequestStore,
        data::contracts::ITransactionBoundary& transactionBoundary);
    CommandResult execute() override;

private:
    common::ChangeRequestId requestId_;
    data::contracts::IChangeRequestStore& changeRequestStore_;
    data::contracts::ITransactionBoundary& transactionBoundary_;
};

}
