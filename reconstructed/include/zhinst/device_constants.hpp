// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// DeviceConstants — per-device hardware configuration constants
//
// Source path (from debug info): /builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp
//
// POD struct, no vtable. Size: 0x90 (144 bytes, with alignment padding).
// Populated by getDeviceConstants(AwgDeviceType) factory at 0x2cc0c0.
//
// Layout revised in Phase 7e after cross-referencing all consumer sites
// (Prefetch, WavetableIR, AWGAssemblerImpl, Cache, WaveformFront).
// The original "RegisterBank" sub-struct groupings were incorrect — most
// of the 16-byte groups are unrelated scalar fields that happen to be
// adjacent. Field names now reflect verified semantic usage from binary
// disassembly, not pattern-guessing.
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
// DeviceConstants — 0x90 bytes (144 bytes)
//
// Offset  Size  Type        Name                  Notes / Consumer evidence
// ------  ----  ----        ----                  -------------------------
// 0x00    4     uint32_t    deviceType            AwgDeviceType enum value
// 0x04    1     bool        hasExtendedReg        true only for HDAWG (type=2)
// 0x05    3     (padding)
//
// --- Waveform register region ---
// 0x08    4     uint32_t    waveformRegBase       HW register address (0x95b2/0x110f6/0x11202/0x11203)
// 0x0C    4     uint32_t    waveformMemorySize    minBlockSize (WavetableIR::allocateWaveformsForFifo),
//                                                  cacheSize (Prefetch), Cache ctor arg1.
//                                                  Values: 0x20000–0x100000
// 0x10    4     uint32_t    sampleLength          Cache ctor arg2 (Prefetch ctor at 0x1c58ee).
//                                                  Values: 16, 32, 48, 64
// 0x14    4     uint32_t    waveformAlignment     pageSize (clampToCache), alignment
//                                                  (WavetableIR::allocateWaveforms{,ForFifo}).
//                                                  Values: 16, 48, 4096
//
// --- Cache / command table parameters ---
// 0x18    4     uint32_t    cachePageCount        clampToCache (0x1d6c40) indexes DC+0x18 as
//                                                  uint32_t[cacheType] where cacheType=0→this field.
//                                                  Values: 4, 104, 0x2000, 0x6000
// 0x1C    4     uint32_t    maxBlocks             clampToCache[cacheType=1], also maxBlocks in
//                                                  WavetableIR::allocateWaveformsForFifo (0x29ed68).
//                                                  Always 2 across all devices
// 0x20    4     uint32_t    field_20              Always 16 — no known consumer
// 0x24    4     uint32_t    field_24              Always 64 — no known consumer
//
// 0x28    8     uint64_t    registerDepth         Register index upper bound (getReg at 0x2892c8).
//                                                  Always 16
// 0x30    8     uint64_t    memoryDepth           Memory address upper bound (opcode4 at 0x28a408).
//                                                  Always 1024
//
// --- Sequencer register region ---
// 0x38    4     uint32_t    sequencerRegBase      HW register address (0x115c Cervino, 0x0d05 Hirzel)
// 0x3C    4     uint32_t    field_3C              Always 6 — no known consumer
// 0x40    4     uint32_t    waveformGranularity   Round-up cap in waveform memory calc. Also
//                                                  WaveformFront.seqRegWidth. Values: 16, 32, 96
// 0x44    4     uint32_t    waveformPageSize      Round-up divisor in waveform memory calc.
//                                                  Values: 8, 16, 48
//
// --- Auxiliary parameters ---
// 0x48    4     uint32_t    field_48              Values: 0, 128, 384 — no known consumer by name
// 0x4C    4     uint32_t    field_4C              Values: 16, 32, 96 — no known consumer by name
// 0x50    4     uint32_t    bitsPerSample         Memory bits = pages * channels * bitsPerSample.
//                                                  Always 16
// 0x54    4     uint32_t    field_54              Values: 0, 2 — no known consumer by name
//
// 0x58    8     uint64_t    waveformMemSize       Max waveform memory (0x400/0x4000/0x8000)
// 0x60    8     uint64_t    maxSequenceLen        Max sequence length (always 16000)
// 0x68    4     uint32_t    seqClockDivider       0 (Cervino) or 1165/0x48D (Hirzel)
// 0x6C    4     (padding)
// 0x70    8     double      samplingRate          Hz (1.8e9, 2.0e9, 2.4e9, 12.0e9)
// 0x78    4     uint32_t    numOutputPorts        0, 10, or 12
// 0x7C    4     uint32_t    numAWGCores           0, 3, or 5
// 0x80    1     bool        hasDIO                DIO support flag
// 0x81    3     (padding)
// 0x84    4     uint32_t    numDIOBits            0, 6, or 8
// 0x88    1     bool        hasPrecomp            Precompensation support. Also controls DA
//                                                  code paths in Prefetch (previously mis-named
//                                                  "useDA" — same field, verified at 0x1dc683 etc.)
// 0x89    7     (padding to 0x90)
// ============================================================================
struct DeviceConstants {

