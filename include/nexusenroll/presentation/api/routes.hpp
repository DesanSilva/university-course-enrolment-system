#pragma once

#include "crow_all.h"
#include "nexusenroll/business/sessions/demonstration_session_service.hpp"

namespace nexusenroll::presentation::api {

void registerRoutes(
    crow::SimpleApp& application,
    const business::sessions::DemonstrationSessionService& sessionService);

}
