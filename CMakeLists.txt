cmake_minimum_required(VERSION 3.27)

project(hyprwinview
    DESCRIPTION "A window overview plugin for Hyprland"
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "src/*.cpp")

add_library(hyprwinview SHARED ${SRC})

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprland
    cairo
    librsvg-2.0
    libdrm
    pixman-1
    wayland-server
    xkbcommon
)

target_link_libraries(hyprwinview PRIVATE rt PkgConfig::deps)

install(TARGETS hyprwinview)
