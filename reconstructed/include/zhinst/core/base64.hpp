// SPDX-License-Identifier: Proprietary
//
// Standard base64 encoder.  Single leaf utility; not currently called
// from any reconstructed code path — coverage is via the
// `diff_unreachable` harness only.

#pragma once

#include <string>

#if __cplusplus >= 202002L
#include <cstdint>
#include <span>
#endif

namespace zhinst::base64 {

#if __cplusplus >= 202002L

//! Encode raw bytes to a base64 string using the standard alphabet
//! `A–Z a–z 0–9 + /`.  Output is `=`-padded so its length is always a
//! multiple of four.
//!
//! \param data Input bytes.  May be empty (returns `""`).
//! \return Base64 ASCII string.
//!
//! \binarynote The binary at 0x2f8620 builds the output via a
//! `std::ostringstream` accumulator, emitting each base64 character
//! through `__put_character_sequence` and extracting the final result
//! via the stringbuf's `.str()` SSO copy path.  This reconstruction
//! uses direct `std::string::push_back` after a single `reserve()`
//! since only the emitted byte sequence is observable at the API
//! boundary.  The diff-test harness `string_spanu8` shape verifies
//! bit-identity across 23 inputs covering: empty, all three pad
//! classes (`n%3 ∈ {0,1,2}`), RFC 4648 vectors, full-alphabet
//! sample, all-bits-set bytes, the libc++ SSO boundary
//! (22B input → 32B output), and long-string-form payloads.
//! The alphabet table lives at `.rodata` 0x90cf90 in the binary
//! and is mirrored as `kAlphabet[65]` in the reconstruction (the
//! extra trailing NUL is unobservable since indexing is always
//! `[0..63]`).  Padding string `"=="` lives at 0x90cfd1 in the
//! binary; the reconstruction emits the two `'='` characters via
//! separate `push_back` calls.
std::string encode(std::span<const std::uint8_t> data);

#endif  // C++20

}  // namespace zhinst::base64
