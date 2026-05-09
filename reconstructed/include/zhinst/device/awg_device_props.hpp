// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// awg_device_props.hpp
//
// Covers:
//   - AwgSequencerType enum (3 named values + sentinel)
//   - AwgPathPatterns struct (3 std::strings)
//   - AwgDeviceProps struct (~128 bytes; AwgDeviceType + 4 strings + limits)
//   - getAwgDeviceProps<AwgDeviceType>(DeviceType const&) — 9 explicit
//     instantiations (one per AwgDeviceType bit)
//   - toAwgDeviceType(DeviceTypeCode, AwgSequencerType) @ 0x2cbd60
//   - makeUnsupportedAwgSequencerErrorMessage @ 0x2cbdd0
//
// Note: The AwgDeviceType enum itself is defined in types.hpp from earlier
//  work (9 one-hot values: UHFLI=1, HDAWG=2, UHFQA=4, SHFQA=8,
// SHFSG=16, SHFQC_SG=32, SHFLI=64, GHFLI=128, VHFLI=256).
// ============================================================================
#pragma once

#include <cstdint>
#include <string>

#include "zhinst/core/types.hpp"             // AwgDeviceType
#include "zhinst/device/device_type.hpp"       // DeviceType, DeviceTypeCode

namespace zhinst {

// ----------------------------------------------------------------------------
// AwgSequencerType — selects which sequencer kind on multi-sequencer devices.
//
// Decoded from:
//   * makeUnsupportedAwgSequencerErrorMessage @ 0x2cbdd0 (stringifies
//     0->"auto", 1->"QA", 2->"SG", else->"unknown")
//   * toAwgDeviceType @ 0x2cbd60 (only differentiates on DeviceTypeCode
//     SHFQC: seq==1 -> SHFQA, seq==2 -> SHFQC_SG, else -> 0/unsupported)
//
// The "unknown" case in the error-message formatter has no defined enum
// value; it's the default branch for any value > 2. We treat it as a
// sentinel-only state (no source-level enumerator).
// ----------------------------------------------------------------------------
enum class AwgSequencerType : int {
    Auto = 0,   // toString -> "auto"
    QA   = 1,   // toString -> "QA"
    SG   = 2,   // toString -> "SG"
};

// toString(AwgSequencerType) @0x2cbce0
std::string toString(AwgSequencerType seq);

// ----------------------------------------------------------------------------
// AwgPathPatterns — 0x48 bytes (3 std::strings)
//
// Decoded from:
//   * AwgPathPatterns::AwgPathPatterns(const&) @ 0x2cc4f0 — copies 3
//     strings at offsets 0x00 / 0x18 / 0x30
//   * AwgPathPatterns::~AwgPathPatterns() @ 0x2cc480 — destroys the
//     same 3 strings
//   * Static-init code at 0xdc900 (in _GLOBAL__sub_I_properties.cpp)
//     populating the per-family anonymous-namespace globals
//
// Six globals exist in the binary's anonymous namespace:
//   - awgPathPatternsDefault     @ 0xb84fc0 (used by UHFLI, HDAWG, UHFQA)
//   - awgPathPatternsGrimselQa   @ 0xb85008 (used by SHFQA)
//   - awgPathPatternsGrimselSg   @ 0xb85050 (used by SHFSG, SHFQC_SG)
//   - awgPathPatternsGrimselLi   @ 0xb85098 (used by SHFLI)
//   - awgPathPatternsGurnigel    @ 0xb850e0 (copy of GrimselLi; for GHFLI)
//   - awgPathPatternsMaloja      @ 0xb85128 (copy of GrimselLi; for VHFLI)
// ----------------------------------------------------------------------------
//! \brief Three node-path templates that locate an AWG's data, progress,
//! and enable nodes within the device's parameter tree.
//!
//! One instance is interned per device family (default for the legacy
//! UHF/HDAWG families, plus dedicated patterns for the SHFQA, SHFSG,
//! SHFLI, GHFLI, and VHFLI families) and copied into each
//! `AwgDeviceProps` returned by `getAwgDeviceProps`.
struct AwgPathPatterns {
    std::string elfDataPattern;        // +0x00
    std::string elfProgressPattern;    // +0x18
    std::string enablePattern;         // +0x30
    // Total: 0x48 bytes.

    // Default ctor — produces empty strings (default for unknown families).
    AwgPathPatterns() = default;

    // Three-arg convenience ctor used to populate the named globals.
    AwgPathPatterns(std::string elfData,
                    std::string elfProgress,
                    std::string enable);

    // 0x2cc4f0 — copy ctor (deep-copies the 3 strings).
    AwgPathPatterns(AwgPathPatterns const&) = default;

    // 0x2cc480 — dtor.
    ~AwgPathPatterns() = default;

