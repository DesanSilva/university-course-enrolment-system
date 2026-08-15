#include "nexusenroll/data/mysql/mysql_data_context.hpp"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nexusenroll::data::mysql {

using namespace common;
using namespace std;

namespace {

using namespace business::domain;

template <typename T>
Result<T> failure(const Error& error) {
    return Result<T>::failure(error.code, error.message);
}

template <typename T>
Result<T> mappingFailure(const string& message) {
    return Result<T>::failure("MYSQL_DATA_MAPPING_FAILED", message);
}

string quoted(MySqlConnection& connection, const string& value) {
    return "'" + connection.escape(value) + "'";
}

size_t parseSize(const string& value) {
    const auto parsed = stoull(value);
    if (parsed > numeric_limits<size_t>::max()) {
        throw out_of_range("stored number exceeds size_t");
    }
    return static_cast<size_t>(parsed);
}

unsigned int parseUnsigned(const string& value) {
    const auto parsed = stoul(value);
    if (parsed > numeric_limits<unsigned int>::max()) {
        throw out_of_range("stored number exceeds unsigned int");
    }
    return static_cast<unsigned int>(parsed);
}

int parseTimeMinutes(const string& value) {
    if (value.size() < 5 || value[2] != ':') {
        throw invalid_argument("invalid stored time");
    }
    return stoi(value.substr(0, 2)) * 60 + stoi(value.substr(3, 2));
}

DayOfWeek parseDay(const string& value) {
    const unsigned int day = parseUnsigned(value);
    if (day < 1 || day > 7) {
        throw invalid_argument("invalid stored day of week");
    }
    return static_cast<DayOfWeek>(day - 1);
}

string userRoleSql(UserRole role) {
    switch (role) {
    case UserRole::Student:
        return "STUDENT";
    case UserRole::Faculty:
        return "FACULTY";
    case UserRole::Administrator:
        return "ADMINISTRATOR";
    }
    throw invalid_argument("unknown user role");
}

UserRole parseUserRole(const string& value) {
    if (value == "STUDENT") {
        return UserRole::Student;
    }
    if (value == "FACULTY") {
        return UserRole::Faculty;
    }
    if (value == "ADMINISTRATOR") {
        return UserRole::Administrator;
    }
    throw invalid_argument("invalid stored user role");
}

string userStatusSql(UserStatus status) {
    return status == UserStatus::Active ? "ACTIVE" : "INACTIVE";
}

UserStatus parseUserStatus(const string& value) {
    if (value == "ACTIVE") {
        return UserStatus::Active;
    }
    if (value == "INACTIVE") {
        return UserStatus::Inactive;
    }
    throw invalid_argument("invalid stored user status");
}

string enrollmentStatusSql(EnrollmentStatus status) {
    switch (status) {
    case EnrollmentStatus::Active:
        return "ACTIVE";
    case EnrollmentStatus::Dropped:
        return "DROPPED";
    case EnrollmentStatus::Completed:
        return "COMPLETED";
    }
    throw invalid_argument("unknown enrolment status");
}

EnrollmentStatus parseEnrollmentStatus(const string& value) {
    if (value == "ACTIVE") {
        return EnrollmentStatus::Active;
    }
    if (value == "DROPPED") {
        return EnrollmentStatus::Dropped;
    }
    if (value == "COMPLETED") {
        return EnrollmentStatus::Completed;
    }
    throw invalid_argument("invalid stored enrolment status");
}

string gradeLifecycleSql(GradeLifecycle lifecycle) {
    return lifecycle == GradeLifecycle::Pending ? "PENDING" : "SUBMITTED";
}

GradeLifecycle parseGradeLifecycle(const string& value) {
    if (value == "PENDING") {
        return GradeLifecycle::Pending;
    }
    if (value == "SUBMITTED") {
        return GradeLifecycle::Submitted;
    }
    throw invalid_argument("invalid stored grade lifecycle");
}

string changeTypeSql(CourseChangeType type) {
    switch (type) {
    case CourseChangeType::Description:
        return "DESCRIPTION";
    case CourseChangeType::Prerequisites:
        return "PREREQUISITES";
    case CourseChangeType::Capacity:
        return "CAPACITY";
    }
    throw invalid_argument("unknown change type");
}

CourseChangeType parseChangeType(const string& value) {
    if (value == "DESCRIPTION") {
        return CourseChangeType::Description;
    }
    if (value == "PREREQUISITES") {
        return CourseChangeType::Prerequisites;
    }
    if (value == "CAPACITY") {
        return CourseChangeType::Capacity;
    }
    throw invalid_argument("invalid stored change type");
}

string changeStatusSql(CourseChangeStatus status) {
    switch (status) {
    case CourseChangeStatus::Pending:
        return "PENDING";
    case CourseChangeStatus::Approved:
        return "APPROVED";
    case CourseChangeStatus::Rejected:
        return "REJECTED";
    }
    throw invalid_argument("unknown change status");
}

CourseChangeStatus parseChangeStatus(const string& value) {
    if (value == "PENDING") {
        return CourseChangeStatus::Pending;
    }
    if (value == "APPROVED") {
        return CourseChangeStatus::Approved;
    }
    if (value == "REJECTED") {
        return CourseChangeStatus::Rejected;
    }
    throw invalid_argument("invalid stored change status");
}

string waitlistStatusSql(WaitlistStatus status) {
    switch (status) {
    case WaitlistStatus::Waiting:
        return "WAITING";
    case WaitlistStatus::Offered:
        return "OFFERED";
    case WaitlistStatus::Removed:
        return "REMOVED";
    }
    throw invalid_argument("unknown waitlist status");
}

WaitlistStatus parseWaitlistStatus(const string& value) {
    if (value == "WAITING") {
        return WaitlistStatus::Waiting;
    }
    if (value == "OFFERED") {
        return WaitlistStatus::Offered;
    }
    if (value == "REMOVED") {
        return WaitlistStatus::Removed;
    }
    throw invalid_argument("invalid stored waitlist status");
}

Result<string> lookupSingleValue(
    MySqlConnection& connection,
    const string& sql,
    const string& description) {
    auto rows = connection.query(sql);
    if (!rows) {
        return failure<string>(rows.error());
    }
    if (rows.value().empty() || rows.value().front().empty()) {
        return Result<string>::failure(
            "MISSING_REFERENCE", description + " does not exist.");
    }
    return Result<string>::success(rows.value().front().front());
}

Result<void> requireRole(
    MySqlConnection& connection,
    const UserId& userId,
    UserRole role) {
    auto rows = connection.query(
        "SELECT role FROM users WHERE user_id = " + quoted(connection, userId.value()));
    if (!rows) {
        return failure<void>(rows.error());
    }
    if (rows.value().empty()) {
        return Result<void>::failure("MISSING_REFERENCE", "The referenced user does not exist.");
    }
    if (rows.value().front().front() != userRoleSql(role)) {
        return Result<void>::failure("INVALID_RECORD", "The user role does not match the profile.");
    }
    return Result<void>::success();
}

Result<void> requireCompatibleRole(
    MySqlConnection& connection,
    const UserId& userId,
    UserRole role) {
    auto rows = connection.query(
        "SELECT EXISTS(SELECT 1 FROM students WHERE user_id = " +
        quoted(connection, userId.value()) + "), EXISTS(SELECT 1 FROM faculty WHERE user_id = " +
        quoted(connection, userId.value()) + ")");
    if (!rows) {
        return failure<void>(rows.error());
    }
    if (rows.value().empty() || rows.value().front().size() != 2) {
        return Result<void>::failure(
            "MYSQL_DATA_MAPPING_FAILED", "A user-profile query returned an unexpected shape.");
    }
    const auto& row = rows.value().front();
    if ((row[0] == "1" && role != UserRole::Student) ||
        (row[1] == "1" && role != UserRole::Faculty)) {
        return Result<void>::failure(
            "INVALID_RECORD", "A user role must remain consistent with its profile.");
    }
    return Result<void>::success();
}

Result<void> ensureAffected(MySqlConnection& connection, const string& description) {
    auto rows = connection.query("SELECT ROW_COUNT()");
    if (!rows) {
        return failure<void>(rows.error());
    }
    if (rows.value().empty() || rows.value().front().empty() || rows.value().front().front() == "0") {
        return Result<void>::failure("RECORD_NOT_FOUND", description + " does not exist.");
    }
    return Result<void>::success();
}

class ConnectionPool {
public:
    class Lease {
    public:
        Lease() = default;
        Lease(ConnectionPool& pool, size_t index) : pool_(&pool), index_(index) {}
        ~Lease() { release(); }

        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;

