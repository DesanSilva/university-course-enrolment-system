# NexusEnroll — Interaction Sequence Diagrams

Exhaustive sequence diagrams for all primary commands, queries, and system interactions across Student, Faculty, Administrator, and Session modules.

---

## 1. Polymorphic Session Creation via Factory Method Pattern (`POST /api/v1/sessions`)

```mermaid
sequenceDiagram
    actor Client as SPA / Browser
    participant Router as api_routes
    participant Demo as DemonstrationSessionService
    participant Store as MySqlDataContext (Facade)
    participant Creator as StudentSessionCreator
    participant Product as StudentSession

    Client->>Router: POST /api/v1/sessions {userId: 101}
    Router->>Demo: create(userId)
    Demo->>Store: findUser(userId)
    Store-->>Demo: User
    Demo->>Store: findStudentByUserId(userId)
    Store-->>Demo: Student Profile
    Demo->>Creator: StudentSessionCreator(studentId)
    Creator->>Product: createSession(userId, name)
    Product-->>Creator: unique_ptr<UserSession>
    Creator-->>Demo: unique_ptr<UserSession>
    Demo-->>Router: Session DTO JSON
    Router-->>Client: 200 OK {token, role, sessionId}
```

---

## 2. Student Course Enrolment Transaction (`EnrollStudentCommand`)

```mermaid
sequenceDiagram
    actor Student
    participant SPA as Web SPA
    participant Router as api_routes
    participant Command as EnrollStudentCommand
    participant Tx as ITransactionBoundary
    participant Store as MySqlDataContext (Facade)

    Student->>SPA: Click "Enroll"
    SPA->>Router: POST /api/v1/students/{id}/enrolments
    Router->>Command: execute()
    activate Command
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: findUser(studentId) (validateActiveStudent)
    Tx->>Store: findOffering(offeringId)
    Tx->>Store: findStudentEnrollment(studentId, offeringId)
    Tx->>Store: findCourse(courseId)
    Tx->>Store: submittedGradesForStudent(studentId)
    Note right of Tx: Validates active, prerequisites, capacity, time conflicts
    Tx->>Store: activeEnrollmentsForStudent(studentId)
    Tx->>Store: findStudentWaitlistEntry(studentId, offeringId)
    Tx->>Store: saveEnrollment(record)
    Note right of Tx: Retire Waitlist Entry if present
    Tx->>Store: saveWaitlistEntry(retiredRecord)
    Tx-->>Command: Transaction Committed
    deactivate Tx
    Command-->>Router: CommandResult::success()
    deactivate Command
    Router-->>SPA: 200 OK (JSON Enrolment DTO)
    SPA-->>Student: Display "Enrolled Successfully"
```

---

## 3. Student Course Drop & Decoupled Waitlist Notification (`DropCourseCommand`)

```mermaid
sequenceDiagram
    actor Student
    participant SPA as Web SPA
    participant Router as api_routes
    participant Command as DropCourseCommand
    participant Tx as ITransactionBoundary
    participant Store as MySqlDataContext
    participant Pub as NotificationPublisher
    participant Obs as WaitlistNotificationObserver

    Student->>SPA: Click "Drop Course"
    SPA->>Router: DELETE /api/v1/students/{id}/enrolments/{offeringId}
    Router->>Command: execute()
    activate Command
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: findUser(studentId) (validateActiveStudent)
    Tx->>Store: findOffering(offeringId)
    Tx->>Store: findStudentEnrollment(studentId, offeringId)
    Tx->>Store: saveEnrollment(droppedRecord)
    Tx->>Store: waitingEntriesForOffering(offeringId)
    Tx-->>Command: Transaction Committed
    deactivate Tx
    
    Note right of Command: Observer pattern dispatches post-commit
    Command->>Pub: publish(CourseSeatAvailable)
    activate Pub
    Pub->>Obs: notify(CourseSeatAvailable)
    activate Obs
    Obs->>Store: log_.append("WAITLIST_SEAT", message)
    Obs-->>Pub: Observer Handled
    deactivate Obs
    Pub-->>Command: All Observers Notified
    deactivate Pub
    Command->>Pub: publishDrop(CourseDropped)
    Command-->>Router: CommandResult::success()
    deactivate Command
    Router-->>SPA: 200 OK (JSON)
    SPA-->>Student: Update UI (Seat Dropped)
```

---

## 4. Student Join Waitlist Workflow (`JoinWaitlistCommand`)

