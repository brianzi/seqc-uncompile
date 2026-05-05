// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Namespace: zhinst::util::wave
//
// Free functions:
//   double2awg(double, uint)        — 0x299630
//   double2awg1m(double, uint)      — 0x299680
//   double2awg16(double)            — 0x299700
//   hash(string const&)             — 0x299760
// ============================================================================

#include "zhinst/waveform/rawwave.hpp"

#include <boost/uuid/detail/sha1.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <emmintrin.h>   // SSE2: _mm_max_sd for binary-faithful NaN propagation
#include <fstream>
#include <vector>

namespace zhinst {
namespace util {
namespace wave {

// ---------------------------------------------------------------------------
// double2awg(sample, marker)   — 0x299630
//
// 14-bit signed waveform sample with 2 marker bits in the low 2 bits.
// Constants from rodata:
//   0x956030 = 1.0          (overflow threshold)
//   0x956068 = -1.0         (clamp floor)
//   0x956330 = 8191.0       (full-scale, 2^13 - 1)
//
// Algorithm:
//   1. If sample > 1.0: result_value = 8191.0 (saturate to max).
//      Else:           result_value = max(-1.0, sample) * 8191.0.
//   2. result = lround(result_value)
//   3. Pack: (result << 2) | (marker & 0x3)
// ---------------------------------------------------------------------------
uint16_t double2awg(double sample, unsigned int marker) {  // 0x299630
    constexpr double kFullScale = 8191.0;
    double scaled;
    if (sample > 1.0) {
        scaled = kFullScale;
    } else {
        scaled = std::max(-1.0, sample) * kFullScale;
    }
    long rounded = std::lround(scaled);
    return static_cast<uint16_t>((marker & 0x3u) + (rounded * 4));
}

// ---------------------------------------------------------------------------
// double2awg1m(sample, marker)  — 0x299680
//
// 15-bit signed waveform sample with 1 marker bit in the low bit.
// Constants:
//   0x956030 = 1.0
//   0x956068 = -1.0
//   0x956338 = 16383.0       (full-scale, 2^14 - 1)
//
// Pack: (result << 1) | (marker & 0x1)
// ---------------------------------------------------------------------------
uint16_t double2awg1m(double sample, unsigned int marker) {  // 0x299680
    constexpr double kFullScale = 16383.0;
    double scaled;
    if (sample > 1.0) {
        scaled = kFullScale;
    } else {
        scaled = std::max(-1.0, sample) * kFullScale;
    }
    long rounded = std::lround(scaled);
    return static_cast<uint16_t>((marker & 0x1u) + (rounded * 2));
}

// ---------------------------------------------------------------------------
// double2awg16(sample)   — 0x299700
//
// 16-bit signed waveform sample, no marker bits.
// Constants:
//   0x956030 = 1.0
//   0x956068 = -1.0
//   0x956340 = 32767.0       (full-scale, 2^15 - 1)
// ---------------------------------------------------------------------------
uint16_t double2awg16(double sample) {  // 0x299700
    constexpr double kFullScale = 32767.0;
    double scaled;
    if (sample > 1.0) {
        scaled = kFullScale;
    } else {
        // Replicate x86 SSE2 maxsd(dst=-1.0, src=sample) NaN-propagation
        // exactly. The binary uses a single `maxsd` instruction whose
        // semantics are: if either operand is NaN, return the second
        // source (i.e. NaN propagates). No portable C++ construct does
        // this:
        //   - std::max(-1.0, NaN)        -> -1.0  (wrong; first arg)
        //   - std::fmax(-1.0, NaN)       -> -1.0  (wrong; treats NaN as missing)
        //   - (sample >= -1.0) ? ... : -1.0  -> -1.0  (NaN compares false)
        // _mm_max_sd lowers to a single `maxsd` on x86 with GCC/Clang/MSVC,
        // matching the binary's emitted instruction and its NaN behaviour.
        __m128d r = _mm_max_sd(_mm_set_sd(-1.0), _mm_set_sd(sample));
        scaled = _mm_cvtsd_f64(r) * kFullScale;
    }
    long rounded = std::lround(scaled);
    return static_cast<uint16_t>(rounded);
}

// ---------------------------------------------------------------------------
// hash(filePath)   — 0x299760
//
// SHA-1 of file contents. The binary uses boost::uuids::detail::sha1
// (calls process_block @0x29a270 and get_digest @0x29a000 directly).
// Output: vector<unsigned int> of 5 words (160 bits), digest words in
// big-endian-of-bytes order — but the binary then bswaps each word so
// the host-endian uint32 representation matches the textual SHA-1
// digest read MSB-first.
//
// Behaviour on file-open failure: the binary still constructs the
// ifstream object and returns whatever uninitialized digest results
// from zero process_block calls (i.e. the SHA-1 IV). We replicate
// that by NOT short-circuiting on failure — boost's get_digest of a
// fresh sha1 object returns the IV bytes.
//
// We pass the file in 1024-byte chunks via process_bytes; boost's
// internal block-buffer handles the 64-byte block boundary identically
// to the disasm's per-byte append loop.
// ---------------------------------------------------------------------------
std::vector<unsigned int> hash(const std::string& filePath) {  // 0x299760
    boost::uuids::detail::sha1 sha;

    std::ifstream in(filePath, std::ios::binary);
    if (in.is_open()) {
        std::vector<char> buf(1024);
        while (in) {
            in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
            std::streamsize n = in.gcount();
            if (n > 0) {
                sha.process_bytes(buf.data(), static_cast<std::size_t>(n));
            }
        }
        in.close();
    }

    // boost::uuids::detail::sha1::get_digest(unsigned char(&)[20]) is the
    // public API in this boost version. Pack each big-endian 4-byte group
    // into a host uint32 (MSB-first); this matches the binary's bswap-then-
    // store sequence (it stored big-endian-from-memory then bswapped to host
    // order, equivalent to reading 4 bytes MSB-first).
    unsigned char digest_bytes[20] = {};
    sha.get_digest(digest_bytes);

    std::vector<unsigned int> out;
    out.reserve(5);
    for (int i = 0; i < 5; ++i) {
        unsigned int w = (static_cast<unsigned int>(digest_bytes[i * 4 + 0]) << 24) |
                         (static_cast<unsigned int>(digest_bytes[i * 4 + 1]) << 16) |
                         (static_cast<unsigned int>(digest_bytes[i * 4 + 2]) << 8)  |
                         (static_cast<unsigned int>(digest_bytes[i * 4 + 3]));
        out.push_back(w);
    }
    return out;
}

}  // namespace wave
}  // namespace util
}  // namespace zhinst
