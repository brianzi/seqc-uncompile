// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// GlobalResources — constructor, destructor, TLS accessors
// ============================================================================

#include "zhinst/resources.hpp"

namespace zhinst {

// TLS variable definitions
thread_local int32_t  GlobalResources::regNumber;    // TLS+0x48
thread_local int32_t  GlobalResources::labelIndex;   // TLS+0x4c
thread_local uint64_t GlobalResources::random[313];  // TLS+0x50 (MT19937-64: 312 state + 1 index)

// ============================================================================
// GlobalResources::GlobalResources — @0x12a710
//
// 1. Calls Resources("global", weak_ptr<Resources>{})
// 2. Sets vptr to GlobalResources vtable+0x10
// 3. Overwrites parent_ (at +0x18) with the passed shared_ptr<Resources>,
//    releasing the old parent (which was null from base ctor)
// 4. Initializes thread-local variables:
//    - regNumber = 1
//    - labelIndex = 0
//    - random[0] = 0x1571 (seed)
//    - Fills random[1..311] using MT19937-64 initialization:
//        random[i] = (0x5851f42d4c957f2d * (random[i-1] ^ (random[i-1] >> 62))) + i
//      Loop runs for indices 1..311 (i.e., 0x138-1 = 311 iterations,
//      but the loop processes 2 entries per iteration up to rdx == 0x138)
//    - random[312] = 0  (index counter, at offset +0x9C0 from start of array)
// ============================================================================
GlobalResources::GlobalResources(
    std::shared_ptr<Resources> const& parent)  // @0x12a710
    : Resources(std::string("global"), std::weak_ptr<Resources>{})
{
    // 0x12a76f: set vptr to GlobalResources vtable+0x10
    // 0x12a779–0x12a78b: copy shared_ptr<Resources> from parent arg into this->parent_
    parent_ = parent;

    // 0x12a7bb–0x12a7da: TLS init + set regNumber = 1
    regNumber = 1;

    // 0x12a7e0–0x12a7ff: TLS init + set labelIndex = 0
    labelIndex = 0;

    // 0x12a80f–0x12a82e: TLS init for random, seed = 0x1571
    random[0] = 0x1571;

    // 0x12a835–0x12a896: MT19937-64 initialization loop
    // multiplier = 0x5851f42d4c957f2d (stored in rbx)
    // Processes 2 elements per loop iteration:
    //   random[i]   = multiplier * (random[i-1] ^ (random[i-1] >> 62)) + i
    //   random[i+1] = multiplier * (random[i]   ^ (random[i]   >> 62)) + (i+1)
    // Loop: rdx goes from 2 to 0x138 (312) in steps of 2
    constexpr uint64_t mult = 0x5851f42d4c957f2dULL;
    for (int i = 1; i < 312; ++i) {
        uint64_t prev = random[i - 1];
        random[i] = mult * (prev ^ (prev >> 62)) + i;
    }

    // 0x12a898: random[312] = 0  (MT index, at byte offset 0x9C0)
    random[312] = 0;
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
