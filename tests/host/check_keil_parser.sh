#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
CHECKER="$REPO_ROOT/scripts/check_keil_build.sh"
FIXTURES="$SCRIPT_DIR/fixtures"

"$CHECKER" APP "$FIXTURES/app-pass.log" "$FIXTURES/app-pass-size.txt" 47104
"$CHECKER" Bootloader "$FIXTURES/bootloader-pass.log" "$FIXTURES/bootloader-pass-size.txt" 16384

if "$CHECKER" APP "$FIXTURES/app-error.log" "$FIXTURES/app-pass-size.txt" 47104 >/dev/null 2>&1; then
    printf 'FAIL: error build fixture was accepted\n' >&2
    exit 1
fi

if "$CHECKER" APP "$FIXTURES/app-pass.log" "$FIXTURES/app-oversize-size.txt" 47104 >/dev/null 2>&1; then
    printf 'FAIL: oversize fixture was accepted\n' >&2
    exit 1
fi

printf 'PASS: Keil parser rejects build errors and oversized images\n'
