# Builds the unit test programs. Sourced by run.sh and coverage.sh, which set
#   work           a directory for the build
#   optimization   the optimization flags,
#   instrument     extra flags (for coverage), possibly none, and
#   sanitize       flags of the memory error checkers for the firmware's own code, possibly none,
# and then call build_all. It leaves one program per group of tests in $work:
#
#   unit_tests            the advanced classes (what the RP2350 runs)
#   unit_tests_classic    the classic classes (what the RP2040 runs); same names as the advanced ones, so built apart
#   unit_tests_board      the camera and button drivers, as for the RP2040, against fakes of the board
#   unit_tests_usb        the USB devices (TinyUSB_Devices), against a fake of Adafruit's HID class
#   unit_tests_firmware_rp2040 / _rp2350   the firmware's own code (OpenFIREmain), as for a Pico (classic trackers) and a Pico 2
#                         (advanced trackers), against the same fakes
#
# Which test file belongs to which group is in its name: test_classic_*, test_board_*, test_firmware_*, or none of these.

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
libraries="$(cd "$here/../../libraries" && pwd)"
library="$libraries/OpenFIREPosition"
firmware="$(cd "$here/../../OpenFIREmain" && pwd)"

# Strict IEEE float behaviour (no fused multiply-add, no fast-math), so results are reproducible.
common_flags=(-std=c++17 "${optimization[@]}" -ffp-contract=off -fno-fast-math "${instrument[@]}" -I"$here/../stub" -I"$library" -I"$firmware")
# Warnings are errors for our own code, so that new ones are noticed.
strict=(-Wall -Wextra -Werror)

compile() {   # compile <flags...> -- <source> <object>   (a failure stops the build)
    g++ "$@" || exit 1
}

