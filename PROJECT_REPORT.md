# NexusEnroll - Project Report

## Overview
NexusEnroll is a complete University Course Enrolment System featuring a C++ backend (Crow framework) and a Vanilla JavaScript Single Page Application (SPA) frontend. 

The frontend successfully integrates with the REST API to provide dedicated workflows for Students, Faculty, and Administrators.

## Architecture
- **Backend:** C++ (Crow framework) providing RESTful JSON APIs and static file serving on Port 8080.
- **Database:** MySQL relational database.
- **Frontend:** Vanilla HTML5, CSS3 (with CSS Variables for theming), and ES6 JavaScript. The SPA architecture relies on a central `app.js` file for DOM manipulation, hash-based routing, and asynchronous API calls using `fetch()`.

## Features Implemented

### 1. Student Portal
- **Dashboard:** Overview of active enrolments and waitlisted courses.
- **Course Catalogue:** Browse and search for courses with live seat availability.
- **Enrolment:** One-click enrolment and waitlisting system.
- **Schedule:** View and drop currently active courses.
- **Academic Progress:** Track degree requirements and completed credits.

### 2. Faculty Portal
- **My Offerings:** View assigned courses for the semester.
- **Course Roster:** View all enrolled and waitlisted students for a specific offering.
- **Grade Management:** A two-step process to draft and officially submit final grades for students.
- **Change Requests:** Submit requests to administration (e.g., requesting a capacity increase).

### 3. Administrator Dashboard
- **User Management:** View all system users (Students, Faculty, Admins) and deactivate accounts.
- **Course Catalog Management:** View all available courses, departments, and credit requirements.
- **Requests Inbox:** Review, approve, or reject capacity change requests submitted by Faculty.
- **System Reports:** Generate data tables for System Enrolment, Course Popularity, Faculty Workload, and Capacity Issues.

## API Integration Details
The frontend `app.js` acts as the API client, mapping frontend actions to the backend endpoints:
- **Authentication:** `POST /api/v1/sessions` (Mock session system)
- **Data Retrieval:** `GET` requests for catalogues, schedules, rosters, and reports.
- **Mutations:** `POST` and `DELETE` requests for enrolling, dropping, submitting grades, and approving requests.

## How to Run
1. Ensure the MySQL database is running and seeded with `001_schema.sql` and `002_seed.sql`.
2. Compile the backend using `make`.
3. Start the server using: `make run`
4. Open a web browser and navigate to `http://localhost:8080`.
5. Login using seed IDs: `U-STU-001` (Student), `U-FAC-001` (Faculty), or `U-ADM-001` (Admin).
