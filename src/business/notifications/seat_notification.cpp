#include "nexusenroll/business/notifications/seat_notification.hpp"

#include <utility>

namespace nexusenroll::business::notifications {

using namespace std;

void NotificationPublisher::subscribe(shared_ptr<INotificationObserver> observer) {
    if (!observer) {
        return;
    }
    lock_guard<mutex> lock(mutex_);
    observers_.push_back(move(observer));
}

void NotificationPublisher::publish(const CourseSeatAvailable& event) const noexcept {
    vector<shared_ptr<INotificationObserver>> observers;
    {
        lock_guard<mutex> lock(mutex_);
        observers = observers_;
    }
    for (const auto& observer : observers) {
        try {
            observer->notify(event);
        } catch (...) {
            // A committed mutation remains successful even if one notification sink fails.
        }
    }
}

void WaitlistNotificationObserver::notify(const CourseSeatAvailable& event) {
    lock_guard<mutex> lock(mutex_);
    notifications_.push_back(event);
}

vector<CourseSeatAvailable> WaitlistNotificationObserver::notifications() const {
    lock_guard<mutex> lock(mutex_);
    return notifications_;
}

}
