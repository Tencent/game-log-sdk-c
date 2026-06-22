# CODEBUDDY.md

This file provides guidance to CodeBuddy Code when working with code in this repository.

## Project Overview

TGLog SDK is a high-performance C99 logging client library supporting 100K-1M logs/sec with dual implementation variants (multi-threaded and single-threaded). Built on libevent and ConcurrencyKit with Python language bindings.

## Project Structure & Module Organization
Core C SDK code is split into two modules:
- `tglog/`: multi-threaded client library (`libtglog.a/.so`) - ~1M logs/sec
- `tglog-st/`: single-threaded client library (`libtglog-st.a/.so`) - ~100-200K logs/sec

Tests and examples live in:
- `test/`: multi-threaded C test programs
- `test-st/`: single-threaded C test programs
- `test-tapp/`: TAPP integration example (shows how to integrate single-threaded version into game server tick loop)

Bindings are under `bindings/` (Python sources in `bindings/python/` with three implementations: ctypes, C extension, Cython).
Vendored dependencies are in `jemalloc/`, `event/`, and `ck/`, with built artifacts staged in `thirdparty/`. Final outputs are copied to `lib/` and `dist/`.

## High-Level Architecture & Data Flow

### The Two Variants
The API remains largely identical for both variants, but the implementations are independent and mirror the same file/module names rather than sharing core implementation files.
1. **Multi-threaded (tglog/)**: Fully thread-safe. Log data ingested via `tglog_client_send()` goes into a lock-free ring buffer (ConcurrencyKit `ck_ring_t`). It is processed by multiple worker threads running libevent loops asynchronously.
2. **Single-threaded (tglog-st/)**: NOT thread-safe. Log data goes directly into a batch buffer rather than a producer/consumer queue. The client must periodically call `tglog_client_update()` to drive the event loop (libevent), batch dispatch, DNS, and I/O in the same thread.

### Core Components
Both variants consist of similar file structures:
- **`client.c/h`**: The entry point. Handles setup, creates the client, and manages global states and threads (if any).
- **`worker.c/h`**: Dispatches logs from the queue/buffer to the endpoints. Batches messages by size, count, or publish delay.
- **`conn.c/h`**: Manages the socket lifecycle (TCP or UDP), connection pools, and automatic reconnection with exponential backoff. Send timeouts and buffering happen here.
- **`discoverer.c/h`**: DNS resolution. Uses a separate thread in `tglog` but timer-driven events in `tglog-st`.
- **`request.c/h`**: Manages building the pipe-delimited payload (`tglogname|field=value...`) into request structures.
- **`endpoint.c/h`**: Endpoint (resolved server IP) management.
- **`pool.c/h`**: Pre-allocated memory pools heavily utilized to bypass frequent malloc/free calls. Pool capacity defaults differ between variants: multi-threaded defaults are ~10x the single-threaded defaults.
- **`metrics.c/h`**: Maintains statistics like sent, dropped, and latency. Uses atomic operations (`ck_pr_*`) for multi-threaded counters.
- **`conf.c/h`**: Configuration struct and `tglog_conf_set_default()`.
- **`debug_logger.c/h`**: Pluggable debug logging via `can_log` / `write_log` function pointers.
- **`err.c/h`**: Error code definitions (`TGLOG_ERR_*`) used in callbacks.

*Important Note: If you are modifying core logic, usually you need to update both `tglog/` and `tglog-st/` respectively to keep them in sync. The two variants mirror the same file/module names but have fully independent implementations — there is no shared core.*

## Build, Test, and Development Commands
- `make`: build dependencies, both SDK variants, tests, test-tapp, and bindings.
- `make deps`: build only third-party dependencies.
- `make tglog` / `make tglog-st`: build a single SDK variant.
- `make test` / `make test-tapp`: build C tests or the TAPP integration example.
- `make bindings` or `make python`: build Python bindings.
- `make install` / `make uninstall`: install to or remove `dist/`.
- `make check-deps`: verify expected libraries/binaries exist.
- `make clean` / `make clean-deps` / `make clean-bindings`: clean project outputs, dependency outputs, or binding outputs.
- `make init-submodules`: initialize vendored dependencies and check out pinned versions.
- `make RELEASE=1`: optimized build (`-O2 -DNDEBUG`).
- `make V=1`: verbose Makefile output.
- `make help`: show Makefile targets.

