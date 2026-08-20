#include "nexusenroll/business/notifications/seat_notification.hpp"

#include <chrono>
#include <utility>

namespace nexusenroll::business::notifications {

using namespace std;
using namespace chrono;

// ---------------------------------------------------------------------------
// NotificationLog
// ---------------------------------------------------------------------------

void NotificationLog::append(string type, string message) {
    const auto now = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    lock_guard<mutex> lock(mutex_);
    if (entries_.size() >= 200) {
        entries_.erase(entries_.begin());
    }
    entries_.push_back({move(type), move(message), static_cast<long long>(now)});
}

vector<NotificationAlert> NotificationLog::all() const {
    lock_guard<mutex> lock(mutex_);
    return entries_;
}

// ---------------------------------------------------------------------------
// NotificationPublisher
// ---------------------------------------------------------------------------

void NotificationPublisher::subscribe(shared_ptr<INotificationObserver> observer) {
    if (!observer) {
        return;
    }
    lock_guard<mutex> lock(mutex_);
    observers_.push_back(move(observer));
}

void NotificationPublisher::publish(const CourseSeatAvailable& event) const noexcept {
    vector<shared_ptr<INotificationObserver>> snapshot;
    {
        lock_guard<mutex> lock(mutex_);
        snapshot = observers_;
    }
    for (const auto& observer : snapshot) {
        try {
            observer->notify(event);
        } catch (...) {
            // A committed mutation remains successful even if one notification sink fails.
        }
    }
}

void NotificationPublisher::publishDrop(const CourseDropped& event) const noexcept {
    vector<shared_ptr<INotificationObserver>> snapshot;
    {
        lock_guard<mutex> lock(mutex_);
        snapshot = observers_;
    }
    for (const auto& observer : snapshot) {
        try {
            observer->notifyDrop(event);
        } catch (...) {
            // Observer failures must not reverse committed database work.
        }
    }
}

// ---------------------------------------------------------------------------
// WaitlistNotificationObserver
// ---------------------------------------------------------------------------

WaitlistNotificationObserver::WaitlistNotificationObserver(NotificationLog* log)
    : log_(log) {}

void WaitlistNotificationObserver::notify(const CourseSeatAvailable& event) {
    // Record a UI-visible alert for each waiting student if log is attached.
    if (log_) {
        for (const auto& studentId : event.waitingStudentIds) {
            log_->append(
                "WAITLIST_SEAT",
                "A seat opened in offering " + event.offeringId.value() +
                " — student " + studentId.value() + " is next on the waitlist.");
        }
    }
    lock_guard<mutex> lock(mutex_);
    notifications_.push_back(event);
}

vector<CourseSeatAvailable> WaitlistNotificationObserver::notifications() const {
    lock_guard<mutex> lock(mutex_);
    return notifications_;
}

// ---------------------------------------------------------------------------
// AdvisorNotificationObserver
// ---------------------------------------------------------------------------

AdvisorNotificationObserver::AdvisorNotificationObserver(NotificationLog& log)
    : log_(log) {}

void AdvisorNotificationObserver::notifyDrop(const CourseDropped& event) {
    // In production this would dispatch an email to the student's academic
    // advisor. For this proof of concept the alert is recorded in the shared
    // notification log and surfaced via the REST notification endpoint.
    log_.append(
        "ADVISOR_DROP",
        "Student " + event.studentId.value() +
        " dropped offering " + event.offeringId.value() +
        ". Their academic advisor has been notified.");
}

// ---------------------------------------------------------------------------
// SystemAlertObserver
// ---------------------------------------------------------------------------

SystemAlertObserver::SystemAlertObserver(NotificationLog& log)
    : log_(log) {}

void SystemAlertObserver::recordAlert(const string& message) {
    log_.append("SYSTEM", message);
}

}
