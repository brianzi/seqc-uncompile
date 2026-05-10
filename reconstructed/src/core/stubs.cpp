// ============================================================================
// Stub definitions for symbols that are referenced by the reconstructed code
// but whose implementations were not yet reconstructed or are trivial.
// These stubs allow the _seqc_compiler.so module to load and run basic
// compilation tests.
//
// Each stub is annotated with its binary address for future reconstruction.
// ============================================================================

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

namespace zhinst {

// 0x2ce770 — checks /proc/device-tree/model for "Zurich Instruments MF"
// Always false on PC.
//! \brief Reports whether the compiler is hosted on a Zurich
//!        Instruments MF-class device (an instrument running the
//!        LabOne stack on its own embedded SoC) rather than on a
//!        regular workstation.
//!
//! \details The host check inspects `/proc/device-tree/model` for the
//! `"Zurich Instruments MF"` signature.  On any normal PC the path is
//! either missing or carries a different model string, so the function
//! returns `false`; on an MF device it returns `true`, which gates
//! behaviour such as on-device wave-path resolution.
//!
//! \return `true` only when running on an MF instrument; `false`
//!         elsewhere (always `false` in the current reconstruction
//!         stub).
//!
//! \verifyme — the reconstruction returns a constant `false`; the
//! binary performs the device-tree check at runtime.
bool runningOnMfDevice() {
    return false;
}

// Waveform sample conversion utilities (used by ELF writer / waveform code).
// These convert between AWG hardware sample format and double/marker.

// AWG 14-bit + 2 marker format:
//   bits [15:2] = signed 14-bit sample, bits [1:0] = 2 marker bits
namespace util { namespace wave {

// 0x2996d0 — convert 14+2 AWG sample to double [-1.0, 1.0)
// Binary: AND 0xFFFFFFFC (clear markers), movswl (sign-extend), / 32767.0
double awg2double(uint16_t sample) {
    int16_t cleared = static_cast<int16_t>(sample & 0xFFFC);
    return static_cast<double>(cleared) / 32767.0;
}

// 0x2996f0 — extract marker bits (bottom 2) from AWG sample
// Binary: AND 0x03
uint16_t awg2marker(uint16_t sample) {
    return sample & 0x3;
}

// 0x299740 — convert 32-bit sample (right-shift 2, sign-extend 16-bit) to double
// Binary: shr 2, movswl, / 32767.0
double awg2double16(uint32_t sample) {
    int16_t s16 = static_cast<int16_t>(static_cast<uint16_t>(sample >> 2));
    return static_cast<double>(s16) / 32767.0;
}

// 0x299d60 — format SHA-1 digest words as hex string
// Binary uses ostringstream with hex, setfill('0'), setw(8) per word.
std::string hash2str(const std::vector<uint32_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto w : data) {
        oss << std::setw(8) << w;
    }
    return oss.str();
}

}} // namespace util::wave

// Top-level aliases (the binary exports both namespaced and non-namespaced)
//! \brief Convert a packed 14-bit-sample-plus-2-marker AWG word into a
//!        normalised `double` in roughly the range `[-1.0, 1.0)`.
//!
//! \details Thin forwarder that delegates to
//! `util::wave::awg2double()` so call sites that pulled in the legacy
//! non-namespaced spelling keep compiling.  The conversion clears the
//! two marker bits with `& 0xFFFC`, sign-extends the remaining 14-bit
//! signed sample to 16 bits and divides by `32767.0`.
//!
//! \param sample  Raw 16-bit AWG word (sample in the high 14 bits,
//!                markers in the low 2 bits).
//! \return Sample amplitude as a `double`.
double awg2double(uint16_t sample) { return util::wave::awg2double(sample); }
//! \brief Extract the two marker bits from a packed AWG sample word.
//!
//! \details Thin forwarder that delegates to
//! `util::wave::awg2marker()`; equivalent to `sample & 0x3`.
//!
//! \param sample  Raw 16-bit AWG word (sample in the high 14 bits,
//!                markers in the low 2 bits).
//! \return Two-bit marker pattern in the low bits of a `uint16_t`.
//!
//! \binarynote The return type is `uint16_t` here whereas the
//! `util::wave` overload returns `uint8_t`; both spellings coexist in
//! the binary's symbol table.
uint16_t awg2marker(uint16_t sample) { return util::wave::awg2marker(sample); }

} // namespace zhinst
