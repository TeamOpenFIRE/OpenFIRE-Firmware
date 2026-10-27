# Tracking tests

Tests for the IR tracking in `libraries/OpenFIREPosition`: the advanced Square and Diamond trackers, the perspective
warp and the 1€ filter. They run on a desktop computer, with `g++` and `bash`; no board or camera is needed.

## How it works

`driver.cpp` simulates a light gun in front of the screen: a camera moving and aiming around, rolling up to full turns
with sudden wrist flips, and losing LEDs in every pattern (single LEDs flickering, pairs hidden, complete blackouts).
Each frame goes through the trackers, the 1€ filter and the perspective warp, and every output is printed. The
simulation is deterministic (fixed random seed, simulated clock), so its output is the same on every run.

Each scenario in `scenarios.txt` is one run of the driver with different arguments: Square or Diamond layout, wide
Square, calibration mode, roll speed, how often LEDs are lost. `expected.sha256` holds the SHA-256 of the output each
scenario is expected to produce. The test passes if every output hashes to its expected value, so any change in what
the tracking computes, even in the last bit of a float, is noticed.

`../stub/Arduino.h` stands in for the Arduino core. It defines the core's macros (`PI`, `TWO_PI`, `sq`, `bit`, ...), so that a
name in the library that clashes with one of them fails here too, and not only when building for the board.
The library is built with `-Wall -Wextra -Werror`.

## Running it

```bash
tests/tracking/run.sh
```

It reports `ok`, `DIFFERENT` (the output changed), `NO HASH` (a scenario without an expected hash) or `STALE` (an
expected hash for a scenario that no longer exists), and exits with an error if anything but `ok` was reported.

## When the output changes on purpose

Changing how the tracking behaves changes the output, and the test then fails. Once you have checked that the new
behaviour is the intended one:

```bash
tests/tracking/run.sh --update
git diff tests/tracking/expected.sha256
```

and commit the changed hashes together with the change that caused them. A hash that changes without a
reason is the test doing its job: look into it before updating anything.

## Adding scenarios

Add a line to `scenarios.txt` (the arguments are explained at the top of the file), run `run.sh --update`, and commit both
files. A scenario is worth adding when it exercises something the existing ones don't: for example a situation
from a bug report, reproduced with a seed that triggers it.

## Platform

The output must not depend on the compiler, its optimization level, the C library or the CPU, or a hash recorded on one computer
would not match on another (and did not, on the CI runner, when it moved to a newer Ubuntu). The floating point
arithmetic is plain IEEE (`-ffp-contract=off`, no fast-math) and gives the same everywhere. The C library's `sin`, `cos`
and `atan2f` do not: they are only accurate to within a few units of the last place, and differ between versions of
glibc and even between CPUs. So the driver is built with `-fno-builtin` and `deterministic_math.cpp`, which has
versions of those functions made of nothing but IEEE arithmetic (correctly rounded as floats, in the cases checked), that
the tracking code calls instead. The same hashes come out at `-O0` to `-O3`, and with `-march=native`.

If a hash differs on some platform anyway, a function that the tracking calls and that isn't made of exact operations
(`sqrtf`, `fabsf`, `roundf` and the like are exact) has been added, or the compiler is not following IEEE rules:
find which, and don't update the hashes to hide it.

## What it doesn't cover

- It says that the output didn't change, not that it is right: for that a real gun is needed.
- The sketch (feedback, serial commands, menus) is not tested here yet; new tests for it can go in their own
  directory next to this one.
- Speed on the board is not measured.
