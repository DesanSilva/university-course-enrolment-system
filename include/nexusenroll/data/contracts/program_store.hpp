#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"

#include <optional>
#include <vector>

namespace nexusenroll::data::contracts {

class IProgramStore {
public:
    virtual ~IProgramStore() = default;

    virtual common::Result<std::optional<business::domain::DegreeProgram>> findProgram(
        common::ProgramId id) const = 0;
    virtual common::Result<std::vector<business::domain::DegreeProgram>> programs() const = 0;
    virtual common::Result<void> createProgram(business::domain::DegreeProgram program) = 0;
    virtual common::Result<void> saveProgram(business::domain::DegreeProgram program) = 0;
};

}
