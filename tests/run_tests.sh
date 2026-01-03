#!/usr/bin/env bash
set -euo pipefail # -e 严格模式 -u 未定义变量报错 -o pipefail 管道命令失败报错

# 默认测试可执行文件所在目录（Makefile 已将测试编译到 build/tests）
TEST_DIR="${1:-build/tests}" # -：参数为空或未传入时使用默认值 (有参数时使用参数值,无参数时使用build/tests)

# 在此列表中新增测试用例，可包含参数：
# 示例：
# "${TEST_DIR}/benchmark -n 10000 -p 7788"

TESTS=(
  # "${TEST_DIR}/sample_test"
    "${TEST_DIR}/http_parse_test"
    "${TEST_DIR}/HttpParser_test"
    "${TEST_DIR}/HttpRouter_test"
)

if [[ ${#TESTS[@]} -eq 0 ]]; then
  echo "[WARN] No tests defined in tests/test.sh"
  exit 0
fi

for cmd in "${TESTS[@]}"; do
  echo "Running: ${cmd}"
  bash -c "${cmd}"
done

echo "All tests finished."