# NexusEnroll — 10-Minute Screencast Walkthrough Script

## Introduction (1 min)
- **Title Slide**: Introduce the team and project "NexusEnroll"
- **Architecture Overview**: Briefly show the C++ CQRS backend and Vanilla JS SPA architecture.
- **Goal**: Demonstrate how the 3-Tier architecture and applied design patterns support the Student, Faculty, and Admin workflows.

## Part 1: Student Workflow & Observer Pattern (3 mins)
- **Action**: Login as `U-STU-001` (Alice Perera).
- **Demonstration**:
  - Show the student dashboard and current enrolments.
  - Navigate to the Course Catalogue and filter courses.
  - Attempt to enrol in a full course to demonstrate the **Waitlist** functionality.
  - Drop a currently enrolled course to trigger the **Observer Pattern**.
- **Explanation**: 
  - Briefly explain how the `DropCourseCommand` uses the `NotificationPublisher` to notify the `WaitlistNotificationObserver` (Observer Pattern) without coupling the enrolment logic.
  - Show the SPA receiving the waitlist offer alert dynamically.

## Part 2: Faculty Workflow & Factory Method (3 mins)
- **Action**: Logout and login as `U-FAC-001` (Dr Maya Rao).
- **Demonstration**:
  - Point out that the SPA correctly routed to the Faculty dashboard.
  - Explain how `POST /api/v1/sessions` uses the **Factory Method** (`SessionCreator` hierarchy) to return polymorphic user sessions.
  - Show the assigned course offerings and click to view a Class Roster.
  - Demonstrate the two-step Grade Submission process (saving a draft, then finalising).
  - Submit a Course Capacity Increase request.

## Part 3: Administrator Workflow & CQRS (2.5 mins)
- **Action**: Logout and login as `U-ADM-001` (Ishani Deen).
- **Demonstration**:
  - Review the pending Capacity Change request submitted by Faculty and click "Approve".
  - Demonstrate an Enrolment Override (e.g., bypassing a prerequisite rule for a student).
  - Show the **CQRS Queries** in action by generating the "Faculty Workload" and "Course Popularity" reports.
- **Explanation**:
  - Highlight how CQRS separates these read-heavy analytics reports from the complex write logic seen earlier in enrolment.

## Conclusion (0.5 mins)
- Reiterate how the combination of the C++ Command pattern, Observer pattern, and Factory Method resulted in a clean, maintainable backend.
- Conclude the presentation.
