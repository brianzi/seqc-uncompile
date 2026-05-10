// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Wavetable-family shared helpers
// ============================================================================
#pragma once

#include <sstream>
#include <string>

namespace zhinst {
namespace detail {

// 0x2a0fd0 — getUniqueName(const string& base, int index, int counter)
//
// In the binary this is `zhinst::(anonymous namespace)::getUniqueName`. The
// original source almost certainly had it as a single anon-namespace helper
// in one TU; here we expose it as an inline detail-namespace helper so the
// 3+ wavetable TUs that need it can share a single ODR-clean definition.
//
// Format (verified by .rodata at 0x90a344 = "__" and 0x90a347 = "_",
// plus operator<< chain at 0x2a104f / 0x2a106e):
//   "__" + base + "_" + index + "_" + counter
//
// Construction is via std::ostringstream + str() (basic_ostringstream ctor
// at 0x2a0ffa, __put_character_sequence at 0x2a100e/0x2a1033/0x2a1047/
// 0x2a1066, basic_ostream::operator<<(int) at 0x2a1052/0x2a1071, then SSO/
// long-string copy into the sret slot starting at 0x2a1076).
//! Build a synthetic `__<base>_<lineNr>_<counter>` waveform name.
//!
//! Used by the wavetable family to mint deterministic identifiers
//! for synthesised waveforms — typically loop-unrolled waveforms,
//! anonymous `playWave` arguments, and filler entries inserted by
//! `assignWaveIndexImplicit`.  The leading double underscore marks
//! the name as compiler-generated so it cannot collide with a
//! user-declared waveform.
//!
//! \param base     User-visible portion of the name (often the
//!                 source-language waveform identifier or `""` for
//!                 fully anonymous waveforms).
//! \param lineNr   SeqC source line number where the synthesis was
//!                 triggered, used to disambiguate distinct call
//!                 sites of the same `base`.
//! \param counter  Monotonic sequence number within a single line,
//!                 used to disambiguate multiple syntheses on the
//!                 same line.
//! \return Newly allocated `std::string` of the form
//!         `__<base>_<lineNr>_<counter>`.
inline std::string getUniqueName(const std::string& base, int lineNr, int counter)
{
    std::ostringstream oss;
    oss << "__" << base << "_" << lineNr << "_" << counter;
    return oss.str();
}

} // namespace detail
} // namespace zhinst
