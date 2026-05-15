// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// GlobalResources — constructor, destructor, TLS accessors
// ============================================================================

#include "zhinst/runtime/resources.hpp"

#include <array>
#include <cstdint>

namespace zhinst {

namespace {

//! Dynamic initializer for the per-thread MT19937-64 state.
//!
//! Used as the initializer for `GlobalResources::random`, so the
//! compiler emits a TLS init wrapper (`_ZTHN6zhinst15GlobalResources6randomE`)
//! that runs this once per thread on first access.  The binary
//! achieves the same once-per-thread effect with an explicit guard
//! byte at TLS+0xa18 (see `_seqc_compiler.so` @0x1f6090); the
//! compiler-generated wrapper provides the equivalent guard for us.
//!
//! Layout:
//!   [0]      = seed `0x1571` (deterministic — successive compilations
//!              on the same thread see the same PRNG sequence)
//!   [1..311] = MT19937-64 init recurrence
//!                state[i] = mult * (state[i-1] ^ (state[i-1] >> 62)) + i
//!              with mult = 0x5851f42d4c957f2d
//!   [312]    = refill counter `mti`, initialised to 0
std::array<uint64_t, 313> seed_mt19937_64_state() {
    constexpr uint64_t mult = 0x5851f42d4c957f2dULL;
    std::array<uint64_t, 313> s{};
    s[0] = 0x1571;
    for (std::size_t i = 1; i < 312; ++i) {
        const uint64_t prev = s[i - 1];
        s[i] = mult * (prev ^ (prev >> 62)) + i;
    }
    s[312] = 0;
    return s;
}

} // namespace

// TLS variable definitions
thread_local int32_t  GlobalResources::regNumber;    // TLS+0x48
thread_local int32_t  GlobalResources::labelIndex;   // TLS+0x4c

// Dynamic-init initializer: gcc emits `_ZTHN6zhinst15GlobalResources6randomE`
// (TLS init wrapper) and `_ZTWN6zhinst15GlobalResources6randomE` (TLS access
// wrapper) for this declaration, mirroring the binary's symbols at
// 0x1f6090 and 0x1f6180 respectively.  The wrappers seed the array once
// per thread on first access.
thread_local std::array<uint64_t, 313> GlobalResources::random  // TLS+0x50
    = seed_mt19937_64_state();

// ============================================================================
// GlobalResources::GlobalResources — @0x12a710
//
// 1. Calls Resources("global", weak_ptr<Resources>{})
// 2. Sets vptr to GlobalResources vtable+0x10
// 3. Overwrites grandparent_ (at +0x18) with the passed shared_ptr<Resources>,
//    releasing the old grandparent (which was null from base ctor)
// 4. Touches thread-local variables to trigger their TLS init wrappers:
//    - regNumber  = 1   (zero-init, set to 1 here)
//    - labelIndex = 0   (zero-init, redundantly set to 0 here)
//    - random[]   — first access in this thread triggers the once-per-thread
//                   seeding via `_ZTHN6zhinst15GlobalResources6randomE`.
//                   Successive constructions in the same thread do NOT
//                   re-seed (binary's TLS+0xa18 guard byte; recon's
//                   compiler-emitted equivalent).
// ============================================================================
GlobalResources::GlobalResources(
    std::shared_ptr<Resources> const& grandparent)  // @0x12a710
    : Resources(std::string("global"), std::weak_ptr<Resources>{})
{
    // 0x12a76f: set vptr to GlobalResources vtable+0x10
    // 0x12a779–0x12a78b: copy shared_ptr<Resources> from grandparent arg into this->grandparent_
    grandparent_ = grandparent;

    // 0x12a7bb–0x12a7da: TLS init + set regNumber = 1
    regNumber = 1;

    // 0x12a7e0–0x12a7ff: TLS init + set labelIndex = 0
    labelIndex = 0;

    // 0x12a80f–0x12a898: in the binary, this region inlines the
    // MT19937-64 seeding (also gated by the TLS+0xa18 guard byte).
    // In the recon, the equivalent work lives in the TLS dynamic-init
    // wrapper for `random` (auto-emitted from the array's initializer
    // above) and is therefore not repeated here.  Touch the variable
    // so the wrapper runs at least once before the ctor returns,
    // matching the binary's observable behaviour for the first
    // construction on any given thread.
    (void)random[0];
}

// ============================================================================
// GlobalResources::~GlobalResources — D0 @0x12ab40
//
// Note: There is NO D1 (non-deleting) override for GlobalResources!
// The vtable entry for D1 points to Resources::~Resources() (0x12a8f0).
// This means GlobalResources has no extra instance fields to clean up.
//
// D0 simply calls Resources::~Resources() then operator delete(this, 0xd8).
// ============================================================================
GlobalResources::~GlobalResources()  // D0 @0x12ab40
{
    // 0x12ab49: call Resources::~Resources()  (D1 at 0x12a8f0)
    // 0x12ab4e: mov esi, 0xd8; jmp operator delete(void*, size_t)
}

// ============================================================================
// TLS wrapper functions
//
// These are compiler-generated functions that ensure the TLS variables
// are properly initialized before access.
//
// regNumber  wrapper — @0x1f6140
//   Checks TLS init function pointer; if non-null, calls it.
//   Then returns address of TLS regNumber via __tls_get_addr.
//
// labelIndex wrapper — @0x1f6160
//   Same pattern.
//
// random init — @0x1f6090
//   Called once per thread to initialize the random[] TLS array.
//   (Separate from the GlobalResources ctor initialization which
//    re-seeds on each GlobalResources construction.)
//
// random wrapper — @0x1f6180
//   Same pattern as others.
// ============================================================================

} // namespace zhinst
