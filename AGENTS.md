# Repository Guidelines

## Project Structure & Module Organization
Core C SDK code is split into two modules:
- `tglog/`: multi-threaded client library (`libtglog.a/.so`)
- `tglog-st/`: single-threaded client library (`libtglog-st.a/.so`)

Tests and examples live in:
- `test/`: multi-threaded C test programs
- `test-st/`: single-threaded C test programs
- `test-tapp/`: TAPP integration example

Bindings are under `bindings/` (Python sources in `bindings/python/`).
Vendored dependencies are in `jemalloc/`, `event/`, and `ck/`, with built artifacts staged in `thirdparty/`. Final outputs are copied to `lib/` and `dist/`.

## Build, Test, and Development Commands
- `make`: build dependencies, both SDK variants, tests, test-tapp, and bindings.
- `make deps`: build only third-party dependencies.
- `make tglog` / `make tglog-st`: build a single SDK variant.
- `make test` / `make test-tapp`: build test executables.
- `make bindings` or `make python`: build Python bindings.
- `make check-deps`: verify expected libraries/binaries exist.
- `make clean` / `make clean-deps`: clean project outputs or dependency outputs.

CMake workflow is also supported:
- `cmake -S . -B build && cmake --build build -j4`

## Coding Style & Naming Conventions
Use C99 and keep code warning-free under project flags (`-Wall`, module-specific strict flags where enabled).
- Indentation: 4 spaces, no tabs.
- Braces: same line (`if (...) {`).
- Naming: `snake_case`; public APIs prefixed with `tglog_`.
- Keep module headers local (`*.h`) and include ordering consistent with existing files.

## Testing Guidelines
Add or update C tests in `test/` or `test-st/` as `*_test.c` or focused executable sources.
- Build tests with `make test`.
- Run generated binaries from `test/` and `test-st/` directly.
- For Python binding changes, run `python3 bindings/python/test_ext.py` or `python3 bindings/python/test_cython.py` after building bindings.

Include failure-path coverage for networking, reconnection, and memory/resource cleanup where relevant.

## Commit & Pull Request Guidelines
Follow the repository commit style seen in history:
- Format: `<type>: <short description>`
- Common types: `fix`, `feat`, `opt`, `docs`, `chore`

Keep commits scoped to one logical change. For pull requests, include:
- What changed and why
- Affected modules (for example `tglog-st/worker.c`)
- Build/test evidence (commands + results)
- Related issue/ticket link if available

## Dependency & Configuration Notes
Initialize submodules before first build:
- `git submodule update --init --recursive`

If building from a ZIP archive, use `./download-deps.sh` before `make` when dependencies are missing.
