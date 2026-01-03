#pragma once
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include <memory>

// 声明全局 logger
extern std::shared_ptr<spdlog::logger> g_logger;