        Lease(Lease&& other) noexcept : pool_(other.pool_), index_(other.index_) {
            other.pool_ = nullptr;
        }

        Lease& operator=(Lease&& other) noexcept {
            if (this != &other) {
                release();
                pool_ = other.pool_;
                index_ = other.index_;
                other.pool_ = nullptr;
            }
            return *this;
        }

        MySqlConnection& connection() const { return *pool_->connections_.at(index_); }

    private:
        void release() {
            if (pool_) {
                pool_->release(index_);
                pool_ = nullptr;
            }
        }

        ConnectionPool* pool_{nullptr};
        size_t index_{0};
    };

    ConnectionPool(const MySqlConfig& config, size_t size) {
        if (size == 0) {
            throw invalid_argument("The MySQL connection pool cannot be empty.");
        }
        connections_.reserve(size);
        for (size_t index = 0; index < size; ++index) {
            connections_.push_back(make_unique<MySqlConnection>(config));
            available_.push_back(index);
        }
    }

    Lease acquire() const {
        unique_lock<mutex> lock(mutex_);
        availableCondition_.wait(lock, [this] { return !available_.empty(); });
        const size_t index = available_.front();
        available_.pop_front();
        return Lease(const_cast<ConnectionPool&>(*this), index);
    }

    Result<void> verifyConnections() {
        for (const auto& connection : connections_) {
            auto result = connection->query("SELECT 1");
            if (!result) {
                return failure<void>(result.error());
            }
        }
        return Result<void>::success();
    }

private:
    void release(size_t index) {
        {
            lock_guard<mutex> lock(mutex_);
            available_.push_back(index);
        }
        availableCondition_.notify_one();
    }

    vector<unique_ptr<MySqlConnection>> connections_;
    mutable deque<size_t> available_;
    mutable mutex mutex_;
    mutable condition_variable availableCondition_;
};

}

class MySqlDataContext::Implementation {
public:
    Implementation(MySqlConfig config, size_t poolSize) : pool_(config, poolSize) {}

    template <typename T, typename Function>
    Result<T> withConnection(Function operation) const {
        try {
            if (connectionOwner_ == this) {
                return operation(*currentConnection_);
            }
            auto lease = pool_.acquire();
            struct ConnectionScope {
                ConnectionScope(const Implementation* owner, MySqlConnection* connection) {
                    connectionOwner_ = owner;
                    currentConnection_ = connection;
                    transactionActive_ = false;
                }
                ~ConnectionScope() {
                    transactionActive_ = false;
                    currentConnection_ = nullptr;
                    connectionOwner_ = nullptr;
                }
            } scope(this, &lease.connection());
            return operation(lease.connection());
        } catch (const exception& exception) {
            return Result<T>::failure("MYSQL_OPERATION_FAILED", exception.what());
        }
    }

    Result<void> atomically(
        const function<Result<void>(MySqlConnection&)>& operation) {
        if (connectionOwner_ == this) {
            if (transactionActive_) {
                return operation(*currentConnection_);
            }
            return currentConnection_->transaction(operation);
        }
        auto lease = pool_.acquire();
        return lease.connection().transaction(operation);
    }

