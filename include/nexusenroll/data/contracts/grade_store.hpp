#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"

#include <optional>
#include <vector>

namespace nexusenroll::data::contracts {

class IGradeStore {
public:
    virtual ~IGradeStore() = default;

    virtual common::Result<std::optional<business::domain::GradeRecord>> findGradeRecord(
        common::GradeRecordId id) const = 0;
    virtual common::Result<std::vector<business::domain::GradeRecord>> gradeRecords() const = 0;
    virtual common::Result<void> saveGradeRecord(business::domain::GradeRecord record) = 0;
};

}
