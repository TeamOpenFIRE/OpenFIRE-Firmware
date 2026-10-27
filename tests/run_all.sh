#!/usr/bin/env bash
# Runs all tests; the exit status is 1 if any of them fails.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
status=0
for test in unit tracking; do
    echo "== $test"
    "$here/$test/run.sh" || status=1
done
exit $status
