cmake_minimum_required(VERSION 3.12)
project(vietnamesekey VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Utils REQUIRED)

option(VKEY_MARCH_NATIVE "Build with -march=native for the local machine" ON)

add_library(vietnamesekey MODULE
    src/shadow_engine.cpp
    src/vietnamesekey_fcitx.cpp
)

target_include_directories(vietnamesekey PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Fcitx5Core_INCLUDE_DIRS}
)

target_compile_options(vietnamesekey PRIVATE
    -O3
    -flto
    -fno-exceptions
    -Wall
    -Wextra
)

if(VKEY_MARCH_NATIVE)
    target_compile_options(vietnamesekey PRIVATE -march=native)
endif()

target_link_options(vietnamesekey PRIVATE -flto)
target_link_libraries(vietnamesekey PRIVATE Fcitx5::Core Fcitx5::Utils)

set_target_properties(vietnamesekey PROPERTIES PREFIX "")

# ── Tests (ctest) ────────────────────────────────────────────────────────────
option(VKEY_BUILD_TESTS "Build ShadowEngine test suite" ON)
if(VKEY_BUILD_TESTS)
    enable_testing()
    add_executable(vkey_test tests/test_shadow_engine.cpp src/shadow_engine.cpp)
    target_include_directories(vkey_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(vkey_test PRIVATE cxx_std_20)
    target_compile_options(vkey_test PRIVATE -O2 -Wall -Wextra)
    add_test(NAME shadow_engine COMMAND vkey_test)
endif()

# Tự động cài đặt file thư viện .so
install(TARGETS vietnamesekey DESTINATION "${FCITX_INSTALL_ADDONDIR}")

# Tự động cài đặt các file cấu hình .conf vào đúng thư mục dữ liệu hệ thống của Fcitx5
install(FILES addon/vietnamesekey.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
install(FILES 
    inputmethod/vietnamesekey-telex.conf 
    inputmethod/vietnamesekey-vni.conf 
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
)

# Cài đặt logo vào system icon theme
install(FILES assets/logo.png DESTINATION "share/icons/hicolor/256x256/apps" RENAME "vietnamesekey.png")