build_all() {
    local advanced_tests=() classic_tests=() board_tests=() firmware_tests=() usb_tests=() test
    for test in "$here"/test_*.cpp; do
        case "$test" in
            *_classic_*) classic_tests+=("$test") ;;
            *_board_*) board_tests+=("$test") ;;
            *_firmware_*) firmware_tests+=("$test") ;;
            *_usb_*) usb_tests+=("$test") ;;
            *) advanced_tests+=("$test") ;;
        esac
    done

    # The advanced classes
    local advanced_objects=() source
    for source in OpenFIRE_Square_Advanced OpenFIRE_Diamond_Advanced OpenFIRE_OneEuro_Filter OpenFIRE_Perspective_Advanced; do
        compile "${common_flags[@]}" "${strict[@]}" -c "$library/$source.cpp" -o "$work/$source.o"
        advanced_objects+=("$work/$source.o")
    done
    compile "${common_flags[@]}" "${strict[@]}" "$here/main.cpp" "${advanced_tests[@]}" "${advanced_objects[@]}" -o "$work/unit_tests"

    # The classic classes are upstream code that isn't ours to change: its warnings are not errors here.
    local classic_objects=()
    for source in OpenFIRE_Square OpenFIRE_Diamond OpenFIRE_Perspective; do
        compile "${common_flags[@]}" -DNO_ADVANCED_TRACKING -w -c "$library/$source.cpp" -o "$work/classic_$source.o"
        classic_objects+=("$work/classic_$source.o")
    done
    compile "${common_flags[@]}" -DNO_ADVANCED_TRACKING "${strict[@]}" "$here/main.cpp" "${classic_tests[@]}" "${classic_objects[@]}" -o "$work/unit_tests_classic"

    # The camera and button drivers, as for the RP2040, whose start-up order of the camera differs from other boards'.
    local board_flags=("${common_flags[@]}" -DARDUINO_ARCH_RP2040 -I"$libraries/DFRobotIRPositionEx" -I"$libraries/LightgunButtons")
    local board_objects=()
    for source in DFRobotIRPositionEx/DFRobotIRPositionEx LightgunButtons/LightgunButtons; do
        compile "${board_flags[@]}" -w -c "$libraries/$source.cpp" -o "$work/board_$(basename "$source").o"
        board_objects+=("$work/board_$(basename "$source").o")
    done
    compile "${board_flags[@]}" "${strict[@]}" "$here/main.cpp" "${board_tests[@]}" "${board_objects[@]}" -o "$work/unit_tests_board"

    # The USB devices (mouse, keyboard, gamepad): the real TinyUSB_Devices.cpp, built for Adafruit's TinyUSB, against a fake of its
    # HID class. The library's own header goes before the fake of it in ../stub, which the other groups use.
    compile -I"$libraries/TinyUSB_Devices" "${common_flags[@]}" -std=c++23 -DUSE_TINYUSB -w -c "$libraries/TinyUSB_Devices/TinyUSB_Devices.cpp" -o "$work/usb_TinyUSB_Devices.o"
    compile -I"$libraries/TinyUSB_Devices" "${common_flags[@]}" -std=c++23 -DUSE_TINYUSB "${strict[@]}" "$here/main.cpp" "${usb_tests[@]}" "$work/usb_TinyUSB_Devices.o" -o "$work/unit_tests_usb"

    # The firmware's own code, as built for a Raspberry Pi Pico (an RP2040: it runs the classic trackers), and as for a Pico 2
    # (an RP2350 with an FPU: it runs the advanced trackers). The tests are the same. The board files use #elifdef,
    # which is C++23, and narrowing of constants is accepted as the board's compiler accepts it. An onboard NeoPixel
    # with a power pin is defined as well, though a Pico has none, so that every path of the lights is built.
    local variant
    for variant in rp2040 rp2350; do
        local firmware_flags=("${board_flags[@]}" -std=c++23 -DARDUINO_RASPBERRY_PI_PICO -DPIN_NEOPIXEL=16 -DNEOPIXEL_POWER=11 -Wno-narrowing)
        local tracking_sources=(OpenFIRE_Square OpenFIRE_Diamond OpenFIRE_Perspective OpenFIRE_OneEuro_Filter)
        if [ "$variant" = rp2350 ]; then
            firmware_flags+=(-DPICO_RP2350 -D__ARM_FP=14)
            tracking_sources=(OpenFIRE_Square OpenFIRE_Diamond OpenFIRE_Perspective OpenFIRE_Square_Advanced OpenFIRE_Diamond_Advanced OpenFIRE_Perspective_Advanced OpenFIRE_OneEuro_Filter)
        fi
        # The firmware's code reads files and serial data that can be damaged: with the checkers on, a write past a buffer is
        # an error of the test that does it, even where it would change nothing visible.
        firmware_flags+=("${sanitize[@]}")
        local firmware_objects=()
        for source in OpenFIREprefs OpenFIRElights OpenFIREFeedback OpenFIREdisplay OpenFIREcommon; do
            compile "${firmware_flags[@]}" -w -c "$firmware/$source.cpp" -o "$work/${variant}_$source.o"
            firmware_objects+=("$work/${variant}_$source.o")
        done
        for source in "${tracking_sources[@]}"; do
            compile "${firmware_flags[@]}" -w -c "$library/$source.cpp" -o "$work/${variant}_lib_$source.o"
            firmware_objects+=("$work/${variant}_lib_$source.o")
        done
        for source in DFRobotIRPositionEx/DFRobotIRPositionEx LightgunButtons/LightgunButtons; do
            compile "${firmware_flags[@]}" -w -c "$libraries/$source.cpp" -o "$work/${variant}_lib_$(basename "$source").o"
            firmware_objects+=("$work/${variant}_lib_$(basename "$source").o")
        done
        # The firmware's headers have warnings of their own (a variable that only some files use, comparisons of signed and
        # unsigned): those are left alone, the tests' own are errors.
        compile "${firmware_flags[@]}" "${strict[@]}" -Wno-unused-variable -Wno-sign-compare -Wno-unused-parameter -Wno-missing-field-initializers "$here/main.cpp" "${firmware_tests[@]}" "${firmware_objects[@]}" -o "$work/unit_tests_firmware_$variant"
    done
}

# The programs, in the order they run.
test_programs=(unit_tests unit_tests_classic unit_tests_board unit_tests_usb unit_tests_firmware_rp2040 unit_tests_firmware_rp2350)
