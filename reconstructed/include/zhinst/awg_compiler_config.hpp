// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Class: zhinst::AWGCompilerConfig
// Methods: dtor @0xf8080, getAwgDeviceTypeString @0x270080,
//          getAwgDeviceTypeFromString @0x270180, getChannelGroupingModeString @0x270b10
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <boost/filesystem/path.hpp>

#include "types.hpp"

namespace zhinst {

// ============================================================================
// AWGCompilerConfig struct layout
// Total size: 0xC0 (192 bytes) — based on dtor field accesses up to 0xB8
//
// Offset  Size  Type                        Field Name          Notes
// ------  ----  --------------------------  ------------------  ----------------------------
// 0x00    4     AwgDeviceType (int)         deviceType          Checked by getChannelGroupingModeString, AsmCommands
// 0x04    4     int                         unknown_04          Padding or small field
// 0x08    4     int                         unknown_08
// 0x0C    4     int                         unknown_0c
// 0x10    4     uint32_t                    addressImpl         Passed as AddressImpl<uint> to WavetableFront
// 0x14    4     int                         unknown_14
// 0x18    1     bool                        isHirzel            1 = Hirzel device (verified: cmpb $0x1,0x18(%rax) at 0x1d6c47)
// 0x19    1     uint8_t                     cacheType           0=Normal, 1=Aligned (verified: movzbl 0x19(%rax) at 0x1d6c4d)
// 0x1A    2     (padding)
// 0x1C    4     int                         numChannelGroups    Used by getChannelGroupingModeString (values 1,2,4)
// 0x20    4     (unknown)                   unknown_20
// 0x24    4     int                         deviceIndex         Device index for waveform lookup (Prefetch::moveLoadsToFront)
// 0x28    8     (unknown)                   unknown_28
// 0x30    24    std::string                 string_30           Dtor frees if heap-allocated; dtor checks +0x48 flag
// 0x48    1     bool                        string_30_owned     If true, string_30 was allocated
//         7     (pad)
// 0x50    24    std::string                 string_50           Dtor frees if heap-allocated; dtor checks +0x68 flag
// 0x68    1     bool                        string_50_owned     If true, string_50 was allocated
//         7     (pad)
// 0x70    24    std::vector<std::string>    includePaths        vector: begin @+0x70, end @+0x78, cap @+0x80
// 0x88    8     (unknown)                   unknown_88
// 0x90    4     uint32_t                    debugFlags          Bitmask: 0x02=print old AST, 0x04=print SeqC AST,
//                                                               0x08=print tree/assembly (verified: testb at 0x11f379)
// 0x94    12    (unknown)                   unknown_94          Padding or additional fields
// 0xA0    4     int32_t                     wavetableSize       sign-extended to size_t, passed to WavetableFront
// 0xA4    4     (pad)
// 0xA8    24    boost::filesystem::path     searchPath          Path passed to WavetableFront; dtor frees at +0xA8/+0xB8
//
// Note: libc++ std::string is 24 bytes (SSO). Layout: [capacity_or_sso_flag | size | data_ptr_or_inline]
//       Actually in libc++ SSO: byte 0 = short flag/len, bytes 1-22 = inline data, OR
//       byte 0 bit0=1 means long: [cap|size|ptr] at offsets 0,8,16.
//       The dtor checks bit0 of the first byte to decide if heap-allocated.
// ============================================================================

struct AWGCompilerConfig {
    AwgDeviceType deviceType;           // 0x00
    int unknown_04;                     // 0x04
    int unknown_08;                     // 0x08
    int unknown_0c;                     // 0x0C
    uint32_t addressImpl;               // 0x10 — AddressImpl<unsigned int> value
    int unknown_14;                     // 0x14
    bool isHirzel;                      // 0x18 — 1 = Hirzel device (cmpb $0x1,0x18(%rax) at 0x1d6c47)
    uint8_t cacheType;                  // 0x19 — 0=Normal, 1=Aligned (movzbl 0x19(%rax) at 0x1d6c4d)
    char pad_1a[2];                     // 0x1A — padding
    int numChannelGroups;               // 0x1C — 1, 2, or 4
    uint64_t unknown_20;                // 0x20
    uint64_t unknown_28;                // 0x28
    std::string string_30;              // 0x30 — 24 bytes, conditionally owned
    bool string_30_owned;               // 0x48
    char pad_49[7];                     // 0x49
    std::string string_50;              // 0x50 — 24 bytes, conditionally owned
    bool string_50_owned;               // 0x68
    char pad_69[7];                     // 0x69
    std::vector<std::string> includePaths; // 0x70 — begin/end/cap at 0x70/0x78/0x80
    uint64_t unknown_88;                // 0x88 — 8 bytes unknown
    uint32_t debugFlags;                // 0x90 — bitmask: 0x02=old AST, 0x04=SeqC AST, 0x08=tree/asm
    char unknown_94[12];                // 0x94 — 12 bytes unknown
    int32_t wavetableSize;              // 0xA0 — sign-extended to size_t
    int32_t pad_a4;                     // 0xA4
    boost::filesystem::path searchPath; // 0xA8 — dtor frees; path is a string wrapper (24 bytes)

    ~AWGCompilerConfig();

    static std::string getAwgDeviceTypeString(AwgDeviceType type);
    static AwgDeviceType getAwgDeviceTypeFromString(std::string const& str);
    std::string getChannelGroupingModeString() const;
};

} // namespace zhinst
