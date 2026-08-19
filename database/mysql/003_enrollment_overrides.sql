USE nexusenroll;

CREATE TABLE IF NOT EXISTS enrollment_overrides (
    override_id VARCHAR(40) PRIMARY KEY,
    administrator_user_id VARCHAR(32) NOT NULL,
    enrollment_id VARCHAR(40) NOT NULL,
    bypassed_rule VARCHAR(24) NOT NULL,
    reason VARCHAR(1000) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_overrides_enrollment (enrollment_id),
    CONSTRAINT chk_override_rule CHECK (
        bypassed_rule IN ('PREREQUISITE', 'CAPACITY', 'TIME_CONFLICT')
    ),
    CONSTRAINT chk_override_reason CHECK (CHAR_LENGTH(TRIM(reason)) > 0),
    CONSTRAINT fk_override_administrator FOREIGN KEY (administrator_user_id)
        REFERENCES users (user_id),
    CONSTRAINT fk_override_enrollment FOREIGN KEY (enrollment_id)
        REFERENCES enrollments (enrollment_id)
) ENGINE = InnoDB;
