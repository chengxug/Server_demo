CXX := g++

# 通用编译选项
COMMON_FLAGS := -std=c++11 -Wall -Wextra -Iinclude -pthread

# 默认模式为 debug，可通过 make MODE=release 切换
MODE ?= debug

# 根据模式设置特定选项和输出目录
ifeq ($(MODE), release)
    CXXFLAGS := $(COMMON_FLAGS) -O3 -DNDEBUG
    BUILD_ROOT := build/release
else
    # debug 模式：开启调试信息和地址检查
    CXXFLAGS := $(COMMON_FLAGS) -g -O0 -fsanitize=address
    BUILD_ROOT := build/debug
endif

# SRCS := Server.cpp
# TARGET := $(BUILD_ROOT)/server

BUILD_DIR := build

# 简单Server的编译配置
SIMPLE_DIR := simple_impl
SIMPLE_SRCS := $(wildcard $(SIMPLE_DIR)/*.cpp)
SIMPLE_BINS := $(patsubst $(SIMPLE_DIR)/%.cpp,$(BUILD_ROOT)/simple_impl/%,$(SIMPLE_SRCS))

# 测试文件的编译配置
TEST_DIR := tests
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_BINS := $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_ROOT)/tests/%,$(TEST_SRCS))

.PHONY: all tests run-tests run-benchmark clean

all: $(TARGET) tests simple_impl

# 打印帮助信息
help:
	@echo "Usage:"
	@echo "  make [MODE=debug|release]    # Build main server (default: debug)"
	@echo "  make tests [MODE=...]        # Build tests"
	@echo "  make run-tests               # Run functional tests"
	@echo "  make run-benchmark           # Run benchmark (recommend MODE=release)"
	@echo "  make clean                   # Clean all builds"

# ensure build directories exist
$(BUILD_ROOT):
	mkdir -p $(BUILD_ROOT)

$(BUILD_ROOT)/simple_impl:
	mkdir -p $(BUILD_ROOT)/simple_impl

$(BUILD_ROOT)/tests:
	mkdir -p $(BUILD_ROOT)/tests

# 主程序的编译规则
# $(TARGET): $(SRCS) | $(BUILD_ROOT)
# 	$(CXX) $(CXXFLAGS) -o $@ $^

# each test .cpp -> build/<mode>/tests/<name>
$(BUILD_ROOT)/tests/%: $(TEST_DIR)/%.cpp | $(BUILD_ROOT)/tests
	$(CXX) $(CXXFLAGS) -o $@ $<

# simple_implementation -> build/<mode>/simple_impl/<name>
$(BUILD_ROOT)/simple_impl/%: $(SIMPLE_DIR)/%.cpp | $(BUILD_ROOT)/simple_impl
	$(CXX) $(CXXFLAGS) -o $@ $<

tests: $(TEST_BINS)

simple_impl: $(SIMPLE_BINS)

# 执行功能测试 (默认使用当前 MODE 构建的程序)
run-tests: tests
	@echo "Running functional tests ($(MODE) mode)..."
	@chmod +x ./tests/run_tests.sh
	@# 传递构建目录给脚本，以便脚本知道去哪里找可执行文件
	@./tests/run_tests.sh $(BUILD_ROOT)

# 执行性能基准测试
# run-benchmark: tests
# 	@echo "Running benchmark tests ($(MODE) mode)..."
# 	@if [ "$(MODE)" = "debug" ]; then \
# 	    echo "WARNING: Running benchmark in DEBUG mode. Use 'make run-benchmark MODE=release' for accurate results."; \
# 	fi
# 	@chmod +x ./tests/run_benchmark.sh
# 	@./tests/run_benchmark.sh $(BUILD_ROOT)
run-wrk: simple_impl
	@echo "Running wrk benchmark tests ($(MODE) mode)..."
	@if [ "$(MODE)" = "debug" ]; then \
	    echo "WARNING: Running benchmark in DEBUG mode. Use 'make run-wrk MODE=release' for accurate results."; \
	fi
	@chmod +x ./tests/run_wrk.sh
	@./tests/run_wrk.sh $(BUILD_ROOT)

clean:
	rm -rf $(BUILD_ROOT)