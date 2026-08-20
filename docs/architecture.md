# NexusEnroll — Architectural Analysis & Justification

## 1. Chosen Architectural Pattern

**CQRS-Flavoured 3-Tier Architecture**

NexusEnroll adopts a **3-Tier Architecture** (Presentation, Business Logic, Data
Access) with an internal **Command Query Responsibility Segregation (CQRS)**
refinement applied within the Business Logic Tier.

CQRS is not an additional architectural tier. It is an internal organising
principle that separates read (Query) and write (Command) responsibilities inside
the Business Logic Tier into distinct class hierarchies with different contracts
and execution semantics.

### Tier 1: Presentation Tier
- **Components**: Vanilla JS SPA (`frontend/`), Crow C++ REST API (`src/presentation/api/`)
- **Communication**: HTTP / JSON DTOs flow downward to the Business Logic Tier.

### Tier 2: Business Logic Tier
- **Components**:
  - **Queries (Reads)**: e.g., BrowseCatalogue, GetSchedule, GetRoster, GetReports
  - **Commands (Writes)**: e.g., EnrollStudent, DropCourse, SubmitGrades, Override
- **Core Concerns**: Domain Models, Validation, Sessions, Notification
- **Communication**: Interacts with the Data Access Tier through Abstract Data Store Interfaces.

### Tier 3: Data Access Tier
- **Components**: Abstract Contracts (`IUserStore`, `ICourseStore`, `IEnrollmentStore`, etc.), MySQL 8 / InnoDB (`MySqlDataContext` + Connection Pool)

---

## 2. Justification Over Alternative Architectures

### 2.1 Why Not Microservices?

| Concern | Microservices | 3-Tier (NexusEnroll) |
|---------|---------------|----------------------|
| **Operational complexity** | Requires service discovery, load balancing, container orchestration, distributed tracing, and circuit breakers. | Single deployable artifact; one process, one build. |
| **Transaction integrity** | Enrolment spanning Student, Offering, and Waitlist tables requires Saga patterns or 2PC, introducing eventual consistency and compensation logic. | Native InnoDB transactions provide ACID guarantees within a single database connection. |
| **Development overhead** | Each service needs its own build, test, and deployment pipeline. | One Makefile, one test suite, one binary. |
| **Suitability** | Appropriate for large organisations with independent deployment cadences and separate team ownership. | NexusEnroll is a proof-of-concept for one team, delivered within four weeks. |

### 2.2 Why Not SOA?

| Concern | SOA | 3-Tier (NexusEnroll) |
|---------|-----|----------------------|
| **ESB dependency** | SOA traditionally requires an Enterprise Service Bus for service orchestration and protocol mediation. | Direct function calls within the process eliminate network hops and serialisation overhead. |
| **Protocol overhead** | SOA commonly uses SOAP/XML with WSDL contracts. | Lightweight JSON REST over HTTP. |
| **Scale mismatch** | SOA targets enterprise-wide integration of heterogeneous systems. | NexusEnroll is a single bounded context. |

### 2.3 Why 3-Tier with CQRS Refinement?

1. **Proof-of-Concept Scope (KISS)**: A 3-Tier architecture provides clean
   separation of concerns (Presentation, Business, Data) without the distributed
   system complexity of Microservices or the heavyweight middleware of SOA.

2. **Durable ACID Enrolment Transactions**: Course registration requires strict
   atomicity — prerequisite verification, capacity checks, seat allocation, and
   waitlist retirement must succeed or fail as a unit. Native InnoDB transactions
   guarantee this without Saga choreography.

3. **Read/Write Asymmetry**: University enrolment systems experience massive read
   spikes (students browsing catalogues, checking schedules) alongside strict
   write operations (registering, dropping). CQRS separates these concerns so
   Queries execute lightweight SQL joins while Commands invoke transaction-bound
   domain validators.

4. **Future Integration Readiness**: Clear contract boundaries (abstract
   `IUserStore`, `ICourseStore`, versioned REST DTOs) ensure future systems
   (Financial Aid, Mobile App) can integrate via the same REST API or replacement
   data implementations without altering core domain logic.

---

## 3. Tier Responsibilities

### 3.1 Presentation Tier

**Location**: `frontend/`, `src/presentation/api/`, `include/nexusenroll/presentation/`

| Responsibility | Example |
|----------------|---------|
| Crow server configuration and static file serving | `main.cpp` starts Crow on port 8080 |
| HTTP route registration | `registerRoutes()`, `registerFacultyRoutes()`, `registerAdministratorRoutes()` |
| Request parsing (JSON, query parameters, path segments) | `offeringIdFromBody()`, `queryParameter()` |
| Business result → HTTP status code mapping | `statusFor()` maps `CAPACITY_FULL` → 409 |
| JSON response envelope construction | `errorResponse()`, `commandResponse()` |
| SPA rendering and user interaction | `frontend/js/app.js` calls REST endpoints via `fetch()` |

The Presentation Tier must **never** contain enrolment validation, prerequisite
checks, grade lifecycle rules, or SQL queries.

### 3.2 Business Logic Tier

**Location**: `src/business/`, `include/nexusenroll/business/`

| Responsibility | Example |
|----------------|---------|
| Domain entities and value objects | `User`, `Course`, `CourseOffering`, `Enrollment`, `GradeRecord` |
| CQRS Commands (state mutations) | `EnrollStudentCommand`, `DropCourseCommand`, `SubmitGradesCommand` |
| CQRS Queries (read-only operations) | `BrowseCourseCatalogueQuery`, `GetClassRosterQuery` |
| Business validation rules | Prerequisite checks, capacity checks, timetable-conflict detection |
| Notification event publication (Observer) | `NotificationPublisher` broadcasts `CourseSeatAvailable` events |
| Session creation (Factory Method) | `SessionCreator` hierarchy produces `UserSession` subtypes |

### 3.3 Data Access Tier

**Location**: `src/data/mysql/`, `include/nexusenroll/data/`, `database/mysql/`

| Responsibility | Example |
|----------------|---------|
| Abstract data access interfaces | `IUserStore`, `ICourseStore`, `IEnrollmentStore` (8 contracts total) |
| Concrete MySQL repository implementations | `MySqlDataContext` implements all 8 contracts |
| Native InnoDB transaction boundaries | `executeTransaction()` with `BEGIN`, `COMMIT`, `ROLLBACK` |
| Connection pool management | Bounded pool with per-thread lease semantics |
| Schema and seed migrations | `001_schema.sql`, `002_seed.sql`, `003_enrollment_overrides.sql` |

---

## 4. Dependency Rules

```
Presentation  →  Business Logic  →  Data Access Abstractions
                                          ↑
                                   Concrete Implementation
```

1. Crow route code must not contain business rules.
2. Crow-specific types (`crow::request`, `crow::json::wvalue`) must not leak
   into the Business Logic Tier.
3. Business classes operate on standard C++ objects and `Result<T>` types.
4. Business logic depends on abstract data-access interfaces, never on concrete
   `MySqlDataContext` directly.
5. `main.cpp` acts as the Composition Root, wiring concrete implementations.
6. The Data Access Tier must not depend on HTTP or frontend concepts.
7. The SPA accesses system functionality only through REST endpoints.
