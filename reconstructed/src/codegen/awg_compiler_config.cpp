// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Class: zhinst::AWGCompilerConfig — method implementations
// ============================================================================

#include "zhinst/codegen/awg_compiler_config.hpp"
#include <boost/algorithm/string/predicate.hpp>  // iequals
#include <stdexcept>

namespace zhinst {

// 0xf8080 — AWGCompilerConfig::~AWGCompilerConfig()
//
// Destroys heap-allocated strings and the vector<string>.
// Destruction order (reverse of construction):
//   1. searchPath (+0xA8) — if long-string (bit0 set), free buffer at +0xB8
//   2. includePaths (+0x70) — destroy each element string, then free vector buffer
//   3. debugJsonPath (+0x50) — only if debugJsonEnabled (+0x68) is true AND string is heap
//   4. debugDumpPath (+0x30) — only if debugDumpEnabled (+0x48) is true AND string is heap
AWGCompilerConfig::~AWGCompilerConfig() {  // 0xf8080
    // searchPath at +0xA8: check if heap-allocated (bit0 of first byte)
    // Done implicitly by std::string/path dtor in real code.
    // The binary manually checks and calls operator delete.

    // includePaths at +0x70: iterate backwards destroying each string,
    // then free the vector's buffer.

    // debugJsonPath at +0x50: only freed if debugJsonEnabled (+0x68) is true
    // debugDumpPath at +0x30: only freed if debugDumpEnabled (+0x48) is true
}

// 0x270080 — static AWGCompilerConfig::getAwgDeviceTypeString(AwgDeviceType)
//
// Returns a short SSO string for the given device type.
// Uses a jump table for values 1..64, then explicit comparisons for 128, 256.
// Returns empty string for unknown types.
std::string AWGCompilerConfig::getAwgDeviceTypeString(AwgDeviceType type) {  // 0x270080
    switch (static_cast<int>(type)) {
        case 1:   return "UHFLI";       // 0x2700a3 — movl $0x4C464855 + 'I'
        case 2:   return "HDAWG";       // 0x27010a — movl $0x57414448 + 'G'
        case 4:   return "UHFQA";       // 0x2700f7 — movl $0x51464855 + 'A'
        case 8:   return "SHFQA";       // 0x2700e4 — movl $0x51464853 + 'A'
        case 16:  return "SHFSG";       // 0x27014c — movl $0x53464853 + 'G'
        case 32:  return "SHFQC (SG)";  // 0x270130 — movabs $0x5328204351464853 + 0x2947
        case 64:  return "SHFLI";       // 0x27011d — movl $0x4C464853 + 'I'
        case 128: return "GHFLI";       // 0x27015f — movl $0x4C464847 + 'I'
        case 256: return "VHFLI";       // 0x2700ca — movl $0x4C464856 + 'I'
        default:  return "";            // 0x2700dd
    }
}

// 0x270180 — static AWGCompilerConfig::getAwgDeviceTypeFromString(string const&)
//
// Case-insensitive comparison using boost::algorithm::iequals against codenames.
// Throws ZIAWGCompilerException (ErrorMessageT=0xd9) if no match.
AwgDeviceType AWGCompilerConfig::getAwgDeviceTypeFromString(std::string const& str) {  // 0x270180
    if (boost::algorithm::iequals(str, "cervino"))       return static_cast<AwgDeviceType>(1);    // 0x2701bb
    if (boost::algorithm::iequals(str, "hirzel"))        return static_cast<AwgDeviceType>(2);    // 0x2701f3
    if (boost::algorithm::iequals(str, "klausen"))       return static_cast<AwgDeviceType>(4);    // 0x27022b
    if (boost::algorithm::iequals(str, "grimsel_qa"))    return static_cast<AwgDeviceType>(8);    // 0x270263
    if (boost::algorithm::iequals(str, "grimsel_sg"))    return static_cast<AwgDeviceType>(16);   // 0x27029b
    if (boost::algorithm::iequals(str, "grimsel_qc_sg")) return static_cast<AwgDeviceType>(32);   // 0x2702d3
    if (boost::algorithm::iequals(str, "grimsel_li"))    return static_cast<AwgDeviceType>(64);   // 0x27030b
    if (boost::algorithm::iequals(str, "gurnigel"))      return static_cast<AwgDeviceType>(128);  // 0x27033f
    if (boost::algorithm::iequals(str, "maloja"))        return static_cast<AwgDeviceType>(256);  // 0x270373

    // No match — throw ZIAWGCompilerException with ErrorMessageT=0xd9 (217)
    // throw ZIAWGCompilerException(ErrorMessages::format<string>(0xd9, str));  // 0x2703b6
    throw std::runtime_error("Unknown device type: " + str);  // simplified
}

// 0x270b10 — AWGCompilerConfig::getChannelGroupingModeString() const
//
// Returns channel grouping description string based on deviceType and numChannelGroups.
// Only returns non-empty for HDAWG (type==2).
// numChannelGroups: 1 → "4x2", 2 → "2x4", 4 → "1x8", else → ""
std::string AWGCompilerConfig::getChannelGroupingModeString() const {  // 0x270b10
    if (static_cast<int>(deviceType) != 2) {  // 0x270b17: cmp $0x2, (%rsi)
        return "";                             // 0x270b46
    }

    switch (numChannelGroups) {               // 0x270b1c: mov 0x1c(%rsi), %ecx
        case 1:  return "4x2";                // 0x270b2e: SSO len=3, "4x2"
        case 2:  return "2x4";                // 0x270b54: SSO len=3, "2x4"
        case 4:  return "1x8";                // 0x270b6c: SSO len=3, "1x8"
        default: return "";                   // 0x270b46
    }
}

} // namespace zhinst
