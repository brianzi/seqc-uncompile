// SPDX-License-Identifier: Proprietary
//
// Standard base64 encoder.  Reconstructed from
// `_seqc_compiler.so` 0x2f8620.
//
// The binary uses `std::ostringstream` internally to build the output
// before extracting via `.str()` into the sret slot (each character
// emitted through `__put_character_sequence`, the libc++ ostream<<char
// path; the final `.str()` copies the streambuf into the return slot
// via libc++'s small-string-optimization logic).  This reconstruction
// uses direct `std::string::push_back` after a single `reserve()`
// since only the emitted byte sequence is observable at the API
// boundary; the `string_spanu8` harness shape verifies bit-identity
// across 23 inputs (see harness.py BASE64_INPUTS).  Verified
// 2026-05-16 via objdump + harness — see IF-290.

#include "zhinst/core/base64.hpp"

#if __cplusplus >= 202002L

namespace zhinst::base64 {

namespace {

// Standard base64 alphabet (RFC 4648).  Located at .rodata 0x90cf90
// in the original binary.
constexpr char kAlphabet[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

}  // namespace

// 0x2f8620
std::string encode(std::span<const std::uint8_t> data)
{
    std::string out;
    // Output length is ceil(n/3) * 4.  Reserve to avoid reallocation;
    // does not affect the emitted bytes.
    out.reserve(((data.size() + 2) / 3) * 4);

    const std::size_t n = data.size();
    std::size_t i = 0;

    // Process complete 3-byte → 4-char quanta.  The binary's loop
    // counter `rbx` starts at 2 and steps by 3; we use the equivalent
    // `i` indexing the *first* byte of each quantum.
    for (; i + 3 <= n; i += 3) {
        const std::uint8_t b0 = data[i];
        const std::uint8_t b1 = data[i + 1];
        const std::uint8_t b2 = data[i + 2];
        out.push_back(kAlphabet[ b0 >> 2]);
        out.push_back(kAlphabet[((b0 << 4) & 0x30) | (b1 >> 4)]);
        out.push_back(kAlphabet[((b1 << 2) & 0x3c) | (b2 >> 6)]);
        out.push_back(kAlphabet[  b2       & 0x3f]);
    }

    // Tail: 1 or 2 leftover bytes get padded with `=` to a quantum.
    const std::size_t rem = n - i;
    if (rem == 2) {
        const std::uint8_t b0 = data[i];
        const std::uint8_t b1 = data[i + 1];
        out.push_back(kAlphabet[ b0 >> 2]);
        out.push_back(kAlphabet[((b0 << 4) & 0x30) | (b1 >> 4)]);
        out.push_back(kAlphabet[ (b1 << 2) & 0x3c]);
        out.push_back('=');
    } else if (rem == 1) {
        const std::uint8_t b0 = data[i];
        out.push_back(kAlphabet[ b0 >> 2]);
        out.push_back(kAlphabet[(b0 << 4) & 0x30]);
        out.push_back('=');
        out.push_back('=');
    }

    return out;
}

}  // namespace zhinst::base64

#endif  // C++20
