# libhegel.cmake — acquire and expose libhegel (Hegel's native engine).
#
# hegel-cpp drives Hegel's engine in-process through libhegel's C ABI (the
# `hegel_*` functions declared in hegel.h). This module
# downloads the prebuilt shared library for the host platform from the
# libhegel GitHub release, verifies it against the published SHA-256
# sidecar, and exposes it as the imported target `hegel::libhegel`.
#
# Override the download by setting `-DHEGEL_LIBHEGEL_LIBRARY=/path/to/lib`
# (e.g. a locally built `target/release/libhegel_c.dylib`); the version
# pin and per-platform mapping are then ignored.

# libhegel release the bundled C header matches. Keep in sync with
# libhegel/hegel.h.
set(HEGEL_LIBHEGEL_VERSION "0.27.0"
    CACHE STRING "libhegel (hegeltest) release version to download")
set(HEGEL_LIBHEGEL_BASE_URL
    "https://github.com/hegeldev/hegel-rust/releases/download/v${HEGEL_LIBHEGEL_VERSION}")

set(_hegel_header_dir "${CMAKE_CURRENT_SOURCE_DIR}/libhegel")

# Map the host platform onto the released asset (libhegel-<goos>-<goarch>.<ext>).
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_goos darwin)
    set(_ext dylib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_goos linux)
    set(_ext so)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_goos windows)
    set(_ext dll)
else()
    message(FATAL_ERROR
        "libhegel: unsupported host OS '${CMAKE_SYSTEM_NAME}'. "
        "Set -DHEGEL_LIBHEGEL_LIBRARY to a locally built libhegel.")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(_goarch amd64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
    set(_goarch arm64)
else()
    message(FATAL_ERROR
        "libhegel: unsupported host arch '${CMAKE_SYSTEM_PROCESSOR}'. "
        "Set -DHEGEL_LIBHEGEL_LIBRARY to a locally built libhegel.")
endif()

# Normalized on-disk name matches the Rust output stem (libhegel_c.<ext>) so the
# macOS @rpath install name and the Linux SONAME both resolve to this file.
set(_hegel_local_name "libhegel_c.${_ext}")
set(_hegel_dl_dir "${CMAKE_CURRENT_BINARY_DIR}/libhegel")
set(_hegel_local "${_hegel_dl_dir}/${_hegel_local_name}")

if(HEGEL_LIBHEGEL_LIBRARY)
    # User-supplied library: use it verbatim, no download or verification.
    set(_hegel_local "${HEGEL_LIBHEGEL_LIBRARY}")
    if(NOT EXISTS "${_hegel_local}")
        message(FATAL_ERROR "HEGEL_LIBHEGEL_LIBRARY does not exist: ${_hegel_local}")
    endif()
    message(STATUS "libhegel: using ${_hegel_local} (HEGEL_LIBHEGEL_LIBRARY)")
else()
    if(_goos STREQUAL "darwin" AND _goarch STREQUAL "amd64")
        message(FATAL_ERROR
            "libhegel: no prebuilt release for darwin/amd64. Build hegel-c from "
            "source and pass -DHEGEL_LIBHEGEL_LIBRARY=/path/to/libhegel_c.dylib.")
    endif()

    set(_asset "libhegel-${_goos}-${_goarch}.${_ext}")
    set(_stamp "${_hegel_dl_dir}/.stamp-${HEGEL_LIBHEGEL_VERSION}")

    if(NOT EXISTS "${_stamp}")
        file(MAKE_DIRECTORY "${_hegel_dl_dir}")

        # Fetch the SHA-256 sidecar ("<hex>  <filename>") and extract the hash.
        set(_sidecar "${_hegel_dl_dir}/${_asset}.sha256")
        message(STATUS "libhegel: downloading ${_asset}.sha256")
        file(DOWNLOAD "${HEGEL_LIBHEGEL_BASE_URL}/${_asset}.sha256" "${_sidecar}"
            STATUS _sha_status TLS_VERIFY ON)
        list(GET _sha_status 0 _sha_code)
        if(NOT _sha_code EQUAL 0)
            list(GET _sha_status 1 _sha_msg)
            message(FATAL_ERROR
                "libhegel: failed to download ${_asset}.sha256: ${_sha_msg}")
        endif()
        file(READ "${_sidecar}" _sha_line)
        string(REGEX MATCH "[0-9a-fA-F]+" _expected_sha "${_sha_line}")
        if(NOT _expected_sha)
            message(FATAL_ERROR "libhegel: could not parse SHA-256 from ${_sidecar}")
        endif()

        # Download the library, verifying the hash as part of the transfer.
        message(STATUS "libhegel: downloading ${_asset}")
        file(DOWNLOAD "${HEGEL_LIBHEGEL_BASE_URL}/${_asset}" "${_hegel_local}"
            EXPECTED_HASH "SHA256=${_expected_sha}"
            TLS_VERIFY ON STATUS _lib_status)
        list(GET _lib_status 0 _lib_code)
        if(NOT _lib_code EQUAL 0)
            list(GET _lib_status 1 _lib_msg)
            file(REMOVE "${_hegel_local}")
            message(FATAL_ERROR "libhegel: failed to download ${_asset}: ${_lib_msg}")
        endif()

        # The released macOS dylib's install name is the absolute CI build path,
        # which does not exist on this machine. Rewrite it to an @rpath name so
        # executables that link it resolve via their RPATH (CMake adds the
        # download directory automatically in the build tree).
        if(_goos STREQUAL "darwin")
            find_program(HEGEL_INSTALL_NAME_TOOL install_name_tool)
            if(NOT HEGEL_INSTALL_NAME_TOOL)
                message(FATAL_ERROR "libhegel: install_name_tool not found (need Xcode CLT)")
            endif()
            execute_process(
                COMMAND "${HEGEL_INSTALL_NAME_TOOL}" -id "@rpath/${_hegel_local_name}" "${_hegel_local}"
                RESULT_VARIABLE _int_rc)
            if(NOT _int_rc EQUAL 0)
                message(FATAL_ERROR "libhegel: install_name_tool -id failed")
            endif()
        endif()

        file(WRITE "${_stamp}" "${_expected_sha}\n")
        message(STATUS "libhegel: verified ${_asset} (sha256 ${_expected_sha})")
    endif()
endif()

add_library(hegel_libhegel SHARED IMPORTED GLOBAL)
set_target_properties(hegel_libhegel PROPERTIES
    IMPORTED_LOCATION "${_hegel_local}"
    INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${_hegel_header_dir}>")
if(_goos STREQUAL "linux")
    # Our file is already named after the SONAME (libhegel_c.so); avoid a stale
    # SONAME load entry by treating the file path as authoritative.
    set_target_properties(hegel_libhegel PROPERTIES IMPORTED_NO_SONAME TRUE)
endif()
add_library(hegel::libhegel ALIAS hegel_libhegel)

# Absolute path to the resolved library, for install() to ship alongside hegel.
set(HEGEL_LIBHEGEL_RESOLVED "${_hegel_local}" CACHE INTERNAL "Resolved libhegel path")
set(HEGEL_LIBHEGEL_LOCAL_NAME "${_hegel_local_name}" CACHE INTERNAL "libhegel file name")
