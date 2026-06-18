#!/usr/bin/env bash
# benchmark/run.sh — 自动化对比 Americano vs espresso-logic
# 用法: cd benchmark && bash run.sh

set -euo pipefail

PROJ=".."
BIN_A="$PROJ/bin/americano"
BIN_E="$PROJ/../espresso-logic/bin/espresso"
EXAMPLES="$PROJ/examples"
TIMEOUT_SEC=30
TMP_A=$(mktemp)
TMP_E=$(mktemp)

# 统计 literal 数（去掉 - 后数 01 字符）
lit_count() {
    grep "^[01-]" "$1" | sed 's/ //g; s/-//g' | tr -d '\n' | wc -c
}

# 解析 Americano 的 -t 输出
parse_a() {
    local trace="$1"
    ITER_A=$(echo "$trace" | grep "TOTAL" | grep -oP 'iterations=\K\d+')
    TIME_A=$(echo "$trace" | grep "TOTAL" | grep -oP '[\d.]+(?= ms)')
    COST_A=$(echo "$trace" | grep "FINAL" | grep -oP 'c=\K\d+')
    ITER_A=${ITER_A:-?}; TIME_A=${TIME_A:-?}; COST_A=${COST_A:-?}
}

# 解析 espresso-logic 的 -t 输出（trace 在 stdout，和 PLA 混在一起）
parse_e() {
    local trace="$1"
    ITER_E=$(echo "$trace" | grep -c "^# EXPAND" || echo 0)
    COST_E=$(echo "$trace" | grep "ADJUST" | grep -oP 'c=\K\d+')
    # 用最后一个有 Time 的行的累计时间
    TIME_E=$(echo "$trace" | grep "Time was" | tail -1 | grep -oP '[\d.]+(?= sec)')
    ITER_E=${ITER_E:-?}; COST_E=${COST_E:-?}; TIME_E=${TIME_E:-?}
}

# 打印表头
printf "%-6s %-14s %6s %6s %6s %6s %6s %6s %5s %5s %7s %7s\n" \
    "规模" "文件" \
    "原项" "A项" "E项" \
    "原lit" "Alit" "Elit" \
    "A轮" "E轮" \
    "A时(ms)" "E时"
printf "%.0s-" {1..110}
echo

run_one() {
    local scale="$1" f="$2"
    local name=$(basename "$f" .pla)

    # 原始数据
    local orig_terms=$(grep -c "^[01-]" "$f" 2>/dev/null || echo 0)
    local orig_lit=$(lit_count "$f")

    # 跑 Americano
    local trace_a=""
    if timeout $TIMEOUT_SEC "$BIN_A" -t "$f" > "$TMP_A" 2>"$TMP_E"; then
        trace_a=$(cat "$TMP_E" 2>/dev/null || true)
    else
        trace_a="TIMEOUT"
    fi
    local lit_a=$(lit_count "$TMP_A")

    if [ "$trace_a" = "TIMEOUT" ]; then
        printf "%-6s %-14s %6d %6s %6s %6d %6s %6s %5s %5s %7s %7s\n" \
            "$scale" "$name" "$orig_terms" "T/O" "-" "$orig_lit" "-" "-" "-" "-" "-" "-"
    else
        parse_a "$trace_a"
        # 跑 espresso-logic（-t 输出在 stdout，和 PLA 混在一起）
        local trace_e=""
        if timeout $TIMEOUT_SEC "$BIN_E" -t "$f" > "$TMP_A" 2>/dev/null; then
            trace_e=$(grep "^#" "$TMP_A" 2>/dev/null || true)
        fi
        local lit_e=$(grep "^[01-]" "$TMP_A" | sed 's/ //g; s/-//g' | tr -d '\n' | wc -c)

        if [ -z "$trace_e" ]; then
            printf "%-6s %-14s %6d %6s %6s %6d %6d %6s %5s %5s %7s %7s\n" \
                "$scale" "$name" "$orig_terms" "$COST_A" "-" "$orig_lit" "$lit_a" "-" "$ITER_A" "-" "$TIME_A" "-"
        else
            parse_e "$trace_e"
            printf "%-6s %-14s %6d %6s %6s %6d %6d %6d %5s %5s %7s %7s\n" \
                "$scale" "$name" "$orig_terms" "$COST_A" "$COST_E" "$orig_lit" "$lit_a" "$lit_e" "$ITER_A" "$ITER_E" "$TIME_A" "$TIME_E"
        fi
    fi
}

# 小规模
for f in $(cat cases_small.txt); do
    run_one "小" "$EXAMPLES/$f"
done

# 中规模
for f in $(cat cases_mid.txt); do
    run_one "中" "$EXAMPLES/$f"
done

# 大规模（绝对路径）
for f in $(cat cases_large.txt); do
    run_one "大" "$f"
done

rm -f "$TMP_A" "$TMP_E"
