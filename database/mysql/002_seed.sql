USE nexusenroll;

START TRANSACTION;

INSERT INTO departments (department_id, code, name) VALUES
    ('DEPT-BUS', 'BUS', 'Business'),
    ('DEPT-TECH', 'TECH', 'Technology'),
    ('DEPT-CS', 'CS', 'Computer Science'),
    ('DEPT-MED', 'MED', 'Medicine'),
    ('DEPT-SCI', 'SCI', 'Science'),
    ('DEPT-ART', 'ART', 'Arts'),
    ('DEPT-LAW', 'LAW', 'Law');

INSERT INTO semesters (semester_id, code, starts_on, ends_on) VALUES
    ('SEM-2025S2', '2025S2', '2025-07-01', '2025-11-30'),
    ('SEM-2026S1', '2026S1', '2026-01-05', '2026-05-31'),
    ('SEM-2026S2', '2026S2', '2026-06-01', '2026-11-30'),
    ('SEM-2027S1', '2027S1', '2027-01-05', '2027-05-31');

INSERT INTO locations (location_id, building, room) VALUES
    ('LOC-BUS-201', 'Business', '201'),
    ('LOC-BUS-305', 'Business', '305'),
    ('LOC-CS-105', 'Computer Science', '105'),
    ('LOC-CS-175', 'Computer Science', '175'),
    ('LOC-CS-235', 'Computer Science', '235'),
    ('LOC-TECH-101', 'Technology', '101'),
    ('LOC-TECH-202', 'Technology', '202'),
    ('LOC-TECH-220', 'Technology', '220'),
    ('LOC-MED-101', 'Medicine', '101'),
    ('LOC-MED-205', 'Medicine', '205'),
    ('LOC-MED-310', 'Medicine', '310'),
    ('LOC-SCI-101', 'Science', '101'),
    ('LOC-SCI-205', 'Science', '205'),
    ('LOC-SCI-310', 'Science', '310'),
    ('LOC-ART-101', 'Arts', '101'),
    ('LOC-ART-205', 'Arts', '205'),
    ('LOC-ART-310', 'Arts', '310'),
    ('LOC-LAW-101', 'Law', '101'),
    ('LOC-LAW-205', 'Law', '205'),
    ('LOC-LAW-310', 'Law', '310');

INSERT INTO users (user_id, name, email, role, status) VALUES
    ('2024CS116', 'G J Liyanage', 'gjl@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024CS226', 'W M O L Wijesinghe', 'wmolw@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024CS122', 'M G K C Madhubhashana', 'mgkcm@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024CS006', 'M R R Ahamed', 'mrra@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024CS196', 'Desan Silva', 'ds@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024CS093', 'A K Y I Jayasena', 'akylj@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024CS001', 'Alice Perera', 'alice@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024BBA001', 'Ben Fernando', 'ben@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024CS003', 'Chamara Silva', 'chamara@nexus.edu', 'STUDENT', 'INACTIVE'),
    ('2024MED001', 'Dilan Perera', 'dilan.perera@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024MED002', 'Nethmi Fernando', 'nethmi.fernando@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024MED003', 'Kasun Jayawardena', 'kasun.j@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024SCI001', 'Ayesha Silva', 'ayesha.silva@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024SCI002', 'Ravindu Senanayake', 'ravindu.s@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024SCI003', 'Hiruni Perera', 'hiruni.p@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024ART001', 'Malith Fernando', 'malith.f@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024ART002', 'Tharushi Jayasuriya', 'tharushi.j@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024ART003', 'Sahan Wijeratne', 'sahan.w@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024LAW001', 'Kavindu Perera', 'kavindu.p@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024LAW002', 'Dinithi Silva', 'dinithi.s@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024LAW003', 'Rashmi Fernando', 'rashmi.f@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024TECH001', 'Isuru Bandara', 'isuru.b@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024TECH002', 'Piumi Rathnayake', 'piumi.r@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024TECH003', 'Nuwan Perera', 'nuwan.p@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024BUS001', 'Sewmi Fernando', 'sewmi.f@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024BUS002', 'Tharindu Silva', 'tharindu.s@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024BUS003', 'Madhavi Perera', 'madhavi.p@nexus.edu', 'STUDENT', 'ACTIVE'),
    ('2024FAC001', 'Dr Maya Rao', 'maya.rao@nexus.edu', 'FACULTY', 'ACTIVE'),
    ('2021FAC002', 'Prof Omar Khan', 'omar.khan@nexus.edu', 'FACULTY', 'ACTIVE'),
    ('2023FAC003', 'Dr Nila Jay', 'nila.jay@nexus.edu', 'FACULTY', 'INACTIVE'),
    ('2022FAC004', 'Dr Arjun Perera', 'arjun.perera@nexus.edu', 'FACULTY', 'ACTIVE'),
    ('2020FAC005', 'Dr Sarah Fernando', 'sarah.fernando@nexus.edu', 'FACULTY', 'ACTIVE'),
    ('2021FAC006', 'Prof Ravi Silva', 'ravi.silva@nexus.edu', 'FACULTY', 'ACTIVE'),
    ('2022FAC007', 'Dr Anjali Sen', 'anjali.sen@nexus.edu', 'FACULTY', 'ACTIVE'),
    ('2023FAC008', 'Prof Kamal Jay', 'kamal.jay@nexus.edu', 'FACULTY', 'ACTIVE'),
    ('2020FAC009', 'Dr Farah Khan', 'farah.khan@nexus.edu', 'FACULTY', 'ACTIVE'),
    ('2020ADM001', 'Ishani Deen', 'ishani@nexus.edu', 'ADMINISTRATOR', 'ACTIVE'),
    ('2020ADM002', 'Ravi Sen', 'ravi@nexus.edu', 'ADMINISTRATOR', 'INACTIVE');

