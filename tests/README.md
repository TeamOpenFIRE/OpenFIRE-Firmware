# Tests

These run on the PC, not on a board; they need only `g++`.

| Directory | What | Run |
|---|---|---|
| `unit/` | tests of single library classes and of the firmware's own files: expected behaviour, one thing per test | `unit/run.sh` |
| `tracking/` | regression test: the exact output of whole tracking runs, compared by hash | `tracking/run.sh` |
| `stub/` | the parts of the Arduino core that the libraries use, for the PC | |

`run_all.sh` runs both tests.
