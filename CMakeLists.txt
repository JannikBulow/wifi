cmake_minimum_required(VERSION 3.29)
project(wifi LANGUAGES C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)

set(SOURCES
    src/cli.c
    src/main.c
)

set(HEADERS
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

add_executable(wifi ${SOURCES} ${HEADERS})

target_include_directories(wifi PUBLIC include)

target_include_directories(wifi PRIVATE ${DBUS_INCLUDE_DIRS})
target_link_directories(wifi PRIVATE ${DBUS_LIBRARY_DIRS})
target_link_libraries(wifi PRIVATE ${DBUS_LIBRARIES})

set_property(TARGET wifi PROPERTY C_STANDARD 17)