INSERT INTO degree_programs (program_id, department_id, name, required_credits) VALUES
    ('PROGRAM-BBA', 'DEPT-BUS', 'Bachelor of Business Administration', 120),
    ('PROGRAM-TECH', 'DEPT-TECH', 'Bachelor of Technology', 120),
    ('PROGRAM-CS', 'DEPT-CS', 'Bachelor of Computer Science', 120),
    ('PROGRAM-MED', 'DEPT-MED', 'Bachelor of Medicine', 150),
    ('PROGRAM-SCI', 'DEPT-SCI', 'Bachelor of Science', 120),
    ('PROGRAM-ART', 'DEPT-ART', 'Bachelor of Arts', 120),
    ('PROGRAM-LAW', 'DEPT-LAW', 'Bachelor of Laws', 120);

INSERT INTO students (student_id, user_id, program_id) VALUES
    ('2024CS116', '2024CS116', 'PROGRAM-CS'),
    ('2024CS226', '2024CS226', 'PROGRAM-CS'),
    ('2024CS122', '2024CS122', 'PROGRAM-CS'),
    ('2024CS006', '2024CS006', 'PROGRAM-CS'),
    ('2024CS196', '2024CS196', 'PROGRAM-CS'),
    ('2024CS093', '2024CS093', 'PROGRAM-CS'),
    ('2024CS001', '2024CS001', 'PROGRAM-CS'),
    ('2024BBA001', '2024BBA001', 'PROGRAM-BBA'),
    ('2024CS003', '2024CS003', 'PROGRAM-CS'),
    ('2024MED001', '2024MED001', 'PROGRAM-MED'),
    ('2024MED002', '2024MED002', 'PROGRAM-MED'),
    ('2024MED003', '2024MED003', 'PROGRAM-MED'),
    ('2024SCI001', '2024SCI001', 'PROGRAM-SCI'),
    ('2024SCI002', '2024SCI002', 'PROGRAM-SCI'),
    ('2024SCI003', '2024SCI003', 'PROGRAM-SCI'),
    ('2024ART001', '2024ART001', 'PROGRAM-ART'),
    ('2024ART002', '2024ART002', 'PROGRAM-ART'),
    ('2024ART003', '2024ART003', 'PROGRAM-ART'),
    ('2024LAW001', '2024LAW001', 'PROGRAM-LAW'),
    ('2024LAW002', '2024LAW002', 'PROGRAM-LAW'),
    ('2024LAW003', '2024LAW003', 'PROGRAM-LAW'),
    ('2024TECH001', '2024TECH001', 'PROGRAM-TECH'),
    ('2024TECH002', '2024TECH002', 'PROGRAM-TECH'),
    ('2024TECH003', '2024TECH003', 'PROGRAM-TECH'),
    ('2024BUS001', '2024BUS001', 'PROGRAM-BBA'),
    ('2024BUS002', '2024BUS002', 'PROGRAM-BBA'),
    ('2024BUS003', '2024BUS003', 'PROGRAM-BBA');

