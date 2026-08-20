# NexusEnroll — Structural UML Class Architecture

The structural design of **NexusEnroll** mirrors all components, contracts, commands, queries, patterns, and data persistence models across the codebase.

---

## 1. Presentation Tier & Business CQRS Structure

```mermaid
classDiagram
    class api_routes {
        +registerRoutes()
        +registerFacultyRoutes()
        +registerAdministratorRoutes()
    }

    class ICommand {
        <<interface>>
        +execute() CommandResult
    }

    class EnrollStudentCommand {
        -StudentId studentId
        -OfferingId offeringId
        +execute() CommandResult
    }

    class DropCourseCommand {
        -StudentId studentId
        -OfferingId offeringId
        +execute() CommandResult
    }

    class JoinWaitlistCommand {
        -StudentId studentId
        -OfferingId offeringId
        +execute() CommandResult
    }

    class SubmitGradesCommand {
        -OfferingId offeringId
        -vector~GradeRecord~ records
        +execute() CommandResult
    }

    class FinalizeGradesCommand {
        -OfferingId offeringId
        +execute() CommandResult
    }

    class SubmitCourseChangeRequestCommand {
        -FacultyId facultyId
        -OfferingId offeringId
        -ChangeType type
        +execute() CommandResult
    }

    class OverrideEnrollmentCommand {
        -StudentId studentId
        -OfferingId offeringId
        -string reason
        +execute() CommandResult
    }

    class ApproveCourseChangeCommand {
        -ChangeRequestId requestId
        +execute() CommandResult
    }

    class RejectCourseChangeCommand {
        -ChangeRequestId requestId
        +execute() CommandResult
    }

    class BrowseCourseCatalogueQuery {
        +execute() CatalogueDto
    }

    class GetStudentScheduleQuery {
        +execute() ScheduleDto
    }

    class GetClassRosterQuery {
        +execute() RosterDto
    }

    class GetCapacityReportQuery {
        +execute() CapacityReportDto
    }

    api_routes --> ICommand : Dispatches
    api_routes --> BrowseCourseCatalogueQuery : Executes
    api_routes --> GetStudentScheduleQuery : Executes
    api_routes --> GetClassRosterQuery : Executes
    api_routes --> GetCapacityReportQuery : Executes

    EnrollStudentCommand ..|> ICommand
    DropCourseCommand ..|> ICommand
    JoinWaitlistCommand ..|> ICommand
    SubmitGradesCommand ..|> ICommand
    FinalizeGradesCommand ..|> ICommand
    SubmitCourseChangeRequestCommand ..|> ICommand
    OverrideEnrollmentCommand ..|> ICommand
    ApproveCourseChangeCommand ..|> ICommand
    RejectCourseChangeCommand ..|> ICommand
```

---

## 2. Creational (Factory Method) & Behavioural (Observer) Design Patterns

```mermaid
classDiagram
    class UserSession {
        <<abstract>>
        +userId() UserId
        +role() UserRole
    }

    class StudentSession {
        -StudentId studentId
        +studentId() StudentId
    }

    class FacultySession {
        -FacultyId facultyId
        +facultyId() FacultyId
    }

    class AdministratorSession {
        -AdminId adminId
        +adminId() AdminId
    }

    class SessionCreator {
        <<interface>>
        +createSession(UserId, string) unique_ptr~UserSession~
    }

    class StudentSessionCreator {
        +createSession(UserId, string) unique_ptr~UserSession~
    }

    class FacultySessionCreator {
        +createSession(UserId, string) unique_ptr~UserSession~
    }

    class AdministratorSessionCreator {
        +createSession(UserId, string) unique_ptr~UserSession~
    }

    class INotificationObserver {
        <<interface>>
        +notify(CourseSeatAvailable)
        +notifyDrop(CourseDropped)
    }

    class WaitlistNotificationObserver {
        +notify(CourseSeatAvailable)
    }

    class AdvisorNotificationObserver {
        +notifyDrop(CourseDropped)
    }

    class SystemAlertObserver {
        +notify(CourseSeatAvailable)
    }

    class NotificationPublisher {
        -vector~INotificationObserver*~ observers
        +subscribe(INotificationObserver*)
        +publish(CourseSeatAvailable)
        +publishDrop(CourseDropped)
    }

    StudentSession --|> UserSession
    FacultySession --|> UserSession
    AdministratorSession --|> UserSession

    StudentSessionCreator ..|> SessionCreator
    FacultySessionCreator ..|> SessionCreator
    AdministratorSessionCreator ..|> SessionCreator

    StudentSessionCreator ..> StudentSession : Instantiates
    FacultySessionCreator ..> FacultySession : Instantiates
    AdministratorSessionCreator ..> AdministratorSession : Instantiates

    WaitlistNotificationObserver ..|> INotificationObserver
    AdvisorNotificationObserver ..|> INotificationObserver
    SystemAlertObserver ..|> INotificationObserver

    NotificationPublisher --> INotificationObserver : Notifies Subscribers
```

