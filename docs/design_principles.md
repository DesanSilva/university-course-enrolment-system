# NexusEnroll — Software Design Principles

This document maps each required software design principle to concrete code
examples in the NexusEnroll C++ codebase.

---

## 1. SOLID Principles

### 1.1 Single Responsibility Principle (SRP)

> A class should have only one reason to change.

**Example 1: `EnrollStudentCommand`**
(`include/nexusenroll/business/cqrs/commands/enroll_student_command.hpp`)

This command class is responsible for exactly one business action: enrolling a
student in a course offering. It validates prerequisites, checks capacity,
detects timetable conflicts, and executes the enrolment transaction — all as one
cohesive mutation responsibility. It does not handle HTTP parsing, JSON
serialisation, or notification delivery.

**Example 2: Separate Query and Command classes**

Read operations (`BrowseCourseCatalogueQuery`, `GetStudentScheduleQuery`) are
separated from write operations (`EnrollStudentCommand`, `DropCourseCommand`).
Each class has exactly one responsibility — either reading data or mutating state.

**Example 3: `NotificationPublisher` vs. `WaitlistNotificationObserver`**

The publisher's only responsibility is managing observers and dispatching events.
The waitlist observer's only responsibility is recording seat-available
notifications. Neither class handles enrolment logic or HTTP responses.

---

### 1.2 Open/Closed Principle (OCP)

> Software entities should be open for extension, closed for modification.

**Example 1: `INotificationObserver` hierarchy**
(`include/nexusenroll/business/notifications/seat_notification.hpp`)

New notification channels (e.g., SMS, push notifications) can be added by
implementing `INotificationObserver` and subscribing to the `NotificationPublisher`
without modifying the publisher or any existing observer:

```cpp
class SmsNotificationObserver final : public INotificationObserver {
    void notify(const CourseSeatAvailable& event) override { /* send SMS */ }
};
// In main.cpp:
notificationPublisher.subscribe(make_shared<SmsNotificationObserver>());
```

**Example 2: `ICommand` interface**
(`include/nexusenroll/business/cqrs/commands/command.hpp`)

New commands (e.g., `TransferStudentCommand`) can be added by implementing
`ICommand::execute()` without modifying existing command classes.

**Example 3: `SessionCreator` hierarchy**

Adding a new user role requires adding one `SessionCreator` subclass and one
`UserSession` subclass — no existing creator or session classes need modification.

---

### 1.3 Liskov Substitution Principle (LSP)

> Objects of a superclass should be replaceable with objects of its subclasses
> without altering the correctness of the program.

**Example: `UserSession` hierarchy**
(`include/nexusenroll/business/sessions/user_session.hpp`)

`StudentSession`, `FacultySession`, and `AdministratorSession` all extend
`UserSession`. Any session subtype can be transparently stored in a
`std::unique_ptr<UserSession>` and queried via `userId()`, `displayName()`, and
`role()` without breaking caller expectations:

```cpp
std::unique_ptr<UserSession> session = creator->createSession(userId, name);
// Works regardless of whether session is Student, Faculty, or Administrator:
session->userId();
session->displayName();
session->role();
```

The Presentation Tier stores and queries the session through the base class
pointer. Only when role-specific data is needed (e.g., `studentId()`) does a
controlled downcast occur.

---

### 1.4 Interface Segregation Principle (ISP)

> Clients should not be forced to depend on interfaces they do not use.

**Example: 8 narrow data access contracts**
(`include/nexusenroll/data/contracts/`)

Instead of one monolithic `IDataStore` interface with 50+ methods, the system
defines 8 narrow, role-focused contracts:

| Contract | Purpose | Methods |
|----------|---------|---------|
| `IUserStore` | User/Student/Faculty CRUD | `findUser()`, `findStudent()`, `createUser()`, ... |
| `ICourseStore` | Course and offering management | `findCourse()`, `browseCatalogue()`, `saveCourse()`, ... |
| `IEnrollmentStore` | Enrolment records | `findEnrollment()`, `saveEnrollment()`, ... |
| `IProgramStore` | Degree programmes | `findProgram()`, `programs()`, ... |
| `IGradeStore` | Grade records | `findGradeRecord()`, `submittedGradesForStudent()`, ... |
| `IChangeRequestStore` | Course-change requests | `findChangeRequest()`, `createChangeRequest()`, ... |
| `IWaitlistStore` | Waitlist entries | `findWaitlistEntry()`, `saveWaitlistEntry()`, ... |
| `ITransactionBoundary` | Transaction control | `executeTransaction()` |

A command that only needs user lookups and course data receives `IUserStore&` and
`ICourseStore&` — not the full set of 8 interfaces. For instance,
`BrowseCourseCatalogueQuery` depends only on `IUserStore&` and `ICourseStore&`.

---

### 1.5 Dependency Inversion Principle (DIP)

> High-level modules should not depend on low-level modules. Both should depend
> on abstractions.

**Example: `EnrollStudentCommand` constructor**
(`include/nexusenroll/business/cqrs/commands/enroll_student_command.hpp`)

```cpp
class EnrollStudentCommand final : public ICommand {
public:
    EnrollStudentCommand(
        common::StudentId studentId,
        common::OfferingId offeringId,
        const data::contracts::IUserStore& userStore,      // abstraction
        const data::contracts::ICourseStore& courseStore,   // abstraction
        data::contracts::IEnrollmentStore& enrollmentStore, // abstraction
        const data::contracts::IGradeStore& gradeStore,     // abstraction
        data::contracts::IWaitlistStore& waitlistStore,     // abstraction
        data::contracts::ITransactionBoundary& txBoundary); // abstraction
```

