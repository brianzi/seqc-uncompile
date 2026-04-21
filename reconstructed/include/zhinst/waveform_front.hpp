// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveformFront — waveform metadata for the compiler frontend
//
// WaveformFront extends Waveform (base class, 0xD8 bytes).
// Total size: 0xF8 (248 bytes). Confirmed from allocate_shared: alloc 0x110
// minus 0x18 shared_ptr control block header = 0xF8.
//
// Confirmed from:
//   - WaveformFront::WaveformFront(shared_ptr<WaveformFront>, string) at 0x2a2510
//   - WaveformFront::toString() const at 0x2c5120
//   - __shared_ptr_emplace<WaveformFront>::__on_zero_shared() at 0x2a1300
//   - WaveformIR::WaveformIR(shared_ptr<WaveformFront>) at 0x114da0
//   - AsmCommands::genPlayConfig() at 0x2789a0
//   - AsmCommands::asmPrefetch() at 0x278720
//   - AsmCommands::asmPlay() at 0x278b40
// ============================================================================
#pragma once

#include "waveform.hpp"
#include "value.hpp"

#include <memory>
#include <string>
#include <vector>

namespace zhinst {

// ============================================================================
// WaveformFront — extends Waveform, total 0xF8 bytes
//
// Offset  Size  Type              Name          Notes
// ------  ----  ----              ----          -----
// 0x00-0xD7     Waveform          (base)
// 0xD8     4    int               frontField1   init=1 in copy ctor
// 0xDC     1    bool              frontBool1    init=0 in copy ctor
// 0xDD     1    bool              frontBool2    copied from source
// 0xDE     2    (padding)
// 0xE0    24    vector<Value>     values        per-parameter values (each 0x28 bytes)
// 0xF8          END
// ============================================================================
struct WaveformFront : Waveform {
    int frontField1;                     // +0xD8  (init=1)
    bool frontBool1;                     // +0xDC  (init=0)
    bool frontBool2;                     // +0xDD  (copied from source)
    // +0xDE: 2 bytes padding
    std::vector<Value> values;           // +0xE0  (24 bytes, each Value is 0x28)
    // +0xF8: END

    // WaveformFront(shared_ptr<WaveformFront> source, string const& newName) — 0x2a2510
    WaveformFront(std::shared_ptr<WaveformFront> source, std::string const& newName);

    // ~WaveformFront — implicit (at 0x2a1300 via __on_zero_shared)
    ~WaveformFront();

    // toString() const — 0x2c5120
    std::string toString() const;
};

} // namespace zhinst