    Result<void> executeTransaction(const contracts::ITransactionBoundary::Operation& operation) {
        if (!operation) {
            return Result<void>::failure(
                "INVALID_TRANSACTION", "A transaction requires an operation.");
        }
        if (transactionActive_) {
            return Result<void>::failure(
                "NESTED_TRANSACTION", "Nested Data Access transactions are not supported.");
        }
        auto lease = pool_.acquire();
        return lease.connection().transaction([this, &operation](MySqlConnection& connection) {
            struct ConnectionScope {
                ConnectionScope(const Implementation* owner, MySqlConnection* connection) {
                    connectionOwner_ = owner;
                    currentConnection_ = connection;
                    transactionActive_ = true;
                }
                ~ConnectionScope() {
                    transactionActive_ = false;
                    currentConnection_ = nullptr;
                    connectionOwner_ = nullptr;
                }
            } scope(this, &connection);
            return operation();
        });
    }

    ConnectionPool pool_;
    static thread_local const Implementation* connectionOwner_;
    static thread_local MySqlConnection* currentConnection_;
    static thread_local bool transactionActive_;
};

thread_local const MySqlDataContext::Implementation*
    MySqlDataContext::Implementation::connectionOwner_ = nullptr;
thread_local MySqlConnection* MySqlDataContext::Implementation::currentConnection_ = nullptr;
thread_local bool MySqlDataContext::Implementation::transactionActive_ = false;

MySqlDataContext::MySqlDataContext(MySqlConfig config, size_t poolSize)
    : implementation_(make_unique<Implementation>(move(config), poolSize)) {}

MySqlDataContext::~MySqlDataContext() = default;

Result<void> MySqlDataContext::verifyConnections() {
    return implementation_->pool_.verifyConnections();
}

Result<optional<User>> MySqlDataContext::findUser(UserId id) const {
    return implementation_->withConnection<optional<User>>([&id](MySqlConnection& connection) {
        auto rows = connection.query(
            "SELECT user_id, name, email, status, role FROM users WHERE user_id = " +
            quoted(connection, id.value()));
        if (!rows) {
            return failure<optional<User>>(rows.error());
        }
        if (rows.value().empty()) {
            return Result<optional<User>>::success(nullopt);
        }
        const auto& row = rows.value().front();
        if (row.size() != 5) {
            return mappingFailure<optional<User>>("A user query returned an unexpected shape.");
        }
        return Result<optional<User>>::success(User{
            UserId{row[0]}, row[1], row[2], parseUserStatus(row[3]), parseUserRole(row[4])});
    });
}

Result<optional<Student>> MySqlDataContext::findStudent(StudentId id) const {
    return implementation_->withConnection<optional<Student>>([&id](MySqlConnection& connection) {
        auto rows = connection.query(
            "SELECT student_id, user_id, program_id FROM students WHERE student_id = " +
            quoted(connection, id.value()));
        if (!rows) {
            return failure<optional<Student>>(rows.error());
        }
        if (rows.value().empty()) {
            return Result<optional<Student>>::success(nullopt);
        }
        const auto& row = rows.value().front();
        if (row.size() != 3) {
            return mappingFailure<optional<Student>>("A student query returned an unexpected shape.");
        }
        return Result<optional<Student>>::success(Student{
            StudentId{row[0]}, UserId{row[1]}, ProgramId{row[2]}});
    });
}

Result<optional<Faculty>> MySqlDataContext::findFaculty(FacultyId id) const {
    return implementation_->withConnection<optional<Faculty>>([&id](MySqlConnection& connection) {
        auto rows = connection.query(
            "SELECT f.faculty_id, f.user_id, d.name FROM faculty f "
            "JOIN departments d ON d.department_id = f.department_id WHERE f.faculty_id = " +
            quoted(connection, id.value()));
        if (!rows) {
            return failure<optional<Faculty>>(rows.error());
        }
        if (rows.value().empty()) {
            return Result<optional<Faculty>>::success(nullopt);
        }
        const auto& row = rows.value().front();
        if (row.size() != 3) {
            return mappingFailure<optional<Faculty>>("A faculty query returned an unexpected shape.");
        }
        return Result<optional<Faculty>>::success(
            Faculty{FacultyId{row[0]}, UserId{row[1]}, row[2]});
    });
}

Result<vector<User>> MySqlDataContext::users() const {
    return implementation_->withConnection<vector<User>>([](MySqlConnection& connection) {
        auto rows = connection.query(
            "SELECT user_id, name, email, status, role FROM users ORDER BY user_id");
        if (!rows) {
            return failure<vector<User>>(rows.error());
        }
        vector<User> values;
        for (const auto& row : rows.value()) {
            if (row.size() != 5) {
                return mappingFailure<vector<User>>("A user query returned an unexpected shape.");
            }
            values.push_back({UserId{row[0]}, row[1], row[2],
                              parseUserStatus(row[3]), parseUserRole(row[4])});
        }
        return Result<vector<User>>::success(move(values));
    });
}

Result<vector<Student>> MySqlDataContext::students() const {
    return implementation_->withConnection<vector<Student>>([](MySqlConnection& connection) {
        auto rows = connection.query(
            "SELECT student_id, user_id, program_id FROM students ORDER BY student_id");
        if (!rows) {
            return failure<vector<Student>>(rows.error());
        }
        vector<Student> values;
        for (const auto& row : rows.value()) {
            if (row.size() != 3) {
                return mappingFailure<vector<Student>>("A student query returned an unexpected shape.");
            }
            values.push_back({StudentId{row[0]}, UserId{row[1]},
                              ProgramId{row[2]}});
        }
        return Result<vector<Student>>::success(move(values));
    });
}

Result<vector<Faculty>> MySqlDataContext::facultyMembers() const {
    return implementation_->withConnection<vector<Faculty>>([](MySqlConnection& connection) {
        auto rows = connection.query(
            "SELECT f.faculty_id, f.user_id, d.name FROM faculty f "
            "JOIN departments d ON d.department_id = f.department_id ORDER BY f.faculty_id");
        if (!rows) {
            return failure<vector<Faculty>>(rows.error());
        }
        vector<Faculty> values;
        for (const auto& row : rows.value()) {
            if (row.size() != 3) {
                return mappingFailure<vector<Faculty>>("A faculty query returned an unexpected shape.");
            }
            values.push_back({FacultyId{row[0]}, UserId{row[1]}, row[2]});
        }
        return Result<vector<Faculty>>::success(move(values));
    });
}

