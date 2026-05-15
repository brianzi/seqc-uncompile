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
//! \verifyme Reconstructed from `_seqc_compiler.so` 0x2f8620.  The
//! binary builds the result via `std::ostringstream`; this
//! reconstruction uses direct `std::string::push_back` since only the
//! output bytes are observable at the API boundary.  Diff-test harness
//! `string_spanu8` shape verifies bit-identity.
std::string encode(std::span<const std::uint8_t> data);

#endif  // C++20

}  // namespace zhinst::base64
