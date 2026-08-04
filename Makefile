CC = g++
CFLAGS = -Wall -Wextra -Wno-implicit-fallthrough -O2 -Iinclude -I.
LDFLAGS = -lpthread -lssl -lcrypto

TARGET = build/api
SRC = main.cpp $(shell find src -name '*.cpp')
HDR = $(shell find include -name '*.hpp' -o -name '*.h')

# src/foo/bar.c -> build/src/foo/bar.o
# main.c -> build/main.o
OBJ = $(SRC:%.cpp=build/%.o)

all: $(TARGET)

# Link objects into executable
$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

# .cpp to .o compilations
build/%.o: %.cpp $(HDR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
