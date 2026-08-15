CREATE DATABASE IF NOT EXISTS nexusenroll
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_0900_ai_ci;

USE nexusenroll;

CREATE TABLE departments (
    department_id VARCHAR(32) PRIMARY KEY,
    code VARCHAR(16) NOT NULL UNIQUE,
    name VARCHAR(120) NOT NULL UNIQUE
) ENGINE = InnoDB;

CREATE TABLE semesters (
    semester_id VARCHAR(32) PRIMARY KEY,
    code VARCHAR(20) NOT NULL UNIQUE,
    starts_on DATE NOT NULL,
    ends_on DATE NOT NULL,
    CONSTRAINT chk_semester_dates CHECK (starts_on < ends_on)
) ENGINE = InnoDB;

CREATE TABLE locations (
    location_id VARCHAR(32) PRIMARY KEY,
    building VARCHAR(80) NOT NULL,
    room VARCHAR(32) NOT NULL,
    CONSTRAINT uq_location UNIQUE (building, room)
) ENGINE = InnoDB;

CREATE TABLE users (
    user_id VARCHAR(32) PRIMARY KEY,
    name VARCHAR(160) NOT NULL,
    email VARCHAR(254) NOT NULL UNIQUE,
    role VARCHAR(20) NOT NULL,
    status VARCHAR(20) NOT NULL,
    CONSTRAINT chk_user_role CHECK (role IN ('STUDENT', 'FACULTY', 'ADMINISTRATOR')),
    CONSTRAINT chk_user_status CHECK (status IN ('ACTIVE', 'INACTIVE'))
) ENGINE = InnoDB;

CREATE TABLE degree_programs (
    program_id VARCHAR(32) PRIMARY KEY,
    department_id VARCHAR(32) NOT NULL,
    name VARCHAR(160) NOT NULL UNIQUE,
    required_credits SMALLINT UNSIGNED NOT NULL,
    CONSTRAINT fk_program_department FOREIGN KEY (department_id)
        REFERENCES departments (department_id)
) ENGINE = InnoDB;

CREATE TABLE students (
    student_id VARCHAR(32) PRIMARY KEY,
    user_id VARCHAR(32) NOT NULL UNIQUE,
    program_id VARCHAR(32) NOT NULL,
    CONSTRAINT fk_student_user FOREIGN KEY (user_id) REFERENCES users (user_id),
    CONSTRAINT fk_student_program FOREIGN KEY (program_id)
        REFERENCES degree_programs (program_id)
) ENGINE = InnoDB;

CREATE TABLE faculty (
    faculty_id VARCHAR(32) PRIMARY KEY,
    user_id VARCHAR(32) NOT NULL UNIQUE,
    department_id VARCHAR(32) NOT NULL,
    CONSTRAINT fk_faculty_user FOREIGN KEY (user_id) REFERENCES users (user_id),
    CONSTRAINT fk_faculty_department FOREIGN KEY (department_id)
        REFERENCES departments (department_id)
) ENGINE = InnoDB;

CREATE TABLE courses (
    course_id VARCHAR(32) PRIMARY KEY,
    department_id VARCHAR(32) NOT NULL,
    course_number VARCHAR(20) NOT NULL,
    code VARCHAR(24) NOT NULL UNIQUE,
    name VARCHAR(160) NOT NULL,
    description TEXT NOT NULL,
    credits SMALLINT UNSIGNED NOT NULL,
    CONSTRAINT uq_department_course_number UNIQUE (department_id, course_number),
    CONSTRAINT fk_course_department FOREIGN KEY (department_id)
        REFERENCES departments (department_id)
) ENGINE = InnoDB;

CREATE TABLE course_prerequisites (
    course_id VARCHAR(32) NOT NULL,
    prerequisite_course_id VARCHAR(32) NOT NULL,
    PRIMARY KEY (course_id, prerequisite_course_id),
    CONSTRAINT chk_distinct_prerequisite CHECK (course_id <> prerequisite_course_id),
    CONSTRAINT fk_prerequisite_course FOREIGN KEY (course_id)
        REFERENCES courses (course_id) ON DELETE CASCADE,
    CONSTRAINT fk_prerequisite_required FOREIGN KEY (prerequisite_course_id)
        REFERENCES courses (course_id)
) ENGINE = InnoDB;

CREATE TABLE program_required_courses (
    program_id VARCHAR(32) NOT NULL,
    course_id VARCHAR(32) NOT NULL,
    PRIMARY KEY (program_id, course_id),
    CONSTRAINT fk_requirement_program FOREIGN KEY (program_id)
        REFERENCES degree_programs (program_id) ON DELETE CASCADE,
    CONSTRAINT fk_requirement_course FOREIGN KEY (course_id)
        REFERENCES courses (course_id)
) ENGINE = InnoDB;

