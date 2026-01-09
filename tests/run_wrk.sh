#!/usr/bin/env bash
set -euo pipefail

# 1. 获取构建根目录 (build/debug 或 build/release)
# 如果未提供参数，默认回退到 build/release
BUILD_ROOT="${1:-build/release}"

# 2. 检查 wrk 是否安装
if ! command -v wrk &> /dev/null; then
    echo "Error: 'wrk' is not installed. Please install it (e.g., sudo apt install wrk)."
    exit 1
fi

# 3. 定义 Server 启动命令
declare -A SERVERS=(
  ["iterative"]="${BUILD_ROOT}/simple_impl/Iterative_server -p 7788"
  ["multithread"]="${BUILD_ROOT}/simple_impl/multithread_server -p 7788"
  ["threadpool"]="${BUILD_ROOT}/simple_impl/threadpool_server -p 7788 -t 8"
)
# 定义测试顺序
SERVER_ORDER=("iterative" "multithread" "threadpool")

# 4. 测试参数配置
# 并发连接数列表 (模拟不同负载场景)
# 建议包含：低并发(1, 10), 中并发(100), 高并发(1000)
# CONCURRENCY_LEVELS=(1 10 50 100 500 1000 2000)
CONCURRENCY_LEVELS=(2000)

# 每次压测持续时间
DURATION="10s"

TARGET_URL="http://127.0.0.1:7788/index.html"

# 结果保存目录
RESULT_DIR="results/wrk_bench"
mkdir -p "${RESULT_DIR}"

echo "Starting Benchmark with wrk..."
echo "Target URL: ${TARGET_URL}"
echo "Duration per test: ${DURATION}"
echo "-------------------------------------------"

for server_name in "${SERVER_ORDER[@]}"; do
  # 检查 server 是否在 SERVERS 定义中存在
  if [[ -z "${SERVERS[$server_name]+x}" ]]; then
      echo "Error: Server '$server_name' is in ORDER list but not defined in SERVERS map."
      continue
  fi

  CMD="${SERVERS[$server_name]}"
  EXE_PATH=$(echo "$CMD" | awk '{print $1}')

  if [[ ! -x "$EXE_PATH" ]]; then
      echo "Warning: Server executable not found: $EXE_PATH. Skipping ${server_name}."
      continue
  fi

  echo ""
  echo "======================================"
  echo "Benchmarking Server: ${server_name}"
  echo "======================================"

  # 1. 启动 Server
  $CMD > /dev/null 2>&1 &
  SERVER_PID=$!
  
  # 等待 Server 启动 (给2秒缓冲)
  sleep 2

  # 2. 循环不同的并发数进行测试
  for c in "${CONCURRENCY_LEVELS[@]}"; do
    OUT_FILE="${RESULT_DIR}/${server_name}_c${c}.txt"
    
    # 计算 wrk 的线程数 (-t)
    # wrk 的线程数不应超过并发数，通常 2-4 个线程足够产生巨大压力
    # 逻辑：如果并发数 < 4，线程数 = 并发数；否则线程数 = 4
    t=4
    if [ "$c" -lt 4 ]; then
        t=$c
    fi

    echo -n "[${server_name}] Concurrency: ${c} ... "

    # 运行 wrk
    # -t: 线程数, -c: 连接数, -d: 持续时间, --latency: 打印延迟分布
    wrk -t"$t" -c"$c" -d"$DURATION" --latency "${TARGET_URL}" > "${OUT_FILE}"

    # 3. 提取并打印关键指标 (QPS)
    # 从输出文件中 grep "Requests/sec"
    QPS=$(grep "Requests/sec" "${OUT_FILE}" | awk '{print $2}')
    echo "QPS: ${QPS}"
  done

  # 4. 关闭 Server
  kill -2 "${SERVER_PID}" 2>/dev/null || kill "${SERVER_PID}" 2>/dev/null
  wait "${SERVER_PID}" 2>/dev/null || true
  
  # 等待端口释放
  sleep 2
done

echo ""
echo "All benchmarks finished. Detailed results saved in ${RESULT_DIR}/"