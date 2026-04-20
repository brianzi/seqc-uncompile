// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// DeviceConstants — per-device hardware configuration constants
//
// Source path (from debug info): /builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp
//
// POD struct, no vtable. Size: 0x90 (144 bytes, with alignment padding).
// Populated by getDeviceConstants(AwgDeviceType) factory at 0x2cc0c0.
//
// Contains four RegisterBank sub-structs describing hardware register
// regions (waveform memory, command table, sequencer registers, etc.),
// plus scalar device parameters (sampling rate, memory sizes, etc.).
//
// The nested type DeviceConstants::Register also contains anonymous enums
// used as compile-time constants for sequencer register addresses:
//   {unnamed type#7} = 0x44 (68) — sync register A
//   {unnamed type#8} = 0x45 (69) — sync register B
// These appear in vector<Immediate>::__emplace_back_slow_path instantiations.
// ============================================================================
#pragma once

#include <cstdint>

namespace zhinst {

// Forward declaration
enum AwgDeviceType : int;

// ============================================================================
// DeviceConstants::RegisterBank — 16-byte hardware register region descriptor
//
// Describes a contiguous block of device registers. Four of these are
// embedded in DeviceConstants at offsets 0x08, 0x18, 0x38, 0x48.
//
// Field interpretation (from cross-device value analysis):
//   base   — starting register address (e.g. 0x95b2, 0x110f6, 0x11202)
//   stride — address stride between logical groups (e.g. 0x20000, 0x60000)
//   width  — number of registers per group (e.g. 16, 32, 48, 64)
//   depth  — number of groups or entries (e.g. 8, 16, 48, 4096)
// ============================================================================
struct RegisterBank {
    uint32_t base;      // +0x00  starting register address
    uint32_t stride;    // +0x04  stride between groups
    uint32_t width;     // +0x08  registers per group
    uint32_t depth;     // +0x0C  number of groups/entries
};

static_assert(sizeof(RegisterBank) == 16, "RegisterBank must be 16 bytes");

// ============================================================================
// DeviceConstants — 0x90 bytes (144 bytes)
//
// Offset  Size  Type           Name              Notes
// ------  ----  ----           ----              -----
// 0x00    4     uint32_t       deviceType        AwgDeviceType enum value (1/2/4/8/16/32/64/128/256)
// 0x04    1     bool           hasExtendedReg    true only for HDAWG (type=2)
// 0x05    3     (padding)
// 0x08    16    RegisterBank   waveformReg       primary waveform register bank
// 0x18    16    RegisterBank   commandTableReg   command table register bank
// 0x28    8     uint64_t       numOutputChannels typically 16
// 0x30    8     uint64_t       registerSpace     typically 1024
// 0x38    16    RegisterBank   sequencerReg      sequencer control register bank
// 0x48    16    RegisterBank   auxReg            auxiliary/DIO register bank
// 0x58    8     uint64_t       waveformMemSize   max waveform memory (0x400/0x4000/0x8000)
// 0x60    8     uint64_t       maxSequenceLen    max sequence length (16000)
// 0x68    4     uint32_t       seqClockDivider   0 or 1165 (0x48D)
// 0x6C    4     (padding)
// 0x70    8     double         samplingRate      Hz (1.8e9, 2.0e9, 2.4e9, 12.0e9)
// 0x78    4     uint32_t       numOutputPorts    0, 10, or 12
// 0x7C    4     uint32_t       numAWGCores       0, 3, or 5
// 0x80    1     bool           hasDIO            DIO support flag
// 0x81    3     (padding)
// 0x84    4     uint32_t       numDIOBits        0, 6, or 8
// 0x88    1     bool           hasPrecomp        precompensation support
// 0x89    7     (padding to 0x90)
//
// Consumers:
//   - WaveformFront stores pointer at +0x78, reads +0x40 (sequencerReg.width) → seqRegWidth
//   - WavetableFront reads +0x60 (maxSequenceLen) → this+0x1d8
//   - AWGAssemblerImpl stores pointer at this+0x00
//   - Waveform ctor receives DC fields as individual parameters from caller
// ============================================================================
struct DeviceConstants {

    // Nested Register type (contains anonymous enums for sequencer register addresses)
    struct Register {
        // Anonymous enum constants used as Immediate values in AsmCommands:
        //   {unnamed type#7} = 0x44 (68) — sync register A
        //   {unnamed type#8} = 0x45 (69) — sync register B
        // These are compile-time constants, not stored in the struct.
        enum : uint32_t { SyncRegA = 0x44 };  // {unnamed type#7}
        enum : uint32_t { SyncRegB = 0x45 };  // {unnamed type#8}
    };

    uint32_t       deviceType;        // +0x00  AwgDeviceType value
    bool           hasExtendedReg;    // +0x04  true for HDAWG only
    // +0x05: 3 bytes padding
    char           _pad05[3];

    RegisterBank   waveformReg;       // +0x08  primary waveform register bank
    RegisterBank   commandTableReg;   // +0x18  command table register bank

    uint64_t       numOutputChannels; // +0x28  typically 16
    uint64_t       registerSpace;     // +0x30  typically 1024

    RegisterBank   sequencerReg;      // +0x38  sequencer control register bank
    RegisterBank   auxReg;            // +0x48  auxiliary/DIO register bank

    uint64_t       waveformMemSize;   // +0x58  max waveform memory in samples
    uint64_t       maxSequenceLen;    // +0x60  max sequence length (16000)
    uint32_t       seqClockDivider;   // +0x68  0 or 1165
    // +0x6C: 4 bytes padding
    uint32_t       _pad6C;

    double         samplingRate;      // +0x70  Hz

    uint32_t       numOutputPorts;    // +0x78  0, 10, or 12
    uint32_t       numAWGCores;       // +0x7C  0, 3, or 5
    bool           hasDIO;            // +0x80  DIO support
    // +0x81: 3 bytes padding
    char           _pad81[3];
    uint32_t       numDIOBits;        // +0x84  0, 6, or 8
    bool           hasPrecomp;        // +0x88  precompensation support
    // +0x89: 7 bytes padding to 0x90
    char           _pad89[7];
};

static_assert(sizeof(DeviceConstants) == 0x90, "DeviceConstants must be 0x90 bytes");

// Factory function — populates DeviceConstants for a given device type.
// Throws ZIAWGCompilerException for unsupported device types.
// Binary address: 0x2cc0c0 (size: 0x3c0)
DeviceConstants getDeviceConstants(AwgDeviceType deviceType);

} // namespace zhinst