The command depends entirely on abstract interfaces. `MySqlDataContext` (the
concrete implementation) is injected at the Composition Root (`main.cpp`) and
never directly referenced from business code.

**Composition Root** (`main.cpp`):

```cpp
MySqlDataContext dataContext(databaseConfig.value());
// ...
EnrollStudentCommand command(
    studentId, offeringId,
    dataContext,        // passed as IUserStore&
    dataContext,        // passed as ICourseStore&
    dataContext, ...);  // passed as IEnrollmentStore&, etc.
```

---

## 2. Encapsulation

> Hide internal state and implementation details; expose only meaningful operations.

**Example: `MySqlDataContext` with Pimpl Idiom**
(`include/nexusenroll/data/mysql/mysql_data_context.hpp`)

```cpp
class MySqlDataContext final : public contracts::IUserStore,
                               public contracts::ICourseStore,
                               /* ... 6 more contracts ... */ {
private:
    class Implementation;                     // forward declaration
    std::unique_ptr<Implementation> implementation_;  // compilation firewall
};
```

The public header exposes only the abstract contract methods. Raw `MYSQL*`
handles, SQL query strings, connection pool state, and prepared statement
management are hidden inside the `Implementation` class defined only in
`src/data/mysql/mysql_data_context.cpp`. Consumers of the header never see
MySQL C-API types.

---

## 3. Programming to an Interface

> Code against abstractions, not concrete implementations.

**Example: Business commands receive interface references**

```cpp
// Business code depends on:
const data::contracts::IUserStore& userStore_;      // interface
const data::contracts::ICourseStore& courseStore_;   // interface

// Never on:
const data::mysql::MySqlDataContext& dataContext_;   // concrete class
```

This is enforced by the project structure: `include/nexusenroll/data/contracts/`
contains pure virtual interface headers, and business code under
`include/nexusenroll/business/` includes only those contracts — never the MySQL
headers.

---

## 4. Composition over Inheritance

> Favour assembling objects with composed references over deep inheritance trees.

**Example: `EnrollStudentCommand` composes services**

```cpp
class EnrollStudentCommand final : public ICommand {
private:
    common::StudentId studentId_;
    common::OfferingId offeringId_;
    const data::contracts::IUserStore& userStore_;           // composed
    const data::contracts::ICourseStore& courseStore_;        // composed
    data::contracts::IEnrollmentStore& enrollmentStore_;      // composed
    const data::contracts::IGradeStore& gradeStore_;          // composed
    data::contracts::IWaitlistStore& waitlistStore_;          // composed
    data::contracts::ITransactionBoundary& transactionBoundary_; // composed
};
```

The command composes 6 abstract service references rather than inheriting from
a base class that provides all of them. Each reference is a focused interface
(ISP) injected at construction (DIP).

Inheritance is used only where polymorphism is genuinely required:
- `ICommand` → concrete commands (Command pattern)
- `INotificationObserver` → concrete observers (Observer pattern)
- `SessionCreator` → concrete creators (Factory Method pattern)
- `UserSession` → concrete session types (Factory Method product)

---

## 5. DRY (Don't Repeat Yourself)

> Every piece of knowledge must have a single, unambiguous, authoritative
> representation within a system.

**Example 1: Centralised route helper functions**

Each route source file (`routes.cpp`, `faculty_routes.cpp`,
`administrator_routes.cpp`) uses shared file-local helpers:

- `errorResponse()` — constructs the standard `{ok: false, error: {...}}` envelope.
- `statusFor()` — maps business error codes to HTTP status codes.
- `commandResponse()` — wraps a `CommandResult` into a JSON response.
- `courseJson()`, `offeringJson()`, `scheduleJson()` — serialise domain objects.

Without these helpers, every route handler would repeat the same JSON
construction, status mapping, and error envelope logic.

**Example 2: `Result<T>` type**
(`include/nexusenroll/common/result.hpp`)

One generic result type is used across all tiers to represent success/failure
outcomes, eliminating scattered error handling patterns.

**Example 3: Student validation helper**
(`include/nexusenroll/business/cqrs/student_validation.hpp`)

Student existence and activity checks are centralised in one validation function
reused by all Student queries and commands, rather than duplicating the same
`findStudent() + findUser() + check active` logic in each class.

---

## 6. KISS (Keep It Simple, Stupid)

> Avoid unnecessary complexity.

**Example 1: Lightweight CQRS**

The CQRS implementation uses direct command objects returning a compact
`CommandResult` struct (which is `Result<void>`). There is no:
- Enterprise command bus or dispatcher middleware
- Event store database
- Asynchronous message broker
- Distributed command queue

Commands are constructed in-line within route handlers and executed immediately.

**Example 2: `CommandResult` is `Result<void>`**
(`include/nexusenroll/business/cqrs/commands/command.hpp`)

```cpp
using CommandResult = common::Result<void>;
```

Rather than a complex response hierarchy, the command result is simply a
success/failure monad. Commands that need to expose additional outcome data
(e.g., `SubmitGradesCommand::outcome()`) provide it through concrete accessors
without complicating the interface contract.

**Example 3: Observer notifications are in-memory**

For the proof of concept, notifications are recorded in a thread-safe
`NotificationLog` and surfaced via `GET /api/v1/notifications` rather than
implementing a real email/SMS delivery system. This demonstrates the Observer
pattern's decoupling without unnecessary infrastructure.