INSERT INTO faculty (faculty_id, user_id, department_id) VALUES
    ('2024FAC001', '2024FAC001', 'DEPT-CS'),
    ('2021FAC002', '2021FAC002', 'DEPT-BUS'),
    ('2023FAC003', '2023FAC003', 'DEPT-CS'),
    ('2022FAC004', '2022FAC004', 'DEPT-MED'),
    ('2020FAC005', '2020FAC005', 'DEPT-MED'),
    ('2021FAC006', '2021FAC006', 'DEPT-SCI'),
    ('2022FAC007', '2022FAC007', 'DEPT-ART'),
    ('2023FAC008', '2023FAC008', 'DEPT-LAW'),
    ('2020FAC009', '2020FAC009', 'DEPT-TECH');

INSERT INTO courses
    (course_id, department_id, course_number, code, name, description, credits) VALUES
    ('COURSE-BUS101', 'DEPT-BUS', '101', 'BUS101', 'Foundations of Business', 'Introduction to organisations and markets.', 3),
    ('COURSE-BUS301', 'DEPT-BUS', '301', 'BUS301', 'Business Analytics', 'Decision-making with business data.', 3),
    ('COURSE-CS101', 'DEPT-CS', '101', 'CS101', 'Programming Fundamentals', 'Foundations of programming in C++.', 4),
    ('COURSE-CS201', 'DEPT-CS', '201', 'CS201', 'Data Structures', 'Core data structures and algorithms.', 4),
    ('COURSE-CS220', 'DEPT-CS', '220', 'CS220', 'Software Architecture', 'Architectural styles and design principles.', 4),
    ('COURSE-TECH101', 'DEPT-TECH', '101', 'TECH101', 'Technology Fundamentals', 'Introduction to modern technology and systems.', 3),
    ('COURSE-TECH201', 'DEPT-TECH', '201', 'TECH201', 'Computer Systems', 'Fundamentals of computer hardware and systems.', 4),
    ('COURSE-TECH301', 'DEPT-TECH', '301', 'TECH301', 'Emerging Technologies', 'Study of emerging technology trends and applications.', 3),
    ('COURSE-MED101', 'DEPT-MED', '101', 'MED101', 'Human Anatomy', 'Introduction to human anatomy and body structures.', 5),
    ('COURSE-MED201', 'DEPT-MED', '201', 'MED201', 'Human Physiology', 'Study of physiological processes in the human body.', 5),
    ('COURSE-MED301', 'DEPT-MED', '301', 'MED301', 'Clinical Medicine', 'Introduction to clinical diagnosis and patient care.', 6),
    ('COURSE-SCI101', 'DEPT-SCI', '101', 'SCI101', 'General Science', 'Foundations of scientific principles and methods.', 3),
    ('COURSE-SCI201', 'DEPT-SCI', '201', 'SCI201', 'Applied Physics', 'Principles of physics and practical applications.', 4),
    ('COURSE-SCI301', 'DEPT-SCI', '301', 'SCI301', 'Scientific Computing', 'Computational methods for scientific research.', 4),
    ('COURSE-ART101', 'DEPT-ART', '101', 'ART101', 'Introduction to Arts', 'Introduction to visual and performing arts.', 3),
    ('COURSE-ART201', 'DEPT-ART', '201', 'ART201', 'Art History', 'Historical development of major artistic movements.', 3),
    ('COURSE-ART301', 'DEPT-ART', '301', 'ART301', 'Creative Practice', 'Practical creative methods and artistic production.', 4),
    ('COURSE-LAW101', 'DEPT-LAW', '101', 'LAW101', 'Introduction to Law', 'Foundations of legal systems and principles.', 3),
    ('COURSE-LAW201', 'DEPT-LAW', '201', 'LAW201', 'Constitutional Law', 'Principles of constitutional law and governance.', 4),
    ('COURSE-LAW301', 'DEPT-LAW', '301', 'LAW301', 'Criminal Law', 'Fundamental principles of criminal law.', 4);

