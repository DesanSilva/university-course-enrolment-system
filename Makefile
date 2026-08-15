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
MYSQL_TEST_TARGET := build/tests/mysql_connection_tests
MYSQL_TEST_SOURCES := $(shell find tests/mysql -type f -name '*.cpp' | sort)
MYSQL_TEST_OBJECTS := $(MYSQL_TEST_SOURCES:%.cpp=build/%.o)
DEPENDENCIES := $(sort $(OBJECTS:.o=.d) $(UNIT_TEST_OBJECTS:.o=.d) $(MYSQL_TEST_OBJECTS:.o=.d))

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

unit-test: $(UNIT_TEST_TARGET)
	./$(UNIT_TEST_TARGET)

$(UNIT_TEST_TARGET): $(UNIT_TEST_OBJECTS) $(DOMAIN_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ -pthread

mysql-test: $(MYSQL_TEST_TARGET)
	./$(MYSQL_TEST_TARGET)

$(MYSQL_TEST_TARGET): $(MYSQL_TEST_OBJECTS) $(DATA_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ -pthread $(MYSQL_LDLIBS)

mysql-schema:
	mysql $(MYSQL_ARGS) < database/mysql/001_schema.sql

mysql-seed:
	mysql $(MYSQL_ARGS) < database/mysql/002_seed.sql

clean:
	rm -rf build

-include $(DEPENDENCIES)
