# NexusEnroll — State Lifecycle Diagrams

Complete state machine specifications for all stateful entities and lifecycle workflows in the NexusEnroll domain model.

---

## 1. Enrolment & Grade Lifecycle State Machine

```mermaid
stateDiagram-v2
    [*] --> Unenrolled
    Unenrolled --> Enrolled_Active : EnrollStudentCommand / OverrideEnrollmentCommand
    Enrolled_Active --> Dropped : DropCourseCommand
    Enrolled_Active --> Grade_Pending : SubmitGradesCommand (Faculty Draft)
    Grade_Pending --> Grade_Pending : SubmitGradesCommand (Update Draft)
    Grade_Pending --> Grade_Submitted_Completed : FinalizeGradesCommand (Atomic Update)
    Grade_Submitted_Completed --> [*]
    Dropped --> [*]
```

---

## 2. Waitlist Entry Lifecycle State Machine

```mermaid
stateDiagram-v2
    [*] --> Waiting : JoinWaitlistCommand
    Waiting --> Removed : EnrollStudentCommand / OverrideEnrollmentCommand
    Removed --> [*]
```

---

## 3. Faculty Course Change Request Lifecycle State Machine

```mermaid
stateDiagram-v2
    [*] --> Pending : SubmitCourseChangeRequestCommand
    Pending --> Approved : ApproveCourseChangeCommand (Admin Review)
    Pending --> Rejected : RejectCourseChangeCommand (Admin Review)
    Approved --> [*]
    Rejected --> [*]
```

---

## 4. User Account Status Lifecycle State Machine

```mermaid
stateDiagram-v2
    [*] --> Active : CreateAccountCommand
    Active --> Inactive : DeactivateUserCommand
    Inactive --> Active : EditAccountCommand (Re-activate)
    Active --> [*]
    Inactive --> [*]
```
