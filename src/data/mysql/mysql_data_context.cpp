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
    if (rows.value().size() != 1 || rows.value().front().size() != 1) {
        return mappingFailure<string>(description + " lookup returned an unexpected shape.");
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
    if (rows.value().size() != 1 || rows.value().front().size() != 1) {
        return mappingFailure<void>("A user-role query returned an unexpected shape.");
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
    if (rows.value().size() != 1 || rows.value().front().size() != 1) {
        return mappingFailure<void>(description + " affected-row check returned an unexpected shape.");
    }
    if (rows.value().front().front() == "0") {
        return Result<void>::failure("RECORD_NOT_FOUND", description + " does not exist.");
    }
    return Result<void>::success();
}

Result<void> ensureInserted(MySqlConnection& connection, const string& description) {
    auto rows = connection.query("SELECT ROW_COUNT()");
    if (!rows) {
        return failure<void>(rows.error());
    }
    if (rows.value().size() != 1 || rows.value().front().size() != 1) {
        return mappingFailure<void>(
            description + " insert check returned an unexpected shape.");
    }
    if (rows.value().front().front() != "1") {
        return Result<void>::failure(
            "PERSISTENCE_ID_CONFLICT",
            description + " conflicts with another persisted record.");
    }
    return Result<void>::success();
}

Result<void> ensurePersistedPrimary(
    MySqlConnection& connection,
    const string& sql,
    const string& description) {
    auto rows = connection.query(sql);
    if (!rows) {
        return failure<void>(rows.error());
    }
    if (rows.value().empty()) {
        return Result<void>::failure(
            "PERSISTENCE_ID_CONFLICT",
            description + " conflicts with another persisted record.");
    }
    if (rows.value().size() != 1 || rows.value().front().size() != 1) {
        return mappingFailure<void>(description + " identity check returned an unexpected shape.");
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
                ConnectionScope(const Implementation* owner, MySqlConnection* connection)
                    : previousOwner(connectionOwner_),
                      previousConnection(currentConnection_),
                      previousTransactionActive(transactionActive_) {
                    connectionOwner_ = owner;
                    currentConnection_ = connection;
                    transactionActive_ = false;
                }
                ~ConnectionScope() {
                    transactionActive_ = previousTransactionActive;
                    currentConnection_ = previousConnection;
                    connectionOwner_ = previousOwner;
                }

                const Implementation* previousOwner;
                MySqlConnection* previousConnection;
                bool previousTransactionActive;
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
        if (connectionOwner_ == this && transactionActive_) {
            return Result<void>::failure(
                "NESTED_TRANSACTION", "Nested Data Access transactions are not supported.");
        }
        auto lease = pool_.acquire();
        return lease.connection().transaction([this, &operation](MySqlConnection& connection) {
            struct ConnectionScope {
                ConnectionScope(const Implementation* owner, MySqlConnection* connection)
                    : previousOwner(connectionOwner_),
                      previousConnection(currentConnection_),
                      previousTransactionActive(transactionActive_) {
                    connectionOwner_ = owner;
                    currentConnection_ = connection;
                    transactionActive_ = true;
                }
                ~ConnectionScope() {
                    transactionActive_ = previousTransactionActive;
                    currentConnection_ = previousConnection;
                    connectionOwner_ = previousOwner;
                }

                const Implementation* previousOwner;
                MySqlConnection* previousConnection;
                bool previousTransactionActive;
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

Result<optional<Student>> MySqlDataContext::findStudentByUserId(UserId userId) const {
    return implementation_->withConnection<optional<Student>>(
        [&userId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT student_id, user_id, program_id FROM students WHERE user_id = " +
                quoted(connection, userId.value()) + " ORDER BY student_id LIMIT 1");
            if (!rows) {
                return failure<optional<Student>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<Student>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 3) {
                return mappingFailure<optional<Student>>(
                    "A student-by-user query returned an unexpected shape.");
            }
            return Result<optional<Student>>::success(
                Student{StudentId{row[0]}, UserId{row[1]}, ProgramId{row[2]}});
        });
}

Result<optional<Faculty>> MySqlDataContext::findFacultyByUserId(UserId userId) const {
    return implementation_->withConnection<optional<Faculty>>(
        [&userId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT f.faculty_id, f.user_id, d.name FROM faculty f "
                "JOIN departments d ON d.department_id = f.department_id WHERE f.user_id = " +
                quoted(connection, userId.value()) + " ORDER BY f.faculty_id LIMIT 1");
            if (!rows) {
                return failure<optional<Faculty>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<Faculty>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 3) {
                return mappingFailure<optional<Faculty>>(
                    "A faculty-by-user query returned an unexpected shape.");
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
        auto saved = connection.execute(
            "INSERT INTO users (user_id, name, email, role, status) VALUES (" +
            quoted(connection, user.id.value()) + ", " + quoted(connection, user.name) + ", " +
            quoted(connection, user.email) + ", " + quoted(connection, userRoleSql(user.role)) +
            ", " + quoted(connection, userStatusSql(user.status)) + ") ON DUPLICATE KEY UPDATE "
            "name = IF(user_id = VALUES(user_id), VALUES(name), name), "
            "email = IF(user_id = VALUES(user_id), VALUES(email), email), "
            "role = IF(user_id = VALUES(user_id), VALUES(role), role), "
            "status = IF(user_id = VALUES(user_id), VALUES(status), status)");
        if (!saved) {
            return saved;
        }
        return ensurePersistedPrimary(
            connection,
            "SELECT 1 FROM users WHERE user_id = " + quoted(connection, user.id.value()),
            "The user ID or email");
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
        auto saved = connection.execute(
            "INSERT INTO students (student_id, user_id, program_id) VALUES (" +
            quoted(connection, student.id.value()) + ", " + quoted(connection, student.userId.value()) +
            ", " + quoted(connection, student.programId.value()) + ") ON DUPLICATE KEY UPDATE "
            "user_id = IF(student_id = VALUES(student_id), VALUES(user_id), user_id), "
            "program_id = IF(student_id = VALUES(student_id), VALUES(program_id), program_id)");
        if (!saved) {
            return saved;
        }
        return ensurePersistedPrimary(
            connection,
            "SELECT 1 FROM students WHERE student_id = " +
                quoted(connection, student.id.value()),
            "The Student ID or linked User");
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
                quoted(connection, faculty.department) + " ORDER BY CASE WHEN department_id = " +
                quoted(connection, faculty.department) + " THEN 0 WHEN code = " +
                quoted(connection, faculty.department) + " THEN 1 ELSE 2 END, department_id LIMIT 1",
            "The faculty department");
        if (!department) {
            return failure<void>(department.error());
        }
        auto saved = connection.execute(
            "INSERT INTO faculty (faculty_id, user_id, department_id) VALUES (" +
            quoted(connection, faculty.id.value()) + ", " + quoted(connection, faculty.userId.value()) +
            ", " + quoted(connection, department.value()) + ") ON DUPLICATE KEY UPDATE "
            "user_id = IF(faculty_id = VALUES(faculty_id), VALUES(user_id), user_id), "
            "department_id = IF(faculty_id = VALUES(faculty_id), "
            "VALUES(department_id), department_id)");
        if (!saved) {
            return saved;
        }
        return ensurePersistedPrimary(
            connection,
            "SELECT 1 FROM faculty WHERE faculty_id = " +
                quoted(connection, faculty.id.value()),
            "The Faculty ID or linked User");
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

Result<vector<CatalogueItem>> MySqlDataContext::browseCatalogue(
    const CatalogueFilter& filter) const {
    return implementation_->withConnection<vector<CatalogueItem>>(
        [this, &filter](MySqlConnection& connection) {
            vector<string> predicates;
            if (!filter.semester.empty()) {
                predicates.push_back(
                    "(s.code = " + quoted(connection, filter.semester) +
                    " OR s.semester_id = " + quoted(connection, filter.semester) + ")");
            }
            if (!filter.department.empty()) {
                predicates.push_back(
                    "(d.code = " + quoted(connection, filter.department) +
                    " OR d.name = " + quoted(connection, filter.department) + ")");
            }
            if (!filter.courseNumber.empty()) {
                predicates.push_back(
                    "c.course_number = " + quoted(connection, filter.courseNumber));
            }
            if (!filter.keyword.empty()) {
                const string keyword = quoted(connection, "%" + filter.keyword + "%");
                predicates.push_back(
                    "(c.code LIKE " + keyword + " OR c.name LIKE " + keyword +
                    " OR c.description LIKE " + keyword + ")");
            }
            if (!filter.instructor.empty()) {
                predicates.push_back(
                    "u.name LIKE " + quoted(connection, "%" + filter.instructor + "%"));
            }

            string sql =
                "SELECT o.offering_id, u.name FROM course_offerings o "
                "JOIN courses c ON c.course_id = o.course_id "
                "JOIN departments d ON d.department_id = c.department_id "
                "JOIN semesters s ON s.semester_id = o.semester_id "
                "JOIN faculty f ON f.faculty_id = o.instructor_id "
                "JOIN users u ON u.user_id = f.user_id";
            if (!predicates.empty()) {
                sql += " WHERE ";
                for (size_t index = 0; index < predicates.size(); ++index) {
                    if (index != 0) {
                        sql += " AND ";
                    }
                    sql += predicates[index];
                }
            }
            sql += " ORDER BY s.code, c.code, o.offering_id";

            auto rows = connection.query(sql);
            if (!rows) {
                return failure<vector<CatalogueItem>>(rows.error());
            }
            vector<CatalogueItem> items;
            for (const auto& row : rows.value()) {
                if (row.size() != 2) {
                    return mappingFailure<vector<CatalogueItem>>(
                        "A catalogue query returned an unexpected shape.");
                }
                auto offering = findOffering(OfferingId{row[0]});
                if (!offering) {
                    return failure<vector<CatalogueItem>>(offering.error());
                }
                if (!offering.value()) {
                    return mappingFailure<vector<CatalogueItem>>(
                        "A catalogue offering disappeared during its read.");
                }
                auto course = findCourse(offering.value()->courseId);
                if (!course) {
                    return failure<vector<CatalogueItem>>(course.error());
                }
                if (!course.value()) {
                    return mappingFailure<vector<CatalogueItem>>(
                        "A catalogue course disappeared during its read.");
                }
                items.push_back({*course.value(), *offering.value(), row[1]});
            }
            return Result<vector<CatalogueItem>>::success(move(items));
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
                quoted(connection, course.department) + " ORDER BY CASE WHEN department_id = " +
                quoted(connection, course.department) + " THEN 0 WHEN code = " +
                quoted(connection, course.department) + " THEN 1 ELSE 2 END, department_id LIMIT 1",
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
            ") ON DUPLICATE KEY UPDATE "
            "department_id = IF(course_id = VALUES(course_id), "
            "VALUES(department_id), department_id), "
            "course_number = IF(course_id = VALUES(course_id), "
            "VALUES(course_number), course_number), "
            "code = IF(course_id = VALUES(course_id), VALUES(code), code), "
            "name = IF(course_id = VALUES(course_id), VALUES(name), name), "
            "description = IF(course_id = VALUES(course_id), "
            "VALUES(description), description), "
            "credits = IF(course_id = VALUES(course_id), VALUES(credits), credits)");
        if (!saved) {
            return saved;
        }
        auto identity = ensurePersistedPrimary(
            connection,
            "SELECT 1 FROM courses WHERE course_id = " +
                quoted(connection, course.id.value()),
            "The course ID, code, or department/number");
        if (!identity) {
            return identity;
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

Result<vector<FacultyOfferingItem>> MySqlDataContext::assignedOfferings(
    FacultyId facultyId) const {
    return implementation_->withConnection<vector<FacultyOfferingItem>>(
        [this, &facultyId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT o.offering_id FROM course_offerings o "
                "JOIN semesters s ON s.semester_id = o.semester_id "
                "JOIN courses c ON c.course_id = o.course_id WHERE o.instructor_id = " +
                quoted(connection, facultyId.value()) +
                " ORDER BY s.code, c.code, o.offering_id");
            if (!rows) {
                return failure<vector<FacultyOfferingItem>>(rows.error());
            }
            vector<FacultyOfferingItem> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 1) {
                    return mappingFailure<vector<FacultyOfferingItem>>(
                        "An assigned-offering query returned an unexpected shape.");
                }
                auto offering = findOffering(OfferingId{row[0]});
                if (!offering) {
                    return failure<vector<FacultyOfferingItem>>(offering.error());
                }
                if (!offering.value()) {
                    return mappingFailure<vector<FacultyOfferingItem>>(
                        "An assigned offering disappeared during its read.");
                }
                auto course = findCourse(offering.value()->courseId);
                if (!course) {
                    return failure<vector<FacultyOfferingItem>>(course.error());
                }
                if (!course.value()) {
                    return mappingFailure<vector<FacultyOfferingItem>>(
                        "An assigned offering has no connected course.");
                }
                values.push_back({*course.value(), *offering.value()});
            }
            return Result<vector<FacultyOfferingItem>>::success(move(values));
        });
}

Result<bool> MySqlDataContext::facultyTeachesCourse(
    FacultyId facultyId,
    CourseId courseId) const {
    return implementation_->withConnection<bool>(
        [&facultyId, &courseId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT EXISTS(SELECT 1 FROM course_offerings WHERE instructor_id = " +
                quoted(connection, facultyId.value()) + " AND course_id = " +
                quoted(connection, courseId.value()) + ")");
            if (!rows) {
                return failure<bool>(rows.error());
            }
            if (rows.value().size() != 1 || rows.value().front().size() != 1) {
                return mappingFailure<bool>(
                    "A Faculty-course ownership query returned an unexpected shape.");
            }
            return Result<bool>::success(rows.value().front().front() == "1");
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
                quoted(connection, offering.semester) + " ORDER BY CASE WHEN semester_id = " +
                quoted(connection, offering.semester) +
                " THEN 0 ELSE 1 END, semester_id LIMIT 1",
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
                    quoted(connection, slot.location()) + " ORDER BY CASE WHEN location_id = " +
                    quoted(connection, slot.location()) +
                    " THEN 0 ELSE 1 END, location_id LIMIT 1",
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
                quoted(connection, program.department) + " ORDER BY CASE WHEN department_id = " +
                quoted(connection, program.department) + " THEN 0 WHEN code = " +
                quoted(connection, program.department) + " THEN 1 ELSE 2 END, department_id LIMIT 1",
            "The programme department");
        if (!department) {
            return failure<void>(department.error());
        }
        auto saved = connection.execute(
            "INSERT INTO degree_programs (program_id, department_id, name, required_credits) VALUES (" +
            quoted(connection, program.id.value()) + ", " + quoted(connection, department.value()) +
            ", " + quoted(connection, program.name) + ", " +
            to_string(program.requiredCredits) + ") ON DUPLICATE KEY UPDATE "
            "department_id = IF(program_id = VALUES(program_id), "
            "VALUES(department_id), department_id), "
            "name = IF(program_id = VALUES(program_id), VALUES(name), name), "
            "required_credits = IF(program_id = VALUES(program_id), "
            "VALUES(required_credits), required_credits)");
        if (!saved) {
            return saved;
        }
        auto identity = ensurePersistedPrimary(
            connection,
            "SELECT 1 FROM degree_programs WHERE program_id = " +
                quoted(connection, program.id.value()),
            "The programme ID or name");
        if (!identity) {
            return identity;
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

Result<optional<Enrollment>> MySqlDataContext::findStudentEnrollment(
    StudentId studentId,
    OfferingId offeringId) const {
    return implementation_->withConnection<optional<Enrollment>>(
        [&studentId, &offeringId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT enrollment_id, student_id, offering_id, status FROM enrollments "
                "WHERE student_id = " + quoted(connection, studentId.value()) +
                " AND offering_id = " + quoted(connection, offeringId.value()) + " LIMIT 1");
            if (!rows) {
                return failure<optional<Enrollment>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<Enrollment>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 4) {
                return mappingFailure<optional<Enrollment>>(
                    "A Student-offering enrolment query returned an unexpected shape.");
            }
            return Result<optional<Enrollment>>::success(Enrollment{
                EnrollmentId{row[0]}, StudentId{row[1]}, OfferingId{row[2]},
                parseEnrollmentStatus(row[3])});
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

Result<vector<Enrollment>> MySqlDataContext::activeEnrollmentsForStudent(
    StudentId studentId) const {
    return implementation_->withConnection<vector<Enrollment>>(
        [&studentId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT enrollment_id, student_id, offering_id, status FROM enrollments "
                "WHERE student_id = " + quoted(connection, studentId.value()) +
                " AND status = 'ACTIVE' ORDER BY offering_id, enrollment_id");
            if (!rows) {
                return failure<vector<Enrollment>>(rows.error());
            }
            vector<Enrollment> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 4) {
                    return mappingFailure<vector<Enrollment>>(
                        "An active Student enrolment query returned an unexpected shape.");
                }
                values.push_back({EnrollmentId{row[0]}, StudentId{row[1]},
                                  OfferingId{row[2]}, parseEnrollmentStatus(row[3])});
            }
            return Result<vector<Enrollment>>::success(move(values));
        });
}

Result<vector<Enrollment>> MySqlDataContext::scheduleEnrollmentsForStudent(
    StudentId studentId,
    const string& semester) const {
    return implementation_->withConnection<vector<Enrollment>>(
        [&studentId, &semester](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT e.enrollment_id, e.student_id, e.offering_id, e.status "
                "FROM enrollments e JOIN course_offerings o ON o.offering_id = e.offering_id "
                "JOIN semesters s ON s.semester_id = o.semester_id WHERE e.student_id = " +
                quoted(connection, studentId.value()) +
                " AND (s.code = " + quoted(connection, semester) +
                " OR s.semester_id = " + quoted(connection, semester) +
                ") AND e.status <> 'DROPPED' ORDER BY o.offering_id, e.enrollment_id");
            if (!rows) {
                return failure<vector<Enrollment>>(rows.error());
            }
            vector<Enrollment> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 4) {
                    return mappingFailure<vector<Enrollment>>(
                        "A Student schedule query returned an unexpected shape.");
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
        auto saved = connection.execute(
            "INSERT INTO enrollments (enrollment_id, student_id, offering_id, status) VALUES (" +
            quoted(connection, enrollment.id.value()) + ", " +
            quoted(connection, enrollment.studentId.value()) + ", " +
            quoted(connection, enrollment.offeringId.value()) + ", " +
            quoted(connection, enrollmentStatusSql(enrollment.status)) +
            ") ON DUPLICATE KEY UPDATE status = IF("
            "enrollment_id = VALUES(enrollment_id) AND "
            "student_id = VALUES(student_id) AND offering_id = VALUES(offering_id), "
            "VALUES(status), status)");
        if (!saved) {
            return saved;
        }
        auto rows = connection.query(
            "SELECT student_id, offering_id, status FROM enrollments WHERE enrollment_id = " +
            quoted(connection, enrollment.id.value()));
        if (!rows) {
            return failure<void>(rows.error());
        }
        if (rows.value().size() != 1 || rows.value().front().size() != 3 ||
            rows.value().front()[0] != enrollment.studentId.value() ||
            rows.value().front()[1] != enrollment.offeringId.value() ||
            parseEnrollmentStatus(rows.value().front()[2]) != enrollment.status) {
            return Result<void>::failure(
                "PERSISTENCE_ID_CONFLICT",
                "The enrolment ID conflicts with another persisted relationship.");
        }
        return Result<void>::success();
    });
}

Result<vector<FacultyRosterEntry>> MySqlDataContext::activeRosterForOffering(
    OfferingId offeringId) const {
    return implementation_->withConnection<vector<FacultyRosterEntry>>(
        [&offeringId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT e.enrollment_id, e.student_id, e.offering_id, e.status, "
                "u.name, u.email FROM enrollments e "
                "JOIN students s ON s.student_id = e.student_id "
                "JOIN users u ON u.user_id = s.user_id WHERE e.offering_id = " +
                quoted(connection, offeringId.value()) +
                " AND e.status = 'ACTIVE' ORDER BY e.student_id, e.enrollment_id");
            if (!rows) {
                return failure<vector<FacultyRosterEntry>>(rows.error());
            }
            vector<FacultyRosterEntry> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 6) {
                    return mappingFailure<vector<FacultyRosterEntry>>(
                        "A Faculty roster query returned an unexpected shape.");
                }
                values.push_back({
                    {EnrollmentId{row[0]}, StudentId{row[1]}, OfferingId{row[2]},
                     parseEnrollmentStatus(row[3])},
                    row[4], row[5]});
            }
            return Result<vector<FacultyRosterEntry>>::success(move(values));
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

Result<vector<GradeRecord>> MySqlDataContext::submittedGradesForStudent(
    StudentId studentId) const {
    return implementation_->withConnection<vector<GradeRecord>>(
        [&studentId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT g.grade_record_id, e.student_id, e.offering_id, o.course_id, "
                "g.grade, g.lifecycle FROM grade_records g "
                "JOIN enrollments e ON e.enrollment_id = g.enrollment_id "
                "JOIN course_offerings o ON o.offering_id = e.offering_id "
                "WHERE e.student_id = " + quoted(connection, studentId.value()) +
                " AND e.status = 'COMPLETED' AND g.lifecycle = 'SUBMITTED' "
                "ORDER BY o.course_id, e.offering_id, g.grade_record_id");
            if (!rows) {
                return failure<vector<GradeRecord>>(rows.error());
            }
            vector<GradeRecord> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 6) {
                    return mappingFailure<vector<GradeRecord>>(
                        "A submitted Student grade query returned an unexpected shape.");
                }
                values.push_back({GradeRecordId{row[0]}, StudentId{row[1]},
                                  OfferingId{row[2]}, CourseId{row[3]}, row[4],
                                  parseGradeLifecycle(row[5])});
            }
            return Result<vector<GradeRecord>>::success(move(values));
    });
}

Result<optional<GradeRecord>> MySqlDataContext::findStudentGradeRecord(
    StudentId studentId,
    OfferingId offeringId) const {
    return implementation_->withConnection<optional<GradeRecord>>(
        [&studentId, &offeringId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT g.grade_record_id, e.student_id, e.offering_id, o.course_id, "
                "g.grade, g.lifecycle FROM grade_records g "
                "JOIN enrollments e ON e.enrollment_id = g.enrollment_id "
                "JOIN course_offerings o ON o.offering_id = e.offering_id "
                "WHERE e.student_id = " + quoted(connection, studentId.value()) +
                " AND e.offering_id = " + quoted(connection, offeringId.value()) +
                " LIMIT 1");
            if (!rows) {
                return failure<optional<GradeRecord>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<GradeRecord>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 6) {
                return mappingFailure<optional<GradeRecord>>(
                    "A Student-offering grade query returned an unexpected shape.");
            }
            return Result<optional<GradeRecord>>::success(GradeRecord{
                GradeRecordId{row[0]}, StudentId{row[1]}, OfferingId{row[2]},
                CourseId{row[3]}, row[4], parseGradeLifecycle(row[5])});
        });
}

Result<vector<FacultyGradeStateEntry>> MySqlDataContext::gradeStateForOffering(
    OfferingId offeringId) const {
    return implementation_->withConnection<vector<FacultyGradeStateEntry>>(
        [&offeringId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT e.enrollment_id, e.student_id, e.offering_id, e.status, "
                "COALESCE(g.grade_record_id, ''), COALESCE(g.grade, ''), "
                "COALESCE(g.lifecycle, ''), o.course_id FROM enrollments e "
                "JOIN course_offerings o ON o.offering_id = e.offering_id "
                "LEFT JOIN grade_records g ON g.enrollment_id = e.enrollment_id "
                "WHERE e.offering_id = " + quoted(connection, offeringId.value()) +
                " AND e.status <> 'DROPPED' ORDER BY e.student_id, e.enrollment_id");
            if (!rows) {
                return failure<vector<FacultyGradeStateEntry>>(rows.error());
            }
            vector<FacultyGradeStateEntry> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 8) {
                    return mappingFailure<vector<FacultyGradeStateEntry>>(
                        "A Faculty grade-state query returned an unexpected shape.");
                }
                Enrollment enrollment{
                    EnrollmentId{row[0]}, StudentId{row[1]}, OfferingId{row[2]},
                    parseEnrollmentStatus(row[3])};
                optional<GradeRecord> grade;
                if (!row[4].empty()) {
                    if (row[5].empty() || row[6].empty()) {
                        return mappingFailure<vector<FacultyGradeStateEntry>>(
                            "A stored grade-state row is incomplete.");
                    }
                    grade = GradeRecord{
                        GradeRecordId{row[4]}, enrollment.studentId, enrollment.offeringId,
                        CourseId{row[7]}, row[5], parseGradeLifecycle(row[6])};
                }
                values.push_back({move(enrollment), move(grade)});
            }
            return Result<vector<FacultyGradeStateEntry>>::success(move(values));
        });
}

Result<vector<GradeRecord>> MySqlDataContext::pendingGradesForOffering(
    OfferingId offeringId) const {
    return implementation_->withConnection<vector<GradeRecord>>(
        [&offeringId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT g.grade_record_id, e.student_id, e.offering_id, o.course_id, "
                "g.grade, g.lifecycle FROM grade_records g "
                "JOIN enrollments e ON e.enrollment_id = g.enrollment_id "
                "JOIN course_offerings o ON o.offering_id = e.offering_id "
                "WHERE e.offering_id = " + quoted(connection, offeringId.value()) +
                " AND g.lifecycle = 'PENDING' ORDER BY e.student_id, g.grade_record_id");
            if (!rows) {
                return failure<vector<GradeRecord>>(rows.error());
            }
            vector<GradeRecord> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 6) {
                    return mappingFailure<vector<GradeRecord>>(
                        "A Pending-grade query returned an unexpected shape.");
                }
                values.push_back({
                    GradeRecordId{row[0]}, StudentId{row[1]}, OfferingId{row[2]},
                    CourseId{row[3]}, row[4], parseGradeLifecycle(row[5])});
            }
            return Result<vector<GradeRecord>>::success(move(values));
        });
}

