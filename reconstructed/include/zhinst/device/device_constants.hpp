// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// DeviceConstants — per-device hardware configuration constants
//
// Source path (from debug info): /builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp
//
// POD struct, no vtable. Size: 0x90 (144 bytes, with alignment padding).
// Populated by getDeviceConstants(AwgDeviceType) factory at 0x2cc0c0.
//
// Layout revised after cross-referencing all consumer sites
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
// 0x20    4     uint32_t    sineNodeBase          Always 16 — base offset for oscillator/sine node index
// 0x24    4     uint32_t    waveformElfAlignment  Always 64 — ELF segment alignment (bitsPerSample × channels)
//
// 0x28    8     uint64_t    registerDepth         Register index upper bound (getReg at 0x2892c8).
//                                                  Always 16
// 0x30    8     uint64_t    memoryDepth           Memory address upper bound (opcode4 at 0x28a408).
//                                                  Always 1024
//
// --- Sequencer register region ---
// 0x38    4     uint32_t    sequencerRegBase      HW register address (0x115c Cervino, 0x0d05 Hirzel)
// 0x3C    4     uint32_t    triggerLatencyCycles  Always 6 — sequencer trigger/wait instruction latency
// 0x40    4     uint32_t    maxWaveformLength   Round-up cap in waveform memory calc. Also
//                                                  WaveformFront.minLengthSamples. Values: 16, 32, 96
// 0x44    4     uint32_t    grainSize      Round-up divisor in waveform memory calc.
//                                                  Values: 8, 16, 48
//
// --- Auxiliary parameters ---
// 0x48    4     uint32_t    playMinSamples        Values: 0, 128, 384 — min play length (checkPlayMinLength)
// 0x4C    4     uint32_t    waveformMinSamples    Values: 16, 32, 96 — initial minLengthSamples (checkOffspecWaveLength)
// 0x50    4     uint32_t    bitsPerSample         Memory bits = pages * channels * bitsPerSample.
//                                                  Always 16
// 0x54    4     uint32_t    numCounters           Values: 0, 2 — hardware loop counter count (getCnt range check)
//
// 0x58    8     uint64_t    maxProgramSize        Max sequencer program in opcode words (1024/16384/32768)
// 0x60    8     uint64_t    maxSequenceLen        Max sequence length (always 16000)
// 0x68    4     uint32_t    seqClockDivider       0 (Cervino) or 1165/0x48D (Hirzel)
// 0x6C    4     (padding)
// 0x70    8     double      samplingRate          Hz (1.8e9, 2.0e9, 2.4e9, 12.0e9)
// 0x78    4     uint32_t    execTableIndexBits        0, 10, or 12
// 0x7C    4     uint32_t    numAWGCores           0, 3, or 5
// 0x80    1     bool        hasDIO                DIO support flag
// 0x81    3     (padding)
// 0x84    4     uint32_t    numDIOBits            0, 6, or 8
// 0x88    1     bool        hasPrecomp            Precompensation support. Also controls DA
//                                                  code paths in Prefetch (verified at 0x1dc683 etc.)
// 0x89    7     (padding to 0x90)
// ============================================================================
//! \brief Per-device hardware-configuration POD consumed by every
//! compilation pipeline (frontend, prefetch, wavetable IR,
//! assembler, ELF writer).
//!
//! Built once per compile by `getDeviceConstants(AwgDeviceType)` and
//! threaded through the compiler as `const DeviceConstants*` so each
//! component can read the values that depend on the target sequencer
//! generation: waveform-memory geometry (`waveformMemorySize`,
//! `waveformAlignment`, `cachePageCount`, `maxBlocks`), waveform
//! length / play-length limits (`maxWaveformLength`, `grainSize`,
//! `playMinSamples`, `waveformMinSamples`), instruction-set features
//! (`hasExtendedReg`, `hasPrecomp`, `hasDIO`, `numCounters`,
//! `numAWGCores`, `numDIOBits`), sequencer / waveform / sine
//! register base addresses, and the program-size / sequence-length
//! / sample-rate caps that drive optimisation and ELF emission.
//!
//! The nested `Register` and `SuserAddr` sub-types collect the
//! hardware register-address constants referenced by the assembler
//! and `CustomFunctions` (sync registers, multi-word write-commit
//! sequences, sine / PRNG / sweep / QA register slots, and the
//! generation-specific sync register variants).  Two convenience
//! accessors (`maxDioTableEntries`, `maxWaveIndex`) expose existing
//! fields under the names used at the call sites that need them.
struct DeviceConstants {

    //! \brief Sequencer-register address constants used by the
    //! synchronisation primitives.
    //! \details The two enumerators give the addresses of the
    //! generation-A sync register pair written by the Cervino
    //! sync handshake (`syncCervino` / `unsyncCervino`).
    struct Register {
        //! \brief Anonymous enum carrying the Cervino sync-A register address as a compile-time constant.
        enum : uint32_t { SyncRegA = 0x44 };  //!< Cervino sync register A address (used by `suser`).
        //! \brief Anonymous enum carrying the Cervino sync-B register address as a compile-time constant.
        enum : uint32_t { SyncRegB = 0x45 };  //!< Cervino sync register B address (used by `suser`).
    };