Result<void> MySqlDataContext::saveUser(User user) {
    return implementation_->atomically([&user](MySqlConnection& connection) {
        if (user.id.empty() || user.name.empty() || user.email.empty()) {
            return Result<void>::failure(
                "INVALID_RECORD", "A user requires an ID, name, and email address.");
        }
        auto compatibleRole = requireCompatibleRole(connection, user.id, user.role);
        if (!compatibleRole) {
            return compatibleRole;
        }
        return connection.execute(
            "INSERT INTO users (user_id, name, email, role, status) VALUES (" +
            quoted(connection, user.id.value()) + ", " + quoted(connection, user.name) + ", " +
            quoted(connection, user.email) + ", " + quoted(connection, userRoleSql(user.role)) +
            ", " + quoted(connection, userStatusSql(user.status)) + ") ON DUPLICATE KEY UPDATE "
            "name = VALUES(name), email = VALUES(email), role = VALUES(role), status = VALUES(status)");
    });
}

Result<void> MySqlDataContext::saveStudent(Student student) {
    return implementation_->atomically([&student](MySqlConnection& connection) {
        if (student.id.empty() || student.userId.empty() || student.programId.empty()) {
            return Result<void>::failure(
                "INVALID_RECORD", "A student requires student, user, and programme IDs.");
        }
        auto role = requireRole(connection, student.userId, UserRole::Student);
        if (!role) {
            return role;
        }
        return connection.execute(
            "INSERT INTO students (student_id, user_id, program_id) VALUES (" +
            quoted(connection, student.id.value()) + ", " + quoted(connection, student.userId.value()) +
            ", " + quoted(connection, student.programId.value()) + ") ON DUPLICATE KEY UPDATE "
            "user_id = VALUES(user_id), program_id = VALUES(program_id)");
    });
}

Result<void> MySqlDataContext::saveFaculty(Faculty faculty) {
    return implementation_->atomically([&faculty](MySqlConnection& connection) {
        if (faculty.id.empty() || faculty.userId.empty() || faculty.department.empty()) {
            return Result<void>::failure(
                "INVALID_RECORD", "A faculty profile requires IDs and a department.");
        }
        auto role = requireRole(connection, faculty.userId, UserRole::Faculty);
        if (!role) {
            return role;
        }
        auto department = lookupSingleValue(
            connection,
            "SELECT department_id FROM departments WHERE department_id = " +
                quoted(connection, faculty.department) + " OR code = " +
                quoted(connection, faculty.department) + " OR name = " +
                quoted(connection, faculty.department) + " LIMIT 1",
            "The faculty department");
        if (!department) {
            return failure<void>(department.error());
        }
        return connection.execute(
            "INSERT INTO faculty (faculty_id, user_id, department_id) VALUES (" +
            quoted(connection, faculty.id.value()) + ", " + quoted(connection, faculty.userId.value()) +
            ", " + quoted(connection, department.value()) + ") ON DUPLICATE KEY UPDATE "
            "user_id = VALUES(user_id), department_id = VALUES(department_id)");
    });
}

Result<optional<Course>> MySqlDataContext::findCourse(CourseId id) const {
    return implementation_->withConnection<optional<Course>>([&id](MySqlConnection& connection) {
        auto rows = connection.query(
            "SELECT c.course_id, c.code, d.name, c.course_number, c.name, c.description, c.credits "
            "FROM courses c JOIN departments d ON d.department_id = c.department_id "
            "WHERE c.course_id = " + quoted(connection, id.value()));
        if (!rows) {
            return failure<optional<Course>>(rows.error());
        }
        if (rows.value().empty()) {
            return Result<optional<Course>>::success(nullopt);
        }
        const auto& row = rows.value().front();
        if (row.size() != 7) {
            return mappingFailure<optional<Course>>("A course query returned an unexpected shape.");
        }
        auto prerequisites = connection.query(
            "SELECT prerequisite_course_id FROM course_prerequisites WHERE course_id = " +
            quoted(connection, id.value()) + " ORDER BY prerequisite_course_id");
        if (!prerequisites) {
            return failure<optional<Course>>(prerequisites.error());
        }
        vector<CourseId> prerequisiteIds;
        for (const auto& prerequisite : prerequisites.value()) {
            if (prerequisite.size() != 1) {
                return mappingFailure<optional<Course>>(
                    "A prerequisite query returned an unexpected shape.");
            }
            prerequisiteIds.emplace_back(prerequisite.front());
        }
        return Result<optional<Course>>::success(Course{
            CourseId{row[0]}, row[1], row[2], row[3], row[4], row[5],
            parseUnsigned(row[6]), move(prerequisiteIds)});
    });
}

Result<optional<CourseOffering>> MySqlDataContext::findOffering(
    OfferingId id) const {
    return implementation_->withConnection<optional<CourseOffering>>(
        [this, &id](MySqlConnection& connection) {
            const bool lockForUpdate = Implementation::connectionOwner_ == implementation_.get() &&
                                       Implementation::transactionActive_;
            auto rows = connection.query(
                "SELECT o.offering_id, o.course_id, s.code, o.instructor_id, o.capacity, "
                "(SELECT COUNT(*) FROM enrollments e WHERE e.offering_id = o.offering_id "
                "AND e.status = 'ACTIVE') FROM course_offerings o "
                "JOIN semesters s ON s.semester_id = o.semester_id WHERE o.offering_id = " +
                quoted(connection, id.value()) + (lockForUpdate ? " FOR UPDATE" : ""));
            if (!rows) {
                return failure<optional<CourseOffering>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<CourseOffering>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 6) {
                return mappingFailure<optional<CourseOffering>>(
                    "An offering query returned an unexpected shape.");
            }
            auto slots = connection.query(
                "SELECT ss.day_of_week, TIME_FORMAT(ss.starts_at, '%H:%i'), "
                "TIME_FORMAT(ss.ends_at, '%H:%i'), CONCAT(l.building, '-', l.room) "
                "FROM schedule_slots ss JOIN locations l ON l.location_id = ss.location_id "
                "WHERE ss.offering_id = " + quoted(connection, id.value()) +
                " ORDER BY ss.day_of_week, ss.starts_at");
            if (!slots) {
                return failure<optional<CourseOffering>>(slots.error());
            }
            vector<ScheduleSlot> schedule;
            for (const auto& slot : slots.value()) {
                if (slot.size() != 4) {
                    return mappingFailure<optional<CourseOffering>>(
                        "A schedule query returned an unexpected shape.");
                }
                auto mapped = ScheduleSlot::create(
                    parseDay(slot[0]), parseTimeMinutes(slot[1]), parseTimeMinutes(slot[2]), slot[3]);
                if (!mapped) {
                    return Result<optional<CourseOffering>>::failure(
                        mapped.error().code, mapped.error().message);
                }
                schedule.push_back(mapped.value());
            }
            return Result<optional<CourseOffering>>::success(CourseOffering{
                OfferingId{row[0]}, CourseId{row[1]}, row[2],
                FacultyId{row[3]}, parseSize(row[4]), parseSize(row[5]),
                move(schedule)});
        });
}

