#!/usr/bin/env bash
set -euo pipefail

# 1. 获取构建根目录 (build/debug 或 build/release)
# 如果未提供参数，默认回退到 build/release
BUILD_ROOT="${1:-build/release}"

# 2. 动态定位 benchmark 工具
BENCHMARK="${BUILD_ROOT}/tests/benchmark"

# 检查 benchmark 工具是否存在
if [[ ! -x "${BENCHMARK}" ]]; then
    echo "Error: Benchmark tool not found at ${BENCHMARK}"
    exit 1
fi

# 3. 动态定义 Server 启动命令
declare -A SERVERS=(
  ["iterative"]="${BUILD_ROOT}/simple_impl/Iterative_server -p 7788"
  ["threadpool"]="${BUILD_ROOT}/server -p 7788"
  # ["epoll"]="${BUILD_ROOT}/epoll_server -p 7788"
)

# 请求规模
REQUEST_COUNTS=(10 100 1000 10000 100000)

RESULT_DIR="results"
mkdir -p "${RESULT_DIR}"

for server_name in "${!SERVERS[@]}"; do
  CMD="${SERVERS[$server_name]}"
  # 提取可执行文件路径（假设命令的第一部分是路径）
  EXE_PATH=$(echo "$CMD" | awk '{print $1}')

  if [[ ! -x "$EXE_PATH" ]]; then
      echo "Warning: Server executable not found: $EXE_PATH. Skipping ${server_name}."
      continue
  fi

  echo "======================================"
  echo "Benchmarking server: ${server_name}"
  echo "Start Command: ${CMD}"
  echo "======================================"

  # 启动 server（后台）
  $CMD &
  SERVER_PID=$!

  # 等 server 启动完成
  sleep 1

  for n in "${REQUEST_COUNTS[@]}"; do
    echo "[${server_name}] Running benchmark with n=${n}"

    OUT_FILE="${RESULT_DIR}/benchmark_${server_name}_n${n}.txt"

    # 运行 benchmark
    "${BENCHMARK}" -n "${n}" -p 7788 \
      | tee "${OUT_FILE}"
  done

  # 关闭 server
  # 使用 kill -2 (SIGINT) 让服务器有机会优雅退出（如果实现了的话），否则用默认 SIGTERM
  kill -2 "${SERVER_PID}" 2>/dev/null || kill "${SERVER_PID}" 2>/dev/null
  wait "${SERVER_PID}" 2>/dev/null || true
  
  # 稍微等待端口释放
  sleep 1
done

echo "All benchmarks finished."