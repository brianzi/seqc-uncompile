# tools/seqcc/CMakeLists.txt
#
# Builds the seqcc toolchain driver and its symlinks (seqas, seqdump).
# Pulled in from reconstructed/CMakeLists.txt via add_subdirectory; can
# also be configured stand-alone if zhinst_seqc is available as an
# installed package (not currently the case, but kept self-contained
# for future).
#
# Sub-phase T1: builds an executable that only handles --help / --version.
# Real pipeline wiring lands in T2+.  See tools/seqcc/DESIGN.md.

cmake_minimum_required(VERSION 3.16)

# When configured stand-alone (rare), declare a project; otherwise inherit
# the parent reconstructed/CMakeLists.txt project settings.
if(NOT DEFINED PROJECT_NAME)
    project(seqcc LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Source list — kept hand-maintained (small TU count, GLOB not justified).
set(SEQCC_SOURCES
    src/main.cpp
    src/options.cpp
    src/compile.cpp
    src/elf_reader.cpp
    src/dump.cpp
    src/stage.cpp
    src/optflags.cpp
    src/seqdump.cpp
)

add_executable(seqcc ${SEQCC_SOURCES})

target_include_directories(seqcc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# When built from reconstructed/CMakeLists.txt, the zhinst_seqc target is
# available and we link it.  Stand-alone builds (which T1 does not yet
# support fully) would need to find it instead.
#
# zhinst_seqc's source-compression path (awg_compiler.cpp::compressSourceString)
# calls zlib's deflate*() directly; the static archive does not pull libz in,
# so seqcc must request it explicitly — matching what the _seqc_compiler
# Python module does at its own link step.
if(TARGET zhinst_seqc)
    find_package(ZLIB REQUIRED)
    target_link_libraries(seqcc PRIVATE zhinst_seqc ZLIB::ZLIB)
endif()

# Make warnings strict — this is fresh code, not reconstructed.
target_compile_options(seqcc PRIVATE
    -Wall -Wextra -Wpedantic
)

# CLI11 triggers a few warnings on modern GCC that we don't want to fix
# in vendored upstream code.  Mark the include path SYSTEM so those are
# suppressed.
target_include_directories(seqcc SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# T9: create `seqdump` symlink → seqcc so `argv[0]`-based personality
# dispatch can find the seqdump entry point at the install/build path.
# `seqas` symlink kept on hold until T7 (legacy AWGAssembler pipeline).
add_custom_command(TARGET seqcc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink seqcc
            $<TARGET_FILE_DIR:seqcc>/seqdump
    COMMENT "Linking seqdump → seqcc"
    VERBATIM
)

message(STATUS "seqcc: default compile path (T2) — pipeline-stage flags arrive in T3+")