Result<vector<Course>> MySqlDataContext::courses() const {
    return implementation_->withConnection<vector<Course>>([this](MySqlConnection& connection) {
        auto rows = connection.query("SELECT course_id FROM courses ORDER BY course_id");
        if (!rows) {
            return failure<vector<Course>>(rows.error());
        }
        vector<Course> values;
        for (const auto& row : rows.value()) {
            if (row.size() != 1) {
                return mappingFailure<vector<Course>>("A course ID query returned an unexpected shape.");
            }
            auto course = findCourse(CourseId{row[0]});
            if (!course) {
                return failure<vector<Course>>(course.error());
            }
            if (course.value()) {
                values.push_back(*course.value());
            }
        }
        return Result<vector<Course>>::success(move(values));
    });
}

Result<vector<CourseOffering>> MySqlDataContext::offerings() const {
    return implementation_->withConnection<vector<CourseOffering>>(
        [this](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT offering_id FROM course_offerings ORDER BY offering_id");
            if (!rows) {
                return failure<vector<CourseOffering>>(rows.error());
            }
            vector<CourseOffering> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 1) {
                    return mappingFailure<vector<CourseOffering>>(
                        "An offering ID query returned an unexpected shape.");
                }
                auto offering = findOffering(OfferingId{row[0]});
                if (!offering) {
                    return failure<vector<CourseOffering>>(offering.error());
                }
                if (offering.value()) {
                    values.push_back(*offering.value());
                }
            }
            return Result<vector<CourseOffering>>::success(move(values));
        });
}

Result<void> MySqlDataContext::saveCourse(Course course) {
    return implementation_->atomically([&course](MySqlConnection& connection) {
        if (course.id.empty() || course.code.empty() || course.department.empty() ||
            course.courseNumber.empty() || course.name.empty()) {
            return Result<void>::failure(
                "INVALID_RECORD", "A course requires an ID, code, department, number, and name.");
        }
        auto department = lookupSingleValue(
            connection,
            "SELECT department_id FROM departments WHERE department_id = " +
                quoted(connection, course.department) + " OR code = " +
                quoted(connection, course.department) + " OR name = " +
                quoted(connection, course.department) + " LIMIT 1",
            "The course department");
        if (!department) {
            return failure<void>(department.error());
        }
        auto saved = connection.execute(
            "INSERT INTO courses (course_id, department_id, course_number, code, name, description, credits) "
            "VALUES (" + quoted(connection, course.id.value()) + ", " +
            quoted(connection, department.value()) + ", " + quoted(connection, course.courseNumber) +
            ", " + quoted(connection, course.code) + ", " + quoted(connection, course.name) + ", " +
            quoted(connection, course.description) + ", " + to_string(course.credits) +
            ") ON DUPLICATE KEY UPDATE department_id = VALUES(department_id), "
            "course_number = VALUES(course_number), code = VALUES(code), name = VALUES(name), "
            "description = VALUES(description), credits = VALUES(credits)");
        if (!saved) {
            return saved;
        }
        auto cleared = connection.execute(
            "DELETE FROM course_prerequisites WHERE course_id = " +
            quoted(connection, course.id.value()));
        if (!cleared) {
            return cleared;
        }
        for (const auto& prerequisiteId : course.prerequisiteCourseIds) {
            auto inserted = connection.execute(
                "INSERT INTO course_prerequisites (course_id, prerequisite_course_id) VALUES (" +
                quoted(connection, course.id.value()) + ", " +
                quoted(connection, prerequisiteId.value()) + ")");
            if (!inserted) {
                return inserted;
            }
        }
        return Result<void>::success();
    });
}

