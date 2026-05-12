#!/usr/bin/env bash
# tests/diff_unreachable/build.sh — build the libc++ recon test .so.
#
# Idempotent.  Skips work that is already done.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/reconstructed/build-libcxx-test"
PROFILE="${PHASE_E_CONAN_PROFILE:-clang20-libcxx}"

# 1. Conan: install Boost (libc++ flavour).
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
if [[ ! -f conan_toolchain.cmake ]]; then
    echo "[build.sh] running conan install (profile=$PROFILE)..."
    conan install --requires=boost/1.91.0 \
        -pr:h "$PROFILE" -pr:b "$PROFILE" \
        -s build_type=Release \
        -o 'boost/*:shared=False' \
        -o 'boost/*:without_python=True' \
        -o 'boost/*:without_test=True' \
        -o 'boost/*:without_graph=True' \
        -o 'boost/*:without_graph_parallel=True' \
        -o 'boost/*:without_mpi=True' \
        -o 'boost/*:without_wave=True' \
        -o 'boost/*:without_log=True' \
        -o 'boost/*:without_stacktrace=True' \
        -g CMakeToolchain -g CMakeDeps \
        --build=missing -of .
else
    echo "[build.sh] conan toolchain already present, skipping install"
fi

# 2. Stage source dir: symlink the CMakeLists-libcxx-test.txt as
# the canonical CMakeLists.txt (CMake refuses non-default names),
# plus include/ and src/ so the relative paths in the file resolve.
SRC_DIR="$BUILD_DIR/_src"
mkdir -p "$SRC_DIR"
ln -sfn "$REPO_ROOT/reconstructed/CMakeLists-libcxx-test.txt" \
        "$SRC_DIR/CMakeLists.txt"
ln -sfn "$REPO_ROOT/reconstructed/include" "$SRC_DIR/include"
ln -sfn "$REPO_ROOT/reconstructed/src"     "$SRC_DIR/src"

# 3. CMake configure (via the conan toolchain so Boost is found).
if [[ ! -f CMakeCache.txt ]]; then
    echo "[build.sh] configuring cmake..."
    cmake -S "$SRC_DIR" -B . \
          -DCMAKE_TOOLCHAIN_FILE="$BUILD_DIR/conan_toolchain.cmake" \
          -DCMAKE_BUILD_TYPE=Release \
          -DDIFF_UNREACHABLE_SHIM="$REPO_ROOT/tests/diff_unreachable/shim.cpp" \
          -G "Unix Makefiles"
fi

# 4. Build.
cmake --build . -- -j"$(nproc)"

OUT="$BUILD_DIR/libdiagtxt_libcxx.so"
test -f "$OUT" || { echo "[build.sh] FAIL: $OUT not produced" >&2; exit 1; }
echo "[build.sh] OK -> $OUT"
