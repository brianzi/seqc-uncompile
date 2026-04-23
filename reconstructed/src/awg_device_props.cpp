// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// awg_device_props.cpp — Phase 14b-iii
//
// Implements:
//   - AwgPathPatterns 3-arg ctor (header-defined ctors are defaulted)
//   - 6 anonymous-namespace AwgPathPatterns globals (Default, GrimselQa,
//     GrimselSg, GrimselLi, Gurnigel, Maloja) populated by static init
//     at binary 0xdc900..0xdce30
//   - AwgDeviceProps::~AwgDeviceProps() @ 0xf81e0 (out-of-line dtor)
//   - 9 explicit specializations of getAwgDeviceProps<T>(DeviceType const&)
//     at binary addresses 0x2cc5f0..0x2cdb00
//   - toAwgDeviceType(DeviceTypeCode, AwgSequencerType) @ 0x2cbd60
//   - makeUnsupportedAwgSequencerErrorMessage(DeviceTypeCode,
//                                              AwgSequencerType) @ 0x2cbdd0
//
// All field offsets and per-template constants verified by inspecting:
//   - the field-by-field store sequences in each getAwgDeviceProps<T>
//   - AwgDeviceProps::~AwgDeviceProps body (4-string layout)
//   - AwgPathPatterns ctor/dtor (3-string layout)
//   - the static-init code populating the 6 path-pattern globals
//   - .rodata strings at 0x90b138..0x90b446 and 0x90b216..0x90b3ec
//
// Note on ABI: the original binary was built against libc++ where
// std::string == 24B; this reconstruction targets libstdc++ where
// std::string == 32B. The struct sizes (0x60 / 0xa0) here therefore
// differ from the binary's (0x48 / 0x80) but field-by-field semantics
// are preserved.
// ============================================================================

#include <zhinst/awg_device_props.hpp>

#include <zhinst/device_type.hpp>   // DeviceType, DeviceOption

