CXX ?= g++
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wno-implicit-fallthrough
LDLIBS := -pthread -lssl -lcrypto

TARGET := build/nexusenroll
SOURCES := main.cpp $(shell find src -type f -name '*.cpp' | sort)
OBJECTS := $(SOURCES:%.cpp=build/%.o)
DEPENDENCIES := $(OBJECTS:.o=.d)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) -o $@ $(LDLIBS)

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build

-include $(DEPENDENCIES)
