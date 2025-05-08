cmake_minimum_required(VERSION 3.15)
project(Pics VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_COLOR_DIAGNOSTICS ON)

include(CTest)
include(cmake/UpdateSubmodules.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release")

# Debug/Release specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Configuring Debug build")
  if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Wall /WX-")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wall -Wno-long-long -pedantic")
  endif()
else()
  message(STATUS "Configuring Release build")
  if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Wall /WX-")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Wall -Wno-long-long -pedantic")
  endif()
  if(MSVC)
    set(HOME_DIR "$ENV{USERPROFILE}")
  else()
    set(HOME_DIR "$ENV{HOME}")
  endif()
endif()

# Include directories
include_directories("${CMAKE_SOURCE_DIR}/include")

# Source files
file(GLOB_RECURSE ROOT_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
add_executable(${CMAKE_PROJECT_NAME} ${ROOT_SRC})

# Set library directory
set(LIB_DIR "${CMAKE_SOURCE_DIR}/deps")

# GTest
set(GTEST_DIR "${LIB_DIR}/googletest")
enable_testing()

add_subdirectory("${GTEST_DIR}")
file(GLOB_RECURSE TEST_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/test/*.cpp")

file(GLOB_RECURSE MAIN_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/Pics/*.cpp")
add_executable("${CMAKE_PROJECT_NAME}Tests" ${TEST_SRC} ${MAIN_SRC})

target_link_libraries("${CMAKE_PROJECT_NAME}Tests" gtest gtest_main)
add_test(NAME "${CMAKE_PROJECT_NAME}Tests" COMMAND "${CMAKE_PROJECT_NAME}Tests")

message(STATUS "Test source files: ${TEST_SRC}")
message(STATUS "Creating test executable: ${CMAKE_PROJECT_NAME}Tests")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE DEBUG=1)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_definitions("${CMAKE_PROJECT_NAME}Tests" PRIVATE ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
else()
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ROOT_DIR="${HOME_DIR}/.config/${CMAKE_PROJECT_NAME}")
endif()

# Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Threads::Threads)
target_link_libraries("${CMAKE_PROJECT_NAME}Tests" Threads::Threads)

# NLOHMANN JSON
set(JSON_DIR "${LIB_DIR}/json")
add_subdirectory("${JSON_DIR}")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries("${CMAKE_PROJECT_NAME}Tests" nlohmann_json::nlohmann_json)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${JSON_DIR}/include")
target_include_directories("${CMAKE_PROJECT_NAME}Tests" PRIVATE "${JSON_DIR}/include")

# SPDLOG
set(SPDLOG_DIR "${LIB_DIR}/spdlog")
add_subdirectory("${SPDLOG_DIR}")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE spdlog)
target_link_libraries("${CMAKE_PROJECT_NAME}Tests" spdlog)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${SPDLOG_DIR}/include")
target_include_directories("${CMAKE_PROJECT_NAME}Tests" PRIVATE "${SPDLOG_DIR}/include")

# GLFW
set(GLFW_DIR "${LIB_DIR}/glfw")
add_subdirectory("${GLFW_DIR}")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE glfw)
target_link_libraries("${CMAKE_PROJECT_NAME}Tests" glfw)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${GLFW_DIR}/include")

# GLAD
set(GLAD_DIR "${LIB_DIR}/glad")
add_library(glad STATIC "${GLAD_DIR}/src/glad.c")
target_include_directories(glad PUBLIC "${GLAD_DIR}/include")
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${GLAD_DIR}/include")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE glad ${CMAKE_DL_LIBS})
target_link_libraries("${CMAKE_PROJECT_NAME}Tests" glad ${CMAKE_DL_LIBS})

# FMT
set(FMT_DIR "${LIB_DIR}/fmt")
add_subdirectory("${FMT_DIR}")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE fmt)
target_link_libraries("${CMAKE_PROJECT_NAME}Tests" fmt)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${FMT_DIR}/include")
target_include_directories("${CMAKE_PROJECT_NAME}Tests" PRIVATE "${FMT_DIR}/include")

# ImGui
set(IMGUI_DIR "${LIB_DIR}/imgui")
add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")

target_include_directories(imgui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
target_link_libraries(imgui PUBLIC glfw glad)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE imgui)
target_link_libraries("${CMAKE_PROJECT_NAME}Tests" imgui)

# SQLite3
include(FetchContent)

FetchContent_Declare(
  sqlite3
  URL https://sqlite.org/2025/sqlite-amalgamation-3490100.zip
  URL_HASH SHA256=6cebd1d8403fc58c30e93939b246f3e6e58d0765a5cd50546f16c00fd805d2c3
)

FetchContent_MakeAvailable(sqlite3)

add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE sqlite3)
target_link_libraries("${PROJECT_NAME}Tests" sqlite3)

# GGML
set(GGML_DIR "${LIB_DIR}/ggml")
add_subdirectory("${GGML_DIR}")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ggml)
target_link_libraries("${CMAKE_PROJECT_NAME}Tests" ggml)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${GGML_DIR}/include")

# Usearch
set(USEARCH_DIR "${LIB_DIR}/usearch")
add_subdirectory("${USEARCH_DIR}")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE usearch)
target_link_libraries("${CMAKE_PROJECT_NAME}Tests" usearch)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${USEARCH_DIR}/include")

# STB
set(STB_DIR "${LIB_DIR}/stb")
file(MAKE_DIRECTORY ${STB_DIR})

set(STB_IMAGE_URL "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h")
set(STB_IMAGE_WRITE_URL "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h")
set(STB_IMAGE_RESIZE_URL "https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h")

file(DOWNLOAD ${STB_IMAGE_URL} ${STB_DIR}/stb_image.h)
file(DOWNLOAD ${STB_IMAGE_WRITE_URL} ${STB_DIR}/stb_image_write.h)
file(DOWNLOAD ${STB_IMAGE_RESIZE_URL} ${STB_DIR}/stb_image_resize2.h)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${STB_DIR}")
target_include_directories("${CMAKE_PROJECT_NAME}Tests" PRIVATE "${STB_DIR}")