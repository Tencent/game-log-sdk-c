# Git Submodules Management

This project uses git submodules to manage third-party dependencies. The build system automatically initializes and checks out the correct versions of these dependencies.

## Submodule Versions

The following versions are automatically configured:

- **jemalloc**: `5.3.0`
- **libevent**: `release-2.1.12-stable`
- **ck (Concurrency Kit)**: `0.7.2`

## Key Features

- **No Network Access**: Version checking uses only local git commands (`git describe`, `git rev-parse`)
- **Smart Detection**: Only switches versions when necessary, avoiding unnecessary operations
- **Fast Execution**: Typically completes in milliseconds when submodules are already at correct versions
- **Clear Feedback**: Shows which submodules are being updated and their version transitions

## Automatic Initialization

### Using Makefile

When you run `make`, the build system will automatically:
1. Check if submodules are initialized
2. If not, run `git submodule update --init --recursive`
3. Checkout each submodule to the specified version

```bash
# Build everything (will auto-initialize submodules)
make

# Or explicitly initialize submodules
make init-submodules

# Then build dependencies
make deps
```

### Using CMake

CMake will automatically initialize and checkout submodules during the configuration phase:

```bash
# Configure (will auto-initialize submodules)
cmake .

# Or for release build
cmake -DRELEASE=1 .

# Then build
make
```

## Manual Submodule Management

If you need to manually manage submodules:

```bash
# Initialize all submodules
git submodule update --init --recursive

# Checkout specific versions
cd jemalloc && git checkout 5.3.0
cd ../event && git checkout release-2.1.12-stable
cd ../ck && git checkout 0.7.2
cd ..
```

## Updating Submodule Versions

To update the versions used by the build system:

### For Makefile

Edit the version variables in `Makefile`:

```makefile
JEMALLOC_VERSION = 5.3.0
EVENT_VERSION = release-2.1.12-stable
CK_VERSION = 0.7.2
```

### For CMake

Edit the version variables in `CMakeLists.txt`:

```cmake
set(JEMALLOC_VERSION "5.3.0")
set(EVENT_VERSION "release-2.1.12-stable")
set(CK_VERSION "0.7.2")
```

## Troubleshooting

### Submodules not initialized

If you see errors about missing submodules:

```bash
# Makefile
make init-submodules

# CMake
rm -rf CMakeCache.txt && cmake .
```

### Wrong version checked out

If submodules are at the wrong version:

```bash
# Makefile
make init-submodules

# CMake
# Just re-run cmake
cmake .
```

### Clean submodules

To completely clean and re-initialize submodules:

```bash
# Remove submodule directories
rm -rf jemalloc event ck

# Re-initialize
git submodule update --init --recursive

# Then rebuild
make clean && make
# or
rm -rf build && mkdir build && cd build && cmake .. && make
```

## First-time Setup

For a fresh clone of the repository:

```bash
# Clone the repository
git clone <repository-url>
cd sdk-c

# Option 1: Let make handle it
make

# Option 2: Let cmake handle it
mkdir build && cd build
cmake ..
make

# Option 3: Manual initialization
git submodule update --init --recursive
cd jemalloc && git checkout 5.3.0 && cd ..
cd event && git checkout release-2.1.12-stable && cd ..
cd ck && git checkout 0.7.2 && cd ..
make
```

## Notes

- The build system checks for the existence of `.git` files in submodule directories to determine if initialization is needed
- Version checkout is done quietly to avoid cluttering the build output
- If a submodule is already at the correct version, the checkout is a no-op
- The `git fetch --tags` command ensures that tag references are up to date before checkout
