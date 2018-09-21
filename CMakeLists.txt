cmake_minimum_required(VERSION 2.8.8)

add_subdirectory(tests/googletest)

enable_testing(TRUE)
set(CTEST_OUTPUT_ON_FAILURE TRUE)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/tests/googletest/googletest/include/gtest)

set(CMAKE_CXX_FLAGS  ${CMAKE_CXX_FLAGS} "-O3 -std=c++11 -Wall -Werror -Wunused -Wextra -Wpedantic -pedantic -Wshadow -Wlogical-op -pedantic-errors -Wold-style-cast")

add_executable(liberal-pool-test tests/liberal-pool-test.cpp)
target_link_libraries(liberal-pool-test gtest)
add_test(liberal-pool-test liberal-pool-test)