---

## 3. Data Access Tier Contracts, Facade & Pimpl Firewall

```mermaid
classDiagram
    class IUserStore { <<interface>> +findUserById() }
    class ICourseStore { <<interface>> +findCourseById() +findOfferingById() }
    class IEnrollmentStore { <<interface>> +saveEnrollment() +removeEnrollment() }
    class IGradeStore { <<interface>> +saveGrade() +finalizeGrades() }
    class IProgramStore { <<interface>> +findProgramById() }
    class IChangeRequestStore { <<interface>> +saveChangeRequest() }
    class IWaitlistStore { <<interface>> +addWaitlistEntry() }
    class ITransactionBoundary { <<interface>> +executeTransaction() }

    class MySqlDataContext {
        <<Facade>>
        -unique_ptr~Implementation~ pimpl
        +executeTransaction()
        +findUserById()
        +saveEnrollment()
    }

    class Implementation {
        <<Pimpl Firewall>>
        -MySqlPool connectionPool
        +executeTransactionImpl()
        +executeQueryImpl()
    }

    MySqlDataContext ..|> IUserStore
    MySqlDataContext ..|> ICourseStore
    MySqlDataContext ..|> IEnrollmentStore
    MySqlDataContext ..|> IGradeStore
    MySqlDataContext ..|> IProgramStore
    MySqlDataContext ..|> IChangeRequestStore
    MySqlDataContext ..|> IWaitlistStore
    MySqlDataContext ..|> ITransactionBoundary

    MySqlDataContext *-- Implementation : Owns Pimpl
```

---

## 4. Domain Entities & Domain Model Relationships

```mermaid
classDiagram
    class User {
        +UserId id
        +string email
        +UserRole role
        +UserStatus status
    }

    class Student {
        +StudentId id
        +UserId userId
        +ProgramId programId
        +int yearOfStudy
    }

    class Course {
        +CourseId id
        +string code
        +string title
        +int credits
        +vector~CourseId~ prerequisiteIds
    }

    class CourseOffering {
        +OfferingId id
        +CourseId courseId
        +FacultyId instructorId
        +int capacity
        +int enrolledCount
        +ScheduleSchedule schedule
    }

    class Enrollment {
        +EnrollmentId id
        +StudentId studentId
        +OfferingId offeringId
        +EnrollmentStatus status
    }

    class GradeRecord {
        +GradeId id
        +EnrollmentId enrollmentId
        +string gradeLetter
        +GradeStatus status
    }

    class CourseChangeRequest {
        +ChangeRequestId id
        +OfferingId offeringId
        +FacultyId requestedBy
        +ChangeType type
        +RequestStatus status
    }

    class WaitlistEntry {
        +WaitlistId id
        +OfferingId offeringId
        +StudentId studentId
        +int position
    }

    class EnrollmentOverride {
        +OverrideId id
        +StudentId studentId
        +OfferingId offeringId
        +AdminId overriddenBy
        +string reason
    }

    Student "1" -- "1" User : Maps to
    Student "1" -- "*" Enrollment : Possesses
    CourseOffering "1" -- "*" Enrollment : Accepts
    Enrollment "1" -- "0..1" GradeRecord : Yields
    CourseOffering "1" -- "*" WaitlistEntry : Queues
    CourseOffering "1" -- "*" CourseChangeRequest : Proposes
    EnrollmentOverride "*" -- "1" Student : Applies to
```
