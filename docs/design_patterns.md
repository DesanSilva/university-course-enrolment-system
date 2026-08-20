# NexusEnroll — Design Patterns

NexusEnroll formally applies three object-oriented design patterns as required by
the assignment. Each pattern is mapped to concrete C++ classes with explanations
of where and why it is used.

---

## Pattern 1: Command (Behavioural)

### Intent

Encapsulate a request as an object, thereby letting you parameterise clients with
different requests, queue or log requests, and support undoable operations.

### Where It Is Used

The Command pattern implements the mutation (write) side of CQRS. Every
state-changing business operation is encapsulated as a concrete command class that
implements the `ICommand` interface.

**Interface** (`include/nexusenroll/business/cqrs/commands/command.hpp`):

```cpp
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual CommandResult execute() = 0;
};
```

**Concrete Commands**:

| Command | Module | Responsibility |
|---------|--------|----------------|
| `EnrollStudentCommand` | Student | Validates prerequisites, capacity, and timetable conflicts; creates an enrolment record atomically within an InnoDB transaction. |
| `DropCourseCommand` | Student | Drops an active enrolment, updates seat availability, and publishes Observer notifications post-commit. |
| `JoinWaitlistCommand` | Student | Adds a student to the waitlist when a course is at capacity. |
| `SubmitGradesCommand` | Faculty | Validates and persists a batch of grades; retains valid entries when individual grades are rejected. |
| `FinalizeGradesCommand` | Faculty | Transitions all Pending grades for an offering to Submitted and marks enrolments as Completed. |
| `SubmitCourseChangeRequestCommand` | Faculty | Creates a Pending course-change request for administrator review. |
| `CreateCourseCommand` | Admin | Creates a new course in the catalogue. |
| `UpdateCourseCommand` | Admin | Applies partial updates to an existing course. |
| `DeleteCourseCommand` | Admin | Removes a course if it has no active references. |
| `CreateProgramCommand` | Admin | Creates a new degree programme. |
| `UpdateProgramCommand` | Admin | Updates programme requirements. |
| `CreateAccountCommand` | Admin | Creates a new user with a role-appropriate profile. |
| `EditAccountCommand` | Admin | Modifies an existing user account. |
| `DeactivateUserCommand` | Admin | Marks a user as inactive. |
| `OverrideEnrollmentCommand` | Admin | Force-adds a student by bypassing a specified restriction with a recorded reason. |
| `ApproveCourseChangeCommand` | Admin | Applies an approved faculty change request to the course record. |
| `RejectCourseChangeCommand` | Admin | Rejects a pending faculty change request. |

### Why It Fits NexusEnroll

Enrolment, grade submission, and administrative overrides are business actions
rather than simple CRUD calls. Representing them as command objects:

- Separates HTTP handling from business operations (SRP).
- Provides a clear mutation boundary for CQRS.
- Allows each use case to encapsulate its own validation and dependencies.
- Makes commands independently testable with mock data stores.
- Gives CQRS write operations a consistent structure.

### Assignment Demonstration

A Student enrolment request arrives as `POST /api/v1/students/{id}/enrolments`.
The Presentation route parses the JSON body, constructs an `EnrollStudentCommand`
with injected data store references, and calls `execute()`. The command:

1. Validates the student exists and is active.
2. Verifies the course offering exists.
3. Checks prerequisite requirements.
4. Checks available capacity.
5. Checks timetable conflicts.
6. Executes the enrolment within a native InnoDB transaction.
7. Returns a `CommandResult` (success or error code).

The Crow route only translates the result to a JSON HTTP response.

---

## Pattern 2: Observer (Behavioural)

### Intent

Define a one-to-many dependency between objects so that when one object changes
state, all its dependents are notified and updated automatically.

### Where It Is Used

The Observer pattern implements the decoupled notification system required by the
assignment.

**Observer Interface** (`include/nexusenroll/business/notifications/seat_notification.hpp`):

```cpp
class INotificationObserver {
public:
    virtual ~INotificationObserver() = default;
    virtual void notify(const CourseSeatAvailable& event) = 0;
    virtual void notifyDrop(const CourseDropped&) {}
};
```

**Publisher** (Subject):

```cpp
class NotificationPublisher {
public:
    void subscribe(std::shared_ptr<INotificationObserver> observer);
    void publish(const CourseSeatAvailable& event) const noexcept;
    void publishDrop(const CourseDropped& event) const noexcept;

private:
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<INotificationObserver>> observers_;
};
```

**Concrete Observers**:

| Observer | Responsibility |
|----------|----------------|
| `WaitlistNotificationObserver` | Records `CourseSeatAvailable` events; alerts waiting students that a seat has opened. |
| `AdvisorNotificationObserver` | Notifies academic advisors when an advisee drops a course. |
| `SystemAlertObserver` | Records system-level alerts (e.g., observer errors) to the shared notification log. |

### Why It Fits NexusEnroll

The assignment explicitly requires that waitlist notification be "automated and
decoupled from the core enrolment logic." When a student drops a course:

1. The `DropCourseCommand` completes the drop within an InnoDB transaction.
2. After `COMMIT`, the command publishes a `CourseSeatAvailable` event (only if
   active occupancy is now below capacity).
3. The `NotificationPublisher` iterates all subscribed observers.
4. The `WaitlistNotificationObserver` records which waiting students should be
   notified.

The drop command has no knowledge of how notifications are delivered (email, SMS,
or in-memory log). This provides:

- **Loose coupling**: Commands do not depend on notification implementation.
- **Extensibility (OCP)**: New observers can be added without modifying existing code.
- **Single Responsibility**: Each observer handles one concern.
- **Thread safety**: The publisher uses mutex-protected observer dispatch.

