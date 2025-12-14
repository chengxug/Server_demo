CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Iinclude -g -fsanitize=address

SRCS := Server.cpp
TARGET := server

BUILD_DIR := build
TEST_DIR := tests
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_BINS := $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/tests/%,$(TEST_SRCS))

.PHONY: all tests run-tests clean

all: $(BUILD_DIR)/$(TARGET) tests

# ensure build directories exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/tests:
	mkdir -p $(BUILD_DIR)/tests

# main binary goes to build/
$(BUILD_DIR)/$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# each test .cpp -> build/tests/<name>
$(BUILD_DIR)/tests/%: $(TEST_DIR)/%.cpp | $(BUILD_DIR)/tests
	$(CXX) $(CXXFLAGS) -o $@ $<

tests: $(TEST_BINS)

# run all tests
run-tests: tests
	@chmod +x ./tests/test.sh
	@./tests/test.sh

clean:
	rm -rf $(BUILD_DIR)