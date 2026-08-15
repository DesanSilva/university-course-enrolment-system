USE nexusenroll;

START TRANSACTION;

INSERT INTO departments (department_id, code, name) VALUES
    ('DEPT-BUS', 'BUS', 'Business'),
    ('DEPT-CS', 'CS', 'Computer Science');

INSERT INTO semesters (semester_id, code, starts_on, ends_on) VALUES
    ('SEM-2025S2', '2025S2', '2025-07-01', '2025-11-30'),
    ('SEM-2026S1', '2026S1', '2026-01-05', '2026-05-31');

INSERT INTO locations (location_id, building, room) VALUES
    ('LOC-BUS-201', 'Business', '201'),
    ('LOC-BUS-305', 'Business', '305'),
    ('LOC-TECH-101', 'Technology', '101'),
    ('LOC-TECH-202', 'Technology', '202'),
    ('LOC-TECH-220', 'Technology', '220');

INSERT INTO users (user_id, name, email, role, status) VALUES
    ('U-STU-001', 'Alice Perera', 'alice@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('U-STU-002', 'Ben Fernando', 'ben@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('U-STU-003', 'Chamara Silva', 'chamara@nexus.edu', 'STUDENT', 'INACTIVE'),
    ('U-FAC-001', 'Dr Maya Rao', 'maya.rao@nexus.edu', 'FACULTY', 'ACTIVE'),
    ('U-FAC-002', 'Prof Omar Khan', 'omar.khan@nexus.edu', 'FACULTY', 'ACTIVE'),
    ('U-FAC-003', 'Dr Nila Jay', 'nila.jay@nexus.edu', 'FACULTY', 'INACTIVE'),
    ('U-ADM-001', 'Ishani Deen', 'ishani@nexus.edu', 'ADMINISTRATOR', 'ACTIVE'),
    ('U-ADM-002', 'Ravi Sen', 'ravi@nexus.edu', 'ADMINISTRATOR', 'INACTIVE');

INSERT INTO degree_programs
    (program_id, department_id, name, required_credits) VALUES
    ('PROGRAM-BBA', 'DEPT-BUS', 'Bachelor of Business Administration', 120),
    ('PROGRAM-CS', 'DEPT-CS', 'Bachelor of Computer Science', 120);

INSERT INTO students (student_id, user_id, program_id) VALUES
    ('STU-001', 'U-STU-001', 'PROGRAM-CS'),
    ('STU-002', 'U-STU-002', 'PROGRAM-BBA'),
    ('STU-003', 'U-STU-003', 'PROGRAM-CS');

INSERT INTO faculty (faculty_id, user_id, department_id) VALUES
    ('FAC-001', 'U-FAC-001', 'DEPT-CS'),
    ('FAC-002', 'U-FAC-002', 'DEPT-BUS'),
    ('FAC-003', 'U-FAC-003', 'DEPT-CS');

INSERT INTO courses
    (course_id, department_id, course_number, code, name, description, credits) VALUES
    ('COURSE-BUS101', 'DEPT-BUS', '101', 'BUS101', 'Foundations of Business',
     'Introduction to organisations and markets.', 3),
    ('COURSE-BUS301', 'DEPT-BUS', '301', 'BUS301', 'Business Analytics',
     'Decision-making with business data.', 3),
    ('COURSE-CS101', 'DEPT-CS', '101', 'CS101', 'Programming Fundamentals',
     'Foundations of programming in C++.', 4),
    ('COURSE-CS201', 'DEPT-CS', '201', 'CS201', 'Data Structures',
     'Core data structures and algorithms.', 4),
    ('COURSE-CS220', 'DEPT-CS', '220', 'CS220', 'Software Architecture',
     'Architectural styles and design principles.', 4);

INSERT INTO course_prerequisites (course_id, prerequisite_course_id) VALUES
    ('COURSE-BUS301', 'COURSE-BUS101'),
    ('COURSE-CS201', 'COURSE-CS101'),
    ('COURSE-CS220', 'COURSE-CS101');

INSERT INTO program_required_courses (program_id, course_id) VALUES
    ('PROGRAM-BBA', 'COURSE-BUS101'),
    ('PROGRAM-BBA', 'COURSE-BUS301'),
    ('PROGRAM-CS', 'COURSE-CS101'),
    ('PROGRAM-CS', 'COURSE-CS201'),
    ('PROGRAM-CS', 'COURSE-CS220');

INSERT INTO course_offerings
    (offering_id, course_id, semester_id, instructor_id, capacity) VALUES
    ('OFFER-BUS101-2026S1', 'COURSE-BUS101', 'SEM-2026S1', 'FAC-002', 2),
    ('OFFER-BUS301-2026S1', 'COURSE-BUS301', 'SEM-2026S1', 'FAC-002', 40),
    ('OFFER-CS101-2025S2', 'COURSE-CS101', 'SEM-2025S2', 'FAC-001', 40),
    ('OFFER-CS101-2026S1', 'COURSE-CS101', 'SEM-2026S1', 'FAC-001', 2),
    ('OFFER-CS201-2026S1', 'COURSE-CS201', 'SEM-2026S1', 'FAC-001', 3),
    ('OFFER-CS220-2026S1', 'COURSE-CS220', 'SEM-2026S1', 'FAC-003', 1);

INSERT INTO schedule_slots
    (offering_id, day_of_week, starts_at, ends_at, location_id) VALUES
    ('OFFER-BUS101-2026S1', 1, '11:00:00', '12:00:00', 'LOC-BUS-201'),
    ('OFFER-BUS301-2026S1', 3, '10:00:00', '11:30:00', 'LOC-BUS-305'),
    ('OFFER-CS101-2025S2', 1, '09:00:00', '10:00:00', 'LOC-TECH-101'),
    ('OFFER-CS101-2026S1', 1, '09:00:00', '10:00:00', 'LOC-TECH-101'),
    ('OFFER-CS201-2026S1', 2, '09:00:00', '10:00:00', 'LOC-TECH-202'),
    ('OFFER-CS220-2026S1', 2, '09:30:00', '10:30:00', 'LOC-TECH-220');

INSERT INTO enrollments (enrollment_id, student_id, offering_id, status) VALUES
    ('ENR-001', 'STU-001', 'OFFER-CS201-2026S1', 'ACTIVE'),
    ('ENR-002', 'STU-002', 'OFFER-BUS101-2026S1', 'ACTIVE'),
    ('ENR-003', 'STU-003', 'OFFER-CS220-2026S1', 'ACTIVE'),
    ('ENR-004', 'STU-001', 'OFFER-CS101-2025S2', 'COMPLETED'),
    ('ENR-005', 'STU-001', 'OFFER-BUS101-2026S1', 'ACTIVE');

INSERT INTO grade_records (grade_record_id, enrollment_id, grade, lifecycle) VALUES
    ('GRADE-001', 'ENR-004', 'A', 'SUBMITTED');

INSERT INTO waitlist_entries
    (waitlist_entry_id, student_id, offering_id, position, status) VALUES
    ('WAIT-001', 'STU-002', 'OFFER-CS220-2026S1', 1, 'WAITING');

INSERT INTO course_change_requests
    (change_request_id, faculty_id, course_id, offering_id, change_type,
     requested_value, status) VALUES
    ('CHANGE-001', 'FAC-002', 'COURSE-BUS301', 'OFFER-BUS301-2026S1',
     'CAPACITY', '60', 'PENDING');

COMMIT;