INSERT INTO course_prerequisites (course_id, prerequisite_course_id) VALUES
    ('COURSE-BUS301', 'COURSE-BUS101'),
    ('COURSE-CS201', 'COURSE-CS101'),
    ('COURSE-CS220', 'COURSE-CS101'),
    ('COURSE-TECH201', 'COURSE-TECH101'),
    ('COURSE-TECH301', 'COURSE-TECH201'),
    ('COURSE-MED201', 'COURSE-MED101'),
    ('COURSE-MED301', 'COURSE-MED201'),
    ('COURSE-SCI201', 'COURSE-SCI101'),
    ('COURSE-SCI301', 'COURSE-SCI201'),
    ('COURSE-ART201', 'COURSE-ART101'),
    ('COURSE-ART301', 'COURSE-ART201'),
    ('COURSE-LAW201', 'COURSE-LAW101'),
    ('COURSE-LAW301', 'COURSE-LAW201');

INSERT INTO program_required_courses (program_id, course_id) VALUES
    ('PROGRAM-BBA', 'COURSE-BUS101'),
    ('PROGRAM-BBA', 'COURSE-BUS301'),
    ('PROGRAM-CS', 'COURSE-CS101'),
    ('PROGRAM-CS', 'COURSE-CS201'),
    ('PROGRAM-CS', 'COURSE-CS220'),
    ('PROGRAM-TECH', 'COURSE-TECH101'),
    ('PROGRAM-TECH', 'COURSE-TECH201'),
    ('PROGRAM-TECH', 'COURSE-TECH301'),
    ('PROGRAM-MED', 'COURSE-MED101'),
    ('PROGRAM-MED', 'COURSE-MED201'),
    ('PROGRAM-MED', 'COURSE-MED301'),
    ('PROGRAM-SCI', 'COURSE-SCI101'),
    ('PROGRAM-SCI', 'COURSE-SCI201'),
    ('PROGRAM-SCI', 'COURSE-SCI301'),
    ('PROGRAM-ART', 'COURSE-ART101'),
    ('PROGRAM-ART', 'COURSE-ART201'),
    ('PROGRAM-ART', 'COURSE-ART301'),
    ('PROGRAM-LAW', 'COURSE-LAW101'),
    ('PROGRAM-LAW', 'COURSE-LAW201'),
    ('PROGRAM-LAW', 'COURSE-LAW301');

INSERT INTO course_offerings
    (offering_id, course_id, semester_id, instructor_id, capacity) VALUES
    ('OFFER-BUS101-2026S1', 'COURSE-BUS101', 'SEM-2026S1', '2021FAC002', 30),
    ('OFFER-BUS301-2026S1', 'COURSE-BUS301', 'SEM-2026S1', '2021FAC002', 40),
    ('OFFER-CS101-2025S2', 'COURSE-CS101', 'SEM-2025S2', '2024FAC001', 40),
    ('OFFER-CS101-2026S1', 'COURSE-CS101', 'SEM-2026S1', '2024FAC001', 40),
    ('OFFER-CS201-2026S1', 'COURSE-CS201', 'SEM-2026S1', '2024FAC001', 30),
    ('OFFER-CS220-2026S1', 'COURSE-CS220', 'SEM-2026S1', '2023FAC003', 25),
    ('OFFER-TECH101-2026S1', 'COURSE-TECH101', 'SEM-2026S1', '2020FAC009', 40),
    ('OFFER-TECH201-2026S1', 'COURSE-TECH201', 'SEM-2026S1', '2020FAC009', 30),
    ('OFFER-TECH301-2026S2', 'COURSE-TECH301', 'SEM-2026S2', '2020FAC009', 25),
    ('OFFER-MED101-2026S1', 'COURSE-MED101', 'SEM-2026S1', '2022FAC004', 30),
    ('OFFER-MED201-2026S2', 'COURSE-MED201', 'SEM-2026S2', '2020FAC005', 25),
    ('OFFER-MED301-2027S1', 'COURSE-MED301', 'SEM-2027S1', '2020FAC005', 20),
    ('OFFER-SCI101-2026S1', 'COURSE-SCI101', 'SEM-2026S1', '2021FAC006', 40),
    ('OFFER-SCI201-2026S1', 'COURSE-SCI201', 'SEM-2026S1', '2021FAC006', 30),
    ('OFFER-SCI301-2026S2', 'COURSE-SCI301', 'SEM-2026S2', '2021FAC006', 25),
    ('OFFER-ART101-2026S1', 'COURSE-ART101', 'SEM-2026S1', '2022FAC007', 40),
    ('OFFER-ART201-2026S1', 'COURSE-ART201', 'SEM-2026S1', '2022FAC007', 30),
    ('OFFER-ART301-2026S2', 'COURSE-ART301', 'SEM-2026S2', '2022FAC007', 25),
    ('OFFER-LAW101-2026S1', 'COURSE-LAW101', 'SEM-2026S1', '2023FAC008', 40),
    ('OFFER-LAW201-2026S1', 'COURSE-LAW201', 'SEM-2026S1', '2023FAC008', 30),
    ('OFFER-LAW301-2026S2', 'COURSE-LAW301', 'SEM-2026S2', '2023FAC008', 25);

