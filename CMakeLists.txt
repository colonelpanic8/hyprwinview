cmake_minimum_required(VERSION 3.27)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HYPRWINVIEW_VERSION LIMIT_COUNT 1)

project(hyprwinview
    DESCRIPTION "A window overview plugin for Hyprland"
    VERSION ${HYPRWINVIEW_VERSION}
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "src/*.cpp")

add_library(hyprwinview SHARED ${SRC})
target_compile_definitions(hyprwinview PRIVATE HYPRWINVIEW_VERSION="${HYPRWINVIEW_VERSION}")
target_compile_options(hyprwinview PRIVATE -Wall -Wextra -Wno-missing-field-initializers $<$<CXX_COMPILER_ID:GNU>:-fno-gnu-unique>)

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

pkg_search_module(lua REQUIRED IMPORTED_TARGET lua55 lua5.5 lua-5.5 "lua>=5.5")

target_link_libraries(hyprwinview PRIVATE rt Threads::Threads PkgConfig::deps PkgConfig::lua)

install(TARGETS hyprwinview)
install(FILES
    LICENSE
    licenses/HYPRTASKING-BSD-3-CLAUSE.txt
    DESTINATION share/licenses/hyprwinview
)
