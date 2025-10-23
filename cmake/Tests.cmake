include(CTest)

set(GTEST_DIR "${DEPS_DIR}/googletest")
enable_testing()

add_subdirectory("${GTEST_DIR}")

file(GLOB_RECURSE TEST_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp" "${CMAKE_SOURCE_DIR}/tests/*.c")
add_executable(${CMAKE_PROJECT_NAME}Tests ${TEST_SRC})

target_link_libraries(${CMAKE_PROJECT_NAME}Tests PRIVATE gtest gtest_main ${LIB_NAME})
add_test(NAME "${CMAKE_PROJECT_NAME}Tests" COMMAND ${CMAKE_PROJECT_NAME}Tests)

message(STATUS "Test source files: ${TEST_SRC}")
message(STATUS "Creating test executable: ${CMAKE_PROJECT_NAME}Tests")