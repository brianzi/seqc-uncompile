// ============================================================================
// reconstructed/include/zhinst/infra/random.hpp
//
// `zhinst::Random` is a thin typed wrapper around the 313 × `uint64_t`
// state of an `std::mt19937_64` engine.  It exposes a single member
// `seedRandom()` that re-seeds the underlying state from
// `/dev/urandom`, mirroring the binary symbol
// `_ZN6zhinst6Random10seedRandomEv` at `0x16be80`.
//
// The state layout is identical to `std::mt19937_64`'s (libc++ / libstdc++
// agree on the 313 × 8 = 2504-byte size and the
// `state[0..311] = mt`, `state[312] = read-index` convention), so the
// same memory is reused as `GlobalResources::random[313]`.  The user-
// facing `randomSeed` SeqC built-in obtains a `Random*` by
// `reinterpret_cast`-ing `&GlobalResources::random[0]` and invokes
// `seedRandom()` on it; this matches the call site at
// `_seqc_compiler.so` 0x149832.
// ============================================================================
#pragma once

#include <cstdint>

namespace zhinst {

//! \brief Thin typed view onto a 313 × `uint64_t` mt19937_64 engine.
//!
//! `Random` owns no resources of its own beyond the inline state
//! array; constructing one is equivalent to default-constructing
//! `std::mt19937_64` (state initialised by the user via
//! `seedRandom()` or by external means such as
//! `GlobalResources::random[0] = 0x1571`).
class Random {
public:
    //! \brief Re-seed the engine from `/dev/urandom`.
    //! \details Opens `/dev/urandom` via `std::random_device`,
    //!          draws a single `uint64_t` via
    //!          `std::uniform_int_distribution<uint64_t>`, writes
    //!          it into `state_[0]`, then expands the remaining
    //!          312 elements using the libc++ / libstdc++
    //!          mt19937_64 seed-expansion recurrence
    //!          \f$ x_i = (x_{i-1} \oplus (x_{i-1} \gg 62))
    //!                       \cdot \text{0x5851F42D4C957F2D}
    //!                       + i \f$ followed by the customary
    //!          `--x_i` decrement that the libc++ / libstdc++
    //!          implementations apply per element.  Finally
    //!          resets the read-index slot at `state_[312]` to
    //!          zero so the next draw refills the buffer on
    //!          first request.
    //!
    //! Behaviourally indistinguishable from
    //! `std::mt19937_64::seed(seed)` for some `seed` drawn from
    //! `/dev/urandom`.  Output is non-deterministic by design.
    //! \binarynote The call site at
    //! `CustomFunctions::randomSeed` (`0x149832`) passes
    //! `&GlobalResources::random[0]` as `this` via a
    //! `reinterpret_cast`; the class itself is a member-function
    //! receiver only and holds no other data.
    void seedRandom();                                                  // @0x16be80

private:
    uint64_t state_[313];                                               // mt[0..311] + index@312
};

}  // namespace zhinst