    AwgPathPatterns(AwgPathPatterns&&) = default;
    AwgPathPatterns& operator=(AwgPathPatterns const&) = default;
    AwgPathPatterns& operator=(AwgPathPatterns&&) = default;
};
// 3 strings, no other fields → 0x48 (libc++) or 0x60 (libstdc++).
static_assert(sizeof(AwgPathPatterns) == 3 * sizeof(std::string),
              "AwgPathPatterns: expected 3 * sizeof(std::string)");

// ----------------------------------------------------------------------------
// AwgDeviceProps — 0x80 bytes (128B)
//
// Decoded layout (from the 9 getAwgDeviceProps<N> instantiations
// 0x2cc5f0..0x2cdb00; field offsets verified by the field-by-field
// store sequence in each template):
//
//   +0x00  4    AwgDeviceType deviceType         (one-hot bit per device)
//   +0x04  4    (padding for next 8B alignment)
//   +0x08  24   std::string elfDataPattern       (path pattern 1)
//   +0x20  24   std::string elfProgressPattern   (path pattern 2)
//   +0x38  24   std::string enablePattern        (path pattern 3)
//   +0x50  8    uint64_t maxElfSize              (max ELF binary size; JSON key "maxelfsize")
//   +0x58  4    uint32_t addressImpl             (waveform memory base address; AddressImpl<uint>)
//   +0x5c  4    uint32_t sampleFormat            (SampleFormat enum: 0/1/2)
//   +0x60  1    bool isHirzel                    (true for Hirzel-generation devices)
//   +0x61  7    (padding)
//   +0x68  24   std::string fpgaRevisionPattern  (path pattern 4)
//   Total: 0x80 bytes.
//
// Field verification:
//   +0x50 maxElfSize   — consumer: compileSeqc @0xf6a41 stores to JSON "maxelfsize"
//   +0x58 addressImpl  — consumer: AWGCompilerImpl ctor @0x103b99 → config.addressImpl (+0x10)
//   +0x5c sampleFormat — consumer: writeWavesToElf* @0x10e049/0x10e1f2 → config.sampleFormat (+0x04)
//   +0x60 isHirzel     — consumer: compileSeqc @0xf67cd → config.isHirzel (+0x18)
//                        true for: HDAWG, SHFSG, SHFQC_SG, SHFLI, GHFLI, VHFLI
//                        false for: UHFLI, UHFQA, SHFQA
//
// The +0x58 field was originally stored as a single qword in the binary
// (e.g. HDAWG writes 0x180000000 = low32 0x80000000, high32 0x00000001),
// but consumers read it as two independent uint32_t fields. Per-template:
//
//   UHFLI/UHFQA       : maxElfSize=0x10000000  addressImpl=0xd0000000  sampleFormat=0
//   HDAWG             : maxElfSize=cond(ME)     addressImpl=0x80000000  sampleFormat=1
//   SHFQA             : maxElfSize=0x80000000  addressImpl=0x00000000  sampleFormat=2
//   SHFSG/SHFQC_SG    : maxElfSize=0x80000000  addressImpl=0x00000000  sampleFormat=1
//   SHFLI/GHFLI/VHFLI : maxElfSize=0x80000000  addressImpl=0x00000000  sampleFormat=1
//
// HDAWG is the ONLY template that consults `dt`: it calls
// `dt.hasOption(DeviceOption(0x13)=ME)` and picks the higher limit when
// the ME (Memory Extension) option is set.
// ----------------------------------------------------------------------------
//! \brief Per-AWG-family device properties consumed by the compile
//! driver and ELF writer.
//!
//! Built once per compile by the appropriate `getAwgDeviceProps<T>`
//! specialisation (one per `AwgDeviceType` bit) and threaded through
//! `compileSeqc` and `AWGCompilerImpl`.  Bundles the family bit, the
//! four node-path templates (the three from `AwgPathPatterns` plus an
//! FPGA-revision path), the maximum permitted ELF size, the waveform
//! memory base address (`addressImpl`, also used as the program entry
//! point), the wave-sample storage format (`sampleFormat`, 0/1/2),
//! and the `isHirzel` flag that selects between the two sequencer
//! generations.
struct AwgDeviceProps {
    AwgDeviceType deviceType;          // +0x00
    std::string   elfDataPattern;      // +0x08
    std::string   elfProgressPattern;  // +0x20
    std::string   enablePattern;       // +0x38
    uint64_t      maxElfSize;          // +0x50  (max ELF binary size; JSON "maxelfsize")
    uint32_t      addressImpl;         // +0x58  (waveform memory base address)
    uint32_t      sampleFormat;        // +0x5c  (SampleFormat enum: 0/1/2)
    bool          isHirzel;            // +0x60  (true for Hirzel-gen devices)
    std::string   fpgaRevisionPattern; // +0x68  (also slaverevision for HDAWG)
    // Total: 0x80 bytes.

