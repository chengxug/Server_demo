#!/usr/bin/env bash
set -euo pipefail # -e 严格模式 -u 未定义变量报错 -o pipefail 管道命令失败报错

# 默认测试可执行文件所在目录
BUILD_ROOT="${1:-build/release}" # -：参数为空或未传入时使用默认值 (有参数时使用参数值,无参数时使用build/release)
TEST_DIR="${BUILD_ROOT}/tests"

# 在此列表中新增测试用例，可包含参数：
# 示例：
# "${TEST_DIR}/benchmark -n 10000 -p 7788"

TESTS=(
  "${TEST_DIR}/HttpParser_test"
  "${TEST_DIR}/HttpRouter_test"
)

for cmd in "${TESTS[@]}"; do
  if [[ ! -x "${cmd}" ]]; then
    echo "Warning: test not found or not executable: ${cmd}"
    continue
  fi
  echo "Running: ${cmd}"
  "${cmd}"
done

echo "All tests finished."