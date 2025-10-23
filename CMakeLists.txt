cmake_minimum_required(VERSION 3.10)
project(Pics VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_COLOR_DIAGNOSTICS ON)

include(CTest)
include(cmake/UpdateSubmodules.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release")

# Clangd needs it
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Custom options
option(BUILD_TESTS "Enable building tests" OFF)
option(BUILD_SHARED_LIBS "Build libraries as shared" OFF)

if (BUILD_SHARED_LIBS)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Other Cmake files
include(cmake/UpdateSubmodules.cmake)
# Platform specific
if (MSVC)
    include(cmake/MSVC.cmake)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(cmake/UNIX.cmake)
endif()

# Include directories
include_directories("${CMAKE_SOURCE_DIR}/include")

# Source files
set(LIB_NAME "libpics")
file(GLOB_RECURSE BASE_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp" "{CMAKE_SOURCE_DIR}/src/*.c")
add_library(${LIB_NAME} ${BASE_SRC})
set_target_properties(${LIB_NAME} PROPERTIES PREFIX "") # So the name isn't liblibpics

# Compile definitions
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	message(STATUS "Defining DEBUG macro")
	target_compile_definitions(${LIB_NAME} PUBLIC DEBUG=1)
endif()
target_compile_definitions(${LIB_NAME} PUBLIC "APP_NAME=\"${CMAKE_PROJECT_NAME}\"")
target_compile_definitions(${LIB_NAME} PUBLIC ROOT_DIR="${CMAKE_SOURCE_DIR}")

# Set library directory
set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps")

# Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${LIB_NAME} PUBLIC Threads::Threads)
# target_link_libraries("${LIB_NAME}Tests" Threads::Threads)

# NLOHMANN JSON
set(JSON_DIR "${DEPS_DIR}/json")
add_subdirectory("${JSON_DIR}")
target_link_libraries(${LIB_NAME} PUBLIC nlohmann_json::nlohmann_json)
target_include_directories(${LIB_NAME} PUBLIC "${JSON_DIR}/include")
# target_link_libraries("${LIB_NAME}Tests" nlohmann_json::nlohmann_json)
# target_include_directories("${LIB_NAME}Tests" PUBLIC "${JSON_DIR}/include")

# SPDLOG
set(SPDLOG_DIR "${DEPS_DIR}/spdlog")
add_subdirectory("${SPDLOG_DIR}")
target_link_libraries(${LIB_NAME} PUBLIC spdlog)
target_include_directories(${LIB_NAME} PUBLIC "${SPDLOG_DIR}/include")
# target_link_libraries("${LIB_NAME}Tests" spdlog)
# target_include_directories("${LIB_NAME}Tests" PUBLIC "${SPDLOG_DIR}/include")

# GLFW
set(GLFW_DIR "${DEPS_DIR}/glfw")
add_subdirectory("${GLFW_DIR}")
target_include_directories(${LIB_NAME} PUBLIC "${GLFW_DIR}/include")
target_link_libraries(${LIB_NAME} PUBLIC glfw)
target_compile_definitions(${LIB_NAME} PUBLIC GLFW_INCLUDE_NONE)
# target_link_libraries("${LIB_NAME}Tests" glfw)

# GLAD
set(GLAD_DIR "${DEPS_DIR}/glad")
add_library(glad STATIC "${GLAD_DIR}/src/glad.c")
target_include_directories(glad PUBLIC "${GLAD_DIR}/include")
target_include_directories(${LIB_NAME} PUBLIC "${GLAD_DIR}/include")
target_link_libraries(${LIB_NAME} PUBLIC glad ${CMAKE_DL_LIBS})
# target_link_libraries("${LIB_NAME}Tests" glad ${CMAKE_DL_LIBS})

# FMT
set(FMT_DIR "${DEPS_DIR}/fmt")
add_subdirectory("${FMT_DIR}")
target_link_libraries(${LIB_NAME} PUBLIC fmt)
target_include_directories(${LIB_NAME} PUBLIC "${FMT_DIR}/include")
# target_link_libraries("${LIB_NAME}Tests" fmt)
# target_include_directories("${LIB_NAME}Tests" PUBLIC "${FMT_DIR}/include")

# ImGui
set(IMGUI_DIR "${DEPS_DIR}/imgui")
add_library("imgui"
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    )
target_include_directories(imgui PUBLIC "${IMGUI_DIR}")
target_link_libraries(imgui PUBLIC glfw glad)
target_include_directories(${LIB_NAME} PUBLIC "${IMGUI_DIR}")
target_link_libraries(${LIB_NAME} PUBLIC imgui)
# target_link_libraries("${LIB_NAME}Tests" imgui)

# SQLite3
include(FetchContent)

FetchContent_Declare(
  sqlite3
  URL https://sqlite.org/2025/sqlite-amalgamation-3490100.zip
  URL_HASH SHA256=6cebd1d8403fc58c30e93939b246f3e6e58d0765a5cd50546f16c00fd805d2c3
)

FetchContent_MakeAvailable(sqlite3)

add_library(sqlite3 ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
target_link_libraries(${LIB_NAME} PUBLIC sqlite3)
# target_link_libraries("${PROJECT_NAME}Tests" sqlite3)

# GGML
set(GGML_DIR "${DEPS_DIR}/ggml")
add_subdirectory("${GGML_DIR}")
target_link_libraries(${LIB_NAME} PUBLIC ggml)
target_include_directories(${LIB_NAME} PUBLIC "${GGML_DIR}/include")
# target_link_libraries("${LIB_NAME}Tests" ggml)

# Usearch
set(USEARCH_DIR "${DEPS_DIR}/usearch")
add_subdirectory("${USEARCH_DIR}")
target_link_libraries(${LIB_NAME} PUBLIC usearch)
target_include_directories(${LIB_NAME} PUBLIC "${USEARCH_DIR}/include")
# target_link_libraries("${LIB_NAME}Tests" usearch)

# STB
set(STB_DIR "${DEPS_DIR}/stb")
file(MAKE_DIRECTORY ${STB_DIR})

set(STB_IMAGE_URL "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h")
set(STB_IMAGE_WRITE_URL "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h")
set(STB_IMAGE_RESIZE_URL "https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h")

file(DOWNLOAD ${STB_IMAGE_URL} ${STB_DIR}/stb_image.h)
file(DOWNLOAD ${STB_IMAGE_WRITE_URL} ${STB_DIR}/stb_image_write.h)
file(DOWNLOAD ${STB_IMAGE_RESIZE_URL} ${STB_DIR}/stb_image_resize2.h)

target_include_directories(${LIB_NAME} PUBLIC "${STB_DIR}")
# target_include_directories("${LIB_NAME}Tests" PUBLIC "${STB_DIR}")

# Tests
if (BUILD_TESTS)
include(cmake/Tests.cmake)
endif()

# Main
add_executable(${CMAKE_PROJECT_NAME} "${CMAKE_SOURCE_DIR}/main.cpp")
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${LIB_NAME})