// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Forward declarations, enums, and basic types
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>

namespace zhinst {

// Device type enum — bitmask-style values.
// Confirmed from getAwgDeviceTypeString (0x270080) and getAwgDeviceTypeFromString (0x270180).
// getInstance() tests (val-2) against bitmask 0x4000000040004041 to select Cervino vs Hirzel.
// Hirzel: HDAWG(2), SHFQA(8), SHFSG(16), SHFQC_SG(32), SHFLI(64)
// Cervino: UHFLI(1), UHFQA(4), GHFLI(128), VHFLI(256)
//! \brief AWG device-type bitmask used throughout codegen and the
//! assembler factory to select per-device instruction-set variants.
//!
//! Each device occupies a single power-of-two bit so that named
//! combinations (`kDevHirzel`, `kDevCervino`, …) can be expressed
//! as bitwise OR of these values and tested with a single AND.
//! `AsmCommandsImpl::getInstance()` consults the bitmask
//! `0x4000000040004041` over `(val - 2)` to dispatch Hirzel vs
//! Cervino implementations: Hirzel covers HDAWG / SHFQA / SHFSG /
//! SHFQC_SG / SHFLI; Cervino covers UHFLI / UHFQA / GHFLI / VHFLI.
//! See `reconstructed/notes/cervino_vs_hirzel.md`.
enum AwgDeviceType : int {
    UHFLI      = 1,     //!< codename "cervino", display "UHFLI"
    HDAWG      = 2,     //!< codename "hirzel",  display "HDAWG" — only Hirzel type
    UHFQA      = 4,     //!< codename "klausen", display "UHFQA"
    SHFQA      = 8,     //!< codename "grimsel_qa", display "SHFQA"
    SHFSG      = 16,    //!< codename "grimsel_sg", display "SHFSG"
    SHFQC_SG   = 32,    //!< codename "grimsel_qc_sg", display "SHFQC (SG)"
    SHFLI      = 64,    //!< codename "grimsel_li", display "SHFLI"
    GHFLI      = 128,   //!< codename "gurnigel", display "GHFLI"
    VHFLI      = 256,   //!< codename "maloja", display "VHFLI"
};

// Named device-type bitmask combinations.
// These replace raw hex literals like static_cast<AwgDeviceType>(0x1ff).
// Values are OR-combinations of the AwgDeviceType power-of-2 enum values.
//! \brief Mask covering all nine `AwgDeviceType` values.
inline constexpr AwgDeviceType kDevAll        = static_cast<AwgDeviceType>(0x1FF); // all 9 devices
//! \brief Mask covering every device except `UHFLI`.
inline constexpr AwgDeviceType kDevAllButUHF  = static_cast<AwgDeviceType>(0x1FE); // all except UHFLI
//! \brief Mask covering every Hirzel-implementation device (HDAWG +
//! SHF family + GHFLI + VHFLI), i.e. those sharing
//! `sequencerRegBase = 0x0d05`.
inline constexpr AwgDeviceType kDevHirzel     = static_cast<AwgDeviceType>(0x1F2); // HDAWG + SHF* + GHFLI + VHFLI (sequencerRegBase=0x0d05)
//! \brief Mask covering all SHF-family devices plus GHFLI and VHFLI
//! (`SHFQA | SHFSG | SHFQC_SG | SHFLI | GHFLI | VHFLI`).
inline constexpr AwgDeviceType kDevSHFPlus    = static_cast<AwgDeviceType>(0x1F8); // SHFQA + SHFSG + SHFQC_SG + SHFLI + GHFLI + VHFLI
//! \brief Mask covering the lock-in family (`SHFLI | GHFLI | VHFLI`).
inline constexpr AwgDeviceType kDevLIFamily   = static_cast<AwgDeviceType>(0x1C0); // SHFLI + GHFLI + VHFLI
//! \brief Mask covering the original Cervino UHF devices
//! (`UHFLI | UHFQA`).
inline constexpr AwgDeviceType kDevCervino    = static_cast<AwgDeviceType>(0x005); // UHFLI + UHFQA
//! \brief Alias for `kDevCervino`.
inline constexpr AwgDeviceType kDevUHF        = static_cast<AwgDeviceType>(0x005); // alias for kDevCervino
//! \brief Mask covering the six pre-SHFLI device generations
//! (`UHFLI`..`SHFQC_SG`).
inline constexpr AwgDeviceType kDevPreSHFLI   = static_cast<AwgDeviceType>(0x03F); // UHFLI..SHFQC_SG (first 6)
//! \brief Mask covering both quantum-analyzer devices
//! (`UHFQA | SHFQA`).
inline constexpr AwgDeviceType kDevQA         = static_cast<AwgDeviceType>(0x00C); // UHFQA + SHFQA
//! \brief `kDevHirzel | UHFLI` — every device except `UHFQA`.
inline constexpr AwgDeviceType kDevHirzelAll  = static_cast<AwgDeviceType>(0x1F7); // all except UHFQA (= kDevHirzel | UHFLI)
//! \brief `kDevHirzel | UHFQA`.
inline constexpr AwgDeviceType kDevHirzelPlusUHFQA = static_cast<AwgDeviceType>(0x1F6); // kDevHirzel | UHFQA
//! \brief Empty mask (no devices).
inline constexpr AwgDeviceType kDevNone       = static_cast<AwgDeviceType>(0x000); // no devices

//! \brief Bitmap indexed by `(devType - 2)` selecting the Hirzel-
//! implementation device subset that dispatches via the bit-test
//! fast path in `AsmCommandsImpl::getInstance`, `customPlay*`, and
//! the `customWait*` family.  Set bits are {0, 6, 14, 30, 62},
//! corresponding to devType values {2, 8, 16, 32, 64} =
//! {`HDAWG`, `SHFQA`, `SHFSG`, `SHFQC_SG`, `SHFLI`}.
//!
//! The bitmap is *not* the same as `kDevHirzel` (which is an
//! `AwgDeviceType`-bit mask): this constant is indexed by the
//! arithmetic shift amount `devType - 2`, not by the AwgDeviceType
//! bit position.  Callers also add `GHFLI` and `VHFLI` to the Hirzel
//! set via a separate `if (devType == 128 || devType == 256)` check
//! (which doesn't fit in a 64-bit shift, since `(256 - 2) = 254`).
//!
//! Usage pattern:
//! ```
//! unsigned shifted = static_cast<unsigned>(devType) - 2u;
//! bool isHirzelCore = shifted <= 62u
//!     && ((kHirzelDevTypeMinus2Mask >> shifted) & 1u);
//! ```
inline constexpr uint64_t kHirzelDevTypeMinus2Mask = 0x4000000040004041ULL;

// EDirection — unified direction enum.
// Binary name: zhinst::EDirection (mangled: NS_10EDirectionE).
// Used both as AST parameter direction (eIN/eOUT/eINOUT in SeqCAstNode ctor
// signatures) and as Resources read/write direction (eIN=read, eOUT=write in
// readConst/readString/readWave/readCvar).
// str() @0x1c1730 maps: 0→"in", 1→"out", 2→"inout".
//! \brief Unified data-direction enumeration shared between AST
//! parameter declarations and the `Resources` read/write API.
//!
//! For AST parameters the values mean "input", "output" or
//! "in/out"; for `Resources` callers the same values select the
//! read-only path (`eIN`) versus the write/strict path (`eOUT`).
//! `Resources` does not use `eINOUT`.
enum class EDirection : int32_t {
    eIN    = 0,   //!< AST: parameter is input.    Resources: read-only path.
    eOUT   = 1,   //!< AST: parameter is output.   Resources: write/strict path.
    eINOUT = 2,   //!< AST: parameter is in+out.   (Resources: not used.)
};

// Sentinel value constants (A15)
// Used across multiple subsystems for "inherit/unset/none" semantics.
constexpr int kRateInherit  = -1;   //!< Sentinel: rate inherits the global value (prefetch.cpp).
constexpr int kNoWaveIndex  = -1;   //!< Sentinel: no waveform index assigned (wavetable_front.cpp).
constexpr int kNoNodeId     = -1;   //!< Sentinel: no AST node ID assigned (node.cpp).
constexpr int kNoPlayIndex  = -1;   //!< Sentinel: no play index assigned (wave_index_tracker.cpp).

//! \brief Maximum value of the 20-bit immediate field carried by the
//!        prefetch / waveform-fetch opcodes (`wvf*`, `wvfi*`, `wvfs*`).
//!
//! Offsets `> kPrefetchAddr20BitMask` do not fit in the immediate form
//! and require the codegen to materialise the high bits in a temporary
//! register before emitting the fetch.  Used as a clamp ceiling for
//! waveform-cache capacity and as the immediate-overflow boundary in
//! `codegen/prefetch_emit.cpp` and `codegen/prefetch_placesingle.cpp`.
constexpr uint32_t kPrefetchAddr20BitMask = 0xFFFFFu;

// Playback trigger-mask constants (IF-228 E2).
//
// The 14-bit mask encodes per-channel "suppress" bits passed to
// `asmPlay`/`asmPlayDIO` and friends.  Bits are arranged as two
// 7-bit groups; bit 6 of each group (`0x40` and `0x2000`) is the
// per-channel marker-trigger bit cleared by
// `mask &= ~(0x40 << (7 * b))` for each `b` in a wave assignment's
// `bits` set.
//! \brief Default 14-bit trigger mask (all bits set).  Used as the
//! initial value for trigger-mask bookkeeping in `playAuxWave`,
//! `playDIOWave`, `playWaveDIO`, `assignWaveIndex`, and the
//! `play_*` low-level helpers before any per-channel marker bits
//! are cleared.
constexpr unsigned int kPlayTriggerMaskFull     = 0x3FFF;
//! \brief Aux-wave variant of `kPlayTriggerMaskFull` with the
//! marker-trigger bit pre-cleared in both 7-bit groups
//! (`0x3FFF & ~(0x40 | 0x2000)`).  Used by `playAuxWave` after
//! the merge step because the aux-wave instruction has a fixed
//! trigger-bit pattern (no dynamic per-bit clearing).
constexpr unsigned int kPlayTriggerMaskAuxMerge = 0x3FC3;

// I/Q channel tags in writeToNode (A16)
constexpr int kChannelTag_I = 0x0C; //!< Channel tag identifying the I component in `writeToNode`.
constexpr int kChannelTag_Q = 0x0D; //!< Channel tag identifying the Q component in `writeToNode`.

// ============================================================================
// SUSER register addresses — hardware register map.
// See reconstructed/notes/special_registers.md for full documentation.
// These are passed as the 'addr' argument to AsmCommands::suser().
// ============================================================================
//! \name SUSER register addresses
//! \brief Hardware-register addresses passed as the `addr` argument
//! to `AsmCommands::suser()`.  Authoritative reference for every
//! address listed here is `reconstructed/notes/special_registers.md`.
//! \{

// -- Write protocol registers (used by writeToNode / setInt / setDouble) --
constexpr uint32_t kSuserNodeTag       = 0x10;  //!< Multi-word write: low word (tag/node ID).
constexpr uint32_t kSuserNodeAddr      = 0x11;  //!< Multi-word write: mid word (register index).
constexpr uint32_t kSuserNodeValue     = 0x12;  //!< Multi-word write: high word (value/commit).
constexpr uint32_t kSuserNodeValueHi   = 0x13;  //!< Double-precision high 32 bits.
constexpr uint32_t kSuserNodeSlowCommit = 0x14; //!< Slow-path commit (`st Rx, 20`).
constexpr uint32_t kSuserNodeCommit    = 0x16;  //!< Write commit/finalize.
constexpr uint32_t kSuserNodeDirect    = 0x17;  //!< Direct single-value write (fast path).
constexpr uint32_t kSuserNodeFreqCommit = 0x18; //!< Frequency commit (`writeToNode` typeIdx=4).
constexpr uint32_t kSuserNodeDirectB   = 0x19;  //!< Direct write B / companion (Q-channel / high32).

// -- Trigger & timing --
constexpr uint32_t kSuserTriggerLoad   = 0x1A;  //!< Trigger value load for `wtrig`.
constexpr uint32_t kSuserTimestamp     = 0x1B;  //!< Timestamp register.
constexpr uint32_t kSuserNow           = 0x1C;  //!< Current sequencer timestamp ("now").
constexpr uint32_t kSuserRTLoggerData  = 0x1D;  //!< RT-logger output data (`at()` builtin).

// -- Sync registers --
constexpr uint32_t kSuserSyncA         = 0x44;  //!< Cervino sync register A.
constexpr uint32_t kSuserSyncB         = 0x45;  //!< Cervino sync register B.
constexpr uint32_t kSuserSyncHirzel    = 0x6E;  //!< Hirzel sync register.

// -- AWG core / wait --
constexpr uint32_t kSuserWaitCycles    = 0x69;  //!< Wait-cycles / AWG-core count.
constexpr uint32_t kSuserQAResult      = 0x61;  //!< QA result read (`getQAResult`).
constexpr uint32_t kSuserRTLoggerReset = 0x62;  //!< RT-logger timestamp reset (non-HDAWG).
constexpr uint32_t kSuserRTLoggerResetHdawg = 0x6D; //!< RT-logger timestamp reset (HDAWG).
constexpr uint32_t kSuserWaitLegacy    = 0x6F;  //!< Wait register (legacy, play-internal).

// -- Oscillator / sine phase --
constexpr uint32_t kSuserSinePhase0    = 0x70;  //!< Sine phase, oscillator 0.
constexpr uint32_t kSuserSinePhase1    = 0x71;  //!< Sine phase, oscillator 1.
constexpr uint32_t kSuserSinePhaseInc0 = 0x72;  //!< Sine phase increment, osc 0.
constexpr uint32_t kSuserSinePhaseInc1 = 0x73;  //!< Sine phase increment, osc 1.

// -- PRNG --
constexpr uint32_t kSuserPrngSeed      = 0x74;  //!< PRNG seed.
constexpr uint32_t kSuserPrngRangeLow  = 0x75;  //!< PRNG range low.
constexpr uint32_t kSuserPrngRangeSpan = 0x76;  //!< PRNG range span.
constexpr uint32_t kSuserPrngValue     = 0x77;  //!< PRNG value read (`getPRNGValue`).

// -- QA (Quantum Analyzer) --
constexpr uint32_t kSuserQAWeights     = 0x78;  //!< QA weights + result-addr / osc reset.
constexpr uint32_t kSuserQATrigger     = 0x79;  //!< QA trigger / monitor composite.
constexpr uint32_t kSuserQAResultLen   = 0x7A;  //!< QA result length / osc-phase reset.

// -- Frequency sweep (SHF+ only) --
constexpr uint32_t kSuserSweepOscIdx   = 0x8C;  //!< Sweep oscillator index.
constexpr uint32_t kSuserSweepControl  = 0x8D;  //!< Sweep control (step/reset).
constexpr uint32_t kSuserSweepStartLo  = 0x8E;  //!< Sweep start frequency, low 32 bits.
constexpr uint32_t kSuserSweepStartHi  = 0x8F;  //!< Sweep start frequency, high 32 bits.
constexpr uint32_t kSuserSweepStepLo   = 0x90;  //!< Sweep step frequency, low 32 bits.
constexpr uint32_t kSuserSweepStepHi   = 0x91;  //!< Sweep step frequency, high 32 bits.

// -- Misc --
constexpr uint32_t kSuserWaitOnSync    = 0x92;  //!< `waitOnSync` register.
//! \}

// ============================================================================
// LD/ST direct addresses — used with ld(reg, AddressImpl{addr}) / st(reg, AddressImpl{addr}).
// These are NOT suser addresses; they are loaded/stored via ld/st instructions.
// ============================================================================
//! \name LD/ST direct addresses
//! \brief Addresses passed to `AsmCommands::ld()` / `AsmCommands::st()`
//! through an `AddressImpl{addr}` wrapper.  These are distinct from
//! the SUSER register space above.
//! \{
constexpr uint32_t kAddrTrigger        = 0x22;  //!< Trigger register (`ltrig` / `strig`).
constexpr uint32_t kAddrInternalTrig   = 0x23;  //!< Internal trigger register (`sinttrig`).
constexpr uint32_t kAddrOscPhaseReset  = 0x5F;  //!< Oscillator phase reset (pulse, written via `st`/`st 0`).  See `notes/special_registers.md` §8.
//! \}

} // namespace zhinst
