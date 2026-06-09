cmake_minimum_required(VERSION 3.10)
project(vietnamesekey VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -march=native produces faster code on the build machine but makes the binary
# non-portable to other CPUs. Disable before packaging / distributing.
option(VKEY_MARCH_NATIVE
    "Optimize binary for the local CPU (non-portable; disable before distributing)"
    ON)

# Find Fcitx5 Core and Utils packages
find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Utils REQUIRED)

# Define the shared library target
add_library(vietnamesekey SHARED
    src/shadow_engine.cpp
    src/vietnamesekey_fcitx.cpp
)

target_include_directories(vietnamesekey PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vietnamesekey PRIVATE
    Fcitx5::Core
    Fcitx5::Utils
)

# Performance flags.
#
# NOTE: -fno-exceptions is intentionally OMITTED.
#   This is a shared library loaded into the Fcitx5 process, which itself is
#   compiled WITH exception support. Linking a -fno-exceptions .so that
#   calls into Fcitx5 APIs (which may propagate exceptions through their STL
#   usage, e.g. std::string allocation) causes undefined behaviour when an
#   exception crosses the ABI boundary. Keep exceptions enabled to remain
#   ABI-compatible with the host process.
#
# -fno-rtti is safe: we do not use dynamic_cast or typeid anywhere.
target_compile_options(vietnamesekey PRIVATE
    -O3
    -flto
    -fno-rtti
    -Wall
    -Wextra
    -Wpedantic
)

if(VKEY_MARCH_NATIVE)
    target_compile_options(vietnamesekey PRIVATE -march=native)
    message(STATUS "vietnamesekey: -march=native enabled (local-CPU optimisation, non-portable)")
else()
    message(STATUS "vietnamesekey: -march=native disabled (portable binary)")
endif()

# Apply LTO at link stage too
target_link_options(vietnamesekey PRIVATE
    -flto
)

# Fcitx5 addon shared objects must NOT have the standard "lib" prefix
# (e.g., must be named "vietnamesekey.so", not "libvietnamsekey.so")
set_target_properties(vietnamesekey PROPERTIES
    PREFIX ""
)

# Installation: addon .so, addon metadata, and input method descriptors
install(TARGETS vietnamesekey DESTINATION "${FCITX_INSTALL_ADDONDIR}")
install(FILES addon/vietnamesekey.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
install(FILES inputmethod/vietnamesekey-telex.conf
              inputmethod/vietnamesekey-vni.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")
