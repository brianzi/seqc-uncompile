// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableFront — waveform lookup table
// ============================================================================
#pragma once

#include <memory>
#include <optional>
#include <string>

namespace zhinst {

class WaveformFront;

// WavetableFront provides lookup of waveforms by name.
// Called repeatedly in asmPlay:
//   wavetable->getWaveformByName(optional<string>) -> shared_ptr<WaveformFront>
//
// TODO(Phase 3b): full symbol enumeration and reconstruction.
class WavetableFront {
public:
    virtual ~WavetableFront() = default;

    // Lookup a waveform by its name.
    // Returns nullptr-holding shared_ptr if not found.
    virtual std::shared_ptr<WaveformFront> getWaveformByName(
        const std::optional<std::string>& name) const = 0;
};

} // namespace zhinst
