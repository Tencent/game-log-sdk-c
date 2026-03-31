# TGLog SDK - Project Context Guide

## Project Overview

TGLog SDK is a high-performance C language logging client library designed for transmitting logs at rates of 100,000 to 1,000,000 logs per second. The SDK provides both multi-threaded (`tglog`) and single-threaded (`tglog-st`) versions with identical APIs, supporting TCP and UDP protocols.

**Key Characteristics:**
- **Language**: C99 standard
- **Performance**: 100K-1M logs/second throughput
- **Architecture**: Dual-version (multi-threaded + single-threaded)
- **Protocols**: TCP (reliable) and UDP (high-performance)
- **Dependencies**: libevent, ConcurrencyKit (ck), jemalloc
- **Bindings**: Python (ctypes, C extension, Cython)

## Project Structure

```
sdk-c/
├── tglog/                 # Multi-threaded version source
├── tglog-st/              # Single-threaded version source
├── bindings/              # Language bindings
│   └── python/            # Python bindings (ctypes, C extension, Cython)
├── test/                  # Multi-threaded test programs
├── test-st/               # Single-threaded test programs
├── test-tapp/             # TAPP framework integration example
├── event/                 # libevent source (submodule)
├── ck/                    # ConcurrencyKit source (submodule)
├── jemalloc/              # jemalloc source (submodule)
├── lib/                   # Compiled libraries output
└── dist/                  # Installation directory
```

## Build Systems

### Makefile (Primary)
The project uses a comprehensive Makefile system that handles:
- Dependency management (submodules or download)
- Library compilation (static and shared)
- Test program building
- Language bindings compilation
- Installation to `dist/` directory

**Key Makefile Targets:**
```bash
make all              # Build everything (dependencies + TGLog + tests + bindings)
make deps             # Build only dependencies
make tglog           # Build multi-threaded version
make tglog-st        # Build single-threaded version
make test            # Build test programs
make bindings        # Build Python bindings
make install         # Install to dist/ directory
make clean           # Clean build artifacts
make check-deps      # Check dependency status
```

### CMake Support
Alternative CMake build system available with similar functionality:
- Out-of-source builds
- IDE integration support
- Cross-platform compatibility

## Dependencies Management

### Git Submodules (Preferred)
```bash
git clone --recurse-submodules https://git.woa.com/tglog/v3/sdk-c.git
# Or if already cloned:
git submodule update --init --recursive
```

### Manual Download
If not using git, dependencies can be downloaded automatically:
```bash
./download-deps.sh
```

**Dependency Versions:**
- jemalloc: 5.3.0
- libevent: release-2.1.12-stable
- ConcurrencyKit: 0.7.2

## Core Components

### Multi-threaded Version (tglog)
- **Performance**: ~1M logs/second
- **Architecture**: Automatic worker threads
- **Use Case**: High-throughput servers
- **API**: Fully thread-safe

### Single-threaded Version (tglog-st)
- **Performance**: ~100K-200K logs/second
- **Architecture**: Manual update calls required
- **Use Case**: Game servers, single-threaded applications
- **API**: Requires periodic `tglog_client_update()` calls

## Language Bindings

### Python Bindings (Three Implementations)

1. **Cython Extension** ⭐⭐⭐⭐⭐ (Recommended)
   - Performance: **105% of C baseline** (1,283,890 msg/s)
   - Implementation: `tglog_cython.pyx`
   - Build: `make python` or `python setup_cython.py build_ext --inplace`

2. **C Extension** ⭐⭐⭐⭐
   - Performance: 78% of C baseline (950,718 msg/s)
   - Implementation: `tglog_ext.c`
   - Build: `make bindings`

3. **ctypes Binding** ❌ (Not recommended for production)
   - Performance: 3% of C baseline (38,615 msg/s)
   - Implementation: `tglog_client.py`
   - Usage: Pure Python, no compilation needed

## Building and Compilation

### Quick Start
```bash
# Clone and build everything
make all

# Or step by step
make deps          # Build dependencies
make tglog         # Build multi-threaded version
make tglog-st      # Build single-threaded version
make test          # Build test programs
make bindings      # Build Python bindings
```

### Compilation Flags
```bash
make RELEASE=1     # Build optimized release version
make V=1           # Verbose build output
```

### Installation
```bash
make install        # Install to dist/ directory
```

## Usage Examples

### C Language Usage

**Multi-threaded Version:**
```c
#include "tglog/tglog.h"

tglog_conf *conf = tglog_conf_create(TGLOG_NETWORK_TCP, "server.com", 8080, NULL, NULL, NULL);
tglog_client *client = tglog_client_create(conf);

tglog_client_send(client, "mylog|field1|field2\n", NULL, NULL, TGLOG_MSG_F_COPY);

tglog_client_destroy(client);
tglog_conf_destroy(conf);
```

**Single-threaded Version:**
```c
#include "tglog-st/tglog.h"

// In main loop:
while (running) {
    tglog_client_send(client, "mylog|data\n", NULL, NULL, TGLOG_MSG_F_COPY);
    tglog_client_update(client);  // REQUIRED for single-threaded version
}
```