INSERT INTO schedule_slots
    (offering_id, day_of_week, starts_at, ends_at, location_id) VALUES
    ('OFFER-BUS101-2026S1', 1, '11:00:00', '12:00:00', 'LOC-BUS-201'),
    ('OFFER-BUS301-2026S1', 3, '10:00:00', '11:30:00', 'LOC-BUS-305'),
    ('OFFER-CS101-2025S2', 1, '09:00:00', '10:00:00', 'LOC-TECH-101'),
    ('OFFER-CS101-2026S1', 1, '09:00:00', '10:00:00', 'LOC-TECH-101'),
    ('OFFER-CS201-2026S1', 2, '09:00:00', '10:00:00', 'LOC-TECH-202'),
    ('OFFER-CS220-2026S1', 2, '09:30:00', '10:30:00', 'LOC-TECH-220'),
    ('OFFER-TECH101-2026S1', 1, '13:00:00', '14:00:00', 'LOC-TECH-101'),
    ('OFFER-TECH201-2026S1', 3, '13:00:00', '14:30:00', 'LOC-TECH-202'),
    ('OFFER-TECH301-2026S2', 4, '14:00:00', '15:30:00', 'LOC-TECH-220'),
    ('OFFER-MED101-2026S1', 1, '08:00:00', '10:00:00', 'LOC-MED-101'),
    ('OFFER-MED201-2026S2', 2, '08:00:00', '10:00:00', 'LOC-MED-205'),
    ('OFFER-MED301-2027S1', 3, '09:00:00', '11:00:00', 'LOC-MED-310'),
    ('OFFER-SCI101-2026S1', 2, '10:00:00', '11:00:00', 'LOC-SCI-101'),
    ('OFFER-SCI201-2026S1', 3, '11:00:00', '12:30:00', 'LOC-SCI-205'),
    ('OFFER-SCI301-2026S2', 4, '10:00:00', '11:30:00', 'LOC-SCI-310'),
    ('OFFER-ART101-2026S1', 1, '14:00:00', '15:00:00', 'LOC-ART-101'),
    ('OFFER-ART201-2026S1', 3, '14:00:00', '15:00:00', 'LOC-ART-205'),
    ('OFFER-ART301-2026S2', 5, '13:00:00', '14:30:00', 'LOC-ART-310'),
    ('OFFER-LAW101-2026S1', 2, '15:00:00', '16:00:00', 'LOC-LAW-101'),
    ('OFFER-LAW201-2026S1', 4, '15:00:00', '16:30:00', 'LOC-LAW-205'),
    ('OFFER-LAW301-2026S2', 5, '15:00:00', '16:30:00', 'LOC-LAW-310');

