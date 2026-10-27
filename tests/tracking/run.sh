#!/usr/bin/env bash
# Tracking test: see README.md in this directory.
#
#   ./run.sh            run the scenarios in scenarios.txt and compare their output with expected.sha256
#   ./run.sh --update   rewrite expected.sha256 from the current library (review the changes before committing them)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
library="$here/../../libraries/OpenFIREPosition"
scenarios_file="$here/scenarios.txt"
expected_file="$here/expected.sha256"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Strict IEEE float behaviour (no fused multiply-add, no fast-math), so results are reproducible.
# -fno-builtin makes the compiler call the sin, cos and atan2f of deterministic_math.cpp, instead of its own or the C library's.
flags=(-std=c++17 -O1 -ffp-contract=off -fno-fast-math -fno-builtin)

# The scenarios' arguments, one per line, without comments and with the spaces normalized.
scenario_arguments() {
    sed 's/#.*//; s/[[:space:]]\+/ /g; s/^ //; s/ $//' "$scenarios_file" | grep -v '^$'
}

# The expected hash of a scenario, or nothing if there is none.
expected_hash() {
    awk -v wanted="$1" '/^#/ { next } { hash = $1; $1 = ""; sub(/^ /, ""); if ($0 == wanted) print hash }' "$expected_file"
}

hash_of() {
    "$@" | sha256sum | cut -d' ' -f1
}

# The first line of a command's output, or "unknown" if there is none. The command may exit with an error once
# `head` has closed the pipe, which must not add a second line from a fallback.
first_line() {
    local line
    line="$("$@" 2>/dev/null | head -1)" || true
    echo "${line:-unknown}"
}

# Warnings are errors for the library, so that new ones are noticed.
build_library() {
    g++ "${flags[@]}" -Wall -Wextra -Werror -I"$here/../stub" -I"$library" "$here/driver.cpp" \
        "$library/OpenFIRE_Square_Advanced.cpp" "$library/OpenFIRE_Diamond_Advanced.cpp" \
        "$library/OpenFIRE_Perspective_Advanced.cpp" "$library/OpenFIRE_OneEuro_Filter.cpp" \
        "$here/deterministic_math.cpp" \
        -o "$work/driver"
}

mode="${1:-check}"
build_library

case "$mode" in
    check)
        failures=0
        while read -r arguments; do
            expected="$(expected_hash "$arguments")"
            if [ -z "$expected" ]; then
                echo "NO HASH   $arguments   (run.sh --update records it)"
                failures=$((failures + 1))
            elif [ "$(hash_of "$work/driver" $arguments)" = "$expected" ]; then
                echo "ok        $arguments"
            else
                echo "DIFFERENT $arguments"
                failures=$((failures + 1))
            fi
        done < <(scenario_arguments)

        # Expected hashes whose scenario was removed or changed would otherwise stay in the file unnoticed.
        while read -r arguments; do
            scenario_arguments | grep -qxF -- "$arguments" || {
                echo "STALE     $arguments   (not in scenarios.txt any more; run.sh --update drops it)"
                failures=$((failures + 1))
            }
        done < <(grep -v '^#' "$expected_file" | cut -d' ' -f2-)

        if [ "$failures" -ne 0 ]; then
            echo "$failures problem(s)"
            exit 1
        fi
        ;;
    --update)
        {
            echo "# Expected sha256 of the driver's output for each scenario in scenarios.txt, then the scenario's arguments."
            echo "# Regenerate with run.sh --update."
            echo "# Recorded with: $(first_line g++ --version), $(uname -m), $(first_line ldd --version)"
            while read -r arguments; do
                echo "$(hash_of "$work/driver" $arguments) $arguments"
            done < <(scenario_arguments)
        } > "$expected_file.new"
        mv "$expected_file.new" "$expected_file"
        echo "Recorded $(grep -vc '^#' "$expected_file") scenarios in expected.sha256"
        ;;
    *)
        echo "usage: $0 [--update]" >&2
        exit 2
        ;;
esac
