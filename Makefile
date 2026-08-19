CXX ?= g++
MYSQL_CONFIG ?= mysql_config
MYSQL_CPPFLAGS ?= $(shell $(MYSQL_CONFIG) --cflags 2>/dev/null)
MYSQL_LDLIBS ?= $(or $(shell $(MYSQL_CONFIG) --libs 2>/dev/null),-lmysqlclient)
CPPFLAGS := -Iinclude $(MYSQL_CPPFLAGS)
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wno-implicit-fallthrough
LDLIBS := -pthread -lssl -lcrypto $(MYSQL_LDLIBS)

TARGET := build/nexusenroll
SOURCES := main.cpp $(shell find src -type f -name '*.cpp' | sort)
OBJECTS := $(SOURCES:%.cpp=build/%.o)
DOMAIN_SOURCES := $(shell find src/business/domain -type f -name '*.cpp' | sort)
DOMAIN_OBJECTS := $(DOMAIN_SOURCES:%.cpp=build/%.o)
DATA_SOURCES := $(shell find src/business/domain src/data/mysql -type f -name '*.cpp' | sort)
DATA_OBJECTS := $(DATA_SOURCES:%.cpp=build/%.o)
UNIT_TEST_TARGET := build/tests/domain_tests
UNIT_TEST_SOURCES := tests/domain_data_tests.cpp
UNIT_TEST_OBJECTS := $(UNIT_TEST_SOURCES:%.cpp=build/%.o)
SESSION_SOURCES := $(shell find src/business/sessions -type f -name '*.cpp' | sort)
SESSION_OBJECTS := $(SESSION_SOURCES:%.cpp=build/%.o)
BUSINESS_TEST_TARGET := build/tests/business_session_tests
BUSINESS_TEST_SOURCES := tests/business_session_tests.cpp
BUSINESS_TEST_OBJECTS := $(BUSINESS_TEST_SOURCES:%.cpp=build/%.o)
STUDENT_BUSINESS_SOURCES := $(shell find src/business/cqrs src/business/domain \
	src/business/notifications -type f -name '*.cpp' | sort)
STUDENT_BUSINESS_OBJECTS := $(STUDENT_BUSINESS_SOURCES:%.cpp=build/%.o)
STUDENT_BUSINESS_TEST_TARGET := build/tests/student_business_tests
STUDENT_BUSINESS_TEST_SOURCES := tests/student_business_tests.cpp
STUDENT_BUSINESS_TEST_OBJECTS := $(STUDENT_BUSINESS_TEST_SOURCES:%.cpp=build/%.o)
FACULTY_BUSINESS_SOURCES := $(shell find src/business/cqrs src/business/domain \
	src/business/notifications \
	-type f -name '*.cpp' | sort)
FACULTY_BUSINESS_OBJECTS := $(FACULTY_BUSINESS_SOURCES:%.cpp=build/%.o)
FACULTY_BUSINESS_TEST_TARGET := build/tests/faculty_business_tests
FACULTY_BUSINESS_TEST_SOURCES := tests/faculty_business_tests.cpp
FACULTY_BUSINESS_TEST_OBJECTS := $(FACULTY_BUSINESS_TEST_SOURCES:%.cpp=build/%.o)
ADMINISTRATOR_BUSINESS_SOURCES := $(shell find src/business/cqrs src/business/domain \
	src/business/notifications -type f -name '*.cpp' | sort)
ADMINISTRATOR_BUSINESS_OBJECTS := $(ADMINISTRATOR_BUSINESS_SOURCES:%.cpp=build/%.o)
ADMINISTRATOR_BUSINESS_TEST_TARGET := build/tests/administrator_business_tests
ADMINISTRATOR_BUSINESS_TEST_SOURCES := tests/administrator_business_tests.cpp
ADMINISTRATOR_BUSINESS_TEST_OBJECTS := $(ADMINISTRATOR_BUSINESS_TEST_SOURCES:%.cpp=build/%.o)
MYSQL_TEST_TARGET := build/tests/mysql_connection_tests
MYSQL_TEST_SOURCES := $(shell find tests/mysql -type f -name '*.cpp' | sort)
MYSQL_TEST_OBJECTS := $(MYSQL_TEST_SOURCES:%.cpp=build/%.o)
MYSQL_BUSINESS_SOURCES := src/business/cqrs/commands/faculty_commands.cpp \
	src/business/cqrs/faculty_validation.cpp src/business/cqrs/persistent_id.cpp
MYSQL_BUSINESS_OBJECTS := $(MYSQL_BUSINESS_SOURCES:%.cpp=build/%.o)
DEPENDENCIES := $(sort $(OBJECTS:.o=.d) $(UNIT_TEST_OBJECTS:.o=.d) \
	$(BUSINESS_TEST_OBJECTS:.o=.d) $(STUDENT_BUSINESS_TEST_OBJECTS:.o=.d) \
	$(FACULTY_BUSINESS_TEST_OBJECTS:.o=.d) $(ADMINISTRATOR_BUSINESS_TEST_OBJECTS:.o=.d) \
	$(MYSQL_TEST_OBJECTS:.o=.d))

.PHONY: all run test unit-test mysql-test mysql-schema mysql-seed clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) -o $@ $(LDLIBS)

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: unit-test mysql-test

unit-test: $(UNIT_TEST_TARGET) $(BUSINESS_TEST_TARGET) $(STUDENT_BUSINESS_TEST_TARGET) \
	$(FACULTY_BUSINESS_TEST_TARGET) $(ADMINISTRATOR_BUSINESS_TEST_TARGET)
	./$(UNIT_TEST_TARGET)
	./$(BUSINESS_TEST_TARGET)
	./$(STUDENT_BUSINESS_TEST_TARGET)
	./$(FACULTY_BUSINESS_TEST_TARGET)
	./$(ADMINISTRATOR_BUSINESS_TEST_TARGET)

$(UNIT_TEST_TARGET): $(UNIT_TEST_OBJECTS) $(DOMAIN_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ -pthread

$(BUSINESS_TEST_TARGET): $(BUSINESS_TEST_OBJECTS) $(SESSION_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ -pthread

$(STUDENT_BUSINESS_TEST_TARGET): $(STUDENT_BUSINESS_TEST_OBJECTS) $(STUDENT_BUSINESS_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ -pthread

$(FACULTY_BUSINESS_TEST_TARGET): $(FACULTY_BUSINESS_TEST_OBJECTS) $(FACULTY_BUSINESS_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ -pthread

$(ADMINISTRATOR_BUSINESS_TEST_TARGET): $(ADMINISTRATOR_BUSINESS_TEST_OBJECTS) $(ADMINISTRATOR_BUSINESS_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ -pthread

mysql-test: $(MYSQL_TEST_TARGET)
	./$(MYSQL_TEST_TARGET)

$(MYSQL_TEST_TARGET): $(MYSQL_TEST_OBJECTS) $(DATA_OBJECTS) $(MYSQL_BUSINESS_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ -pthread $(MYSQL_LDLIBS)

mysql-schema:
	mysql $(MYSQL_ARGS) < database/mysql/001_schema.sql
	mysql $(MYSQL_ARGS) < database/mysql/003_enrollment_overrides.sql

mysql-seed:
	mysql $(MYSQL_ARGS) < database/mysql/002_seed.sql

clean:
	rm -rf build

-include $(DEPENDENCIES)
