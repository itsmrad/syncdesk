cmake_minimum_required(VERSION 3.24)

project(audiomatrix
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "Cross-platform audio routing matrix — route any source to any sink"
)

# ── C++ standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Compiler warnings ────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
elseif(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# ── Sanitizers ────────────────────────────────────────────────────────────
option(ENABLE_ASAN  "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=address)
endif()
if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

# ── vcpkg dependencies ──────────────────────────────────────────────────
find_package(Opus CONFIG REQUIRED)
find_package(SampleRate CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nng CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)
find_package(readerwriterqueue CONFIG REQUIRED)

# Standalone Asio (header-only)
find_path(ASIO_INCLUDE_DIR asio.hpp)
if(NOT ASIO_INCLUDE_DIR)
    message(FATAL_ERROR "Asio not found. Ensure vcpkg installed it (check vcpkg.json).")
endif()
add_library(asio INTERFACE)
target_include_directories(asio SYSTEM INTERFACE ${ASIO_INCLUDE_DIR})
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)

# ── Third-party vendored headers ────────────────────────────────────────
# Marked as SYSTEM so the compiler suppresses warnings from vendored code.
add_library(third_party INTERFACE)
target_include_directories(third_party SYSTEM INTERFACE
    ${CMAKE_SOURCE_DIR}/third_party
)

# ── Core library ────────────────────────────────────────────────────────
add_library(am_core STATIC
    src/core/AudioMatrix.cpp
    src/core/JitterBuffer.cpp
    src/core/SessionManager.cpp
)
target_include_directories(am_core PUBLIC src)
target_link_libraries(am_core PUBLIC
    third_party
    readerwriterqueue::readerwriterqueue
    spdlog::spdlog
)

# ── Transport library ──────────────────────────────────────────────────
add_library(am_transport STATIC
    src/transport/RtpSender.cpp
    src/transport/RtpReceiver.cpp
    src/transport/OpusCodec.cpp
)
target_include_directories(am_transport PUBLIC src)
target_link_libraries(am_transport PUBLIC
    am_core
    Opus::opus
    asio
    spdlog::spdlog
)

# ── Discovery library ──────────────────────────────────────────────────
add_library(am_discovery STATIC
    src/discovery/MdnsService.cpp
)
target_include_directories(am_discovery PUBLIC src)
target_link_libraries(am_discovery PUBLIC
    third_party
    spdlog::spdlog
)

# ── Platform library ───────────────────────────────────────────────────
add_library(am_platform STATIC
    src/platform/AudioEngine.cpp
)
target_include_directories(am_platform PUBLIC src)
target_link_libraries(am_platform PUBLIC
    third_party
    spdlog::spdlog
)
# miniaudio needs platform audio frameworks
if(APPLE)
    find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(am_platform PUBLIC
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOUNIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
    )
elseif(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(am_platform PUBLIC
        Threads::Threads
        dl
        m
    )
elseif(WIN32)
    target_link_libraries(am_platform PUBLIC ole32 winmm)
endif()

# ── IPC library ─────────────────────────────────────────────────────────
add_library(am_ipc STATIC
    src/ipc/ControlServer.cpp
)
target_include_directories(am_ipc PUBLIC src)
target_link_libraries(am_ipc PUBLIC
    nng::nng
    flatbuffers::flatbuffers
    spdlog::spdlog
)

# ── Main executable ─────────────────────────────────────────────────────
add_executable(audiomatrix
    src/main.cpp
)
target_link_libraries(audiomatrix PRIVATE
    am_transport
    am_discovery
    am_platform
    am_ipc
    SampleRate::samplerate
)

# ── Tests ────────────────────────────────────────────────────────────────
enable_testing()
add_executable(am_tests
    tests/test_matrix.cpp
    tests/test_rtp.cpp
    tests/test_discovery.cpp
    tests/test_opus.cpp
    tests/test_jitter_buffer.cpp
    tests/test_ring_buffer.cpp
    tests/test_session_manager.cpp
)
target_link_libraries(am_tests PRIVATE
    am_transport
    am_discovery
    am_platform
    Catch2::Catch2WithMain
)
include(CTest)
include(Catch)
catch_discover_tests(am_tests)
