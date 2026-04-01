# CODEBUDDY.md

This file provides guidance to CodeBuddy Code when working with code in this repository.

## Project Overview

TGLog SDK is a high-performance C99 logging client library supporting 100K-1M logs/sec with dual implementation variants (multi-threaded and single-threaded). Built on libevent and ConcurrencyKit with Python language bindings.

## Project Structure & Module Organization
Core C SDK code is split into two modules:
- `tglog/`: multi-threaded client library (`libtglog.a/.so`) - ~1M logs/sec
- `tglog-st/`: single-threaded client library (`libtglog-st.a/.so`) - 10-20K logs/sec

Tests and examples live in:
- `test/`: multi-threaded C test programs
- `test-st/`: single-threaded C test programs
- `test-tapp/`: TAPP integration example (shows how to integrate single-threaded version into game server tick loop)

Bindings are under `bindings/` (Python sources in `bindings/python/` with three implementations: ctypes, C extension, Cython).
Vendored dependencies are in `jemalloc/`, `event/`, and `ck/`, with built artifacts staged in `thirdparty/`. Final outputs are copied to `lib/` and `dist/`.

## High-Level Architecture & Data Flow

### The Two Variants
The API remains largely identical for both variants, but the internal models differ significantly.
1. **Multi-threaded (tglog/)**: Fully thread-safe. Log data ingested via `tglog_client_send()` goes into a lock-free ring buffer (ConcurrencyKit `ck_ring_t`). It is processed by multiple worker threads running libevent loops asynchronously.
2. **Single-threaded (tglog-st/)**: NOT thread-safe. Ingested data is queued into a batch buffer. The client must periodically call `tglog_client_update()` to drive the event loop (libevent) and dispatch the data within the same thread. DNS and I/O happen inside this update loop.

### Core Components
Both variants consist of similar file structures:
- **`client.c/h`**: The entry point. Handles setup, creates the client, and manages global states and threads (if any).
- **`worker.c/h`**: Dispatches logs from the queue/buffer to the endpoints. Batches messages by size, count, or publish delay.
- **`conn.c/h`**: Manages the socket lifecycle (TCP or UDP), connection pools, and automatic reconnection with exponential backoff. Send timeouts and buffering happen here.
- **`discoverer.c/h`**: DNS resolution. Uses a separate thread in `tglog` but timer-driven events in `tglog-st`.
- **`request.c/h`**: Manages building the pipe-delimited payload (`tglogname|field=value...`) into request structures.
- **`pool.c/h`**: Pre-allocated memory pools heavily utilized to bypass frequent malloc/free calls.
- **`metrics.c/h`**: Maintains statistics like sent, dropped, and latency. Uses atomic operations (`ck_pr_*`) for multi-threaded counters.

*Important Note: If you are modifying core logic, usually you need to update both `tglog/` and `tglog-st/` respectively to keep them in sync.*

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

### How to Run Tests
- **Multi-threaded C tests**: `cd test && ./tglog_test`
- **Single-threaded C tests**: `cd test-st && ./tglog_test_st`
- **Python Binding tests**: `python3 bindings/python/test_ext.py` or `python3 bindings/python/test_cython.py` after building bindings (`make python`).
- **TAPP integration**: `cd test-tapp && ./start.sh`

### Adding Features or Logic
1. **Config changes**: Edit `tglog_conf` struct in `tglog/conf.h` and `tglog-st/conf.h`. Update `tglog_conf_set_default()`.
2. **Metrics**: Add to `tglog_metrics` in `metrics.h` using lock-free `ck_pr_*` (multi-threaded) and regular operations (single-threaded). Print them via `tglog_client_print_metrics()`.
3. **Memory management**: Never link `jemalloc` in the core SDK library. The user app provides it. Use `pool.c/h` for memory object pooling.

## Coding Style & Naming Conventions
Use C99 and keep code warning-free under project flags (`-Wall`, module-specific strict flags where enabled).
- Indentation: 4 spaces, no tabs.
- Braces: same line (`if (...) {`).
- Naming: `snake_case`; public APIs prefixed with `tglog_`.
- Keep module headers local (`*.h`) and include ordering consistent with existing files.
- The C99 standard must be strictly followed.

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
