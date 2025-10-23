message("MSVC build")

# Debug/Release specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /WX")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
endif()

set(HOME_DIR "$ENV{USERPROFILE}")

# --- Why does MSVC have a problem with everything? ---
add_compile_options(/utf-8)
# Turning this flag OFF doesn't compile (idk why)
# if you get some linker error related to CRT, /MDd or /MTd something, turn it ON
option(USE_DYNAMIC_CRT "Use dynamic runtime and dynamic libraries" OFF)
if(USE_DYNAMIC_CRT)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MDd")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MD")
    endif()
    set(gtest_force_shared_crt ON)
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MTd")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MT")
    endif()
    set(gtest_force_shared_crt OFF)
endif()

