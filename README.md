# Synceddesk

> Cross-platform audio routing matrix — route any source to any sink over LAN with imperceptible latency.

## ✨ Key Features

- **Zero-Copy & Lock-Free:** Audio pipeline is double-buffered and lock-free using atomic swaps. The audio thread *never* blocks.
- **Ultra-Low Latency:** Uses Opus configured for `RESTRICTED_LOWDELAY` (2.5ms frames).
- **Standards Compliant:** RFC 3550 RTP transport.
- **Zero-Config Discovery:** Automatic peer discovery on LAN via mDNS/DNS-SD.
- **Cross-Platform:** Uses `miniaudio` for hardware abstraction across macOS, Windows, and Linux.
- **Reproducible Builds:** Managed via vcpkg manifest mode with a pinned `builtin-baseline`.

## 🏗️ Architecture

AudioMatrix is built on a layered architecture, strictly adhering to RAII and C++ Core Guidelines.

- **`am_core`** — The N×M routing engine, RFC 3550-compliant jitter buffer, lock-free ring buffers, and session management.
- **`am_transport`** — Opus codec wrapping and RTP payload serialization/deserialization over UDP (via standalone Asio).
- **`am_discovery`** — mDNS/DNS-SD zero-config LAN discovery using `mdns.h`.
- **`am_platform`** — Hardware audio abstraction via `miniaudio` (device enumeration, capture, playback).
- **`am_ipc`** — NNG-based control plane for inter-process communication, serialized with FlatBuffers.

## 📂 Project Structure

```text
audiomatrix/
├── CMakeLists.txt        # Top-level CMake configuration
├── CMakePresets.json     # Developer build presets (Debug, Release, ASan)
├── vcpkg.json            # vcpkg manifest defining dependencies & baseline
├── schemas/              # FlatBuffers schema definitions
│   └── control.fbs
├── src/                  # Application source code
│   ├── core/             # Routing matrix, jitter buffer, ring buffer, sessions
│   ├── discovery/        # mDNS networking
│   ├── ipc/              # NNG control server
│   ├── platform/         # Audio hardware abstraction
│   ├── transport/        # Opus codec, RTP sender/receiver
│   └── main.cpp          # Application entry point / PoC
├── tests/                # Catch2 unit test suite
│   ├── test_discovery.cpp
│   ├── test_jitter_buffer.cpp
│   ├── test_matrix.cpp
│   ├── test_opus.cpp
│   ├── test_ring_buffer.cpp
│   ├── test_rtp.cpp
│   └── test_session_manager.cpp
└── third_party/          # Vendored header-only libraries (e.g., mdns.h, readerwriterqueue)
```

## 🛠️ Prerequisites

- **CMake** ≥ 3.24
- **Ninja** build system
- **vcpkg** package manager (System-wide install required)
- **C++20 Compiler** (Clang 14+, GCC 12+, MSVC 2022+)

## 🚀 Setup & Build

AudioMatrix uses **vcpkg manifest mode** for dependency management.

### 1. Install vcpkg (System-wide)
*Do not clone `vcpkg` inside the project repository.*
```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
```

### 2. Configure Environment
Add `VCPKG_ROOT` to your shell profile (`~/.zshrc` or `~/.bashrc`):
```bash
export VCPKG_ROOT="$HOME/vcpkg"
```

### 3. Build the Project
Use the predefined CMake presets. This automatically fetches and compiles all dependencies (Opus, Asio, Catch2, spdlog, etc.).

```bash
git clone <repo-url> audiomatrix && cd audiomatrix

# Configure the project 
cmake --preset default

# Build the project
cmake --build --preset default
```

### Build Presets

| Preset | Description |
|---|---|
| `default` | Debug build with Ninja. `compile_commands.json` exported. |
| `release` | Optimized Release build (`-O3`). |
| `asan` | Debug build with AddressSanitizer enabled (`-fsanitize=address`). |

*(Note: UndefinedBehaviorSanitizer (UBSan) is also supported via CMake options: `-DENABLE_UBSAN=ON`).*

## 🎮 Running the Application

The current `main.cpp` implements the Phase 0 Proof-of-Concept, demonstrating 4 key milestones:
1. Audio Device Enumeration
2. Opus Encode/Decode Round-Trip
3. Lock-free Routing Engine Matrix
4. mDNS Service Advertising

Run the executable:
```bash
./build/audiomatrix
```

## 🧪 Running Tests

We maintain strict test coverage for the core and transport layers using **Catch2**.

Run all test suites using CTest:
```bash
ctest --preset default
```

To run a specific test suite or see detailed output, run the test binary directly:
```bash
./build/am_tests
./build/am_tests [opus]        # Run only Opus codec tests
./build/am_tests -s            # Show successful assertions and detailed output
```

## 🛡️ Development Guidelines

1. **RAII Enforcement:** All resources (file handles, sockets, audio buffers) must be strictly managed via RAII. No manual `malloc`/`free` or `new`/`delete`.
2. **Lock-Free Pipeline:** The audio thread must never block. No allocations, no mutexes, and no syscalls are permitted in the real-time path. Data handoffs are done via lock-free `RingBuffer`.
3. **Thread Safety:** Components off the real-time audio path (like `SessionManager` and `JitterBuffer`) use internal `std::mutex`. Callbacks are always fired *outside* the lock to prevent deadlocks.
4. **Third-Party Sanitization:** Vendored headers in `third_party/` are marked as `SYSTEM` in CMake to suppress external compiler warnings and keep our build output clean.

## 📄 License

TBD
