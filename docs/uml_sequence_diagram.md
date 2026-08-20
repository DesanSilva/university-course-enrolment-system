# NexusEnroll — Interaction Sequence Diagrams

Exhaustive sequence diagrams for all primary commands, queries, and system interactions across Student, Faculty, Administrator, and Session modules.

---

## 1. Polymorphic Session Creation via Factory Method Pattern (`POST /api/v1/sessions`)

```mermaid
sequenceDiagram
    actor Client as SPA / Browser
    participant Router as api_routes
    participant Demo as DemonstrationSessionService
    participant Creator as StudentSessionCreator
    participant Product as StudentSession

    Client->>Router: POST /api/v1/sessions {userId: 101, role: "Student"}
    Router->>Demo: createSession(userId, role)
    Demo->>Creator: StudentSessionCreator()
    Creator->>Product: createSession(userId, displayName)
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
    participant Val as StudentValidation
    participant Store as MySqlDataContext (Facade)
    participant Tx as ITransactionBoundary

    Student->>SPA: Click "Enroll"
    SPA->>Router: POST /api/v1/students/{id}/enrolments
    Router->>Command: execute()
    activate Command
    Command->>Val: validateStudentActive() & validatePrerequisites() & checkTimeConflict()
    Val-->>Command: Validation Success
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: lockOfferingForUpdate(offeringId)
    Tx->>Store: saveEnrollment(record)
    Tx->>Store: incrementOfferingEnrolled(offeringId)
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
    participant Store as MySqlDataContext
    participant Tx as ITransactionBoundary
    participant Pub as NotificationPublisher
    participant Obs as WaitlistNotificationObserver

    Student->>SPA: Click "Drop Course"
    SPA->>Router: DELETE /api/v1/students/{id}/enrolments/{offeringId}
    Router->>Command: execute()
    activate Command
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: removeEnrollment(studentId, offeringId)
    Tx->>Store: decrementOfferingEnrolled(offeringId)
    Tx-->>Command: Transaction Committed
    deactivate Tx
    Command->>Pub: publish(CourseSeatAvailable{offeringId})
    activate Pub
    Pub->>Obs: notify(CourseSeatAvailable)
    activate Obs
    Obs->>Store: getWaitlistEntries(offeringId)
    Obs-->>Pub: Observer Handled
    deactivate Obs
    Pub-->>Command: All Observers Notified
    deactivate Pub
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
    participant Store as MySqlDataContext

    Student->>SPA: Click "Join Waitlist"
    SPA->>Router: POST /api/v1/students/{id}/waitlist
    Router->>Command: execute()
    Command->>Store: getNextWaitlistPosition(offeringId)
    Store-->>Command: Position 3
    Command->>Store: addWaitlistEntry(studentId, offeringId, position=3)
    Store-->>Command: Saved
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 200 OK {position: 3}
    SPA-->>Student: Display "Waitlisted at Position #3"
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
    Query->>Store: findCoursesByFilter(filter)
    Store-->>Query: List<CourseOfferingDto>
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
    participant Store as MySqlDataContext

    Faculty->>SPA: Input Grade Batch & Click "Save Draft"
    SPA->>Router: POST /api/v1/faculty/{id}/offerings/{offeringId}/grades
    Router->>Command: execute()
    loop For Each Grade Record
        Command->>Store: saveGradeRecord(enrollmentId, letter, status="Pending")
    end
    Store-->>Command: All Batch Items Persisted
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 200 OK (Draft Saved)
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
    participant Store as MySqlDataContext
    participant Tx as ITransactionBoundary

    Faculty->>SPA: Click "Finalize Grades"
    SPA->>Router: POST /api/v1/faculty/{id}/offerings/{offeringId}/grades/finalize
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: updateGradeStatusByOffering(offeringId, "Submitted")
    Tx->>Store: updateEnrollmentStatusByOffering(offeringId, "Completed")
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
    participant Store as MySqlDataContext

    Faculty->>SPA: Submit Change Proposal (Capacity + Description)
    SPA->>Router: POST /api/v1/faculty/{id}/course-change-requests
    Router->>Command: execute()
    Command->>Store: createChangeRequest(offeringId, facultyId, type, status="Pending")
    Store-->>Command: Saved
    Command-->>Router: CommandResult::success()
    Router-->>SPA: 200 OK {status: "Pending"}
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
    participant Store as MySqlDataContext
    participant Tx as ITransactionBoundary

    Admin->>SPA: Force Enrol Student (Bypass Prereqs)
    SPA->>Router: POST /api/v1/admin/enrolment-overrides
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: recordOverrideAuditEntry(studentId, offeringId, adminId, reason)
    Tx->>Store: saveEnrollment(studentId, offeringId, status="Enrolled_Active")
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
    participant Store as MySqlDataContext
    participant Tx as ITransactionBoundary

    Admin->>SPA: Click "Approve Request"
    SPA->>Router: POST /api/v1/admin/change-requests/{id}/approve
    Router->>Command: execute()
    Command->>Tx: executeTransaction(lambda)
    activate Tx
    Tx->>Store: updateOfferingDetails(offeringId, requestedChanges)
    Tx->>Store: updateChangeRequestStatus(requestId, "Approved")
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
    participant Store as MySqlDataContext

    Admin->>SPA: Click "Reject Request"
    SPA->>Router: POST /api/v1/admin/change-requests/{id}/reject
    Router->>Command: execute()
    Command->>Store: updateChangeRequestStatus(requestId, "Rejected")
    Store-->>Command: Status Updated
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
    participant Store as MySqlDataContext

    Admin->>SPA: Create New Student/Faculty Account
    SPA->>Router: POST /api/v1/admin/users
    Router->>Command: execute()
    Command->>Store: saveUser(email, role, status="Active")
    Store-->>Command: UserId 205
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
    Query->>Store: queryOfferingsWithCapacityUtilization(0.85)
    Store-->>Query: List<CapacityReportDto>
    Query-->>Router: Capacity DTO JSON
    Router-->>SPA: 200 OK (Capacity Projections)
    SPA-->>Admin: Display Over-utilized Course Charts
```