    ~AwgDeviceProps();   // 0xf81e0 — explicit out-of-line dtor
};
// 4 strings + 32 bytes fixed fields → 0x80 (libc++) or 0xa0 (libstdc++).
static_assert(sizeof(AwgDeviceProps) == 32 + 4 * sizeof(std::string),
              "AwgDeviceProps: expected 32 + 4 * sizeof(std::string)");

// ----------------------------------------------------------------------------
// getAwgDeviceProps<AwgDeviceType>(DeviceType const&)
//
// Per-AwgDeviceType factory. Each of the 9 explicit instantiations is a
// unique function in the binary that bakes in:
//   - the AwgDeviceType bit (.deviceType field)
//   - the per-family AwgPathPatterns (3 path strings)
//   - the FPGA-revision path string (varies only for HDAWG)
//   - the maxElfSize, addressImpl, sampleFormat values
//   - the isHirzel bool
//
// Only the HDAWG instantiation actually consults `dt` (calls
// `dt.hasOption(DeviceOption::ME)` to choose between two sample limits).
// All other instantiations ignore their argument.
//
// Binary addresses:
//   getAwgDeviceProps<UHFLI(1)>     @ 0x2cc900
//   getAwgDeviceProps<HDAWG(2)>     @ 0x2ccb80   (only one that reads `dt`)
//   getAwgDeviceProps<UHFQA(4)>     @ 0x2cc5f0
//   getAwgDeviceProps<SHFQA(8)>     @ 0x2cce30
//   getAwgDeviceProps<SHFSG(16)>    @ 0x2cd0c0
//   getAwgDeviceProps<SHFQC_SG(32)> @ 0x2cd350
//   getAwgDeviceProps<SHFLI(64)>    @ 0x2cd5e0
//   getAwgDeviceProps<GHFLI(128)>   @ 0x2cdb00
//   getAwgDeviceProps<VHFLI(256)>   @ 0x2cd870
//
// All instantiations are present (9 functions) — the binary contains
// no template-as-template-definition; only explicit specializations
// generated for each AwgDeviceType value.
// ----------------------------------------------------------------------------
template <AwgDeviceType T>
AwgDeviceProps getAwgDeviceProps(DeviceType const& dt);

// Explicit specialization DECLARATIONS (definitions in awg_device_props.cpp).
// We do not provide a primary template definition: the binary contains
// only these 9 specializations, each emitted as a unique function body.
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::UHFLI>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::HDAWG>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::UHFQA>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFQA>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFSG>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFQC_SG>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFLI>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::GHFLI>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::VHFLI>(DeviceType const&);

// ----------------------------------------------------------------------------
// toAwgDeviceType(DeviceTypeCode, AwgSequencerType) — @ 0x2cbd60
//
// Maps a (code, seq) pair to an AwgDeviceType bit, or 0 ("none"/unsupported).
// Body is a 29-entry jump table over (code - 4) for codes in [4..32],
// plus a special branch for SHFQC (code 21) that consumes `seq`.
//
// Mapping:
//   UHF(4), UHFLI(5), UHFAWG(6), UHFIA(8)        -> UHFLI(1)
//   UHFQA(7)                                     -> UHFQA(4)
//   HDAWG4(13), HDAWG8(14)                       -> HDAWG(2)
//   SHFQA2(16), SHFQA4(17)                       -> SHFQA(8)
//   SHFSG2(18), SHFSG4(19), SHFSG8(20)           -> SHFSG(16)
//   SHFQC(21) + seq==QA(1)                       -> SHFQA(8)
//   SHFQC(21) + seq==SG(2)                       -> SHFQC_SG(32)
//   SHFQC(21) + seq==Auto(0) or other            -> 0 (unsupported)
//   SHFLI(22)                                    -> SHFLI(64)
//   GHFLI(28)                                    -> GHFLI(128)
//   VHFLI(32)                                    -> VHFLI(256)
//   All other codes (0..3, 9..12, 15, 23..27, 29..31, 33+) -> 0
//
// Returns AwgDeviceType(0) (no enumerator) on unsupported combinations.
// ----------------------------------------------------------------------------
AwgDeviceType toAwgDeviceType(DeviceTypeCode code, AwgSequencerType seq);

// ----------------------------------------------------------------------------
// makeUnsupportedAwgSequencerErrorMessage(DeviceTypeCode, AwgSequencerType)
//   @ 0x2cbdd0
//
// Builds the user-facing error string used when toAwgDeviceType returns 0.
// Format (decoded from .rodata 0x90b138 / 0x90b15f / 0x90b16d):
//
//   "Unsupported device or sequencer type (<dtype>, sequencer: <seq>)."
//
// where <dtype> = toString(code) and <seq> = "auto"|"QA"|"SG"|"unknown"
// depending on the AwgSequencerType value (0/1/2/other).
// ----------------------------------------------------------------------------
std::string makeUnsupportedAwgSequencerErrorMessage(DeviceTypeCode code,
                                                    AwgSequencerType seq);

} // namespace zhinst
