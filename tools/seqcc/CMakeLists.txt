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
)

add_executable(seqcc ${SEQCC_SOURCES})

target_include_directories(seqcc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# When built from reconstructed/CMakeLists.txt, the zhinst_seqc target is
# available and we link it.  Stand-alone builds (which T1 does not yet
# support fully) would need to find it instead.
if(TARGET zhinst_seqc)
    target_link_libraries(seqcc PRIVATE zhinst_seqc)
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

# T7+: install symlinks seqas → seqcc, seqdump → seqcc.
# Held until T7 to keep T1 install footprint minimal.

message(STATUS "seqcc: scaffolding (T1) — pipeline flags arrive in T2+")