```mermaid
sequenceDiagram
    actor Student
    participant SPA as Web SPA
    participant Router as api_routes
    participant Command as JoinWaitlistCommand
    participant Tx as ITransactionBoundary
    participant Store as MySqlDataContext

    Student->>SPA: Click "Join Waitlist"
    SPA->>Router: POST /api/v1/students/{id}/waitlist
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: findUser(studentId) (validateActiveStudent)
    Tx->>Store: findOffering(offeringId)
    Tx->>Store: findStudentEnrollment(studentId, offeringId)
    Tx->>Store: findStudentWaitlistEntry(studentId, offeringId)
    Tx->>Store: nextWaitlistPosition(offeringId)
    Store-->>Tx: Position N
    Tx->>Store: saveWaitlistEntry(record)
    Tx-->>Command: Transaction Committed
    deactivate Tx
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 200 OK {position: N}
    SPA-->>Student: Display "Waitlisted at Position #N"
```

---

## 5. Student Read-Only Catalogue Query (`BrowseCourseCatalogueQuery`)

```mermaid
sequenceDiagram
    actor Student
    participant SPA as Web SPA
    participant Router as api_routes
    participant Query as BrowseCourseCatalogueQuery
    participant Store as MySqlDataContext

    Student->>SPA: View Course Catalogue
    SPA->>Router: GET /api/v1/students/{id}/catalogue?search=CS
    Router->>Query: execute(filter)
    Query->>Store: browseCatalogue(filter)
    Store-->>Query: List<CatalogueItem>
    Query-->>Router: Catalogue DTO JSON
    Router-->>SPA: 200 OK (JSON List)
    SPA-->>Student: Display Catalogue Cards
```

---

## 6. Faculty Grade Batch Draft Submission (`SubmitGradesCommand`)

```mermaid
sequenceDiagram
    actor Faculty
    participant SPA as Web SPA
    participant Router as api_routes
    participant Command as SubmitGradesCommand
    participant Tx as ITransactionBoundary
    participant Store as MySqlDataContext

    Faculty->>SPA: Input Grade Batch & Click "Save Draft"
    SPA->>Router: POST /api/v1/faculty/{id}/offerings/{offeringId}/grades
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: findUser(facultyId) (validateActiveFaculty)
    Tx->>Store: findOffering(offeringId)
    loop For Each Candidate Grade
        Tx->>Store: findStudentEnrollment(studentId, offeringId)
        Tx->>Store: findStudentGradeRecord(studentId, offeringId)
        Tx->>Store: saveGradeRecord(record) / createGradeRecord(record)
    end
    Tx-->>Command: Transaction Committed
    deactivate Tx
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 200 OK (Batch Outcome JSON)
    SPA-->>Faculty: Display "Grades Saved as Draft"
```

---

## 7. Faculty Atomic Grade Finalisation (`FinalizeGradesCommand`)

```mermaid
sequenceDiagram
    actor Faculty
    participant SPA as Web SPA
    participant Router as api_routes
    participant Command as FinalizeGradesCommand
    participant Tx as ITransactionBoundary
    participant Store as MySqlDataContext

    Faculty->>SPA: Click "Finalize Grades"
    SPA->>Router: POST /api/v1/faculty/{id}/offerings/{offeringId}/grades/finalize
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: findUser(facultyId) (validateActiveFaculty)
    Tx->>Store: findOffering(offeringId)
    Tx->>Store: pendingGradesForOffering(offeringId)
    loop For Each Pending Grade
        Tx->>Store: findStudentEnrollment(studentId, offeringId)
        Tx->>Store: saveGradeRecord(submittedGradeRecord)
        Tx->>Store: saveEnrollment(completedEnrollmentRecord)
    end
    Tx-->>Command: Transaction Committed
    deactivate Tx
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 200 OK (Grades Finalized)
    SPA-->>Faculty: Display "Grades Approved & Enrolments Completed"
```

---

## 8. Faculty Course Change Request Submission (`SubmitCourseChangeRequestCommand`)

```mermaid
sequenceDiagram
    actor Faculty
    participant SPA as Web SPA
    participant Router as api_routes
    participant Command as SubmitCourseChangeRequestCommand
    participant Tx as ITransactionBoundary
    participant Store as MySqlDataContext

    Faculty->>SPA: Submit Change Proposal (Capacity + Description)
    SPA->>Router: POST /api/v1/faculty/{id}/course-change-requests
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: findUser(facultyId) (validateActiveFaculty)
    Tx->>Store: findCourse(courseId)
    Tx->>Store: createChangeRequest(requestRecord)
    Tx-->>Command: Transaction Committed
    deactivate Tx
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 200 OK {requestId}
    SPA-->>Faculty: Display "Proposal Submitted for Admin Review"
```