CMake workflow is also supported:
- `cmake -S . -B build && cmake --build build -j4`
- `cmake --build build --target help_info`: show CMake target help.
- Windows builds should use CMake + MSVC; they do not use jemalloc or ConcurrencyKit and link Windows socket libraries such as `ws2_32`.

There is no standalone lint target. C test builds use `-Wall -Werror`; the root/core builds use C99 flags without `-Werror`.

### How to Run Tests
- Build C tests first with `make test`.
- **Multi-threaded C test**: `./test/tglog_test` or `cd test && ./tglog_test`.
- **Single-threaded C tests**: `./test-st/tglog_test_st`, `./test-st/simple_perf_test_st`, or `make -C test-st run` to run both.
- **Python binding tests**: after `make python`, run `make -C bindings run_python`, `make -C bindings/python run_ctypes`, `make -C bindings/python run_ext`, or `make -C bindings/python run_cython`.
- **TAPP integration**: build with `make test-tapp`, then run `cd test-tapp && ./start.sh`; stop with `./stop.sh`.

### Adding Features or Logic
1. **Config changes**: Edit `tglog_conf` struct in `tglog/conf.h` and `tglog-st/conf.h`. Update `tglog_conf_set_default()` in both variants.
2. **Metrics**: Add to `tglog_metrics` in `metrics.h` using lock-free `ck_pr_*` (multi-threaded) and regular operations (single-threaded). Print them via `tglog_client_print_metrics()`.
3. **Memory management**: Never link `jemalloc` in the core SDK library. The user app provides it. Use `pool.c/h` for memory object pooling.
4. **Connection logic**: Edit `conn.c/h` in both variants; keep TCP/UDP handling consistent and preserve the exponential-backoff reconnection behavior.

*When in doubt about config defaults, the source (`tglog-st/conf.c` / `tglog/conf.c` `tglog_conf_set_default()`) is authoritative — the README config table may lag behind actual defaults.*

## Public API Reference

### Core send function (both variants)
```c
int tglog_client_send(tglog_client *client, const char *payload,
                      tglog_send_callback cb, void *user_data,
                      tglog_msg_flag flag);
```

### `msg_flag` values
```c
TGLOG_MSG_F_NONE  // zero-copy; caller retains ownership, SDK does NOT free
TGLOG_MSG_F_FREE  // SDK calls free() after send (even on error); payload must be malloc'd
TGLOG_MSG_F_COPY  // SDK copies; caller retains ownership — recommended default
```

### Multi-threaded only
- `tglog_use_thread()` — **must be called once per process** before creating any client.
- Callbacks (`can_log`, `write_log`, `tglog_send_callback`) are invoked from worker threads and **must be thread-safe**.

### Single-threaded only
- `tglog_client_update(client)` — **must be called periodically** in the main loop to drive I/O. If not called, nothing is ever sent.
- Single-threaded-specific config fields: `max_update_us` (default 300µs) and `max_messages_per_update` (default 20).

## Wire Protocol

Payload format:
```
tglogname|field1=value1|field2=value2|...|fieldN=valueN\n
```
- Each message **must end with `\n`**.
- Field values **cannot contain `\n` or `|`** (must be escaped by the caller).
- UDP mode hard limit: **65,507 bytes** per batch.

## Linking Against the SDK

```bash
# Multi-threaded
gcc your_app.c -o app -ltglog -levent -levent_pthreads -ljemalloc -lpthread -latomic -lm

# Single-threaded
gcc your_app.c -o app -ltglog-st -levent -ljemalloc -lpthread -latomic -lm
```
`jemalloc` is **not** linked into the SDK itself — the final executable must provide it.

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

## Important Pitfalls

- **No end-to-end delivery confirmation**: send success means data reached the OS network buffer, not the server. There is no application-layer ack.
- **Single-threaded stall**: forgetting `tglog_client_update()` means no messages are ever dispatched.
- **Process-level init**: `tglog_use_thread()` is required once per process for the multi-threaded variant only; there is no equivalent for `tglog-st`.
- **CLB reconnection**: do not set `max_conn_lifetime` to 0 or negative when behind a Cloud Load Balancer — periodic reconnects are needed for proper load distribution.
- **Platform split**: Unix-like builds use the Makefile/CMake paths with libevent, jemalloc, pthread, and ConcurrencyKit. Windows builds are CMake + MSVC only, use native Windows threading/socket libraries, and do not use jemalloc or ConcurrencyKit.
