CXX      := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -ggdb -fsanitize=address,undefined 
CPPFLAGS := -Iinclude -Iinclude/common -Iinclude/common/details -Iinclude/io -Iinclude/io/async -Ivendor -MMD -MP
LDFLAGS  :=
LDLIBS   := -lssl -lcrypto

SRC_DIR    := src
HEADER_DIR := include
BUILD_DIR  := build
TARGET     := websock_client

# Automatically find all source and header files (excluding example.cpp)
SRCS       := $(shell find $(SRC_DIR) -name "*.cpp" ! -name "example.cpp")
HEADERS    := $(shell find $(HEADER_DIR) -type f \( -name "*.h" -o -name "*.hpp" \))
OBJS       := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
VENDOR_OBJ := $(BUILD_DIR)/simdutf.o
DEPS       := $(OBJS:.o=.d) $(VENDOR_OBJ:.o=.d)

.PHONY: all clean run format test

all: $(TARGET)

# Link executable
$(TARGET): $(OBJS) $(VENDOR_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Compile vendor object (only recompiles if simdutf.cpp changes)
$(BUILD_DIR)/simdutf.o: vendor/simdutf.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Include automatic header dependency rules
-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) coroutine

run: all
	./$(TARGET)

format:
	@clang-format -i $(SRCS) $(HEADERS) --verbose

# Optional coroutine target you had previously
coroutine: src/main.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/main.cpp -o coroutine
