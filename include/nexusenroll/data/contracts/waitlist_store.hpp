#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"

#include <optional>
#include <vector>

namespace nexusenroll::data::contracts {

class IWaitlistStore {
public:
    virtual ~IWaitlistStore() = default;

    virtual common::Result<std::optional<business::domain::WaitlistEntry>> findWaitlistEntry(
        common::WaitlistEntryId id) const = 0;
    virtual common::Result<std::vector<business::domain::WaitlistEntry>> waitlistEntries() const = 0;
    virtual common::Result<void> saveWaitlistEntry(business::domain::WaitlistEntry entry) = 0;
    virtual common::Result<void> removeWaitlistEntry(common::WaitlistEntryId id) = 0;
};

}