    // Note: SUSR (user-register) address constants live as `kSuser*`
    // free constants in `core/types.hpp`.  An earlier reconstruction
    // pass also declared them as a `SuserAddr` member-struct here,
    // but it was recon-invented and never referenced from the source
    // tree (the kSuser* free constants are the only consumed form,
    // with ~100 call sites across the runtime); it has been removed
    // to leave a single canonical naming for these addresses.

    uint32_t       deviceType;            //!< Target device family as the integer value of `AwgDeviceType` (e.g. UHFLI=1, HDAWG=2, SHFQA=8). Set first by the factory before any per-family case populates the rest. +0x00
    bool           hasExtendedReg;        //!< `true` only for HDAWG (the one device that exposes the extended register file used by the assembler's wide-instruction encodings). +0x04
    char           _pad05[3];             //!< Alignment padding to the next 4-byte field. +0x05

    // Waveform register region
    uint32_t       waveformRegBase;       //!< Hardware base address of the waveform register block; the value (e.g. `0x95b2`, `0x110f6`, `0x11202`, `0x11203`) varies by device generation. +0x08
    uint32_t       waveformMemorySize;    //!< Total waveform memory size in samples. Used as `minBlockSize` by `WavetableIR::allocateWaveformsForFifo`, as cache size by `Prefetch`, and as the `Cache` constructor's first argument. Also returned by `maxDioTableEntries()`. Range across devices: `0x20000`–`0x100000`. +0x0C
    uint32_t       sampleLength;          //!< Sample-block length passed as the `Cache` constructor's second argument from the `Prefetch` constructor. Per-device values: 16, 32, 48, 64. +0x10
    uint32_t       waveformAlignment;     //!< Page size used by `clampToCache` and the alignment passed to `WavetableIR::allocateWaveforms{,ForFifo}`. Per-device values: 16, 48, 4096. +0x14

    // Cache / command table parameters
    uint32_t       cachePageCount;        //!< Cache page count for `cacheType=0` (the default cache). `clampToCache` indexes the field at this offset as `uint32_t[cacheType]`. Per-device values: 4, 104, `0x2000`, `0x6000`. +0x18
    uint32_t       maxBlocks;             //!< Cache page count for `cacheType=1`; also used as `maxBlocks` by `WavetableIR::allocateWaveformsForFifo`. Always 2 across all devices. +0x1C
    uint32_t       sineNodeBase;          //!< Base offset for the oscillator / sine-node index space. Always 16. +0x20
    uint32_t       waveformElfAlignment;  //!< Alignment, in bytes, of the ELF segment containing waveform data (matches `bitsPerSample × channels`). Always 64. +0x24

    uint64_t       registerDepth;         //!< Upper bound on the sequencer register index, enforced by `getReg`. Always 16. +0x28
    uint64_t       memoryDepth;           //!< Upper bound on user-memory addresses, enforced by the `opcode4` encoder. Always 1024. +0x30

    // Sequencer register region
    uint32_t       sequencerRegBase;      //!< Hardware base address of the sequencer register block: `0x115c` on Cervino devices, `0x0d05` on Hirzel devices. +0x38
    uint32_t       triggerLatencyCycles;  //!< Sequencer trigger / wait-instruction latency in cycles. Always 6. +0x3C
    uint32_t       maxWaveformLength;     //!< Round-up cap used by the waveform-memory length calculation; also the initial `WaveformFront.minLengthSamples`. Per-device values: 16, 32, 96. +0x40
    uint32_t       grainSize;             //!< Round-up divisor (granularity, in samples) used by the waveform-memory length calculation. Per-device values: 8, 16, 48. +0x44

    // Auxiliary parameters
    uint32_t       playMinSamples;        //!< Minimum sample count enforced by `checkPlayMinLength` for a single `playWave`. Per-device values: 0, 128, 384. +0x48
    uint32_t       waveformMinSamples;    //!< Initial `minLengthSamples` consumed by `checkOffspecWaveLength` when validating a freshly allocated waveform. Per-device values: 16, 32, 96. +0x4C
    uint32_t       bitsPerSample;         //!< Bits per sample. Memory-bit accounting computes `pages × channels × bitsPerSample`. Always 16. +0x50
    uint32_t       numCounters;           //!< Number of hardware loop counters available; the range check in `getCnt` clamps the user index against this. Per-device values: 0 (Cervino) or 2 (Hirzel). +0x54

