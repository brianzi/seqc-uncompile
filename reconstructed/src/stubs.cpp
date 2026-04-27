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
double awg2double(uint16_t sample) { return util::wave::awg2double(sample); }
uint16_t awg2marker(uint16_t sample) { return util::wave::awg2marker(sample); }

} // namespace zhinst
