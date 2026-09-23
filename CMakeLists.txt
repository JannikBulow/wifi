cmake_minimum_required(VERSION 3.29)
project(wifi LANGUAGES C)

set(SOURCES
    src/main.c
)

set(HEADERS
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

add_executable(wifi ${SOURCES} ${HEADERS})

target_include_directories(wifi PUBLIC include)

set_property(TARGET wifi PROPERTY C_STANDARD 17)