    uint64_t       maxProgramSize;        //!< Maximum sequencer program length in opcode words: 1024 (UHFAWG / UHFQA), 16384 (HDAWG), 32768 (SHF*). Confirmed by IF-192 / IF-196 binary analysis. +0x58
                                          // (Was named `waveformMemSize` until 2026-05-07
                                          // IF-195 cleanup — name was misleading.)
    uint64_t       maxSequenceLen;        //!< Maximum sequence length in IR nodes. Always 16000. Truncated to `uint32_t` and exposed by `maxWaveIndex()` to `WaveIndexTracker`. +0x60
    uint32_t       seqClockDivider;       //!< Sequencer clock divider: 0 on Cervino, 1165 (`0x48D`) on Hirzel. +0x68
    uint32_t       _pad6C;                //!< Padding for 8-byte alignment of `samplingRate`. +0x6C

    double         samplingRate;          //!< Native sampling rate of the device, in Hz: 1.8e9 (UHFLI / UHFQA), 2.0e9 (SHF family / VHFLI), 2.4e9 (HDAWG), 12.0e9 (GHFLI). +0x70

    uint32_t       execTableIndexBits;    //!< Width, in bits, of the command-table index used by `executeTableEntry`: 0, 10, or 12. +0x78
    uint32_t       numAWGCores;           //!< Number of AWG sequencer cores on this device family: 0 (Cervino), 3 (SHF/SHFLI/GHFLI/VHFLI), or 5 (HDAWG). +0x7C
    bool           hasDIO;                //!< Whether the device exposes a digital-I/O register block. +0x80
    char           _pad81[3];             //!< Alignment padding to the next 4-byte field. +0x81
    uint32_t       numDIOBits;            //!< Width, in bits, of the DIO bus when `hasDIO` is set: 0, 6, or 8. +0x84
    bool           hasPrecomp;            //!< Whether the device supports waveform pre-compensation. Also gates the DA code paths in `Prefetch`. +0x88
    char           _pad89[7];             //!< Trailing padding so `sizeof(DeviceConstants)` rounds up to `0x90`. +0x89

    // Convenience aliases for legacy .cpp call-sites. These are NOT separate
    // fields — every alias was reconciled to an existing field via disassembly
    // inspection (offsets all within the verified 0x90 layout).
    //
    //   grainSize (+0x44) and maxWaveformLength (+0x40) were formerly
    //   accessor methods returning waveformPageSize / waveformGranularity.
    //   After the field rename (phase-d c16), they are now the field names
    //   themselves, so the accessors were removed.
    //
    //   maxDioTableEntries = waveformMemorySize   (+0x0C, verified
    //                        mov ecx,[rcx+0xc] at 0x29cade in
    //                        WavetableFront::updateDioTableUsage)
    //   maxWaveIndex       = (uint32_t)maxSequenceLen (+0x60, verified
    //                        mov esi,[rbx+0x60] at 0x29ceaa in
    //                        WavetableIR ctor — passed as int to WaveIndexTracker)
    // grainSize() and maxWaveformLength() accessors removed — fields
    // renamed from waveformPageSize→grainSize, waveformGranularity→maxWaveformLength.
    //! \brief Maximum number of entries in the DIO command table.
    //! \details Alias for `waveformMemorySize` (`+0x0C`); the same
    //! storage word is read by `WavetableFront::updateDioTableUsage`
    //! when bounding DIO-table usage.
    //! \return The DIO-table entry-count cap for this device.
    uint32_t maxDioTableEntries() const { return waveformMemorySize; }

    //! \brief Maximum waveform-index value addressable by `WaveIndexTracker`.
    //! \details Alias for `maxSequenceLen` (`+0x60`) narrowed to
    //! `uint32_t`; the `WavetableIR` constructor reads the same
    //! storage word and passes it to `WaveIndexTracker` as a signed
    //! `int`.
    //! \return The maximum legal waveform index for this device.
    uint32_t maxWaveIndex() const       { return static_cast<uint32_t>(maxSequenceLen); }
};

static_assert(sizeof(DeviceConstants) == 0x90, "DeviceConstants must be 0x90 bytes");

// Factory function — populates DeviceConstants for a given device type.
// Throws ZIAWGCompilerException for unsupported device types.
// Binary address: 0x2cc0c0 (size: 0x3c0)
//! \brief Build the per-device hardware-constants block for the given
//! AWG device type.
//! \details Zero-initialises a fresh `DeviceConstants`, stamps
//! `deviceType` with the integer value of `deviceType`, and then
//! dispatches on the device family to populate every other field
//! with the values appropriate to that hardware generation
//! (waveform geometry, sequencer-register addresses, sample rate,
//! feature flags, program-size and sequence-length caps, ...).
//! Threaded through the rest of the compiler as `const
//! DeviceConstants*`.
//! \param deviceType The target AWG device family.
//! \return A fully populated `DeviceConstants` for `deviceType`.
//! \throws ZIAWGCompilerException If `deviceType` is not one of the
//! supported families (UHFLI, HDAWG, UHFQA, SHFQA, SHFSG, SHFQC_SG,
//! SHFLI, GHFLI, VHFLI).
DeviceConstants getDeviceConstants(AwgDeviceType deviceType);

} // namespace zhinst