Result<void> MySqlDataContext::saveOffering(CourseOffering offering) {
    return implementation_->atomically([&offering](MySqlConnection& connection) {
        if (offering.id.empty() || offering.courseId.empty() || offering.semester.empty() ||
            offering.instructorId.empty() || offering.capacity == 0) {
            return Result<void>::failure(
                "INVALID_RECORD", "An offering requires IDs, a semester, and positive capacity.");
        }
        auto semester = lookupSingleValue(
            connection,
            "SELECT semester_id FROM semesters WHERE semester_id = " +
                quoted(connection, offering.semester) + " OR code = " +
                quoted(connection, offering.semester) + " LIMIT 1",
            "The offering semester");
        if (!semester) {
            return failure<void>(semester.error());
        }
        auto saved = connection.execute(
            "INSERT INTO course_offerings (offering_id, course_id, semester_id, instructor_id, capacity) "
            "VALUES (" + quoted(connection, offering.id.value()) + ", " +
            quoted(connection, offering.courseId.value()) + ", " + quoted(connection, semester.value()) +
            ", " + quoted(connection, offering.instructorId.value()) + ", " +
            to_string(offering.capacity) + ") ON DUPLICATE KEY UPDATE "
            "course_id = VALUES(course_id), semester_id = VALUES(semester_id), "
            "instructor_id = VALUES(instructor_id), capacity = VALUES(capacity)");
        if (!saved) {
            return saved;
        }
        auto cleared = connection.execute(
            "DELETE FROM schedule_slots WHERE offering_id = " +
            quoted(connection, offering.id.value()));
        if (!cleared) {
            return cleared;
        }
        for (const auto& slot : offering.schedule) {
            auto location = lookupSingleValue(
                connection,
                "SELECT location_id FROM locations WHERE location_id = " +
                    quoted(connection, slot.location()) + " OR CONCAT(building, '-', room) = " +
                    quoted(connection, slot.location()) + " LIMIT 1",
                "The schedule location");
            if (!location) {
                return failure<void>(location.error());
            }
            auto inserted = connection.execute(
                "INSERT INTO schedule_slots "
                "(offering_id, day_of_week, starts_at, ends_at, location_id) VALUES (" +
                quoted(connection, offering.id.value()) + ", " +
                to_string(static_cast<unsigned int>(slot.day()) + 1) + ", " +
                quoted(connection, to_string(slot.startMinutes() / 60) + ":" +
                    to_string(slot.startMinutes() % 60) + ":00") + ", " +
                quoted(connection, to_string(slot.endMinutes() / 60) + ":" +
                    to_string(slot.endMinutes() % 60) + ":00") + ", " +
                quoted(connection, location.value()) + ")");
            if (!inserted) {
                return inserted;
            }
        }
        return Result<void>::success();
    });
}

Result<optional<DegreeProgram>> MySqlDataContext::findProgram(
    ProgramId id) const {
    return implementation_->withConnection<optional<DegreeProgram>>(
        [&id](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT p.program_id, p.name, d.name, p.required_credits FROM degree_programs p "
                "JOIN departments d ON d.department_id = p.department_id WHERE p.program_id = " +
                quoted(connection, id.value()));
            if (!rows) {
                return failure<optional<DegreeProgram>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<DegreeProgram>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 4) {
                return mappingFailure<optional<DegreeProgram>>(
                    "A programme query returned an unexpected shape.");
            }
            auto requirements = connection.query(
                "SELECT course_id FROM program_required_courses WHERE program_id = " +
                quoted(connection, id.value()) + " ORDER BY course_id");
            if (!requirements) {
                return failure<optional<DegreeProgram>>(requirements.error());
            }
            vector<CourseId> courseIds;
            for (const auto& requirement : requirements.value()) {
                if (requirement.size() != 1) {
                    return mappingFailure<optional<DegreeProgram>>(
                        "A programme requirement query returned an unexpected shape.");
                }
                courseIds.emplace_back(requirement.front());
            }
            return Result<optional<DegreeProgram>>::success(DegreeProgram{
                ProgramId{row[0]}, row[1], row[2], move(courseIds),
                parseUnsigned(row[3])});
        });
}

Result<vector<DegreeProgram>> MySqlDataContext::programs() const {
    return implementation_->withConnection<vector<DegreeProgram>>(
        [this](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT program_id FROM degree_programs ORDER BY program_id");
            if (!rows) {
                return failure<vector<DegreeProgram>>(rows.error());
            }
            vector<DegreeProgram> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 1) {
                    return mappingFailure<vector<DegreeProgram>>(
                        "A programme ID query returned an unexpected shape.");
                }
                auto program = findProgram(ProgramId{row[0]});
                if (!program) {
                    return failure<vector<DegreeProgram>>(program.error());
                }
                if (program.value()) {
                    values.push_back(*program.value());
                }
            }
            return Result<vector<DegreeProgram>>::success(move(values));
        });
}

Result<void> MySqlDataContext::saveProgram(DegreeProgram program) {
    return implementation_->atomically([&program](MySqlConnection& connection) {
        if (program.id.empty() || program.name.empty() || program.department.empty()) {
            return Result<void>::failure(
                "INVALID_RECORD", "A programme requires an ID, name, and department.");
        }
        auto department = lookupSingleValue(
            connection,
            "SELECT department_id FROM departments WHERE department_id = " +
                quoted(connection, program.department) + " OR code = " +
                quoted(connection, program.department) + " OR name = " +
                quoted(connection, program.department) + " LIMIT 1",
            "The programme department");
        if (!department) {
            return failure<void>(department.error());
        }
        auto saved = connection.execute(
            "INSERT INTO degree_programs (program_id, department_id, name, required_credits) VALUES (" +
            quoted(connection, program.id.value()) + ", " + quoted(connection, department.value()) +
            ", " + quoted(connection, program.name) + ", " +
            to_string(program.requiredCredits) + ") ON DUPLICATE KEY UPDATE "
            "department_id = VALUES(department_id), name = VALUES(name), "
            "required_credits = VALUES(required_credits)");
        if (!saved) {
            return saved;
        }
        auto cleared = connection.execute(
            "DELETE FROM program_required_courses WHERE program_id = " +
            quoted(connection, program.id.value()));
        if (!cleared) {
            return cleared;
        }
        for (const auto& courseId : program.requiredCourseIds) {
            auto inserted = connection.execute(
                "INSERT INTO program_required_courses (program_id, course_id) VALUES (" +
                quoted(connection, program.id.value()) + ", " +
                quoted(connection, courseId.value()) + ")");
            if (!inserted) {
                return inserted;
            }
        }
        return Result<void>::success();
    });
}

Result<optional<Enrollment>> MySqlDataContext::findEnrollment(
    EnrollmentId id) const {
    return implementation_->withConnection<optional<Enrollment>>(
        [&id](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT enrollment_id, student_id, offering_id, status FROM enrollments "
                "WHERE enrollment_id = " + quoted(connection, id.value()));
            if (!rows) {
                return failure<optional<Enrollment>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<Enrollment>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 4) {
                return mappingFailure<optional<Enrollment>>(
                    "An enrolment query returned an unexpected shape.");
            }
            return Result<optional<Enrollment>>::success(Enrollment{
                EnrollmentId{row[0]}, StudentId{row[1]},
                OfferingId{row[2]}, parseEnrollmentStatus(row[3])});
        });
}

