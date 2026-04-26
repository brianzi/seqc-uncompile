// ============================================================================
// Stub definitions for symbols that are referenced by the reconstructed code
// but whose implementations were not yet reconstructed or are trivial.
// These stubs allow the _seqc_compiler.so module to load and run basic
// compilation tests.
//
// Each stub is annotated with its binary address for future reconstruction.
// ============================================================================

#include <cstdint>
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
// Binary addresses from util_wave.cpp analysis.
namespace util { namespace wave {

// 0x2cf960 — convert 16-bit AWG sample to double [-1.0, 1.0)
double awg2double(uint16_t sample) {
    // AWG format: signed 16-bit, range [-32768, 32767] → [-1.0, 1.0)
    return static_cast<double>(static_cast<int16_t>(sample)) / 32768.0;
}

// 0x2cf990 — extract marker bits from 16-bit AWG sample
uint16_t awg2marker(uint16_t sample) {
    // Upper 2 bits are marker bits in some formats
    return (sample >> 14) & 0x3;
}

// 0x2cf9c0 — convert 32-bit sample (16-bit AWG + 16-bit unused) to double
double awg2double16(uint32_t sample) {
    return static_cast<double>(static_cast<int16_t>(sample & 0xFFFF)) / 32768.0;
}

// 0x2cfa00 — hash a vector of uint32_t to a hex string
std::string hash2str(const std::vector<uint32_t>& data) {
    // Simple placeholder — not critical for compilation
    return "0000000000000000";
}

}} // namespace util::wave

// Top-level aliases (the binary exports both namespaced and non-namespaced)
double awg2double(uint16_t sample) { return util::wave::awg2double(sample); }
uint16_t awg2marker(uint16_t sample) { return util::wave::awg2marker(sample); }

} // namespace zhinst