---

## 9. Administrator Enrolment Force-Add Override (`OverrideEnrollmentCommand`)

```mermaid
sequenceDiagram
    actor Admin
    participant SPA as Web SPA
    participant Router as api_routes
    participant Command as OverrideEnrollmentCommand
    participant Tx as ITransactionBoundary
    participant Store as MySqlDataContext

    Admin->>SPA: Force Enrol Student (Bypass Prereqs)
    SPA->>Router: POST /api/v1/admin/enrolment-overrides
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: findUser(administratorUserId) (activeAdministrator)
    Tx->>Store: findUser(studentId) (validateActiveStudent)
    Tx->>Store: findOffering(offeringId)
    Tx->>Store: saveEnrollment(record)
    Tx->>Store: createEnrollmentOverride(overrideRecord)
    Tx-->>Command: Transaction Committed
    deactivate Tx
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 200 OK (Override Complete)
    SPA-->>Admin: Display "Force Enrolment Audit Entry Recorded"
```

---

## 10. Administrator Course Change Approval Workflow (`ApproveCourseChangeCommand`)

```mermaid
sequenceDiagram
    actor Admin
    participant SPA as Web SPA
    participant Router as api_routes
    participant Command as ApproveCourseChangeCommand
    participant Tx as ITransactionBoundary
    participant Store as MySqlDataContext

    Admin->>SPA: Click "Approve Request"
    SPA->>Router: POST /api/v1/admin/change-requests/{id}/approve
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: findChangeRequest(requestId)
    Tx->>Store: findCourse(courseId)
    Tx->>Store: saveCourse(updatedCourse) / updateOfferingCapacity()
    Tx->>Store: saveChangeRequest(approvedRequest)
    Tx-->>Command: Transaction Committed
    deactivate Tx
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 200 OK (Request Approved)
    SPA-->>Admin: Display "Changes Applied to Course Offering"
```

---

## 11. Administrator Course Change Rejection (`RejectCourseChangeCommand`)

```mermaid
sequenceDiagram
    actor Admin
    participant SPA as Web SPA
    participant Router as api_routes
    participant Command as RejectCourseChangeCommand
    participant Tx as ITransactionBoundary
    participant Store as MySqlDataContext

    Admin->>SPA: Click "Reject Request"
    SPA->>Router: POST /api/v1/admin/change-requests/{id}/reject
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: findChangeRequest(requestId)
    Tx->>Store: saveChangeRequest(rejectedRequest)
    Tx-->>Command: Transaction Committed
    deactivate Tx
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 200 OK (Request Rejected)
    SPA-->>Admin: Display "Faculty Change Request Rejected"
```

---

## 12. Administrator Provision User Account (`CreateAccountCommand`)

```mermaid
sequenceDiagram
    actor Admin
    participant SPA as Web SPA
    participant Router as api_routes
    participant Command as CreateAccountCommand
    participant Tx as ITransactionBoundary
    participant Store as MySqlDataContext

    Admin->>SPA: Create New Student/Faculty Account
    SPA->>Router: POST /api/v1/admin/users
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: findUser(userId)
    Tx->>Store: createUser(userRecord)
    Tx->>Store: createStudent(profileRecord) / createFaculty(profileRecord)
    Tx-->>Command: Transaction Committed
    deactivate Tx
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 201 Created {userId: 205}
    SPA-->>Admin: Display "User Provisioned Successfully"
```

---

## 13. Administrator Read-Only CQRS Capacity Utilization Query (`GetCapacityReportQuery`)

```mermaid
sequenceDiagram
    actor Admin
    participant SPA as Web SPA
    participant Router as api_routes
    participant Query as GetCapacityReportQuery
    participant Store as MySqlDataContext

    Admin->>SPA: View Capacity Utilization Report
    SPA->>Router: GET /api/v1/admin/reports/capacity?minUtilization=0.85
    Router->>Query: execute()
    Query->>Store: offerings()
    Query->>Store: findCourse(courseId)
    Query->>Store: findFaculty(instructorId)
    Query->>Store: findUser(faculty->userId)
    Query-->>Router: Capacity DTO JSON
    Router-->>SPA: 200 OK (Capacity Projections)
    SPA-->>Admin: Display Over-utilized Course Charts
```
