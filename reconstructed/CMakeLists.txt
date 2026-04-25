cmake_minimum_required(VERSION 3.16)
project(zhinst_seqc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Flex/Bison for ASM parser ---
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

BISON_TARGET(asm_parser
    src/asm_parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/asm_parser.tab.c
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/asm_parser.tab.h
)

FLEX_TARGET(asm_lexer
    src/asm_lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/asm_lexer.yy.c
)

ADD_FLEX_BISON_DEPENDENCY(asm_lexer asm_parser)

# Force generated .c files to compile as C++ (they #include C++ headers)
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/asm_parser.tab.c
    ${CMAKE_CURRENT_BINARY_DIR}/asm_lexer.yy.c
    PROPERTIES LANGUAGE CXX
)

# --- Sources ---
file(GLOB ZHINST_SEQC_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(zhinst_seqc STATIC
    ${ZHINST_SEQC_SOURCES}
    ${BISON_asm_parser_OUTPUTS}
    ${FLEX_asm_lexer_OUTPUTS}
)

target_include_directories(zhinst_seqc PUBLIC include)

# Generated bison/flex headers (asm_parser.tab.h) need to be findable
target_include_directories(zhinst_seqc PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# --- Boost ---
find_package(Boost REQUIRED COMPONENTS filesystem json)
target_link_libraries(zhinst_seqc PUBLIC Boost::filesystem Boost::json)

# --- pybind11 (header-only, for type-checking the Python binding layer) ---
find_package(pybind11 QUIET CONFIG)
find_package(Python3 QUIET COMPONENTS Development.Module)
if(pybind11_FOUND AND Python3_FOUND)
    message(STATUS "Found pybind11 ${pybind11_VERSION} + Python ${Python3_VERSION}")
    target_link_libraries(zhinst_seqc PUBLIC pybind11::headers)
    target_include_directories(zhinst_seqc PRIVATE ${Python3_INCLUDE_DIRS})
    target_compile_definitions(zhinst_seqc PRIVATE ZHINST_HAS_PYBIND11=1)
else()
    message(STATUS "pybind11 not found — Python binding sources will not type-check")
endif()

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
