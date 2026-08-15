#!/usr/bin/env bash

set -euo pipefail

fail() {
    printf 'FAIL: Keil build check: %s\n' "$*" >&2
    exit 1
}

[[ $# -eq 4 ]] || fail "usage: $0 TARGET BUILD_LOG SIZE_REPORT MAX_FLASH_BYTES"

target=$1
build_log=$2
size_report=$3
max_flash=$4

[[ -f "$build_log" ]] || fail "missing build log: $build_log"
[[ -f "$size_report" ]] || fail "missing size report: $size_report"
[[ "$max_flash" =~ ^[0-9]+$ ]] || fail "MAX_FLASH_BYTES must be decimal"

grep -Fq "Build target '$target'" "$build_log" || fail "log is not for target $target"
if grep -Eq '[1-9][0-9]*[[:space:]]+Error\(s\)' "$build_log"; then
    fail "$target build reports errors"
fi
grep -Eq '0[[:space:]]+Error\(s\)' "$build_log" || fail "$target log lacks a zero-error result"
grep -Fq "Target: $target" "$size_report" || fail "size report is not for target $target"

size_line=$(grep -E 'Program Size:[[:space:]]+Code=[0-9]+[[:space:]]+RO-data=[0-9]+[[:space:]]+RW-data=[0-9]+[[:space:]]+ZI-data=[0-9]+' "$size_report" | tail -n 1 || true)
[[ -n "$size_line" ]] || fail "size report lacks the ARMCC Program Size summary"

read -r code ro_data rw_data zi_data < <(
    printf '%s\n' "$size_line" | sed -E \
        's/.*Code=([0-9]+)[[:space:]]+RO-data=([0-9]+)[[:space:]]+RW-data=([0-9]+)[[:space:]]+ZI-data=([0-9]+).*/\1 \2 \3 \4/'
)

flash_used=$((code + ro_data + rw_data))
ram_used=$((rw_data + zi_data))
((flash_used <= max_flash)) || fail "$target uses $flash_used Flash bytes, limit is $max_flash"
((ram_used <= 20480)) || fail "$target uses $ram_used RAM bytes, limit is 20480"

printf 'PASS: %s uses %d/%d Flash bytes and %d/20480 RAM bytes\n' \
    "$target" "$flash_used" "$max_flash" "$ram_used"
