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
//! \brief Selects which sequencer kind is targeted on a multi-sequencer
//! device.
//!
//! Only relevant for device families that host more than one sequencer
//! kind behind a single device code (currently just the SHFQC, which
//! exposes both a QA generator and an SG sequencer); for single-kind
//! families the value is ignored.  Used by `toAwgDeviceType` to resolve
//! the `(DeviceTypeCode, sequencer)` pair into a concrete
//! `AwgDeviceType` and by `toString` / the unsupported-sequencer error
//! formatter to render the user-facing name.
enum class AwgSequencerType : int {
    Auto = 0,   //!< \brief Family default: pick the only sequencer on
                //!  single-sequencer devices, or fail (unsupported) on
                //!  multi-sequencer devices where an explicit choice is
                //!  required.
                // toString -> "auto"
    QA   = 1,   //!< \brief Quantum-analyser sequencer (the QA generator
                //!  on SHFQA / SHFQC).
                // toString -> "QA"
    SG   = 2,   //!< \brief Signal-generator sequencer (the SG sequencer
                //!  on SHFSG / SHFQC).
                // toString -> "SG"
};

//! \brief Renders an `AwgSequencerType` as the short human-readable
//! name used in error messages and diagnostics (`"auto"`, `"QA"`,
//! `"SG"`, or `"unknown"` for out-of-range values).
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
    //! \brief Node-path template (with `%d`-style placeholders) locating the AWG's ELF data sink.
    std::string elfDataPattern;        // +0x00
    //! \brief Node-path template locating the AWG's ELF upload-progress / status node.
    std::string elfProgressPattern;    // +0x18
    //! \brief Node-path template locating the AWG's run / enable node.
    std::string enablePattern;         // +0x30
    // Total: 0x48 bytes.

    //! \brief Default-construct with empty pattern strings (used as a
    //!        zero-initialised placeholder for unknown families).
    // Default ctor — produces empty strings (default for unknown families).
    AwgPathPatterns() = default;

    //! \brief Construct from the three node-path template strings.
    //! \param elfData      Template stored in `elfDataPattern`.
    //! \param elfProgress  Template stored in `elfProgressPattern`.
    //! \param enable       Template stored in `enablePattern`.
    // Three-arg convenience ctor used to populate the named globals.
    AwgPathPatterns(std::string elfData,
                    std::string elfProgress,
                    std::string enable);

    //! \brief Deep-copy ctor (member-wise copy of the three strings).
    // 0x2cc4f0 — copy ctor (deep-copies the 3 strings).
    AwgPathPatterns(AwgPathPatterns const&) = default;

    //! \brief Release the three owned strings in reverse declaration order.
    // 0x2cc480 — dtor.
    ~AwgPathPatterns() = default;

    //! \brief Defaulted move constructor.
    AwgPathPatterns(AwgPathPatterns&&) = default;
    //! \brief Defaulted copy assignment.
    //! \return  Reference to `*this`.
    AwgPathPatterns& operator=(AwgPathPatterns const&) = default;
    //! \brief Defaulted move assignment.
    //! \return  Reference to `*this`.
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
    //! \brief One-hot AWG device-family bit.
    AwgDeviceType deviceType;          // +0x00
    //! \brief Node-path template for the AWG's ELF data sink.
    std::string   elfDataPattern;      // +0x08
    //! \brief Node-path template for the AWG's ELF upload-progress node.
    std::string   elfProgressPattern;  // +0x20
    //! \brief Node-path template for the AWG's run / enable node.
    std::string   enablePattern;       // +0x38
    //! \brief Maximum permitted size of the emitted ELF (JSON key `"maxelfsize"`).
    uint64_t      maxElfSize;          // +0x50  (max ELF binary size; JSON "maxelfsize")
    //! \brief Waveform-memory base address (also the program entry point).
    uint32_t      addressImpl;         // +0x58  (waveform memory base address)
    //! \brief Wave-sample storage format (`SampleFormat` enum encoded as `0`/`1`/`2`).
    uint32_t      sampleFormat;        // +0x5c  (SampleFormat enum: 0/1/2)
    //! \brief `true` for Hirzel-generation device families (selects the modern sequencer).
    bool          isHirzel;            // +0x60  (true for Hirzel-gen devices)
    //! \brief Node-path template for the FPGA revision (and slave revision on HDAWG).
    std::string   fpgaRevisionPattern; // +0x68  (also slaverevision for HDAWG)
    // Total: 0x80 bytes.

    //! \brief Release the four owned strings in reverse declaration order.
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
// ----------------------------------------------------------------------------
//! \brief Returns the `AwgDeviceProps` for a specific AWG device family,
//! selected at compile time by the `AwgDeviceType` template argument.
//!
//! Each AWG-capable device family has a fixed set of properties — the
//! family bit, the three node-path templates (data / progress /
//! enable), the FPGA- or slave-revision path, the maximum permitted
//! ELF size, the waveform memory base address, the wave-sample storage
//! format, and the `isHirzel` generation flag — that the compiler and
//! ELF writer need to drive the rest of the pipeline.  Those values
//! are baked into one explicit specialisation per family, so the
//! template argument both names the target family and selects the
//! correct value table without a runtime dispatch.
//!
//! For most families the `DeviceType` argument is ignored; the
//! HDAWG specialisation is the only one that inspects it (to enable
//! the larger ELF-size limit when the Memory Extension option is
//! present).  The set of explicit specialisations matches the set of
//! one-hot `AwgDeviceType` bits exactly — there is no fallback for
//! an arbitrary `AwgDeviceType` value.
//!
//! \tparam T One-hot `AwgDeviceType` selecting the family whose
//!         properties to return.  Must be one of `UHFLI`, `HDAWG`,
//!         `UHFQA`, `SHFQA`, `SHFSG`, `SHFQC_SG`, `SHFLI`, `GHFLI`,
//!         or `VHFLI`; any other value is a link-time error
//!         (no primary-template definition exists).
//! \param dt Device handle, consulted only by the HDAWG
//!         specialisation; ignored by all other specialisations.
//! \return The fixed `AwgDeviceProps` value table for the selected
//!         family.  The HDAWG return is parameterised on
//!         `dt.options()` to honour the Memory Extension option's
//!         larger ELF-size limit.
template <AwgDeviceType T>
AwgDeviceProps getAwgDeviceProps(DeviceType const& dt);

