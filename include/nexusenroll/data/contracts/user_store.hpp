#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"

#include <optional>
#include <vector>

namespace nexusenroll::data::contracts {

class IUserStore {
public:
    virtual ~IUserStore() = default;

    virtual common::Result<std::optional<business::domain::User>> findUser(common::UserId id) const = 0;
    virtual common::Result<std::optional<business::domain::Student>> findStudent(common::StudentId id) const = 0;
    virtual common::Result<std::optional<business::domain::Faculty>> findFaculty(common::FacultyId id) const = 0;

    virtual common::Result<std::vector<business::domain::User>> users() const = 0;
    virtual common::Result<std::vector<business::domain::Student>> students() const = 0;
    virtual common::Result<std::vector<business::domain::Faculty>> facultyMembers() const = 0;

    virtual common::Result<void> saveUser(business::domain::User user) = 0;
    virtual common::Result<void> saveStudent(business::domain::Student student) = 0;
    virtual common::Result<void> saveFaculty(business::domain::Faculty faculty) = 0;
};

}
