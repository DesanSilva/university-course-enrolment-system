# NexusEnroll

A university course enrolment proof-of-concept demonstrating a **CQRS-flavoured
3-Tier Architecture** implemented in C++17 with a Vanilla JavaScript SPA
front-end.

---

## Overview

NexusEnroll is the modernisation project for "Nexus University" legacy enrolment
system. It demonstrates how software design principles and object-oriented design
patterns translate directly into maintainable, testable application code.

Three user modules are implemented end-to-end:

| Module | Representative capabilities |
|---|---|
| **Student** | Browse catalogue, enrol/drop courses, view schedule and academic progress, join/leave waitlist |
| **Faculty** | View assigned offerings, class roster, submit/finalise grades, submit course-change requests |
| **Administrator** | Manage courses, programmes and accounts, override enrolment rules, approve/reject change requests, run analytics reports |

The system validates all business rules in the C++ back-end (the browser never
duplicates logic) and exposes every operation through a versioned JSON REST API
that is consumed by the SPA and is reusable by any future mobile client.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        PRESENTATION TIER                        │
│                                                                 │
│   Vanilla JS SPA (frontend/)      Crow REST API (src/presentation/) │
└──────────────────────────┬──────────────────────────────────────┘
                           │  JSON / DTOs
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                      BUSINESS LOGIC TIER                        │
│                                                                 │
│   ┌──────────────────┐        ┌──────────────────┐             │
│   │  CQRS – Queries  │        │  CQRS – Commands │             │
│   │  (read-only)     │        │  (state-changing)│             │
│   └──────────────────┘        └──────────────────┘             │
│                                                                 │
│   Domain rules · Sessions (Factory Method) · Observer          │
└──────────────────────────┬──────────────────────────────────────┘
                           │  abstract data contracts
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                       DATA ACCESS TIER                          │
│                                                                 │
│   InnoDB transactions · MySQL repositories · Schema/seed        │
└─────────────────────────────────────────────────────────────────┘
```

### Design Patterns

| Pattern | Category | Where applied |
|---|---|---|
| **Command** | Behavioural | Every state-changing operation (`EnrollStudentCommand`, `DropCourseCommand`, `SubmitGradesCommand`, `OverrideEnrollmentCommand`, …) implements `ICommand::execute()`. |
| **Observer** | Behavioural | `NotificationPublisher` broadcasts `CourseSeatAvailable` events to `INotificationObserver` implementations (e.g. `WaitlistNotificationObserver`) after a successful drop transaction. |
| **Factory Method** | Creational | `SessionCreator` hierarchy (`StudentSessionCreator`, `FacultySessionCreator`, `AdministratorSessionCreator`) produces role-specific `UserSession` subtypes without scattering role checks into route code. |

---

## Project Structure

```
.
├── main.cpp                        Composition root – wires concrete
│                                   implementations and starts Crow
├── Makefile
├── setup.sh                        Installs system build dependencies
├── database/
│   └── mysql/
│       ├── 001_schema.sql          Normalised InnoDB schema (17 tables)
│       ├── 002_seed.sql            Deterministic demonstration data
│       └── 003_enrollment_overrides.sql  Additive migration for override audit
├── include/
│   ├── crow_all.h                  Vendored Crow single-header (v1.3.2)
│   └── nexusenroll/
│       ├── common/                 Strong ID types, Result<T> alias
│       ├── business/
│       │   ├── domain/             Domain model structs and enums
│       │   ├── cqrs/
│       │   │   ├── commands/       ICommand + concrete command headers
│       │   │   └── queries/        Query type definitions
│       │   ├── notifications/      Observer pattern (publisher + observers)
│       │   └── sessions/           Factory Method (creators + session types)
│       ├── data/
│       │   ├── contracts/          Abstract data-access interfaces
│       │   └── mysql/              Concrete MySQL header
│       └── presentation/
│           └── api/                Route registration headers
├── src/
│   ├── business/
│   │   ├── cqrs/
│   │   │   ├── commands/           student_commands, faculty_commands,
│   │   │   │                       administrator_commands
│   │   │   └── queries/            student_queries, faculty_queries,
│   │   │                           administrator_queries
│   │   ├── domain/                 Schedule-overlap logic
│   │   ├── notifications/          NotificationPublisher, WaitlistObserver
│   │   └── sessions/               Session creators and demo service
│   ├── data/
│   │   └── mysql/                  MySqlDataContext (115 KB, all contracts)
│   │                               MySqlConnection/pool
│   └── presentation/
│       └── api/                    routes.cpp, faculty_routes.cpp,
│                                   administrator_routes.cpp
├── frontend/
│   ├── index.html                  SPA shell (login + role-based views)
│   ├── css/style.css               Modern-retro light theme
│   └── js/app.js                   Vanilla JS, fetch-based API client
├── tests/
│   ├── domain_data_tests.cpp       Schedule-overlap and domain model
│   ├── business_session_tests.cpp  Factory Method + session service
│   ├── student_business_tests.cpp  Enrolment commands, Observer
│   ├── faculty_business_tests.cpp  Grade commands, roster queries
│   ├── administrator_business_tests.cpp  Admin commands + report queries
│   └── mysql/                      MySQL integration tests
└── report/
    └── ASSIGNMENT.md               Original assignment specification