//! \cond INTERNAL
// Explicit specialization DECLARATIONS (definitions in awg_device_props.cpp).
// We do not provide a primary template definition: the binary contains
// only these 9 specializations, each emitted as a unique function body.
// Hidden from the public Doxygen site because Doxygen 1.9.x cannot
// bind doc blocks to explicit specialisations; the primary template
// above carries the full public contract.

template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::UHFLI>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::HDAWG>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::UHFQA>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFQA>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFSG>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFQC_SG>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFLI>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::GHFLI>(DeviceType const&);
template <> AwgDeviceProps getAwgDeviceProps<AwgDeviceType::VHFLI>(DeviceType const&);
//! \endcond

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
//! \brief Resolves a `(DeviceTypeCode, AwgSequencerType)` pair to the
//! one-hot `AwgDeviceType` bit identifying the AWG family the compiler
//! should target.
//!
//! The sequencer argument only affects multi-sequencer device codes —
//! currently just SHFQC, which resolves to `SHFQA` when the QA
//! sequencer is requested and to `SHFQC_SG` when the SG sequencer is
//! requested.  For all other supported codes the sequencer argument is
//! ignored.  Unsupported codes — and SHFQC with a missing or `Auto`
//! sequencer choice — return `AwgDeviceType(0)`, which callers treat
//! as the "no matching family" sentinel and surface via
//! `makeUnsupportedAwgSequencerErrorMessage`.
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
//! \brief Builds the user-facing diagnostic shown when
//! `toAwgDeviceType` cannot resolve the given device code and
//! sequencer choice to a supported AWG family.
//!
//! The returned string has the form
//! `"Unsupported device or sequencer type (<device>, sequencer: <seq>)."`,
//! where `<device>` is `toString(code)` and `<seq>` is one of
//! `"auto"`, `"QA"`, `"SG"`, or `"unknown"` per `toString(seq)`.
std::string makeUnsupportedAwgSequencerErrorMessage(DeviceTypeCode code,
                                                    AwgSequencerType seq);

} // namespace zhinst
