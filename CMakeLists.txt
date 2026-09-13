cmake_minimum_required(VERSION 3.10)
project(kave VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Werror -std=c11 -D_GNU_SOURCE")
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
include_directories(include)

file(GLOB_RECURSE CORE_SOURCES src/*.c)
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")

add_library(kave_core STATIC ${CORE_SOURCES})
target_link_libraries(kave_core pthread rt)

add_executable(kave-server src/main.c)
target_link_libraries(kave-server kave_core)

enable_testing()
add_subdirectory(tests)