### Assignment Demonstration

In `main.cpp` (the Composition Root), three observers are constructed and
subscribed to the publisher:

```cpp
NotificationPublisher notificationPublisher;
auto waitlistObserver = make_shared<WaitlistNotificationObserver>(&notificationLog);
auto advisorObserver  = make_shared<AdvisorNotificationObserver>(notificationLog);
auto systemObserver   = make_shared<SystemAlertObserver>(notificationLog);
notificationPublisher.subscribe(waitlistObserver);
notificationPublisher.subscribe(advisorObserver);
notificationPublisher.subscribe(systemObserver);
```

The `NotificationLog` is surfaced via `GET /api/v1/notifications` so the SPA can
display Observer-generated alerts without sending real emails.

---

## Pattern 3: Factory Method (Creational)

### Intent

Define an interface for creating an object, but let subclasses decide which class
to instantiate. Factory Method lets a class defer instantiation to subclasses.

### Where It Is Used

Factory Method creates role-specific user session objects.

**Product Hierarchy** (`include/nexusenroll/business/sessions/user_session.hpp`):

```
UserSession (base)
    ├── StudentSession      (carries StudentId)
    ├── FacultySession      (carries FacultyId)
    └── AdministratorSession (no additional profile)
```

**Creator Hierarchy** (`include/nexusenroll/business/sessions/session_creator.hpp`):

```
SessionCreator (base)
    ├── StudentSessionCreator
    ├── FacultySessionCreator
    └── AdministratorSessionCreator
```

Each concrete creator overrides the factory method:

```cpp
class SessionCreator {
public:
    virtual ~SessionCreator() = default;
    virtual std::unique_ptr<UserSession> createSession(
        common::UserId userId, std::string displayName) const = 0;
};
```

### Why It Fits NexusEnroll

The application exposes different capabilities to Students, Faculty, and
Administrators. Without Factory Method, the Presentation Tier would contain
scattered conditional logic:

```cpp
if (role == STUDENT) {
    // construct student context, load student profile...
} else if (role == FACULTY) {
    // construct faculty context, load faculty profile...
} else if (role == ADMIN) {
    // construct admin context...
}
```

Factory Method centralises role-specific session creation. The
`DemonstrationSessionService` selects the appropriate creator based on the user's
role and delegates session construction:

```cpp
// Simplified logic inside DemonstrationSessionService::create()
switch (user.role) {
    case UserRole::Student:
        creator = std::make_unique<StudentSessionCreator>(student.id);
        break;
    case UserRole::Faculty:
        creator = std::make_unique<FacultySessionCreator>(faculty.id);
        break;
    case UserRole::Administrator:
        creator = std::make_unique<AdministratorSessionCreator>();
        break;
}
return creator->createSession(user.id, user.name);
```

This demonstrates:
- **Encapsulated object creation**: Each creator knows how to build its product.
- **Programming to an interface**: Callers work with `UserSession*` regardless of role.
- **Open/Closed Principle**: A new role adds one creator and one session subclass.
- **Separation of concerns**: Role selection is separated from session construction.

### Assignment Demonstration

`POST /api/v1/sessions` accepts a seeded User ID. The Presentation route passes
it to `DemonstrationSessionService::create()`, which uses the Factory Method
hierarchy to produce the correct `UserSession` subtype. The route extracts
profile data (e.g., `StudentId`, `FacultyId`) from the polymorphic session and
returns it as JSON.

---

## Pattern 4: Facade (Structural)

### Intent

Provide a unified interface to a set of interfaces in a subsystem. Facade defines
a higher-level interface that makes the subsystem easier to use.

### Where It Is Used

The `MySqlDataContext` class acts as a Facade for the data access tier.

**Implementation** (`include/nexusenroll/data/mysql/mysql_data_context.hpp`):

```cpp
class MySqlDataContext final : public contracts::IUserStore,
                               public contracts::ICourseStore,
                               public contracts::IEnrollmentStore,
                               public contracts::IProgramStore,
                               public contracts::IGradeStore,
                               public contracts::IChangeRequestStore,
                               public contracts::IWaitlistStore,
                               public contracts::ITransactionBoundary {
    // ...
};
```

### Why It Fits NexusEnroll

Instead of requiring the composition root to instantiate 8 separate repository
classes and manage database connections across all of them, the `MySqlDataContext`
provides a single, unified facade that satisfies all 8 data access contracts.
It encapsulates the connection pool and transaction lifecycle, drastically
simplifying initialization and dependency injection.

---

## Pattern 5: Pimpl Idiom (Structural / C++ Specific)

### Intent

Remove compilation dependencies on internal implementation details and improve
encapsulation by moving private data members into a separate, opaque struct or class.

### Where It Is Used

The `MySqlDataContext` class uses the Pimpl (Pointer to Implementation) idiom.

**Implementation** (`include/nexusenroll/data/mysql/mysql_data_context.hpp`):

```cpp
class MySqlDataContext final : /* interfaces */ {
public:
    explicit MySqlDataContext(MySqlConfig config, std::size_t poolSize = 4);
    ~MySqlDataContext();
    // ... public interface methods ...

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};
```

### Why It Fits NexusEnroll

The MySQL C-API requires complex handles (`MYSQL*`, `MYSQL_STMT*`) and structs
that would otherwise need to be included in the header file. By using the Pimpl
idiom, the `mysql.h` include is restricted entirely to the `.cpp` source file.
This creates a strict compilation firewall, preventing the low-level database
types from leaking into the Business Logic Tier and speeding up compilation times.

