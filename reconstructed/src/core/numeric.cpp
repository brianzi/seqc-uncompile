// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Namespace: zhinst (top-level numeric helpers)
//
//   almostEqual(double, double)                        — 0x2ec070
//   toRawByteArray(string_view, span<uint8_t>)         — 0x2f27c0   (C++20 only)
//   fromRawByteArray(span<uint8_t const>)              — 0x2f2830   (C++20 only)
// ============================================================================

#include "zhinst/core/numeric.hpp"

#include <boost/math/special_functions/relative_difference.hpp>

#if __cplusplus >= 202002L
#  include <cstring>
#endif

namespace zhinst {

// ---------------------------------------------------------------------------
// almostEqual(a, b)   — 0x2ec070
//
// Direct lowering of `boost::math::epsilon_difference(a, b) <= 1.0`.
// The disassembly is a hand-vectorised SSE2 sequence emitted by
// boost's headers-only specialisation (unpcklpd/maxpd/subpd/divpd).
// Constants matched against .rodata:
//   0x8fc4d0 = abs-mask {0x7FFF...F, 0x7FFF...F}     (clear sign bit ×2)
//   0x956358 = DBL_MAX  (0x7FEFFFFFFFFFFFFF)         max-value clamp
//   0x956360 = DBL_MIN  (0x7CAFFFFFFFFFFFFF — boost's "min_value")
//   0x956368 = 1/eps    (0x4330000000000000 = 2^52)  divide-by-eps
//   0x956030 = 1.0                                   threshold
// ---------------------------------------------------------------------------
bool almostEqual(double a, double b) {  // 0x2ec070
    return boost::math::epsilon_difference(a, b) <= 1.0;
}

#if __cplusplus >= 202002L

// ---------------------------------------------------------------------------
// toRawByteArray(src, dst)   — 0x2f27c0
//
// Copy a string_view into a raw byte buffer with NUL terminator.
// Returns true iff `src.size()` fit entirely (no truncation).
// Special cases: dst.size()==0 → false (no write); dst.size()==1 →
// write lone NUL, return false.
// ---------------------------------------------------------------------------
bool toRawByteArray(std::string_view src, std::span<std::uint8_t> dst) {  // 0x2f27c0
    const std::size_t dstSize = dst.size();
    if (dstSize == 0) {
        return false;
    }
    if (dstSize == 1) {
        dst[0] = 0;
        return false;
    }
    const std::size_t cap = dstSize - 1;
    const std::size_t n = (src.size() < cap) ? src.size() : cap;
    if (n != 0) {
        std::memmove(dst.data(), src.data(), n);
    }
    dst[n] = 0;
    return src.size() <= cap;
}

// ---------------------------------------------------------------------------
// fromRawByteArray(buf)   — 0x2f2830
//
// Find the first NUL byte in `buf`; return a string_view spanning
// from `buf.data()` to that NUL (exclusive).  If no NUL is present
// the view spans the entire buffer.
// ---------------------------------------------------------------------------
std::string_view fromRawByteArray(std::span<const std::uint8_t> buf) {  // 0x2f2830
    const std::size_t n = buf.size();
    if (n == 0) {
        return std::string_view(reinterpret_cast<const char*>(buf.data()), 0);
    }
    std::size_t i = 0;
    for (; i < n; ++i) {
        if (buf[i] == 0) {
            break;
        }
    }
    return std::string_view(reinterpret_cast<const char*>(buf.data()), i);
}

#endif  // C++20

}  // namespace zhinst