INSERT INTO enrollments (enrollment_id, student_id, offering_id, status) VALUES
    ('ENR-001', '2024CS001', 'OFFER-CS201-2026S1', 'ACTIVE'),
    ('ENR-002', '2024BBA001', 'OFFER-BUS101-2026S1', 'ACTIVE'),
    ('ENR-003', '2024CS003', 'OFFER-CS220-2026S1', 'ACTIVE'),
    ('ENR-004', '2024CS001', 'OFFER-CS101-2025S2', 'COMPLETED'),
    ('ENR-005', '2024CS001', 'OFFER-BUS101-2026S1', 'ACTIVE'),
    ('ENR-006', '2024MED001', 'OFFER-MED101-2026S1', 'ACTIVE'),
    ('ENR-007', '2024MED002', 'OFFER-MED101-2026S1', 'ACTIVE'),
    ('ENR-008', '2024SCI001', 'OFFER-SCI101-2026S1', 'ACTIVE'),
    ('ENR-009', '2024SCI002', 'OFFER-SCI201-2026S1', 'ACTIVE'),
    ('ENR-010', '2024ART001', 'OFFER-ART101-2026S1', 'ACTIVE'),
    ('ENR-011', '2024ART002', 'OFFER-ART201-2026S1', 'ACTIVE'),
    ('ENR-012', '2024LAW001', 'OFFER-LAW101-2026S1', 'ACTIVE'),
    ('ENR-013', '2024LAW002', 'OFFER-LAW201-2026S1', 'ACTIVE'),
    ('ENR-014', '2024TECH001', 'OFFER-TECH101-2026S1', 'ACTIVE'),
    ('ENR-015', '2024TECH002', 'OFFER-TECH201-2026S1', 'ACTIVE'),
    ('ENR-016', '2024BUS001', 'OFFER-BUS101-2026S1', 'ACTIVE'),
    ('ENR-017', '2024BUS002', 'OFFER-BUS301-2026S1', 'ACTIVE');

INSERT INTO grade_records (grade_record_id, enrollment_id, grade, lifecycle) VALUES
    ('GRADE-001', 'ENR-004', 'A', 'SUBMITTED'),
    ('GRADE-002', 'ENR-006', 'A-', 'SUBMITTED'),
    ('GRADE-003', 'ENR-008', 'B+', 'SUBMITTED'),
    ('GRADE-004', 'ENR-010', 'A', 'SUBMITTED'),
    ('GRADE-005', 'ENR-012', 'B+', 'SUBMITTED'),
    ('GRADE-006', 'ENR-014', 'A-', 'SUBMITTED'),
    ('GRADE-007', 'ENR-016', 'B', 'SUBMITTED');

INSERT INTO waitlist_entries
    (waitlist_entry_id, student_id, offering_id, position, status) VALUES
    ('WAIT-001', '2024BBA001', 'OFFER-CS220-2026S1', 1, 'WAITING'),
    ('WAIT-002', '2024MED003', 'OFFER-MED101-2026S1', 1, 'WAITING'),
    ('WAIT-003', '2024SCI003', 'OFFER-SCI201-2026S1', 1, 'WAITING'),
    ('WAIT-004', '2024LAW003', 'OFFER-LAW201-2026S1', 1, 'WAITING'),
    ('WAIT-005', '2024ART003', 'OFFER-ART201-2026S1', 1, 'WAITING');

INSERT INTO course_change_requests
    (change_request_id, faculty_id, course_id, offering_id, change_type, requested_value, status) VALUES
    ('CHANGE-001', '2021FAC002', 'COURSE-BUS301', 'OFFER-BUS301-2026S1', 'CAPACITY', '60', 'PENDING'),
    ('CHANGE-002', '2022FAC004', 'COURSE-MED101', 'OFFER-MED101-2026S1', 'CAPACITY', '40', 'PENDING'),
    ('CHANGE-003', '2021FAC006', 'COURSE-SCI201', 'OFFER-SCI201-2026S1', 'CAPACITY', '40', 'PENDING'),
    ('CHANGE-004', '2022FAC007', 'COURSE-ART201', 'OFFER-ART201-2026S1', 'CAPACITY', '40', 'PENDING'),
    ('CHANGE-005', '2023FAC008', 'COURSE-LAW201', 'OFFER-LAW201-2026S1', 'CAPACITY', '40', 'PENDING');

COMMIT;
