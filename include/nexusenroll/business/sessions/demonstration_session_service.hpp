#pragma once

#include "nexusenroll/business/sessions/user_session.hpp"
#include "nexusenroll/common/result.hpp"
#include "nexusenroll/data/contracts/user_store.hpp"

#include <memory>

namespace nexusenroll::business::sessions {

class DemonstrationSessionService {
public:
    explicit DemonstrationSessionService(const data::contracts::IUserStore& userStore);

    common::Result<std::unique_ptr<UserSession>> create(common::UserId userId) const;

private:
    const data::contracts::IUserStore& userStore_;
};

}
