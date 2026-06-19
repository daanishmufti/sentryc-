cmake_minimum_required(VERSION 3.14)
project(http_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include headers
include_directories(include)
include_directories(sentry-native/include)

# Sentry native library output
link_directories(${CMAKE_SOURCE_DIR}/sentry-native/build)

# Source files
set(SOURCES
    src/request.cpp
    src/response.cpp
    src/router.cpp
    src/server.cpp
    sentry.cpp
    main.cpp
)

add_executable(http_server ${SOURCES})

# Platform-specific libraries
if(WIN32)
    target_link_libraries(http_server ws2_32 sentry)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(http_server Threads::Threads sentry)
endif()

# Enable warnings
if(MSVC)
    target_compile_options(http_server PRIVATE /W4)
else()
    target_compile_options(http_server PRIVATE -Wall -Wextra -Wpedantic -g)
    target_link_options(http_server PRIVATE -Wl,--build-id)
endif()

# ─── Upload debug-information files to Sentry ─────────────────────────────────
# Sentry symbolicates stack traces server-side, so it needs the debug symbols
# (and, with --include-sources, the source) for the EXACT binary that crashed.
# Build with debug info (RelWithDebInfo or -g) for this to be useful.
#
# Opt in with:  cmake -DSENTRY_UPLOAD_SYMBOLS=ON ...
# Requires sentry-cli on PATH and these env vars set at build time:
#   SENTRY_AUTH_TOKEN, SENTRY_ORG, SENTRY_PROJECT
# Re-runs on every build, because each rebuild produces a new debug-id.
option(SENTRY_UPLOAD_SYMBOLS "Upload debug-information files to Sentry after build" OFF)

if(SENTRY_UPLOAD_SYMBOLS)
    find_program(SENTRY_CLI sentry-cli)
    if(NOT SENTRY_CLI)
        message(WARNING
            "SENTRY_UPLOAD_SYMBOLS=ON but sentry-cli was not found on PATH; "
            "skipping symbol upload.")
    else()
        set(SENTRY_DLL "${CMAKE_SOURCE_DIR}/sentry-native/build/libsentry.dll")
        add_custom_command(TARGET http_server POST_BUILD
            COMMAND ${SENTRY_CLI} debug-files upload --include-sources
                    "$<TARGET_FILE:http_server>" "${SENTRY_DLL}"
            COMMENT "Uploading debug-information files to Sentry"
            VERBATIM)
    endif()
endif()
