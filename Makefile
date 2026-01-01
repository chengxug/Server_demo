CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Iinclude -g -fsanitize=address

SRCS := Server.cpp
TARGET := server

BUILD_DIR := build

# 简单Server的编译配置
SIMPLE_DIR := simple_impl
SIMPLE_SRCS := $(wildcard $(SIMPLE_DIR)/*.cpp)
SIMPLE_BINS := $(patsubst $(SIMPLE_DIR)/%.cpp,$(BUILD_DIR)/simple_impl/%,$(SIMPLE_SRCS))

# 测试文件的编译配置
TEST_DIR := tests
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_BINS := $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/tests/%,$(TEST_SRCS))

.PHONY: all tests run-tests clean

all: $(BUILD_DIR)/$(TARGET) tests simple_impl

# ensure build directories exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 简单Server的输出目录
$(BUILD_DIR)/simple_impl:
	mkdir -p $(BUILD_DIR)/simple_impl

# 测试程序的输出目录
$(BUILD_DIR)/tests:
	mkdir -p $(BUILD_DIR)/tests

# 主程序的编译规则
$(BUILD_DIR)/$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# each test .cpp -> build/tests/<name>
$(BUILD_DIR)/tests/%: $(TEST_DIR)/%.cpp | $(BUILD_DIR)/tests
	$(CXX) $(CXXFLAGS) -o $@ $<

# 将 simple_implementation/*.cpp 编译到 build/simple_impl/*
$(BUILD_DIR)/simple_impl/%: $(SIMPLE_DIR)/%.cpp | $(BUILD_DIR)/simple_impl
	$(CXX) $(CXXFLAGS) -o $@ $<

tests: $(TEST_BINS)

simple_impl: $(SIMPLE_BINS)

# run all tests
run-tests: tests
	@chmod +x ./tests/test.sh
	@./tests/test.sh

clean:
	rm -rf $(BUILD_DIR)