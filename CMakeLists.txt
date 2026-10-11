cmake_minimum_required(VERSION 3.30)
project(vietnamesekey VERSION 3.1.0 LANGUAGES CXX)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 26)
endif()
if(CMAKE_CXX_STANDARD LESS 26)
    message(FATAL_ERROR "VietnameseKey requires C++26 or newer")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Utils REQUIRED)

# OFF by default: -march=native bakes the *build* machine's ISA into the .so, so a plain
# `cmake -S . -B build` produces a binary that SIGILLs on older CPUs. Packaging (PKGBUILD,
# debian/rules, CI) already passes OFF; local dev scripts opt in explicitly.
option(VKEY_MARCH_NATIVE "Build with -march=native for the local machine" OFF)

add_library(vietnamesekey MODULE
    src/shadow_engine.cpp
    src/vietnamesekey_fcitx.cpp
)

target_compile_features(vietnamesekey PRIVATE cxx_std_26)
target_include_directories(vietnamesekey PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Fcitx5Core_INCLUDE_DIRS}
)

target_compile_options(vietnamesekey PRIVATE
    -fno-exceptions
    -Wall
    -Wextra
)

if(VKEY_MARCH_NATIVE)
    target_compile_options(vietnamesekey PRIVATE -march=native)
endif()

option(VKEY_ENABLE_IPO "Enable supported interprocedural optimization in Release" ON)
if(VKEY_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT vkey_ipo_supported OUTPUT vkey_ipo_error)
    if(vkey_ipo_supported)
        set_property(TARGET vietnamesekey PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
        message(STATUS "IPO unavailable: ${vkey_ipo_error}")
    endif()
endif()
target_link_libraries(vietnamesekey PRIVATE Fcitx5::Core Fcitx5::Utils)

set_target_properties(vietnamesekey PROPERTIES PREFIX "")

# ── Tests (ctest) ────────────────────────────────────────────────────────────
option(VKEY_BUILD_TESTS "Build ShadowEngine test suite" ON)
if(VKEY_BUILD_TESTS)
    enable_testing()
    add_executable(vkey_test tests/test_shadow_engine.cpp src/shadow_engine.cpp)
    target_include_directories(vkey_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(vkey_test PRIVATE cxx_std_26)
    target_compile_options(vkey_test PRIVATE -Wall -Wextra)
    add_test(NAME shadow_engine COMMAND vkey_test)

    add_executable(vkey_boundary_test tests/test_fcitx_boundary.cpp src/shadow_engine.cpp)
    target_compile_features(vkey_boundary_test PRIVATE cxx_std_26)
    target_include_directories(vkey_boundary_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(vkey_boundary_test PRIVATE Fcitx5::Core Fcitx5::Utils)
    target_compile_options(vkey_boundary_test PRIVATE -Wall -Wextra)
    add_test(NAME fcitx_boundary COMMAND vkey_boundary_test)
    set_tests_properties(fcitx_boundary PROPERTIES TIMEOUT 10)

    add_executable(vkey_bench tests/bench_shadow_engine.cpp src/shadow_engine.cpp)
    target_include_directories(vkey_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(vkey_bench PRIVATE cxx_std_26)
    target_compile_options(vkey_bench PRIVATE -Wall -Wextra)
endif()

# Tự động cài đặt file thư viện .so
install(TARGETS vietnamesekey DESTINATION "${FCITX_INSTALL_ADDONDIR}")

# Tự động cài đặt các file cấu hình .conf vào đúng thư mục dữ liệu hệ thống của Fcitx5
install(FILES addon/vietnamesekey.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
install(FILES
    inputmethod/vietnamesekey-telex.conf
    inputmethod/vietnamesekey-raw.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
)

# Cài đặt logo vào system icon theme
install(FILES assets/logo.png DESTINATION "share/icons/hicolor/256x256/apps" RENAME "vietnamesekey.png")
