message("UNIX like (just no MSVC) build")

# Debug/Release specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wno-long-long -pedantic -fsanitize=address")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Wall -Wno-long-long -pedantic")
endif()

set(HOME_DIR "$ENV{HOME}")

# # This might be needed for imgui
# find_package(X11 REQUIRED)