CXX      := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -ggdb -fsanitize=address,undefined 
CPPFLAGS := -Iinclude -Ivendor -MMD -MP
LDFLAGS  :=
LDLIBS   := -lssl -lcrypto

SRC_DIR    := src
HEADER_DIR := include
BUILD_DIR  := build
TARGET     := websock_client

# Automatically find source and header files
SRCS    := $(wildcard $(SRC_DIR)/*.cpp)
HEADERS := $(shell find $(HEADER_DIR) -type f \( -name "*.h" -o -name "*.hpp" \))
OBJS    := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
VENDOR_OBJ := $(BUILD_DIR)/simdutf.o
DEPS    := $(OBJS:.o=.d) $(VENDOR_OBJ:.o=.d)

.PHONY: all clean run format test

all: $(TARGET)

# Link executable (LDLIBS must come AFTER $^)
$(TARGET): $(OBJS) $(VENDOR_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Compile vendor object (only recompiles if simdutf.cpp changes)
$(BUILD_DIR)/simdutf.o: vendor/simdutf.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Create build directory if missing
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Include automatic header dependency rules
-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

run: all
	./$(TARGET)

format:
	@clang-format -i $(SRCS) $(HEADERS) --verbose

coroutine:src/main.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/main.cpp -o coroutine