    // Nested Register type (contains anonymous enums for sequencer register addresses)
    struct Register {
        enum : uint32_t { SyncRegA = 0x44 };  // {unnamed type#7}
        enum : uint32_t { SyncRegB = 0x45 };  // {unnamed type#8}
    };

    uint32_t       deviceType;            // +0x00  AwgDeviceType value
    bool           hasExtendedReg;        // +0x04  true for HDAWG only
    char           _pad05[3];             // +0x05

    // Waveform register region
    uint32_t       waveformRegBase;       // +0x08  HW register address
    uint32_t       waveformMemorySize;    // +0x0C  minBlockSize / cacheSize
    uint32_t       sampleLength;          // +0x10  Cache ctor arg
    uint32_t       waveformAlignment;     // +0x14  pageSize / alignment

    // Cache / command table parameters
    uint32_t       cachePageCount;        // +0x18  cache pages (cacheType=0)
    uint32_t       maxBlocks;             // +0x1C  cache pages (cacheType=1) / maxBlocks
    uint32_t       field_20;              // +0x20  always 16
    uint32_t       field_24;              // +0x24  always 64

    uint64_t       registerDepth;         // +0x28  register index upper bound
    uint64_t       memoryDepth;           // +0x30  memory address upper bound

    // Sequencer register region
    uint32_t       sequencerRegBase;      // +0x38  HW register address
    uint32_t       field_3C;              // +0x3C  always 6
    uint32_t       waveformGranularity;   // +0x40  waveform page cap
    uint32_t       waveformPageSize;      // +0x44  waveform page divisor

    // Auxiliary parameters
    uint32_t       field_48;              // +0x48  values: 0, 128, 384
    uint32_t       field_4C;              // +0x4C  values: 16, 32, 96
    uint32_t       bitsPerSample;         // +0x50  always 16
    uint32_t       field_54;              // +0x54  values: 0, 2

    uint64_t       waveformMemSize;       // +0x58  max waveform memory in samples
    uint64_t       maxSequenceLen;        // +0x60  max sequence length (16000)
    uint32_t       seqClockDivider;       // +0x68  0 or 1165
    uint32_t       _pad6C;               // +0x6C

    double         samplingRate;          // +0x70  Hz

    uint32_t       numOutputPorts;        // +0x78  0, 10, or 12
    uint32_t       numAWGCores;           // +0x7C  0, 3, or 5
    bool           hasDIO;                // +0x80  DIO support
    char           _pad81[3];             // +0x81
    uint32_t       numDIOBits;            // +0x84  0, 6, or 8
    bool           hasPrecomp;            // +0x88  precompensation / DA support
    char           _pad89[7];             // +0x89

    // Convenience aliases for legacy .cpp call-sites. These are NOT separate
    // fields — every TBD alias was reconciled to an existing field via
    // disassembly inspection (offsets all within the verified 0x90 layout).
    //
    //   grainSize          = waveformPageSize     (+0x44, verified
    //                        mov r8d,[rcx+0x44] at 0x1d919b in placeSingleCommand)
    //   maxWaveformLength  = waveformGranularity  (+0x40, verified
    //                        mov edi,[rcx+0x40] at 0x1d9198 in placeSingleCommand)
    //   maxDioTableEntries = waveformMemorySize   (+0x0C, verified
    //                        mov ecx,[rcx+0xc] at 0x29cade in
    //                        WavetableFront::updateDioTableUsage)
    //   maxWaveIndex       = (uint32_t)maxSequenceLen (+0x60, verified
    //                        mov esi,[rbx+0x60] at 0x29ceaa in
    //                        WavetableIR ctor — passed as int to WaveIndexTracker)
    uint32_t grainSize() const          { return waveformPageSize; }
    uint32_t maxWaveformLength() const  { return waveformGranularity; }
    uint32_t maxDioTableEntries() const { return waveformMemorySize; }
    uint32_t maxWaveIndex() const       { return static_cast<uint32_t>(maxSequenceLen); }
};

static_assert(sizeof(DeviceConstants) == 0x90, "DeviceConstants must be 0x90 bytes");

// Factory function — populates DeviceConstants for a given device type.
// Throws ZIAWGCompilerException for unsupported device types.
// Binary address: 0x2cc0c0 (size: 0x3c0)
DeviceConstants getDeviceConstants(AwgDeviceType deviceType);

} // namespace zhinst