### Python Usage

**Cython (Recommended):**
```python
from tglog_cython import TglogClient

client = TglogClient("tcp", "server.com", 8080, workers=4)
client.send("mylog|data\n")
client.destroy()
```

## Testing and Verification

### Test Programs
```bash
# Build and run tests
make test
cd test
./tglog_test          # Multi-threaded test
./tglog_test_st        # Single-threaded test
```

### Performance Testing
```bash
cd test-st
./simple_perf_test_st  # Single-threaded performance test
```

### Python Binding Tests
```bash
cd bindings/python
python test_cython.py  # Test Cython binding
python test_ext.py     # Test C extension
```

## Configuration and Tuning

### Key Configuration Parameters
- `worker_num`: Number of worker threads (multi-threaded only)
- `batching_max_messages`: Maximum messages per batch
- `batching_max_size`: Maximum batch size in bytes
- `max_pending_messages`: Queue capacity
- `send_timeout`: Send operation timeout

### Performance Tuning Examples

**High Throughput:**
```c
conf->worker_num = CPU_CORES * 2;
conf->batching_max_messages = 200;
conf->batching_max_size = 100 * 1024;
```

**Low Latency:**
```c
conf->worker_num = CPU_CORES;
conf->batching_max_messages = 10;
conf->batching_max_delay = 5;
```

## Integration Examples

### TAPP Framework Integration
See `test-tapp/` directory for complete TAPP application integration:
- TAPP tick callback integration
- Configuration file support
- Performance monitoring

### Custom Application Integration
```c
// Game server example
void game_tick() {
    process_game_logic();
    
    // Send game events
    tglog_client_send(client, game_event_log, NULL, NULL, TGLOG_MSG_F_COPY);
    
    // Update TGLog (single-threaded version)
    tglog_client_update(client);
}
```

## Troubleshooting

### Common Issues

**Compilation Errors:**
- Ensure dependencies are properly initialized
- Check submodule versions
- Verify compiler supports C99 standard

**Runtime Issues:**
- Single-threaded version: Must call `tglog_client_update()` regularly
- Connection failures: Check server address and port
- Performance issues: Adjust batch sizes and worker counts

**Python Binding Issues:**
- Cython requires Cython package: `pip install cython`
- C extension requires Python development headers
- ctypes binding: Performance limitations expected

### Debugging
```c
// Enable debug logging
tglog_conf_set_debug_logger(conf, tglog_can_log_debug, tglog_std_write_log, NULL);

// Check metrics
char metrics[8192];
tglog_client_print_metrics(client, metrics, sizeof(metrics), true);
printf("Metrics: %s\n", metrics);
```

## Documentation References

### Key Documentation Files
- `README.md`: Comprehensive project overview
- `tglog/DESIGN.md`: Multi-threaded architecture design
- `tglog-st/DESIGN.md`: Single-threaded architecture design
- `bindings/python/README.md`: Python binding documentation
- `CMAKE_BUILD_GUIDE.md`: CMake build instructions
- `CLONE_AND_BUILD.md`: Cloning and building guide

### API Documentation
- Header files in `tglog/` and `tglog-st/` directories
- Function documentation in source code comments
- Example usage in test programs

## Development Guidelines

### Code Style
- Follow C99 standard
- Use `tglog_` prefix for functions and types
- Include comprehensive error handling
- Add unit tests for new features

### Contribution Process
1. Fork the repository
2. Create feature branch
3. Implement changes with tests
4. Submit pull request
5. Code review and merge

### Testing Requirements
- All new features must include tests
- Performance benchmarks for optimizations
- Compatibility testing for different platforms

## Performance Benchmarks

### C Language Performance
| Version | Throughput | Latency | Use Case |
|---------|------------|---------|----------|
| Multi-threaded | ~1M logs/sec | <1ms | High-throughput servers |
| Single-threaded | ~150K logs/sec | <1ms | Game servers, embedded |

### Python Binding Performance
| Implementation | Throughput | Relative Performance | Recommendation |
|---------------|------------|---------------------|----------------|
| Cython | 1,283,890 msg/s | 105% | ⭐⭐⭐⭐⭐ (Production) |
| C Extension | 950,718 msg/s | 78% | ⭐⭐⭐⭐ (Production) |
| ctypes | 38,615 msg/s | 3% | ❌ (Development only) |

## Support and Resources

### Getting Help
- Check existing documentation in `README.md` files
- Review test programs for usage examples
- Examine header files for API documentation

### Issue Reporting
- Provide detailed reproduction steps
- Include system information and versions
- Attach relevant logs and configuration

### Community
- Project repository: https://git.woa.com/tglog/v3/sdk-c
- Documentation: Comprehensive README files
- Examples: Test programs and integration examples

---

*This GEMINI.md file provides essential context for working with the TGLog SDK project. For detailed technical specifications, refer to the specific component documentation files.*