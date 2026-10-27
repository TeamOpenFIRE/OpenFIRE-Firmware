#!/usr/bin/env bash
# Unit tests: see README.md in this directory.
#
#   ./run.sh              build and run all tests
#   ./run.sh <text>       run only the tests whose "Suite.name" contains the text
#   ./run.sh --list       list the tests
#   ./run.sh --seed <n>   run in the random order of seed n (a run prints its seed); --ordered keeps the file order
set -euo pipefail

echo "Running unit tests..."

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

optimization=(-O3)
instrument=()
sanitize=(-fsanitize=address,undefined -fno-sanitize-recover=all)
source "$(dirname "$0")/build.sh"
echo "Building all test programs..."
build_all

echo "Running test programs..."
# All programs are run, and the exit status says whether any failed.
status=0
for program in "${test_programs[@]}"; do
    "$work/$program" "$@" || status=1
    echo
done
echo "All test programs finished, exit status: $status"
exit $status
