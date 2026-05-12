// tests/diff_unreachable/shim.cpp
//
// libc++-built C-ABI shim used by tests/diff_unreachable/harness.py.
//
// This TU is compiled INTO `libdiagtxt_libcxx.so` (the libc++ test
// library produced by reconstructed/CMakeLists-libcxx-test.txt).
// It is deliberately located OUTSIDE `reconstructed/src/` so the
// main CMakeLists' `file(GLOB_RECURSE src/*.cpp)` does not pick it
// up — its `static_assert(sizeof(std::string) == 24)` would fail
// under libstdc++ (which is 32 bytes), and that is the entire
// point of the assert.
//
// Purpose: give Python ctypes a way to allocate and inspect
// `std::__1::basic_string<char>` objects with the libc++ ABI used
// by both the original `_seqc_compiler.so` (statically-linked
// libc++) and this libc++ test library.  Pointers returned here
// can be passed to either side; both speak the same ABI.

#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

extern "C" {

// Allocate and value-construct a libc++ std::string from (data, n).
// Caller must release via diff_unreachable_string_free.
void* diff_unreachable_string_make(const char* data, std::size_t n) {
    auto* s = new std::string(data, n);
    return static_cast<void*>(s);
}

// Allocate an empty libc++ std::string with capacity hint.  Useful
// for in-place mutators that may grow their argument; pre-reserving
// avoids hitting the heap during the call (not strictly required —
// the string can grow itself — but documents intent).
void* diff_unreachable_string_make_empty(std::size_t reserve_hint) {
    auto* s = new std::string();
    if (reserve_hint != 0) {
        s->reserve(reserve_hint);
    }
    return static_cast<void*>(s);
}

void diff_unreachable_string_free(void* p) {
    delete static_cast<std::string*>(p);
}

const char* diff_unreachable_string_data(const void* p) {
    return static_cast<const std::string*>(p)->data();
}

std::size_t diff_unreachable_string_size(const void* p) {
    return static_cast<const std::string*>(p)->size();
}

// Compile-time sanity: a libc++ std::string must be 24 bytes (the
// SSO layout the harness assumes elsewhere).  This static_assert
// will fail at build time if the shim is somehow built against a
// non-libc++ runtime.
static_assert(sizeof(std::string) == 24,
              "libc++ std::string is expected to be 24 bytes; "
              "this TU was built against the wrong stdlib.");

}  // extern "C"