Result<vector<Enrollment>> MySqlDataContext::enrollments() const {
    return implementation_->withConnection<vector<Enrollment>>(
        [](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT enrollment_id, student_id, offering_id, status FROM enrollments "
                "ORDER BY enrollment_id");
            if (!rows) {
                return failure<vector<Enrollment>>(rows.error());
            }
            vector<Enrollment> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 4) {
                    return mappingFailure<vector<Enrollment>>(
                        "An enrolment query returned an unexpected shape.");
                }
                values.push_back({EnrollmentId{row[0]}, StudentId{row[1]},
                                  OfferingId{row[2]}, parseEnrollmentStatus(row[3])});
            }
            return Result<vector<Enrollment>>::success(move(values));
        });
}

Result<void> MySqlDataContext::saveEnrollment(Enrollment enrollment) {
    return implementation_->atomically([&enrollment](MySqlConnection& connection) {
        if (enrollment.id.empty() || enrollment.studentId.empty() || enrollment.offeringId.empty()) {
            return Result<void>::failure(
                "INVALID_RECORD", "An enrolment requires enrolment, student, and offering IDs.");
        }
        return connection.execute(
            "INSERT INTO enrollments (enrollment_id, student_id, offering_id, status) VALUES (" +
            quoted(connection, enrollment.id.value()) + ", " +
            quoted(connection, enrollment.studentId.value()) + ", " +
            quoted(connection, enrollment.offeringId.value()) + ", " +
            quoted(connection, enrollmentStatusSql(enrollment.status)) +
            ") ON DUPLICATE KEY UPDATE student_id = VALUES(student_id), "
            "offering_id = VALUES(offering_id), status = VALUES(status)");
    });
}

Result<void> MySqlDataContext::removeEnrollment(EnrollmentId id) {
    return implementation_->atomically([&id](MySqlConnection& connection) {
        auto removed = connection.execute(
            "DELETE FROM enrollments WHERE enrollment_id = " + quoted(connection, id.value()));
        if (!removed) {
            return removed;
        }
        return ensureAffected(connection, "The enrolment");
    });
}

Result<optional<GradeRecord>> MySqlDataContext::findGradeRecord(
    GradeRecordId id) const {
    return implementation_->withConnection<optional<GradeRecord>>(
        [&id](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT g.grade_record_id, e.student_id, e.offering_id, o.course_id, "
                "g.grade, g.lifecycle FROM grade_records g "
                "JOIN enrollments e ON e.enrollment_id = g.enrollment_id "
                "JOIN course_offerings o ON o.offering_id = e.offering_id "
                "WHERE g.grade_record_id = " + quoted(connection, id.value()));
            if (!rows) {
                return failure<optional<GradeRecord>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<GradeRecord>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 6) {
                return mappingFailure<optional<GradeRecord>>(
                    "A grade query returned an unexpected shape.");
            }
            return Result<optional<GradeRecord>>::success(GradeRecord{
                GradeRecordId{row[0]}, StudentId{row[1]},
                OfferingId{row[2]}, CourseId{row[3]}, row[4],
                parseGradeLifecycle(row[5])});
        });
}

Result<vector<GradeRecord>> MySqlDataContext::gradeRecords() const {
    return implementation_->withConnection<vector<GradeRecord>>(
        [this](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT grade_record_id FROM grade_records ORDER BY grade_record_id");
            if (!rows) {
                return failure<vector<GradeRecord>>(rows.error());
            }
            vector<GradeRecord> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 1) {
                    return mappingFailure<vector<GradeRecord>>(
                        "A grade ID query returned an unexpected shape.");
                }
                auto grade = findGradeRecord(GradeRecordId{row[0]});
                if (!grade) {
                    return failure<vector<GradeRecord>>(grade.error());
                }
                if (grade.value()) {
                    values.push_back(*grade.value());
                }
            }
            return Result<vector<GradeRecord>>::success(move(values));
        });
}

Result<void> MySqlDataContext::saveGradeRecord(GradeRecord record) {
    return implementation_->atomically([&record](MySqlConnection& connection) {
        if (record.id.empty() || record.studentId.empty() || record.offeringId.empty() ||
            record.courseId.empty()) {
            return Result<void>::failure(
                "INVALID_RECORD", "A grade requires grade, student, offering, and course IDs.");
        }
        auto enrollment = lookupSingleValue(
            connection,
            "SELECT e.enrollment_id FROM enrollments e JOIN course_offerings o "
            "ON o.offering_id = e.offering_id WHERE e.student_id = " +
                quoted(connection, record.studentId.value()) + " AND e.offering_id = " +
                quoted(connection, record.offeringId.value()) + " AND o.course_id = " +
                quoted(connection, record.courseId.value()) + " LIMIT 1",
            "The grade enrolment");
        if (!enrollment) {
            return failure<void>(enrollment.error());
        }
        return connection.execute(
            "INSERT INTO grade_records (grade_record_id, enrollment_id, grade, lifecycle) VALUES (" +
            quoted(connection, record.id.value()) + ", " + quoted(connection, enrollment.value()) +
            ", " + quoted(connection, record.grade) + ", " +
            quoted(connection, gradeLifecycleSql(record.lifecycle)) +
            ") ON DUPLICATE KEY UPDATE enrollment_id = VALUES(enrollment_id), "
            "grade = VALUES(grade), lifecycle = VALUES(lifecycle)");
    });
}

Result<optional<CourseChangeRequest>> MySqlDataContext::findChangeRequest(
    ChangeRequestId id) const {
    return implementation_->withConnection<optional<CourseChangeRequest>>(
        [&id](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT change_request_id, faculty_id, course_id, COALESCE(offering_id, ''), "
                "change_type, status, requested_value FROM course_change_requests "
                "WHERE change_request_id = " +
                quoted(connection, id.value()));
            if (!rows) {
                return failure<optional<CourseChangeRequest>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<CourseChangeRequest>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 7) {
                return mappingFailure<optional<CourseChangeRequest>>(
                    "A change-request query returned an unexpected shape.");
            }
            const auto type = parseChangeType(row[4]);
            optional<OfferingId> offeringId;
            if (!row[3].empty()) {
                offeringId = OfferingId{row[3]};
            }
            return Result<optional<CourseChangeRequest>>::success(CourseChangeRequest{
                ChangeRequestId{row[0]}, FacultyId{row[1]},
                CourseId{row[2]}, move(offeringId), type,
                parseChangeStatus(row[5]), row[6]});
        });
}