Result<void> MySqlDataContext::createGradeRecord(GradeRecord record) {
    return implementation_->atomically([&record](MySqlConnection& connection) {
        if (record.id.empty() || record.studentId.empty() || record.offeringId.empty() ||
            record.courseId.empty() || record.grade.empty() ||
            record.lifecycle != GradeLifecycle::Pending) {
            return Result<void>::failure(
                "INVALID_RECORD", "A new grade requires connected IDs, a value, and Pending lifecycle.");
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
        auto inserted = connection.execute(
            "INSERT INTO grade_records (grade_record_id, enrollment_id, grade, lifecycle) VALUES (" +
            quoted(connection, record.id.value()) + ", " + quoted(connection, enrollment.value()) +
            ", " + quoted(connection, record.grade) + ", 'PENDING') "
            "ON DUPLICATE KEY UPDATE grade_record_id = grade_record_id");
        if (!inserted) {
            return inserted;
        }
        return ensureInserted(connection, "The new grade ID or enrolment");
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
        auto saved = connection.execute(
            "INSERT INTO grade_records (grade_record_id, enrollment_id, grade, lifecycle) VALUES (" +
            quoted(connection, record.id.value()) + ", " + quoted(connection, enrollment.value()) +
            ", " + quoted(connection, record.grade) + ", " +
            quoted(connection, gradeLifecycleSql(record.lifecycle)) +
            ") ON DUPLICATE KEY UPDATE grade = IF("
            "grade_record_id = VALUES(grade_record_id) AND "
            "enrollment_id = VALUES(enrollment_id) AND lifecycle = 'PENDING', "
            "VALUES(grade), grade), "
            "lifecycle = IF(grade_record_id = VALUES(grade_record_id) AND "
            "enrollment_id = VALUES(enrollment_id) AND lifecycle = 'PENDING', "
            "VALUES(lifecycle), lifecycle)");
        if (!saved) {
            return saved;
        }
        auto rows = connection.query(
            "SELECT enrollment_id, grade, lifecycle FROM grade_records "
            "WHERE grade_record_id = " + quoted(connection, record.id.value()));
        if (!rows) {
            return failure<void>(rows.error());
        }
        if (rows.value().size() != 1 || rows.value().front().size() != 3 ||
            rows.value().front()[0] != enrollment.value() ||
            rows.value().front()[1] != record.grade ||
            parseGradeLifecycle(rows.value().front()[2]) != record.lifecycle) {
            return Result<void>::failure(
                "PERSISTENCE_ID_CONFLICT",
                "The grade ID or enrolment conflicts with another persisted grade.");
        }
        return Result<void>::success();
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

Result<vector<CourseChangeRequest>> MySqlDataContext::changeRequestsForFaculty(
    FacultyId facultyId) const {
    return implementation_->withConnection<vector<CourseChangeRequest>>(
        [this, &facultyId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT change_request_id FROM course_change_requests WHERE faculty_id = " +
                quoted(connection, facultyId.value()) +
                " ORDER BY course_id, COALESCE(offering_id, ''), change_type, change_request_id");
            if (!rows) {
                return failure<vector<CourseChangeRequest>>(rows.error());
            }
            vector<CourseChangeRequest> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 1) {
                    return mappingFailure<vector<CourseChangeRequest>>(
                        "A Faculty change-request query returned an unexpected shape.");
                }
                auto request = findChangeRequest(ChangeRequestId{row[0]});
                if (!request) {
                    return failure<vector<CourseChangeRequest>>(request.error());
                }
                if (!request.value()) {
                    return mappingFailure<vector<CourseChangeRequest>>(
                        "A Faculty change request disappeared during its read.");
                }
                values.push_back(*request.value());
            }
            return Result<vector<CourseChangeRequest>>::success(move(values));
        });
}

Result<void> MySqlDataContext::createChangeRequest(CourseChangeRequest request) {
    return implementation_->atomically([&request](MySqlConnection& connection) {
        if (request.id.empty() || request.facultyId.empty() || request.courseId.empty() ||
            (request.requestedValue.empty() &&
             request.type != CourseChangeType::Prerequisites) ||
            request.status != CourseChangeStatus::Pending) {
            return Result<void>::failure(
                "INVALID_RECORD", "A new course-change request requires IDs, a value, and Pending status.");
        }
        const string offeringSql = request.offeringId
                                            ? quoted(connection, request.offeringId->value())
                                            : "NULL";
        auto inserted = connection.execute(
            "INSERT INTO course_change_requests "
            "(change_request_id, faculty_id, course_id, offering_id, change_type, "
            "requested_value, status) VALUES (" +
            quoted(connection, request.id.value()) + ", " +
            quoted(connection, request.facultyId.value()) + ", " +
            quoted(connection, request.courseId.value()) + ", " + offeringSql + ", " +
            quoted(connection, changeTypeSql(request.type)) + ", " +
            quoted(connection, request.requestedValue) + ", 'PENDING') "
            "ON DUPLICATE KEY UPDATE change_request_id = change_request_id");
        if (!inserted) {
            return inserted;
        }
        return ensureInserted(connection, "The new course-change request ID");
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
        auto saved = connection.execute(
            "INSERT INTO course_change_requests "
            "(change_request_id, faculty_id, course_id, offering_id, change_type, "
            "requested_value, status) VALUES (" +
            quoted(connection, request.id.value()) + ", " +
            quoted(connection, request.facultyId.value()) + ", " +
            quoted(connection, request.courseId.value()) + ", " + offeringSql + ", " +
            quoted(connection, changeTypeSql(request.type)) + ", " +
            quoted(connection, request.requestedValue) + ", " +
            quoted(connection, changeStatusSql(request.status)) +
            ") ON DUPLICATE KEY UPDATE requested_value = IF("
            "faculty_id = VALUES(faculty_id) AND course_id = VALUES(course_id) AND "
            "offering_id <=> VALUES(offering_id) AND change_type = VALUES(change_type), "
            "VALUES(requested_value), requested_value), status = IF("
            "faculty_id = VALUES(faculty_id) AND course_id = VALUES(course_id) AND "
            "offering_id <=> VALUES(offering_id) AND change_type = VALUES(change_type), "
            "VALUES(status), status)");
        if (!saved) {
            return saved;
        }
        auto rows = connection.query(
            "SELECT faculty_id, course_id, COALESCE(offering_id, ''), change_type, "
            "requested_value, status FROM course_change_requests WHERE change_request_id = " +
            quoted(connection, request.id.value()));
        if (!rows) {
            return failure<void>(rows.error());
        }
        const string expectedOffering = request.offeringId ? request.offeringId->value() : "";
        if (rows.value().size() != 1 || rows.value().front().size() != 6 ||
            rows.value().front()[0] != request.facultyId.value() ||
            rows.value().front()[1] != request.courseId.value() ||
            rows.value().front()[2] != expectedOffering ||
            parseChangeType(rows.value().front()[3]) != request.type ||
            rows.value().front()[4] != request.requestedValue ||
            parseChangeStatus(rows.value().front()[5]) != request.status) {
            return Result<void>::failure(
                "PERSISTENCE_ID_CONFLICT",
                "The change-request ID conflicts with another persisted request.");
        }
        return Result<void>::success();
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

Result<optional<WaitlistEntry>> MySqlDataContext::findStudentWaitlistEntry(
    StudentId studentId,
    OfferingId offeringId) const {
    return implementation_->withConnection<optional<WaitlistEntry>>(
        [&studentId, &offeringId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT waitlist_entry_id, student_id, offering_id, position, status "
                "FROM waitlist_entries WHERE student_id = " +
                quoted(connection, studentId.value()) + " AND offering_id = " +
                quoted(connection, offeringId.value()) + " LIMIT 1");
            if (!rows) {
                return failure<optional<WaitlistEntry>>(rows.error());
            }
            if (rows.value().empty()) {
                return Result<optional<WaitlistEntry>>::success(nullopt);
            }
            const auto& row = rows.value().front();
            if (row.size() != 5) {
                return mappingFailure<optional<WaitlistEntry>>(
                    "A Student-offering waitlist query returned an unexpected shape.");
            }
            return Result<optional<WaitlistEntry>>::success(WaitlistEntry{
                WaitlistEntryId{row[0]}, StudentId{row[1]}, OfferingId{row[2]},
                parseSize(row[3]), parseWaitlistStatus(row[4])});
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

Result<vector<WaitlistEntry>> MySqlDataContext::waitlistEntriesForStudent(
    StudentId studentId) const {
    return implementation_->withConnection<vector<WaitlistEntry>>(
        [&studentId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT waitlist_entry_id, student_id, offering_id, position, status "
                "FROM waitlist_entries WHERE student_id = " +
                quoted(connection, studentId.value()) +
                " AND status <> 'REMOVED' ORDER BY offering_id, position, waitlist_entry_id");
            if (!rows) {
                return failure<vector<WaitlistEntry>>(rows.error());
            }
            vector<WaitlistEntry> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 5) {
                    return mappingFailure<vector<WaitlistEntry>>(
                        "A Student waitlist query returned an unexpected shape.");
                }
                values.push_back({WaitlistEntryId{row[0]}, StudentId{row[1]},
                                  OfferingId{row[2]}, parseSize(row[3]),
                                  parseWaitlistStatus(row[4])});
            }
            return Result<vector<WaitlistEntry>>::success(move(values));
        });
}

Result<vector<WaitlistEntry>> MySqlDataContext::waitingEntriesForOffering(
    OfferingId offeringId) const {
    return implementation_->withConnection<vector<WaitlistEntry>>(
        [&offeringId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT waitlist_entry_id, student_id, offering_id, position, status "
                "FROM waitlist_entries WHERE offering_id = " +
                quoted(connection, offeringId.value()) +
                " AND status = 'WAITING' ORDER BY position, waitlist_entry_id");
            if (!rows) {
                return failure<vector<WaitlistEntry>>(rows.error());
            }
            vector<WaitlistEntry> values;
            for (const auto& row : rows.value()) {
                if (row.size() != 5) {
                    return mappingFailure<vector<WaitlistEntry>>(
                        "An offering waitlist query returned an unexpected shape.");
                }
                values.push_back({WaitlistEntryId{row[0]}, StudentId{row[1]},
                                  OfferingId{row[2]}, parseSize(row[3]),
                                  parseWaitlistStatus(row[4])});
            }
            return Result<vector<WaitlistEntry>>::success(move(values));
        });
}

Result<size_t> MySqlDataContext::nextWaitlistPosition(OfferingId offeringId) const {
    return implementation_->withConnection<size_t>(
        [&offeringId](MySqlConnection& connection) {
            auto rows = connection.query(
                "SELECT COALESCE(MAX(position), 0) + 1 FROM waitlist_entries "
                "WHERE offering_id = " + quoted(connection, offeringId.value()));
            if (!rows) {
                return failure<size_t>(rows.error());
            }
            if (rows.value().size() != 1 || rows.value().front().size() != 1) {
                return mappingFailure<size_t>(
                    "A next waitlist-position query returned an unexpected shape.");
            }
            const size_t position = parseSize(rows.value().front().front());
            if (position == 0) {
                return mappingFailure<size_t>("A waitlist position must be positive.");
            }
            return Result<size_t>::success(position);
        });
}

Result<void> MySqlDataContext::saveWaitlistEntry(WaitlistEntry entry) {
    return implementation_->atomically([&entry](MySqlConnection& connection) {
        if (entry.id.empty() || entry.studentId.empty() || entry.offeringId.empty() ||
            entry.position == 0) {
            return Result<void>::failure(
                "INVALID_RECORD", "A waitlist entry requires IDs and a positive position.");
        }
        auto saved = connection.execute(
            "INSERT INTO waitlist_entries "
            "(waitlist_entry_id, student_id, offering_id, position, status) VALUES (" +
            quoted(connection, entry.id.value()) + ", " +
            quoted(connection, entry.studentId.value()) + ", " +
            quoted(connection, entry.offeringId.value()) + ", " +
            to_string(entry.position) + ", " +
            quoted(connection, waitlistStatusSql(entry.status)) +
            ") ON DUPLICATE KEY UPDATE position = IF("
            "waitlist_entry_id = VALUES(waitlist_entry_id) AND "
            "student_id = VALUES(student_id) AND offering_id = VALUES(offering_id), "
            "VALUES(position), position), status = IF("
            "waitlist_entry_id = VALUES(waitlist_entry_id) AND "
            "student_id = VALUES(student_id) AND offering_id = VALUES(offering_id), "
            "VALUES(status), status)");
        if (!saved) {
            return saved;
        }
        auto rows = connection.query(
            "SELECT student_id, offering_id, position, status FROM waitlist_entries "
            "WHERE waitlist_entry_id = " + quoted(connection, entry.id.value()));
        if (!rows) {
            return failure<void>(rows.error());
        }
        if (rows.value().size() != 1 || rows.value().front().size() != 4 ||
            rows.value().front()[0] != entry.studentId.value() ||
            rows.value().front()[1] != entry.offeringId.value() ||
            parseSize(rows.value().front()[2]) != entry.position ||
            parseWaitlistStatus(rows.value().front()[3]) != entry.status) {
            return Result<void>::failure(
                "PERSISTENCE_ID_CONFLICT",
                "The waitlist ID or position conflicts with another persisted entry.");
        }
        return Result<void>::success();
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
