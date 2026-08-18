#pragma once

#include "nexusenroll/business/domain/model.hpp"
#include "nexusenroll/common/result.hpp"

#include <optional>
#include <vector>

namespace nexusenroll::data::contracts {

class ICourseStore {
public:
    virtual ~ICourseStore() = default;

    virtual common::Result<std::optional<business::domain::Course>> findCourse(common::CourseId id) const = 0;
    virtual common::Result<std::optional<business::domain::CourseOffering>> findOffering(
        common::OfferingId id) const = 0;
    virtual common::Result<std::vector<business::domain::Course>> courses() const = 0;
    virtual common::Result<std::vector<business::domain::CourseOffering>> offerings() const = 0;
    virtual common::Result<std::vector<business::domain::CatalogueItem>> browseCatalogue(
        const business::domain::CatalogueFilter& filter) const = 0;
    virtual common::Result<std::vector<business::domain::FacultyOfferingItem>>
    assignedOfferings(common::FacultyId facultyId) const = 0;
    virtual common::Result<bool> facultyTeachesCourse(
        common::FacultyId facultyId,
        common::CourseId courseId) const = 0;
    virtual common::Result<void> saveCourse(business::domain::Course course) = 0;
    virtual common::Result<void> saveOffering(business::domain::CourseOffering offering) = 0;
};

}
