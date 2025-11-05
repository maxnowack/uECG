# Repository Guidelines

## Project Structure & Module Organization
Main firmware sources live in `uECG_v5/`, with each hardware unit split into paired `.c/.h` files such as `ecg_processor.c` and `radio_functions.c`. Generated outputs land in `uECG_v5/build/`; leave this directory out of commits. The Nordic micro-SDK clone stays in `urf_lib/` beside this repository and is consumed via relative paths in the Makefile. Container assets that reproduce the toolchain are under `docker/`, while helper scripts (`build.sh`, `upload.sh`) reside at the repository root.

## Build, Test, and Development Commands
- `./build.sh` — starts the Podman toolchain image, runs `make clean && make` in `uECG_v5/`, and emits fresh HEX/BIN files into `uECG_v5/build/`.
- `cd uECG_v5 && make` — uses the locally installed `arm-none-eabi-gcc` toolchain referenced in `urf_lib/nrf_usdk52/gcc/Makefile.posix`.
- `./upload.sh` — flashes `build/uECG5.hex` through OpenOCD and an ST-Link probe; ensure the board is in bootloader mode.
Always confirm `urf_lib/` is checked out at the same directory depth and that `arm-none-eabi-gcc` matches the version expected by the SDK.

## Coding Style & Naming Conventions
Source files follow GNU C17 with tab indentation and brace placement on the following line (`void foo()\n{`). Use `lower_snake_case` for functions/variables (`fast_clock_start`), capitalized constants for macros, and keep module-scope globals in the matching `.c` file unless shared via headers. Maintain existing header include ordering: project headers first, then SDK includes. When touching protocol or BLE definitions, document new values inline to aid reverse-engineering on the mobile side.

## Testing Guidelines
There are no automated unit tests; rely on device-level validation. After building, flash the firmware and verify ECG streaming, BLE connection, and accelerometer data using the companion app or `uECG_nodejs`. When altering filters or timing, capture at least one five-minute recording and compare HRV stability against a known-good build. Prefer logging temporary diagnostics via LED patterns or radio debug packets instead of `printf`, which impacts timing.

## Commit & Pull Request Guidelines
Recent history favors concise, imperative summaries (e.g., `chore: ignore urf_lib`). Use the same style for new commits, grouping related firmware changes together. For pull requests, describe the hardware setup used for validation, link any relevant issues, and attach screenshots or logs from the companion app when behavior changes. Highlight protocol or timing adjustments so reviewers can sync updates across upstream apps and bootloaders.