CREATE TABLE course_offerings (
    offering_id VARCHAR(48) PRIMARY KEY,
    course_id VARCHAR(32) NOT NULL,
    semester_id VARCHAR(32) NOT NULL,
    instructor_id VARCHAR(32) NOT NULL,
    capacity SMALLINT UNSIGNED NOT NULL,
    CONSTRAINT chk_offering_capacity CHECK (capacity > 0),
    CONSTRAINT fk_offering_course FOREIGN KEY (course_id) REFERENCES courses (course_id),
    CONSTRAINT fk_offering_semester FOREIGN KEY (semester_id)
        REFERENCES semesters (semester_id),
    CONSTRAINT fk_offering_instructor FOREIGN KEY (instructor_id)
        REFERENCES faculty (faculty_id)
) ENGINE = InnoDB;

CREATE TABLE schedule_slots (
    schedule_slot_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    offering_id VARCHAR(48) NOT NULL,
    day_of_week TINYINT UNSIGNED NOT NULL,
    starts_at TIME NOT NULL,
    ends_at TIME NOT NULL,
    location_id VARCHAR(32) NOT NULL,
    CONSTRAINT uq_offering_slot UNIQUE (offering_id, day_of_week, starts_at, ends_at),
    CONSTRAINT chk_schedule_day CHECK (day_of_week BETWEEN 1 AND 7),
    CONSTRAINT chk_schedule_times CHECK (starts_at < ends_at),
    CONSTRAINT fk_schedule_offering FOREIGN KEY (offering_id)
        REFERENCES course_offerings (offering_id) ON DELETE CASCADE,
    CONSTRAINT fk_schedule_location FOREIGN KEY (location_id)
        REFERENCES locations (location_id)
) ENGINE = InnoDB;

CREATE TABLE enrollments (
    enrollment_id VARCHAR(40) PRIMARY KEY,
    student_id VARCHAR(32) NOT NULL,
    offering_id VARCHAR(48) NOT NULL,
    status VARCHAR(20) NOT NULL,
    CONSTRAINT uq_student_offering UNIQUE (student_id, offering_id),
    CONSTRAINT chk_enrollment_status CHECK (status IN ('ACTIVE', 'DROPPED', 'COMPLETED')),
    CONSTRAINT fk_enrollment_student FOREIGN KEY (student_id)
        REFERENCES students (student_id),
    CONSTRAINT fk_enrollment_offering FOREIGN KEY (offering_id)
        REFERENCES course_offerings (offering_id)
) ENGINE = InnoDB;

CREATE TABLE grade_records (
    grade_record_id VARCHAR(40) PRIMARY KEY,
    enrollment_id VARCHAR(40) NOT NULL UNIQUE,
    grade VARCHAR(8) NOT NULL,
    lifecycle VARCHAR(20) NOT NULL,
    CONSTRAINT chk_grade_lifecycle CHECK (lifecycle IN ('PENDING', 'SUBMITTED')),
    CONSTRAINT fk_grade_enrollment FOREIGN KEY (enrollment_id)
        REFERENCES enrollments (enrollment_id)
) ENGINE = InnoDB;

CREATE TABLE course_change_requests (
    change_request_id VARCHAR(40) PRIMARY KEY,
    faculty_id VARCHAR(32) NOT NULL,
    course_id VARCHAR(32) NOT NULL,
    offering_id VARCHAR(48) NULL,
    change_type VARCHAR(24) NOT NULL,
    requested_value TEXT NOT NULL,
    status VARCHAR(20) NOT NULL,
    CONSTRAINT chk_change_type CHECK (
        change_type IN ('DESCRIPTION', 'PREREQUISITES', 'CAPACITY')
    ),
    CONSTRAINT chk_change_status CHECK (status IN ('PENDING', 'APPROVED', 'REJECTED')),
    CONSTRAINT fk_change_faculty FOREIGN KEY (faculty_id) REFERENCES faculty (faculty_id),
    CONSTRAINT fk_change_course FOREIGN KEY (course_id) REFERENCES courses (course_id),
    CONSTRAINT fk_change_offering FOREIGN KEY (offering_id)
        REFERENCES course_offerings (offering_id)
) ENGINE = InnoDB;

CREATE TABLE waitlist_entries (
    waitlist_entry_id VARCHAR(40) PRIMARY KEY,
    student_id VARCHAR(32) NOT NULL,
    offering_id VARCHAR(48) NOT NULL,
    position SMALLINT UNSIGNED NOT NULL,
    status VARCHAR(20) NOT NULL,
    CONSTRAINT uq_waitlist_student_offering UNIQUE (student_id, offering_id),
    CONSTRAINT uq_waitlist_position UNIQUE (offering_id, position),
    CONSTRAINT chk_waitlist_position CHECK (position > 0),
    CONSTRAINT chk_waitlist_status CHECK (status IN ('WAITING', 'OFFERED', 'REMOVED')),
    CONSTRAINT fk_waitlist_student FOREIGN KEY (student_id) REFERENCES students (student_id),
    CONSTRAINT fk_waitlist_offering FOREIGN KEY (offering_id)
        REFERENCES course_offerings (offering_id)
) ENGINE = InnoDB;

CREATE INDEX idx_offerings_semester ON course_offerings (semester_id);
CREATE INDEX idx_enrollments_offering_status ON enrollments (offering_id, status);
CREATE INDEX idx_changes_status ON course_change_requests (status);
CREATE INDEX idx_waitlist_offering_status ON waitlist_entries (offering_id, status, position);
