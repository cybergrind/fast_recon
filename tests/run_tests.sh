#!/usr/bin/env bash
# Run every test_* binary in BUILD; report pass/fail; exit non-zero if any fail.
set -u
build="${1:-build}"
pass=0
fail=0
failed=()
for bin in "$build"/test_*; do
    [ -x "$bin" ] || continue
    name=$(basename "$bin")
    if "$bin"; then
        printf '  \033[32mPASS\033[0m %s\n' "$name"
        pass=$((pass+1))
    else
        rc=$?
        printf '  \033[31mFAIL\033[0m %s (exit %d)\n' "$name" "$rc"
        failed+=("$name")
        fail=$((fail+1))
    fi
done
printf '\n%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
