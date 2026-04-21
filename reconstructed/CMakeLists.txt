cmake_minimum_required(VERSION 3.16)
project(zhinst_seqc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# --- Sources ---
file(GLOB ZHINST_SEQC_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(zhinst_seqc STATIC ${ZHINST_SEQC_SOURCES})

target_include_directories(zhinst_seqc PUBLIC include)

# --- Boost ---
find_package(Boost REQUIRED COMPONENTS filesystem json)
target_link_libraries(zhinst_seqc PUBLIC Boost::filesystem Boost::json)

# --- ELFIO ---
# Try CMake config first (conan, vcpkg, system install with cmake files).
# Fall back to searching for the header directly.
find_package(elfio QUIET CONFIG)
if(elfio_FOUND)
    target_link_libraries(zhinst_seqc PUBLIC elfio::elfio)
else()
    find_path(ELFIO_INCLUDE_DIR elfio/elfio.hpp
        HINTS /usr/include /usr/local/include
    )
    if(ELFIO_INCLUDE_DIR)
        message(STATUS "Found ELFIO headers at ${ELFIO_INCLUDE_DIR}")
        target_include_directories(zhinst_seqc SYSTEM PUBLIC ${ELFIO_INCLUDE_DIR})
    else()
        message(FATAL_ERROR
            "Could not find ELFIO. Install it or set ELFIO_INCLUDE_DIR.")
    endif()
endif()
