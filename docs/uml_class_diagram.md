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
        -StudentId studentId_
        -OfferingId offeringId_
        -IUserStore userStore_
        -ICourseStore courseStore_
        -IEnrollmentStore enrollmentStore_
        -IGradeStore gradeStore_
        -IWaitlistStore waitlistStore_
        -ITransactionBoundary transactionBoundary_
        +execute() CommandResult
    }

    class DropCourseCommand {
        -StudentId studentId_
        -OfferingId offeringId_
        -IUserStore userStore_
        -ICourseStore courseStore_
        -IEnrollmentStore enrollmentStore_
        -IWaitlistStore waitlistStore_
        -ITransactionBoundary transactionBoundary_
        -NotificationPublisher notificationPublisher_
        +execute() CommandResult
    }

    class JoinWaitlistCommand {
        -StudentId studentId_
        -OfferingId offeringId_
        -IUserStore userStore_
        -ICourseStore courseStore_
        -IEnrollmentStore enrollmentStore_
        -IWaitlistStore waitlistStore_
        -ITransactionBoundary transactionBoundary_
        +execute() CommandResult
    }

    class SubmitGradesCommand {
        -FacultyId facultyId_
        -OfferingId offeringId_
        -vector~GradeInput~ grades_
        -IUserStore userStore_
        -ICourseStore courseStore_
        -IEnrollmentStore enrollmentStore_
        -IGradeStore gradeStore_
        -ITransactionBoundary transactionBoundary_
        +execute() CommandResult
        +outcome() GradeBatchOutcome
    }

    class FinalizeGradesCommand {
        -FacultyId facultyId_
        -OfferingId offeringId_
        -IUserStore userStore_
        -ICourseStore courseStore_
        -IEnrollmentStore enrollmentStore_
        -IGradeStore gradeStore_
        -ITransactionBoundary transactionBoundary_
        +execute() CommandResult
        +finalizedCount() size_t
    }

    class SubmitCourseChangeRequestCommand {
        -FacultyId facultyId_
        -CourseChangeInput input_
        -IUserStore userStore_
        -ICourseStore courseStore_
        -IChangeRequestStore changeRequestStore_
        +execute() CommandResult
        +requestId() ChangeRequestId
    }

    class OverrideEnrollmentCommand {
        -EnrollmentOverrideInput input_
        -IUserStore userStore_
        -ICourseStore courseStore_
        -IEnrollmentStore enrollmentStore_
        -IGradeStore gradeStore_
        -IWaitlistStore waitlistStore_
        -ITransactionBoundary transactionBoundary_
        +execute() CommandResult
        +enrollmentId() EnrollmentId
        +overrideId() EnrollmentOverrideId
    }

    class ApproveCourseChangeCommand {
        -ChangeRequestId requestId_
        -ICourseStore courseStore_
        -IChangeRequestStore changeRequestStore_
        -ITransactionBoundary transactionBoundary_
        +execute() CommandResult
    }

    class RejectCourseChangeCommand {
        -ChangeRequestId requestId_
        -IChangeRequestStore changeRequestStore_
        -ITransactionBoundary transactionBoundary_
        +execute() CommandResult
    }

    class CreateCourseCommand { +execute() CommandResult }
    class UpdateCourseCommand { +execute() CommandResult }
    class DeleteCourseCommand { +execute() CommandResult }
    class CreateProgramCommand { +execute() CommandResult }
    class UpdateProgramCommand { +execute() CommandResult }
    class CreateAccountCommand { +execute() CommandResult }
    class EditAccountCommand { +execute() CommandResult }
    class DeactivateUserCommand { +execute() CommandResult }

    class BrowseCourseCatalogueQuery {
        +execute() vector~CatalogueItem~
    }

    class GetStudentScheduleQuery {
        +execute() vector~Enrollment~
    }

    class GetClassRosterQuery {
        +execute() vector~FacultyRosterEntry~
    }

    class GetCapacityReportQuery {
        +execute() vector~CapacityReportItem~
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
    CreateCourseCommand ..|> ICommand
    UpdateCourseCommand ..|> ICommand
    DeleteCourseCommand ..|> ICommand
    CreateProgramCommand ..|> ICommand
    UpdateProgramCommand ..|> ICommand
    CreateAccountCommand ..|> ICommand
    EditAccountCommand ..|> ICommand
    DeactivateUserCommand ..|> ICommand
```

---

## 2. Creational (Factory Method) & Behavioural (Observer) Design Patterns

```mermaid
classDiagram
    class UserSession {
        <<abstract>>
        -UserId userId_
        -string displayName_
        -UserRole role_
        +userId() UserId
        +displayName() string
        +role() UserRole
    }

    class StudentSession {
        -StudentId studentId_
        +studentId() StudentId
    }

    class FacultySession {
        -FacultyId facultyId_
        +facultyId() FacultyId
    }

    class AdministratorSession {
    }

    class SessionCreator {
        <<interface>>
        +createSession(UserId, string) unique_ptr~UserSession~
    }

    class StudentSessionCreator {
        -StudentId studentId_
        +createSession(UserId, string) unique_ptr~UserSession~
    }

    class FacultySessionCreator {
        -FacultyId facultyId_
        +createSession(UserId, string) unique_ptr~UserSession~
    }

    class AdministratorSessionCreator {
        +createSession(UserId, string) unique_ptr~UserSession~
    }

    class DemonstrationSessionService {
        -IUserStore userStore_
        +create(UserId) Result~unique_ptr~UserSession~~
    }

    class INotificationObserver {
        <<interface>>
        +notify(CourseSeatAvailable) void
        +notifyDrop(CourseDropped) void
    }

    class WaitlistNotificationObserver {
        -NotificationLog* log_
        +notify(CourseSeatAvailable) void
        +notifications() vector~CourseSeatAvailable~
    }

    class AdvisorNotificationObserver {
        -NotificationLog log_
        +notifyDrop(CourseDropped) void
    }

    class SystemAlertObserver {
        -NotificationLog log_
        +recordAlert(string) void
    }

    class NotificationPublisher {
        -vector~shared_ptr~INotificationObserver~~ observers_
        +subscribe(shared_ptr~INotificationObserver~)
        +publish(CourseSeatAvailable) void
        +publishDrop(CourseDropped) void
    }

    class NotificationLog {
        -vector~NotificationAlert~ entries_
        +append(string, string) void
        +all() vector~NotificationAlert~
    }

    StudentSession --|> UserSession
    FacultySession --|> UserSession
    AdministratorSession --|> UserSession

    StudentSessionCreator ..|> SessionCreator
    FacultySessionCreator ..|> SessionCreator
    AdministratorSessionCreator ..|> SessionCreator

    StudentSessionCreator ..> StudentSession : creates
    FacultySessionCreator ..> FacultySession : creates
    AdministratorSessionCreator ..> AdministratorSession : creates

    DemonstrationSessionService ..> SessionCreator : uses

    WaitlistNotificationObserver ..|> INotificationObserver
    AdvisorNotificationObserver ..|> INotificationObserver
    SystemAlertObserver ..|> INotificationObserver

    NotificationPublisher --> INotificationObserver : notifies subscribers
    WaitlistNotificationObserver --> NotificationLog : writes to
    AdvisorNotificationObserver --> NotificationLog : writes to
    SystemAlertObserver --> NotificationLog : writes to
```

---

## 3. Data Access Tier Contracts, Facade & Pimpl Firewall

```mermaid
classDiagram
    class IUserStore { <<interface>> +findUser() +findStudent() +findFaculty() +saveUser() +createUser() }
    class ICourseStore { <<interface>> +findCourse() +findOffering() +browseCatalogue() +saveCourse() +saveOffering() }
    class IEnrollmentStore { <<interface>> +saveEnrollment() +removeEnrollment() +findStudentEnrollment() +createEnrollmentOverride() }
    class IGradeStore { <<interface>> +createGradeRecord() +saveGradeRecord() +pendingGradesForOffering() +submittedGradesForStudent() }
    class IProgramStore { <<interface>> +findProgram() +createProgram() +saveProgram() }
    class IChangeRequestStore { <<interface>> +createChangeRequest() +saveChangeRequest() +changeRequestsForFaculty() }
    class IWaitlistStore { <<interface>> +saveWaitlistEntry() +removeWaitlistEntry() +nextWaitlistPosition() +waitingEntriesForOffering() }
    class ITransactionBoundary { <<interface>> +executeTransaction(Operation) }

    class MySqlDataContext {
        <<Facade>>
        -unique_ptr~Implementation~ implementation_
        +verifyConnections()
    }

    class Implementation {
        <<Pimpl Firewall>>
        -MySqlPool connectionPool
    }

    MySqlDataContext ..|> IUserStore
    MySqlDataContext ..|> ICourseStore
    MySqlDataContext ..|> IEnrollmentStore
    MySqlDataContext ..|> IGradeStore
    MySqlDataContext ..|> IProgramStore
    MySqlDataContext ..|> IChangeRequestStore
    MySqlDataContext ..|> IWaitlistStore
    MySqlDataContext ..|> ITransactionBoundary

    MySqlDataContext *-- Implementation : owns (Pimpl)
```

---

## 4. Domain Entities & Domain Model Relationships

```mermaid
classDiagram
    class User {
        +UserId id
        +string name
        +string email
        +UserStatus status
        +UserRole role
    }

    class Student {
        +StudentId id
        +UserId userId
        +ProgramId programId
    }

    class Faculty {
        +FacultyId id
        +UserId userId
        +string department
    }

    class Course {
        +CourseId id
        +string code
        +string department
        +string courseNumber
        +string name
        +string description
        +unsigned int credits
        +vector~CourseId~ prerequisiteCourseIds
    }

    class CourseOffering {
        +OfferingId id
        +CourseId courseId
        +string semester
        +FacultyId instructorId
        +size_t capacity
        +size_t enrolledCount
        +vector~ScheduleSlot~ schedule
    }

    class Enrollment {
        +EnrollmentId id
        +StudentId studentId
        +OfferingId offeringId
        +EnrollmentStatus status
    }

    class DegreeProgram {
        +ProgramId id
        +string name
        +string department
        +vector~CourseId~ requiredCourseIds
        +unsigned int requiredCredits
    }

    class GradeRecord {
        +GradeRecordId id
        +StudentId studentId
        +OfferingId offeringId
        +CourseId courseId
        +string grade
        +GradeLifecycle lifecycle
    }

    class CourseChangeRequest {
        +ChangeRequestId id
        +FacultyId facultyId
        +CourseId courseId
        +optional~OfferingId~ offeringId
        +CourseChangeType type
        +CourseChangeStatus status
        +string requestedValue
    }

    class WaitlistEntry {
        +WaitlistEntryId id
        +StudentId studentId
        +OfferingId offeringId
        +size_t position
        +WaitlistStatus status
    }

    class EnrollmentOverride {
        +EnrollmentOverrideId id
        +UserId administratorUserId
        +EnrollmentId enrollmentId
        +EnrollmentRule bypassedRule
        +string reason
    }

    Student "1" -- "1" User : maps to
    Faculty "1" -- "1" User : maps to
    Student "1" -- "1" DegreeProgram : enrolled in
    Student "1" -- "*" Enrollment : possesses
    CourseOffering "1" -- "*" Enrollment : accepts
    CourseOffering "1" -- "1" Course : instance of
    GradeRecord "*" -- "1" Student : belongs to
    GradeRecord "*" -- "1" CourseOffering : for
    CourseOffering "1" -- "*" WaitlistEntry : queues
    CourseChangeRequest "*" -- "1" Faculty : proposed by
    CourseChangeRequest "*" -- "1" Course : targets
    EnrollmentOverride "*" -- "1" Enrollment : overrides
```
