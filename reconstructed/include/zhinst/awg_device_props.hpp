// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// awg_device_props.hpp — Phase 14b-iii
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
// Phase 3d work (9 one-hot values: UHFLI=1, HDAWG=2, UHFQA=4, SHFQA=8,
// SHFSG=16, SHFQC_SG=32, SHFLI=64, GHFLI=128, VHFLI=256).
// ============================================================================
#pragma once

#include <cstdint>
#include <string>

#include "types.hpp"             // AwgDeviceType
#include "device_type.hpp"       // DeviceType, DeviceTypeCode

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
static_assert(sizeof(AwgPathPatterns) == 0x60,
              "AwgPathPatterns must be 0x60 bytes (3 std::strings @ 32B each, libstdc++ ABI). "
              "Note: in the original libc++ binary it is 0x48 bytes (3*24).");

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
//   +0x50  8    uint64_t maxWaveformSamples      (binary stores as qword)
//   +0x58  8    uint64_t maxWaveformBytes        (binary stores as qword)
//   +0x60  1    bool supportsExtraFeature        (semantics not yet
//                                                  pinned down; see notes)
//   +0x61  7    (padding)
//   +0x68  24   std::string fpgaRevisionPattern  (path pattern 4)
//   Total: 0x80 bytes.
//
// The +0x50/+0x58 limits are very probably waveform sample/byte limits
// but the exact semantics are inferred from values, not confirmed by
// callers (TODO: verify in next sub-phase). Per-template values:
//
//   UHFLI/UHFQA       : +0x50 = 0x10000000 (256M),  +0x58 = 0xd0000000  (~3.5G)
//   HDAWG             : +0x50 = (ME ? 0x80000000 : 0x10000000), +0x58 = 0x180000000 (6G)
//   SHFQA             : +0x50 = 0x80000000 (2G),    +0x58 = 0x200000000 (8G)
//   SHFSG/SHFQC_SG    : +0x50 = 0x80000000 (2G),    +0x58 = 0x100000000 (4G)
//   SHFLI/GHFLI/VHFLI : +0x50 = 0x80000000 (2G),    +0x58 = 0x100000000 (4G)
//
// HDAWG is the ONLY template that consults `dt`: it calls
// `dt.hasOption(DeviceOption(0x13)=ME)` and picks the higher limit when
// the ME (Memory Extension) option is set.
// ----------------------------------------------------------------------------
struct AwgDeviceProps {
    // NOTE: field NAMES below are INFERRED from values, not confirmed by
    // callers. Field offsets and types are confirmed from disassembly.
    // TODO(14b-iv): Verify field names by inspecting consumers.
    AwgDeviceType deviceType;          // +0x00
    std::string   elfDataPattern;      // +0x08
    std::string   elfProgressPattern;  // +0x20
    std::string   enablePattern;       // +0x38
    uint64_t      maxWaveformSamples;  // +0x50  (inferred name)
    uint64_t      maxWaveformBytes;    // +0x58  (inferred name)
    bool          supportsExtraFeature;// +0x60  (inferred name; semantics TBD)
    std::string   fpgaRevisionPattern; // +0x68  (also slaverevision for HDAWG)
    // Total: 0x80 bytes.

    ~AwgDeviceProps();   // 0xf81e0 — explicit out-of-line dtor
};
static_assert(sizeof(AwgDeviceProps) == 0xa0,
              "AwgDeviceProps must be 0xa0 bytes on libstdc++ (4*32B strings + 4+4 type/pad + 8 + 8 + 1 + 7 pad). "
              "Note: in the original libc++ binary it is exactly 0x80 (128) bytes.");

// ----------------------------------------------------------------------------
// getAwgDeviceProps<AwgDeviceType>(DeviceType const&)
//
// Per-AwgDeviceType factory. Each of the 9 explicit instantiations is a
// unique function in the binary that bakes in:
//   - the AwgDeviceType bit (.deviceType field)
//   - the per-family AwgPathPatterns (3 path strings)
//   - the FPGA-revision path string (varies only for HDAWG)
//   - the maxWaveformSamples/Bytes limits
//   - the supportsExtraFeature bool
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