```

---

## Tech Stack

| Layer | Technology |
|---|---|
| Back-end language | C++17 |
| HTTP framework | [Crow v1.3.2](https://crowcpp.org/) |
| Database | MySQL 8 / InnoDB |
| Database connector | MySQL Connector/C (`libmysqlclient`) |
| Build system | GNU Make |
| Front-end | HTML5, Vanilla CSS, Vanilla JavaScript (no framework) |
| Typography | [Inter](https://fonts.google.com/specimen/Inter) via Google Fonts |
| Icons | [Material Symbols Outlined](https://fonts.google.com/icons) via Google Fonts |
| TLS/Crypto | OpenSSL (`libssl`, `libcrypto`) — required by Crow |

### External Libraries

| Library | License | Purpose |
|---|---|---|
| [Crow v1.3.2](https://github.com/CrowCpp/Crow) | BSD 3-Clause | C++ REST micro-framework; vendored as `include/crow_all.h` |
| [Boost](https://www.boost.org/) | Boost Software License | Required by Crow for ASIO networking |
| [OpenSSL](https://openssl.org/) | Apache-2.0 | Required by Crow for optional TLS support |
| [libmysqlclient](https://dev.mysql.com/doc/c-api/en/) | GPL-2.0 (system package) | MySQL C connector for data persistence |

---

## Prerequisites

| Requirement | Notes |
|---|---|
| Linux (Debian/Ubuntu recommended) | Build scripts target `apt` |
| GCC 10+ or Clang 12+ (C++17 support) | Tested with `g++` |
| MySQL Server 8.x | Must be running before the application starts |
| Boost development headers | `libboost-all-dev` |
| OpenSSL development headers | `libssl-dev` |
| MySQL client development headers | `libmysqlclient-dev` |

---

## Setup

### 1. Install system dependencies

```bash
chmod +x setup.sh
./setup.sh
```

`setup.sh` installs `g++`, `libboost-all-dev`, `libmysqlclient-dev`,
`libssl-dev`, `make`, and `mysql-server` via `apt`, then downloads the Crow
single-header to `include/crow_all.h`.

> If you manage dependencies manually, ensure `include/crow_all.h` is present
> before running `make`.

### 2. Configure the database connection

Create a `.env` file in the repository root (it is git-ignored):

```bash
NEXUSENROLL_DB_HOST=127.0.0.1
NEXUSENROLL_DB_NAME=nexusenroll
NEXUSENROLL_DB_USER=nexusenrolluser
NEXUSENROLL_DB_PASSWORD=<your_password>
NEXUSENROLL_DB_SOCKET=
```

Set `NEXUSENROLL_DB_SOCKET` to the MySQL socket path if you want a Unix socket
connection; leave it empty to use TCP via `NEXUSENROLL_DB_HOST`.

### 3. Create the database and apply schema

```bash
# Example using a root/admin account to create the schema
mysql -u root -p < database/mysql/001_schema.sql
mysql -u root -p < database/mysql/003_enrollment_overrides.sql
```

Or, if you have set `MYSQL_ARGS` (e.g. `-u root -p`):

```bash
make mysql-schema MYSQL_ARGS="-u root -p"
```

### 4. Seed demonstration data

```bash
mysql -u root -p < database/mysql/002_seed.sql
```

Or:

```bash
make mysql-seed MYSQL_ARGS="-u root -p"
```

---

## Building

Export the environment variables before building so Make can verify the MySQL
path if needed:

```bash
set -a && source .env && set +a
make
```

The binary is produced at `build/nexusenroll`.

### XAMPP / non-standard MySQL header location

If your MySQL headers are not on the default include path (e.g. XAMPP on Linux):

```bash
make MYSQL_CPPFLAGS='-I/opt/lampp/include' \
     MYSQL_LDLIBS='/usr/lib/x86_64-linux-gnu/libmysqlclient.so.21'
```

---

## Running

```bash
set -a && source .env && set +a
make run
```

The server listens on `http://localhost:8080`.

Open your browser at `http://localhost:8080` to load the SPA.

### Demonstration user IDs (seeded)

| User ID | Name | Role |
|---|---|---|
| `U-ADM-001` | Ishani Deen | Administrator |
| `U-FAC-001` | Dr Maya Rao | Faculty (CS) |
| `U-FAC-002` | Prof Omar Khan | Faculty (Business) |
| `U-STU-001` | Alice Perera | Student (CS) |
| `U-STU-002` | Ben Fernando | Student (Business) |

Enter any of these IDs on the login screen. The SPA will call `POST /api/v1/sessions`
and redirect to the role-appropriate interface.

---

## API Reference

All endpoints are prefixed with `/api/v1`. Responses use the envelope:

```json
{ "ok": true,  "data": {} }
{ "ok": false, "error": { "code": "CAPACITY_FULL", "message": "..." } }
```

### Health

| Method | Path |
|---|---|
| `GET` | `/api/v1/health` |