namespace zhinst {

// ----------------------------------------------------------------------------
// AwgPathPatterns 3-arg ctor
// ----------------------------------------------------------------------------
AwgPathPatterns::AwgPathPatterns(std::string elfData,
                                 std::string elfProgress,
                                 std::string enable)
    : elfDataPattern(std::move(elfData)),
      elfProgressPattern(std::move(elfProgress)),
      enablePattern(std::move(enable))
{}

// ----------------------------------------------------------------------------
// AwgDeviceProps::~AwgDeviceProps  @ 0xf81e0
//
// The binary has an explicit out-of-line dtor (called from at least one
// site at 0xf7b83). With our default in-class destruction this would be
// inlined, but we emit it explicitly so that other TUs can rely on
// AwgDeviceProps having a key function in this TU.
// ----------------------------------------------------------------------------
AwgDeviceProps::~AwgDeviceProps() = default;

// ----------------------------------------------------------------------------
// Anonymous-namespace AwgPathPatterns globals.
//
// Original binary builds these via static init at 0xdc900..0xdce30 using
// per-string operator new + manual buffer fill. Here we just
// default-construct Meyers-singleton helpers — semantically equivalent.
//
// String contents copied verbatim from .rodata:
//   Default     -> 0x90b216 + 0x90b236 + 0x90b25a
//   GrimselQa   -> 0x90b278 + 0x90b2a8 + 0x90b2dc
//   GrimselSg   -> 0x90b30a + 0x90b334 + 0x90b362
//   GrimselLi   -> 0x90b38a + 0x90b3ae + 0x90b3d6
//   Gurnigel    -> copy-constructed from GrimselLi at static-init time
//   Maloja      -> copy-constructed from GrimselLi at static-init time
// ----------------------------------------------------------------------------
namespace {

// String length annotations (binary-confirmed):
//  Default 1: cap=0x21 size=0x1d  ("/$device$/awgs/$index$/elf/data"           29)
//  Default 2: cap=0x29 size=0x23  ("/$device$/awgs/$index$/elf/progress"       35)
//  Default 3: cap=0x21 size=0x1d  ("/$device$/awgs/$index$/enable"             29)
const AwgPathPatterns& awgPathPatternsDefault() {
    static const AwgPathPatterns p(
        "/$device$/awgs/$index$/elf/data",
        "/$device$/awgs/$index$/elf/progress",
        "/$device$/awgs/$index$/enable"
    );
    return p;
}

//  GrimselQa 1: cap=0x31 size=0x2f  ("/$device$/qachannels/$index$/generator/elf/data"     47)
//  GrimselQa 2: cap=0x39 size=0x33  ("/$device$/qachannels/$index$/generator/elf/progress" 51)
//  GrimselQa 3: cap=0x31 size=0x2d  ("/$device$/qachannels/$index$/generator/enable"       45)
const AwgPathPatterns& awgPathPatternsGrimselQa() {
    static const AwgPathPatterns p(
        "/$device$/qachannels/$index$/generator/elf/data",
        "/$device$/qachannels/$index$/generator/elf/progress",
        "/$device$/qachannels/$index$/generator/enable"
    );
    return p;
}

//  GrimselSg 1: cap=0x31 size=0x29  ("/$device$/sgchannels/$index$/awg/elf/data"     41)
//  GrimselSg 2: cap=0x31 size=0x2d  ("/$device$/sgchannels/$index$/awg/elf/progress" 45)
//  GrimselSg 3: cap=0x29 size=0x27  ("/$device$/sgchannels/$index$/awg/enable"       39)
const AwgPathPatterns& awgPathPatternsGrimselSg() {
    static const AwgPathPatterns p(
        "/$device$/sgchannels/$index$/awg/elf/data",
        "/$device$/sgchannels/$index$/awg/elf/progress",
        "/$device$/sgchannels/$index$/awg/enable"
    );
    return p;
}

//  GrimselLi 1: cap=0x29 size=0x23  ("/$device$/raw/awgs/$index$/elf/data"     35)
//  GrimselLi 2: cap=0x29 size=0x27  ("/$device$/raw/awgs/$index$/elf/progress" 39)
//  GrimselLi 3: cap=0x29 size=0x21  ("/$device$/raw/awgs/$index$/enable"       33)
const AwgPathPatterns& awgPathPatternsGrimselLi() {
    static const AwgPathPatterns p(
        "/$device$/raw/awgs/$index$/elf/data",
        "/$device$/raw/awgs/$index$/elf/progress",
        "/$device$/raw/awgs/$index$/enable"
    );
    return p;
}

// Gurnigel and Maloja are copies of GrimselLi in the binary
// (constructed via copy-ctor at static-init time at 0xdcd20 / 0xdcd40).
const AwgPathPatterns& awgPathPatternsGurnigel() { return awgPathPatternsGrimselLi(); }
const AwgPathPatterns& awgPathPatternsMaloja  () { return awgPathPatternsGrimselLi(); }

// FPGA / slave-revision patterns used as the 4th string in AwgDeviceProps:
//   ".../system/fpgarevision"   29 chars  (.rodata 0x90b3f8)  — used by all except HDAWG
//   ".../system/slaverevision"  30 chars  (.rodata 0x90b416)  — used by HDAWG only
constexpr const char kFpgaRevisionPattern [] = "/$device$/system/fpgarevision";
constexpr const char kSlaveRevisionPattern[] = "/$device$/system/slaverevision";

// Helper that builds an AwgDeviceProps from the per-template inputs.
// Each getAwgDeviceProps<T> in the binary inlines this same pattern;
// hoisting it here keeps our source readable while preserving the
// observed field-store semantics.
AwgDeviceProps buildAwgDeviceProps(AwgDeviceType        type,
                                   const AwgPathPatterns& patterns,
                                   uint64_t             maxSamples,
                                   uint64_t             maxBytes,
                                   bool                 supportsExtra,
                                   const char*          fpgaPattern)
{
    AwgDeviceProps p{};
    p.deviceType            = type;
    p.elfDataPattern        = patterns.elfDataPattern;
    p.elfProgressPattern    = patterns.elfProgressPattern;
    p.enablePattern         = patterns.enablePattern;
    p.maxWaveformSamples    = maxSamples;
    p.maxWaveformBytes      = maxBytes;
    p.supportsExtraFeature  = supportsExtra;
    p.fpgaRevisionPattern   = fpgaPattern;
    return p;
}

} // namespace

// ----------------------------------------------------------------------------
// 9 explicit specializations of getAwgDeviceProps<T>.
//
// Per-template constants table (verified from disassembly):
//
//   T            addr      bool  +0x50         +0x58          patterns      4th-string
//   UHFLI(1)     0x2cc900   0    0x10000000    0xd0000000     Default       fpgarevision
//   HDAWG(2)     0x2ccb80   1    cond(ME)      0x180000000    Default       slaverevision
//   UHFQA(4)     0x2cc5f0   0    0x10000000    0xd0000000     Default       fpgarevision
//   SHFQA(8)     0x2cce30   0    0x80000000    0x200000000    GrimselQa     fpgarevision
//   SHFSG(16)    0x2cd0c0   1    0x80000000    0x100000000    GrimselSg     fpgarevision
//   SHFQC_SG(32) 0x2cd350   1    0x80000000    0x100000000    GrimselSg     fpgarevision
//   SHFLI(64)    0x2cd5e0   1    0x80000000    0x100000000    GrimselLi     fpgarevision
//   GHFLI(128)   0x2cdb00   1    0x80000000    0x100000000    Gurnigel      fpgarevision
//   VHFLI(256)   0x2cd870   1    0x80000000    0x100000000    Maloja        fpgarevision
//
// HDAWG is the ONLY template that consults `dt`:
//   r14d = dt.hasOption(DeviceOption(0x13))   // 0x13 = ME
//   maxSamples = r14d ? 0x80000000 : 0x10000000
// ----------------------------------------------------------------------------

// 0x2cc900
template <>
AwgDeviceProps getAwgDeviceProps<AwgDeviceType::UHFLI>(DeviceType const& /*dt*/) {
    return buildAwgDeviceProps(
        AwgDeviceType::UHFLI, awgPathPatternsDefault(),
        0x10000000ull, 0xd0000000ull, false, kFpgaRevisionPattern);
}

// 0x2ccb80
template <>
AwgDeviceProps getAwgDeviceProps<AwgDeviceType::HDAWG>(DeviceType const& dt) {
    // ME = DeviceOption value 0x13 — confirmed by `mov esi, 0x13` followed
    // by call to DeviceType::hasOption at 0x2ccb95.
    const bool hasME = dt.hasOption(static_cast<DeviceOption>(0x13));
    const uint64_t maxSamples = hasME ? 0x80000000ull : 0x10000000ull;
    return buildAwgDeviceProps(
        AwgDeviceType::HDAWG, awgPathPatternsDefault(),
        maxSamples, 0x180000000ull, true, kSlaveRevisionPattern);
}

// 0x2cc5f0
template <>
AwgDeviceProps getAwgDeviceProps<AwgDeviceType::UHFQA>(DeviceType const& /*dt*/) {
    return buildAwgDeviceProps(
        AwgDeviceType::UHFQA, awgPathPatternsDefault(),
        0x10000000ull, 0xd0000000ull, false, kFpgaRevisionPattern);
}

// 0x2cce30
template <>
AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFQA>(DeviceType const& /*dt*/) {
    return buildAwgDeviceProps(
        AwgDeviceType::SHFQA, awgPathPatternsGrimselQa(),
        0x80000000ull, 0x200000000ull, false, kFpgaRevisionPattern);
}

// 0x2cd0c0
template <>
AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFSG>(DeviceType const& /*dt*/) {
    return buildAwgDeviceProps(
        AwgDeviceType::SHFSG, awgPathPatternsGrimselSg(),
        0x80000000ull, 0x100000000ull, true, kFpgaRevisionPattern);
}

// 0x2cd350
template <>
AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFQC_SG>(DeviceType const& /*dt*/) {
    return buildAwgDeviceProps(
        AwgDeviceType::SHFQC_SG, awgPathPatternsGrimselSg(),
        0x80000000ull, 0x100000000ull, true, kFpgaRevisionPattern);
}

// 0x2cd5e0
template <>
AwgDeviceProps getAwgDeviceProps<AwgDeviceType::SHFLI>(DeviceType const& /*dt*/) {
    return buildAwgDeviceProps(
        AwgDeviceType::SHFLI, awgPathPatternsGrimselLi(),
        0x80000000ull, 0x100000000ull, true, kFpgaRevisionPattern);
}

// 0x2cdb00
template <>
AwgDeviceProps getAwgDeviceProps<AwgDeviceType::GHFLI>(DeviceType const& /*dt*/) {
    return buildAwgDeviceProps(
        AwgDeviceType::GHFLI, awgPathPatternsGurnigel(),
        0x80000000ull, 0x100000000ull, true, kFpgaRevisionPattern);
}

// 0x2cd870
template <>
AwgDeviceProps getAwgDeviceProps<AwgDeviceType::VHFLI>(DeviceType const& /*dt*/) {
    return buildAwgDeviceProps(
        AwgDeviceType::VHFLI, awgPathPatternsMaloja(),
        0x80000000ull, 0x100000000ull, true, kFpgaRevisionPattern);
}

// ----------------------------------------------------------------------------
// toAwgDeviceType(DeviceTypeCode, AwgSequencerType)  @ 0x2cbd60
//
// Body (verified from disassembly):
//   add  edi, -4               ; rebase code so jump table indexes by (code-4)
//   cmp  edi, 0x1c
//   ja   default               ; codes outside [4..32] -> 0
//   jmp  *(jump_table[edi])    ; jump table @ .rodata 0x961a38
//
// Jump-table results decoded by reading the per-target moves:
//   code 4  (UHF)      -> 1   (UHFLI)
//   code 5  (UHFLI)    -> 1   (UHFLI)
//   code 6  (UHFAWG)   -> 1   (UHFLI)
//   code 7  (UHFQA)    -> 4   (UHFQA)
//   code 8  (UHFIA)    -> 1   (UHFLI)
//   code 13 (HDAWG4)   -> 2   (HDAWG)
//   code 14 (HDAWG8)   -> 2   (HDAWG)
//   code 16 (SHFQA2)   -> 8   (SHFQA)
//   code 17 (SHFQA4)   -> 8   (SHFQA)
//   code 18 (SHFSG2)   -> 16  (SHFSG)
//   code 19 (SHFSG4)   -> 16  (SHFSG)
//   code 20 (SHFSG8)   -> 16  (SHFSG)
//   code 21 (SHFQC)    -> seq==1 -> 8 (SHFQA), seq==2 -> 32 (SHFQC_SG), else 0
//   code 22 (SHFLI)    -> 64  (SHFLI)
//   code 28 (GHFLI)    -> 128 (GHFLI)
//   code 32 (VHFLI)    -> 256 (VHFLI)
//   all other codes 4..32 -> 0 (jump-table slot points to the default 'xor eax,eax' path)
//
// The SHFQC branch uses a clever cmov sequence:
//     xor   ecx, ecx
//     cmp   esi, 1            ; sequencer == QA?
//     sete  cl
//     shl   ecx, 3            ; cl = 8 if QA else 0
//     cmp   esi, 2            ; sequencer == SG?
//     mov   eax, 0x20         ; provisional: 32 (SHFQC_SG)
//     cmovne eax, ecx         ; if not SG, take cl (0 or 8)
// ----------------------------------------------------------------------------
AwgDeviceType toAwgDeviceType(DeviceTypeCode code, AwgSequencerType seq) {
    const int c = static_cast<int>(code);
    switch (c) {
        case  4: case  5: case  6: case  8:
            return AwgDeviceType::UHFLI;       // 1
        case  7:
            return AwgDeviceType::UHFQA;       // 4
        case 13: case 14:
            return AwgDeviceType::HDAWG;       // 2
        case 16: case 17:
            return AwgDeviceType::SHFQA;       // 8
        case 18: case 19: case 20:
            return AwgDeviceType::SHFSG;       // 16
        case 21: { // SHFQC: depends on sequencer
            if (seq == AwgSequencerType::QA) return AwgDeviceType::SHFQA;     // 8
            if (seq == AwgSequencerType::SG) return AwgDeviceType::SHFQC_SG;  // 32
            return static_cast<AwgDeviceType>(0);                              // unsupported
        }
        case 22:
            return AwgDeviceType::SHFLI;       // 64
        case 28:
            return AwgDeviceType::GHFLI;       // 128
        case 32:
            return AwgDeviceType::VHFLI;       // 256
        default:
            return static_cast<AwgDeviceType>(0);
    }
}

// ----------------------------------------------------------------------------
// makeUnsupportedAwgSequencerErrorMessage  @ 0x2cbdd0
//
// Constructs:
//   "Unsupported device or sequencer type (<dtype>, sequencer: <seq>)."
//
// where:
//   <dtype> = toString(code)         — produced by zhinst::toString(DeviceTypeCode)
//   <seq>   = "auto" | "QA" | "SG" | "unknown"
//             depending on (int)seq == 0 / 1 / 2 / other
//
// Decoded from inline string-builder ops (insert/append) and the
// SSO-string immediates:
//   seq==0 -> 4-char "auto"  (0x6f747561 LE) , size 4
//   seq==1 -> 2-char "QA"    (0x4151   LE)  , size 2
//   seq==2 -> 2-char "SG"    (0x4753   LE)  , size 2
//   else   -> 7-char "unknown" (split LE writes), size 7
// ----------------------------------------------------------------------------
std::string makeUnsupportedAwgSequencerErrorMessage(DeviceTypeCode code,
                                                    AwgSequencerType seq)
{
    // Sequencer-name dispatch matches the binary's SSO writes exactly.
    const char* seqName;
    switch (static_cast<int>(seq)) {
        case 0:  seqName = "auto";    break;
        case 1:  seqName = "QA";      break;
        case 2:  seqName = "SG";      break;
        default: seqName = "unknown"; break;
    }
    std::string out;
    out.reserve(64);
    out += "Unsupported device or sequencer type (";
    out += toString(code);
    out += ", sequencer: ";
    out += seqName;
    out += ").";
    return out;
}

} // namespace zhinst
