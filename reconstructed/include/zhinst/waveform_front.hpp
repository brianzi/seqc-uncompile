// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveformFront — waveform metadata
// ============================================================================
#pragma once

#include <cstdint>
#include <string>

namespace zhinst {

// WaveformFront holds metadata about a single waveform.
// AsmCommands accesses it at many offsets during asmPlay/asmTable/genPlayConfig.
//
// Field layout (from offset accesses in disassembly):
//   +0x48  bool     prefetched / used  — set by asmPrefetch, asmPlay
//   +0x68  uint32_t playWord           — packed PlayConfig, written by asmPlay
//   +0x6C  int32_t  playIndex          — if < 0, asmPlay computes playWord
//   +0xB0  uint8_t* markerData         — start of marker byte array
//   +0xB8  uint8_t* markerDataEnd      — end of marker byte array
//   +0xC8  uint16_t sampleWidth        — bits per sample (determines channelMask)
//
// The name of the waveform is accessed by dereferencing the object
// (likely operator* or a getName() method returning const std::string&).
//
// TODO(Phase 2d): full symbol enumeration and reconstruction.
struct WaveformFront {
    // Padding to reach known offsets
    char _pad0[0x48];

    bool used;                    // +0x48 — set true by asmPlay/asmPrefetch

    char _pad1[0x68 - 0x48 - 1];

    uint32_t playWord;            // +0x68 — packed play configuration
    int32_t  playIndex;           // +0x6C — index, or -1 to trigger computation

    char _pad2[0xB0 - 0x6C - 4];

    uint8_t* markerData;          // +0xB0
    uint8_t* markerDataEnd;       // +0xB8

    char _pad3[0xC8 - 0xB8 - 8];

    uint16_t sampleWidth;         // +0xC8

    // Name access — exact mechanism TBD
    // const std::string& getName() const;
};

} // namespace zhinst