### Session (Factory Method entry point)

| Method | Path |
|---|---|
| `POST` | `/api/v1/sessions` |

### Student

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/v1/students/{id}/catalogue` | Browse course catalogue |
| `GET` | `/api/v1/students/{id}/schedule` | View semester schedule |
| `GET` | `/api/v1/students/{id}/progress` | Academic progress |
| `GET` | `/api/v1/students/{id}/waitlist` | Waitlist entries |
| `POST` | `/api/v1/students/{id}/enrolments` | Enrol in a course |
| `DELETE` | `/api/v1/students/{id}/enrolments/{offeringId}` | Drop a course |
| `POST` | `/api/v1/students/{id}/waitlist` | Join waitlist |
| `DELETE` | `/api/v1/students/{id}/waitlist/{offeringId}` | Leave waitlist |

### Faculty

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/v1/faculty/{id}/offerings` | Assigned course offerings |
| `GET` | `/api/v1/faculty/{id}/offerings/{offeringId}/roster` | Class roster |
| `GET` | `/api/v1/faculty/{id}/offerings/{offeringId}/grades` | Grade submission state |
| `POST` | `/api/v1/faculty/{id}/offerings/{offeringId}/grades` | Submit/update grade batch |
| `POST` | `/api/v1/faculty/{id}/offerings/{offeringId}/grades/submit` | Finalise pending grades |
| `GET` | `/api/v1/faculty/{id}/course-change-requests` | View submitted requests |
| `POST` | `/api/v1/faculty/{id}/course-change-requests` | Submit a change request |

### Administrator

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/v1/admin/courses` | List courses |
| `POST` | `/api/v1/admin/courses` | Create course |
| `PATCH` | `/api/v1/admin/courses/{id}` | Edit course |
| `DELETE` | `/api/v1/admin/courses/{id}` | Delete course |
| `GET` | `/api/v1/admin/programs` | List degree programmes |
| `POST` | `/api/v1/admin/programs` | Create programme |
| `PATCH` | `/api/v1/admin/programs/{id}` | Edit programme |
| `GET` | `/api/v1/admin/users` | List users (optional `?role=`) |
| `POST` | `/api/v1/admin/users` | Create user account |
| `PATCH` | `/api/v1/admin/users/{id}` | Edit user account |
| `POST` | `/api/v1/admin/users/{id}/deactivate` | Deactivate account |
| `GET` | `/api/v1/admin/course-change-requests` | List change requests |
| `POST` | `/api/v1/admin/course-change-requests/{id}/approve` | Approve request |
| `POST` | `/api/v1/admin/course-change-requests/{id}/reject` | Reject request |
| `POST` | `/api/v1/admin/enrolment-overrides` | Override enrolment restriction |
| `GET` | `/api/v1/admin/reports/enrolment` | Enrolment statistics |
| `GET` | `/api/v1/admin/reports/faculty-workload` | Faculty workload report |
| `GET` | `/api/v1/admin/reports/course-popularity` | Course popularity report |
| `GET` | `/api/v1/admin/reports/capacity` | Capacity utilisation report |

---

## Testing

```bash
set -a && source .env && set +a

# Unit tests only (no database required)
make unit-test

# MySQL integration tests (database must be running and seeded)
make mysql-test

# Both
make test
```

On systems with non-standard MySQL headers:

```bash
make unit-test MYSQL_CPPFLAGS='-I/opt/lampp/include' \
               MYSQL_LDLIBS='/usr/lib/x86_64-linux-gnu/libmysqlclient.so.21'
```

### Test suites

| Binary | Tests |
|---|---|
| `domain_tests` | Schedule-overlap validation, domain model |
| `business_session_tests` | Factory Method, `DemonstrationSessionService` |
| `student_business_tests` | Enrolment/drop commands, Observer notifications, waitlist |
| `faculty_business_tests` | Grade batch submission, finalisation, change request creation |
| `administrator_business_tests` | Course/programme/account management, override, report queries |
| `mysql_connection_tests` | MySQL connection pool, InnoDB transaction commit/rollback, integration |

---

## Makefile Targets

| Target | Description |
|---|---|
| `make` / `make all` | Build `build/nexusenroll` |
| `make run` | Build and run the server |
| `make unit-test` | Compile and run all business/domain unit test binaries |
| `make mysql-test` | Compile and run MySQL integration tests |
| `make test` | Run unit-test + mysql-test |
| `make mysql-schema` | Apply `001_schema.sql` and `003_enrollment_overrides.sql` |
| `make mysql-seed` | Apply `002_seed.sql` |
| `make clean` | Remove the `build/` directory |

---

## Credits

- **Crow** — CrowCpp contributors, BSD 3-Clause licence.
  <https://github.com/CrowCpp/Crow>
- **Boost.Asio** — Christopher M. Kohlhoff and Boost contributors, Boost
  Software Licence.
  <https://www.boost.org/>
- **Inter typeface** — Rasmus Andersson, SIL Open Font Licence 1.1.
  <https://rsms.me/inter/>
- **Material Symbols** — Google LLC, Apache 2.0 licence.
  <https://fonts.google.com/icons>