Result<vector<CourseChangeRequest>> MySqlDataContext::changeRequests() const {
    return implementation_->withConnection<vector<CourseChangeRequest>>(
        [this](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT change_request_id FROM course_change_requests ORDER BY change_request_id");
            if (!rows) {
                return failure<vector<CourseChangeRequest>>(rows.error());
            }
            vector<CourseChangeRequest> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 1) {
                    return mappingFailure<vector<CourseChangeRequest>>(
                        "A change-request ID query returned an unexpected shape.");
                }
                auto request = findChangeRequest(ChangeRequestId{row[0]});
                if (!request) {
                    return failure<vector<CourseChangeRequest>>(request.error());
                }
                if (request.value()) {
                    values.push_back(*request.value());
                }
            }
            return Result<vector<CourseChangeRequest>>::success(move(values));
        });
}

Result<void> MySqlDataContext::saveChangeRequest(CourseChangeRequest request) {
    return implementation_->atomically([&request](MySqlConnection& connection) {
        if (request.id.empty() || request.facultyId.empty() || request.courseId.empty()) {
            return Result<void>::failure(
                "INVALID_RECORD", "A course-change request requires request, faculty, and course IDs.");
        }
        const string offeringSql = request.offeringId
                                            ? quoted(connection, request.offeringId->value())
                                            : "NULL";
        return connection.execute(
            "INSERT INTO course_change_requests "
            "(change_request_id, faculty_id, course_id, offering_id, change_type, "
            "requested_value, status) VALUES (" +
            quoted(connection, request.id.value()) + ", " +
            quoted(connection, request.facultyId.value()) + ", " +
            quoted(connection, request.courseId.value()) + ", " + offeringSql + ", " +
            quoted(connection, changeTypeSql(request.type)) + ", " +
            quoted(connection, request.requestedValue) + ", " +
            quoted(connection, changeStatusSql(request.status)) +
            ") ON DUPLICATE KEY UPDATE faculty_id = VALUES(faculty_id), "
            "course_id = VALUES(course_id), offering_id = VALUES(offering_id), "
            "change_type = VALUES(change_type), requested_value = VALUES(requested_value), "
            "status = VALUES(status)");
    });
}

Result<optional<WaitlistEntry>> MySqlDataContext::findWaitlistEntry(
    WaitlistEntryId id) const {
    return implementation_->withConnection<optional<WaitlistEntry>>(
        [&id](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT waitlist_entry_id, student_id, offering_id, position, status "
                "FROM waitlist_entries WHERE waitlist_entry_id = " +
                quoted(connection, id.value()));
            if (!rows) {
                return failure<optional<WaitlistEntry>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<WaitlistEntry>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 5) {
                return mappingFailure<optional<WaitlistEntry>>(
                    "A waitlist query returned an unexpected shape.");
            }
            return Result<optional<WaitlistEntry>>::success(WaitlistEntry{
                WaitlistEntryId{row[0]}, StudentId{row[1]},
                OfferingId{row[2]}, parseSize(row[3]), parseWaitlistStatus(row[4])});
        });
}

Result<vector<WaitlistEntry>> MySqlDataContext::waitlistEntries() const {
    return implementation_->withConnection<vector<WaitlistEntry>>(
        [](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT waitlist_entry_id, student_id, offering_id, position, status "
                "FROM waitlist_entries ORDER BY waitlist_entry_id");
            if (!rows) {
                return failure<vector<WaitlistEntry>>(rows.error());
            }
            vector<WaitlistEntry> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 5) {
                    return mappingFailure<vector<WaitlistEntry>>(
                        "A waitlist query returned an unexpected shape.");
                }
                values.push_back({WaitlistEntryId{row[0]}, StudentId{row[1]},
                                  OfferingId{row[2]}, parseSize(row[3]),
                                  parseWaitlistStatus(row[4])});
            }
            return Result<vector<WaitlistEntry>>::success(move(values));
        });
}

Result<void> MySqlDataContext::saveWaitlistEntry(WaitlistEntry entry) {
    return implementation_->atomically([&entry](MySqlConnection& connection) {
        if (entry.id.empty() || entry.studentId.empty() || entry.offeringId.empty() ||
            entry.position == 0) {
            return Result<void>::failure(
                "INVALID_RECORD", "A waitlist entry requires IDs and a positive position.");
        }
        return connection.execute(
            "INSERT INTO waitlist_entries "
            "(waitlist_entry_id, student_id, offering_id, position, status) VALUES (" +
            quoted(connection, entry.id.value()) + ", " +
            quoted(connection, entry.studentId.value()) + ", " +
            quoted(connection, entry.offeringId.value()) + ", " +
            to_string(entry.position) + ", " +
            quoted(connection, waitlistStatusSql(entry.status)) +
            ") ON DUPLICATE KEY UPDATE student_id = VALUES(student_id), "
            "offering_id = VALUES(offering_id), position = VALUES(position), "
            "status = VALUES(status)");
    });
}

Result<void> MySqlDataContext::removeWaitlistEntry(WaitlistEntryId id) {
    return implementation_->atomically([&id](MySqlConnection& connection) {
        auto removed = connection.execute(
            "DELETE FROM waitlist_entries WHERE waitlist_entry_id = " +
            quoted(connection, id.value()));
        if (!removed) {
            return removed;
        }
        return ensureAffected(connection, "The waitlist entry");
    });
}

Result<void> MySqlDataContext::executeTransaction(const Operation& operation) {
    return implementation_->executeTransaction(operation);
}

}
