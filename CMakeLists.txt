cmake_minimum_required(VERSION 3.12)
project(vietnamesekey VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Utils REQUIRED)

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
    -march=native
    -flto
    -fno-exceptions
    -Wall
    -Wextra
)

target_link_options(vietnamesekey PRIVATE -flto)
target_link_libraries(vietnamesekey PRIVATE Fcitx5::Core Fcitx5::Utils)

set_target_properties(vietnamesekey PROPERTIES PREFIX "")

# Tự động cài đặt file thư viện .so
install(TARGETS vietnamesekey DESTINATION "${FCITX_INSTALL_ADDONDIR}")

# Tự động cài đặt các file cấu hình .conf vào đúng thư mục dữ liệu hệ thống của Fcitx5
install(FILES addon/vietnamesekey.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
install(FILES 
    inputmethod/vietnamesekey-telex.conf 
    inputmethod/vietnamesekey-vni.conf 
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
)
