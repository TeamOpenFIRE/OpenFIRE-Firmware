#!/usr/bin/env bash
# Runs the unit tests with coverage and prints, for each source file, the share of lines and branches they run.
#
#   ./coverage.sh                  summary on the terminal
#   ./coverage.sh --html <dir>     also an HTML report (open <dir>/index.html) with every line marked run or not
#   ./coverage.sh --min <percent>  fail (exit status 3) if the total share of lines run is below the percentage, e.g. for CI
#
# The options can be combined. The exit status is 1 if a test fails, 2 for a wrong option, and 3 for too little coverage.
#
# Needs gcovr (apt install gcovr). The tests are built without optimization here, since the optimizer merges and
# removes lines, which distorts the numbers.
set -euo pipefail

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

html_directory=""
minimum=""
while [ $# -gt 0 ]; do
    case "$1" in
        --html) html_directory="${2:?--html needs a directory}"; shift 2 ;;
        --min)  minimum="${2:?--min needs a percentage}"; shift 2 ;;
        *) echo "usage: $0 [--html <directory>] [--min <percent>]" >&2; exit 2 ;;
    esac
done
if [ -n "$minimum" ] && ! [[ "$minimum" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
    echo "--min needs a percentage such as 90 or 87.5, not '$minimum'" >&2
    exit 2
fi

optimization=(-O0)
instrument=(--coverage)
sanitize=()
source "$(dirname "$0")/build.sh"
build_all

# The tests run in their usual random order: what they cover doesn't depend on it.
for program in "${test_programs[@]}"; do
    "$work/$program" > "$work/$program.log" || { cat "$work/$program.log"; echo "Tests failed; no coverage report."; exit 1; }
    tail -1 "$work/$program.log"
done

# The libraries, and the firmware files that are tested on the PC (not the .ino sketch, nor the serial protocol yet).
options=(--root "$here/../.." --object-directory "$work" --print-summary
         --filter "$library/" --filter "$libraries/DFRobotIRPositionEx/" --filter "$libraries/LightgunButtons/" --filter "$libraries/TinyUSB_Devices/"
         --filter "$firmware/OpenFIRE(prefs|lights|Feedback|display|common|calibration)\\.")
gcovr "${options[@]}" --txt --exclude-unreachable-branches --exclude-throw-branches
if [ -n "$html_directory" ]; then
    mkdir -p "$html_directory"
    gcovr "${options[@]}" --html-details "$html_directory/index.html" --exclude-unreachable-branches --exclude-throw-branches > /dev/null
    echo "HTML report: $html_directory/index.html"
fi

# The total share of lines run, from the summary above, against the minimum asked for.
if [ -n "$minimum" ]; then
    gcovr "${options[@]}" --json-summary "$work/summary.json" --exclude-unreachable-branches --exclude-throw-branches > /dev/null
    total="$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["line_percent"])' "$work/summary.json")"
    if python3 -c "import sys; sys.exit(0 if float('$total') >= float('$minimum') else 1)"; then
        echo "Line coverage $total% is at least the minimum of $minimum%."
    else
        echo "Line coverage $total% is below the minimum of $minimum%." >&2
        exit 3
    fi
fi
