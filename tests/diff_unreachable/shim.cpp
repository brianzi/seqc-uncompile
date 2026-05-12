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
#include <cstdint>
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

// ---------- sret support ----------
//
// The Itanium C++ ABI lowers `std::string f(...)` to
// `void f(std::string* sret, ...)`: the caller owns 24 B of raw
// storage and the callee placement-constructs the return value
// there.  These helpers let Python ctypes drive that pattern:
//
//   slot = diff_unreachable_string_alloc_uninit();   // 24 B raw
//   fn(slot, ...);                                   // sret call
//   bytes = string_data/size(slot);                  // read result
//   diff_unreachable_string_destroy_in_place(slot);  // ~basic_string
//   diff_unreachable_string_free_uninit(slot);       // free raw 24 B
//
// Splitting destruction from deallocation is necessary because the
// callee constructed the string in caller-owned storage; we must
// run its destructor before reclaiming that storage.

void* diff_unreachable_string_alloc_uninit() {
    // operator new for raw 24 B aligned for std::string.
    return ::operator new(sizeof(std::string),
                          std::align_val_t{alignof(std::string)});
}

void diff_unreachable_string_free_uninit(void* p) {
    ::operator delete(p, std::align_val_t{alignof(std::string)});
}

void diff_unreachable_string_destroy_in_place(void* p) {
    static_cast<std::string*>(p)->~basic_string();
}

// Placement-construct a libc++ std::string into caller-owned raw
// storage (typically obtained from diff_unreachable_string_alloc_uninit).
// Used by the harness to materialise a by-value `string` argument
// that the callee will destroy.
void diff_unreachable_string_place_construct(
        void* slot, const char* data, std::size_t n) {
    new (slot) std::string(data, n);
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

// ---------- exception-safe trampoline ----------
//
// Several of the in-scope D16 symbols throw on bad input
// (`generateSfc` raises for non-MF families, `generateMfSfc` raises
// for unrecognised MF subtypes, etc.).  Letting the throw escape
// across the ctypes boundary terminates the Python process via
// libc++abi's terminate handler.  This trampoline catches anything
// the target throws, copies the exception's `what()` text into the
// caller's optional out-buffer, and returns a status code:
//
//   0 = success, *out_value populated, *what untouched
//   1 = std::exception thrown; *what populated, *out_value untouched
//   2 = unknown exception thrown; *what set to "<unknown>"
//
// The caller passes a function-pointer cast to the canonical
// "two-string-args returning uint64_t" shape, which currently
// covers `generateSfc`.  Adding more shapes is a matter of one
// extra trampoline each; that's intentional, since each shape has
// a different ABI.

#include <exception>
#include <cstring>

extern "C" {

using Fn_pod_u64_cref2 = std::uint64_t (*)(const std::string*,
                                           const std::string*);

int diff_unreachable_try_pod_u64_cref2(
        Fn_pod_u64_cref2 fn,
        const std::string* a,
        const std::string* b,
        std::uint64_t* out_value,
        char* what_buf,
        std::size_t what_cap) {
    try {
        std::uint64_t v = fn(a, b);
        if (out_value) *out_value = v;
        return 0;
    } catch (const std::exception& e) {
        if (what_buf && what_cap > 0) {
            std::strncpy(what_buf, e.what(), what_cap - 1);
            what_buf[what_cap - 1] = '\0';
        }
        return 1;
    } catch (...) {
        if (what_buf && what_cap > 0) {
            std::strncpy(what_buf, "<unknown>", what_cap - 1);
            what_buf[what_cap - 1] = '\0';
        }
        return 2;
    }
}

}  // extern "C"
