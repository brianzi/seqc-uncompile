// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Namespace: zhinst (top-level numeric helpers)
//
// Free functions (no recon callers — covered only by diff_unreachable harness):
//   almostEqual(double, double)                        — 0x2ec070
//   toRawByteArray(string_view, span<uint8_t>)         — 0x2f27c0
//   fromRawByteArray(span<uint8_t const>)              — 0x2f2830
//
// The two raw-byte-array helpers use C++20 `std::span` in the binary's
// signature, so they only declare in C++20 mode.  The main build is
// C++17 so it sees only `almostEqual`; the diff_unreachable harness
// build is C++20 (libcxx-test) and sees all three.  Neither has any
// recon caller, so the C++17 build losing the declarations is harmless.
// ============================================================================

#ifndef ZHINST_CORE_NUMERIC_HPP
#define ZHINST_CORE_NUMERIC_HPP

#include <cstdint>

#if __cplusplus >= 202002L
#  include <span>
#  include <string_view>
#endif

namespace zhinst {

//! Loose floating-point equality test (boost::math::epsilon_difference).
/*!
 * Returns true when \p a and \p b are within one IEEE-754 ULP of each
 * other (i.e. `epsilon_difference(a, b) <= 1.0`).  NaN inputs always
 * compare false.  Opposite-sign infinities compare false; same-sign
 * infinities compare true.  Sign-different finite operands always
 * compare false (except when one of them is exactly +0/-0).
 *
 * \binarynote The binary uses the canonical
 * `boost::math::epsilon_difference(a, b) <= 1.0` formula, lowered by
 * the boost-headers-only specialisation into a hand-vectorised SSE2
 * sequence (`unpcklpd`/`maxpd`/`subpd`/`divpd`).  The recon defers
 * directly to boost so the compiler emits the same lowering.
 */
bool almostEqual(double a, double b);

#if __cplusplus >= 202002L

//! Copy a `string_view` into a fixed-size raw byte buffer with NUL terminator.
/*!
 * Copies the contents of \p src into \p dst, reserving the last slot
 * for a `\0` terminator.  At most `dst.size() - 1` payload bytes are
 * written; the remaining \p src bytes (if any) are silently
 * truncated.
 *
 * \return `true` if the entire \p src bytes fit (i.e. truncation did
 *         not occur), `false` if `dst.size()` was 0/1 or the buffer
 *         was too small to hold all of \p src plus the terminator.
 *
 * \binarynote An empty \p dst (`size==0`) returns false without
 * touching memory.  A 1-byte \p dst writes a lone NUL and returns
 * false (no payload room).  When `src.size() == dst.size() - 1`
 * the result is true (exact fit including terminator).
 */
bool toRawByteArray(std::string_view src, std::span<std::uint8_t> dst);

//! Decode a NUL-terminated byte view into a `string_view`.
/*!
 * Returns a `string_view` over \p buf up to (but not including) the
 * first NUL byte, or over the full \p buf if no NUL is present.
 * The returned view aliases \p buf — callers must outlive the
 * underlying storage.
 */
std::string_view fromRawByteArray(std::span<const std::uint8_t> buf);

#endif  // C++20

}  // namespace zhinst

#endif  // ZHINST_CORE_NUMERIC_HPP
