CXX      := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -Iinclude -MMD -MP -ggdb
SRC_DIR  := src
HEADER_DIR  := include
BUILD_DIR := build
TARGET   := websock_client

# Automatically find all .cpp files in src/
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
HEADERS  := $(shell find $(HEADER_DIR) -type f)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean run format

all: $(TARGET) format

# Link executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Include header dependency rules
-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

run: all
	./$(TARGET)
format: 
	@clang-format -i $(SRCS) $(HEADERS) --verbose
