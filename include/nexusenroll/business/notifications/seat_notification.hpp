#pragma once

#include "nexusenroll/common/identifiers.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace nexusenroll::business::notifications {

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

// Published by DropCourseCommand after a successful drop that opens capacity.
struct CourseSeatAvailable {
    common::OfferingId offeringId;
    std::vector<common::StudentId> waitingStudentIds;
};

// Published by DropCourseCommand for every successful drop so advisors are
// informed when an advisee leaves a course.
struct CourseDropped {
    common::StudentId studentId;
    common::OfferingId offeringId;
};

// ---------------------------------------------------------------------------
// Alert log – shared in-memory record consumed by the REST notification
// endpoint.  Each entry is produced by one of the concrete observers.
// ---------------------------------------------------------------------------

struct NotificationAlert {
    std::string type;    // e.g. "WAITLIST_SEAT", "ADVISOR_DROP", "SYSTEM"
    std::string message;
    long long   timestampMs; // milliseconds since epoch
};

// Thread-safe, append-only log capped at 200 recent entries.
class NotificationLog {
public:
    void append(std::string type, std::string message);
    // Returns a snapshot of all recorded alerts.
    std::vector<NotificationAlert> all() const;

private:
    mutable std::mutex mutex_;
    std::vector<NotificationAlert> entries_;
};

// ---------------------------------------------------------------------------
// Observer abstraction
// ---------------------------------------------------------------------------

class INotificationObserver {
public:
    virtual ~INotificationObserver() = default;
    virtual void notify(const CourseSeatAvailable& event) = 0;
    virtual void notifyDrop(const CourseDropped&) {}
};

// ---------------------------------------------------------------------------
// Publisher
// ---------------------------------------------------------------------------

class NotificationPublisher {
public:
    void subscribe(std::shared_ptr<INotificationObserver> observer);
    // Publish a seat-available event (fired after drop releases capacity).
    void publish(const CourseSeatAvailable& event) const noexcept;
    // Publish a course-dropped event (fired for every successful drop).
    void publishDrop(const CourseDropped& event) const noexcept;

private:
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<INotificationObserver>> observers_;
};

// ---------------------------------------------------------------------------
// Concrete observers
// ---------------------------------------------------------------------------

// Records CourseSeatAvailable events and writes an alert to the shared log so
// waiting students can be shown a UI notification.
class WaitlistNotificationObserver final : public INotificationObserver {
public:
    explicit WaitlistNotificationObserver(NotificationLog* log = nullptr);

    void notify(const CourseSeatAvailable& event) override;
    void notifyDrop(const CourseDropped&) override {}  // not relevant here

    std::vector<CourseSeatAvailable> notifications() const;

private:
    NotificationLog* log_;
    mutable std::mutex mutex_;
    std::vector<CourseSeatAvailable> notifications_;
};

// Notifies academic advisors whenever an advisee drops a course.  In this
// proof of concept the notification is recorded in the shared alert log
// instead of sending a real email.
class AdvisorNotificationObserver final : public INotificationObserver {
public:
    explicit AdvisorNotificationObserver(NotificationLog& log);

    void notify(const CourseSeatAvailable&) override {} // not relevant here
    void notifyDrop(const CourseDropped& event) override;

private:
    NotificationLog& log_;
};

// Records system-level alerts (e.g. observer errors) to the shared log.
class SystemAlertObserver final : public INotificationObserver {
public:
    explicit SystemAlertObserver(NotificationLog& log);

    void notify(const CourseSeatAvailable&) override {}  // not used here
    void notifyDrop(const CourseDropped&) override {}    // not used here
    // Called directly (not via the ICommand flow) to record system errors.
    void recordAlert(const std::string& message);

private:
    NotificationLog& log_;
};

}
