#pragma once

#include "nexusenroll/common/identifiers.hpp"

#include <memory>
#include <mutex>
#include <vector>

namespace nexusenroll::business::notifications {

struct CourseSeatAvailable {
    common::OfferingId offeringId;
    std::vector<common::StudentId> waitingStudentIds;
};

class INotificationObserver {
public:
    virtual ~INotificationObserver() = default;
    virtual void notify(const CourseSeatAvailable& event) = 0;
};

class NotificationPublisher {
public:
    void subscribe(std::shared_ptr<INotificationObserver> observer);
    void publish(const CourseSeatAvailable& event) const noexcept;

private:
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<INotificationObserver>> observers_;
};

class WaitlistNotificationObserver final : public INotificationObserver {
public:
    void notify(const CourseSeatAvailable& event) override;
    std::vector<CourseSeatAvailable> notifications() const;

private:
    mutable std::mutex mutex_;
    std::vector<CourseSeatAvailable> notifications_;
};

}
