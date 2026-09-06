cmake_minimum_required(VERSION 3.27)

project(hyprwinview
    DESCRIPTION "A window overview plugin for Hyprland"
    VERSION 0.2.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "src/*.cpp")

add_library(hyprwinview SHARED ${SRC})

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprland
    cairo
    librsvg-2.0
    libdrm
    libinput
    libudev
    pangocairo
    pixman-1
    wayland-server
    xkbcommon
)

target_link_libraries(hyprwinview PRIVATE rt Threads::Threads PkgConfig::deps)

install(TARGETS hyprwinview)
install(FILES
    LICENSE
    licenses/HYPRTASKING-BSD-3-CLAUSE.txt
    DESTINATION share/licenses/hyprwinview
)
