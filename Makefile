CXX      := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -ggdb
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
DEPS    := $(OBJS:.o=.d)

.PHONY: all clean run format test

all: $(TARGET)

# Link executable (LDLIBS must come AFTER $^)
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

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

test:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c autobahn_runner.cpp -o wa
