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
#include "waveform_ir.hpp"   // for SampleFormat

namespace zhinst {

// ============================================================================
// AWGCompilerConfig struct layout
// Total size: 0xC0 (192 bytes) — based on dtor field accesses up to 0xB8
//
// Offset  Size  Type                        Field Name          Notes
// ------  ----  --------------------------  ------------------  ----------------------------
// 0x00    4     AwgDeviceType (int)         deviceType          Checked by getChannelGroupingModeString, AsmCommands
// 0x04    4     SampleFormat (int)          sampleFormat        Used by writeWavesToElf* lambdas (0x10e049, 0x10e1f2)
// 0x08    8     double                      deviceSampleRate    NaN→default; verified movsd xmm0,[r14+0x8] at 0x1ee69b
//                                                               in StaticResources::init
// 0x10    4     uint32_t                    addressImpl         Passed as AddressImpl<uint> to WavetableFront
// 0x14    4     uint16_t[2]                 channelsPerGroup    [0]=normal, [1]=indexed; read by PlayArgs ctor
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
    SampleFormat sampleFormat;              // 0x04 — used by writeWavesToElf* (verified at 0x10e049, 0x10e1f2)
    double deviceSampleRate;            // 0x08 — NaN means "use default"; read in StaticResources::init at 0x1ee69b
    uint32_t addressImpl;               // 0x10 — AddressImpl<unsigned int> value
    uint16_t channelsPerGroup[2];       // 0x14 — [0]=normal, [1]=indexed; read by PlayArgs ctor
    bool isHirzel;                      // 0x18 — 1 = Hirzel device (cmpb $0x1,0x18(%rax) at 0x1d6c47)
    uint8_t cacheType;                  // 0x19 — 0=Normal, 1=Aligned (movzbl 0x19(%rax) at 0x1d6c4d)
    char pad_1a[2];                     // 0x1A — padding
    int numChannelGroups;               // 0x1C — 1, 2, or 4
    int32_t unknown_20;                 // 0x20
    int32_t deviceIndex;                // 0x24 — Device index for waveform lookup
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
    int32_t numCores = 1;               // 0x94 — number of AWG cores
    char unknown_98[8];                 // 0x98 — 8 bytes unknown
    int32_t wavetableSize;              // 0xA0 — sign-extended to size_t
    int32_t pad_a4;                     // 0xA4
    boost::filesystem::path searchPath; // 0xA8 — dtor frees; path is a string wrapper (24 bytes)

    // ========================================================================
    // Aliases / forwarding accessors (no separate storage)
    //
    // Several reconstructed .cpp call sites used misleading names that turned
    // out to be reads of existing fields. Provide accessors so legacy call
    // sites continue to compile while the names are migrated:
    //
    //   appendMode    : was actually `isHirzel` test at +0x18 (verified
    //                   0x1cbfc6 in Prefetch::placeLoads:
    //                   cmp BYTE PTR [rax+0x18], 0x0)
    //   splitIndex    : same field as `cacheType` at +0x19 (verified
    //                   0x1d6c4d in Prefetch::clampToCache:
    //                   movzx edx, BYTE PTR [rax+0x19])
    //   syncVersion   : same field as `numChannelGroups` at +0x1C (verified
    //                   0x1d7bb5 in Prefetch::placeSingleCommand and
    //                   0x270b1c in getChannelGroupingModeString:
    //                   cmp DWORD PTR [rax+0x1c], <imm>)
    //
    // Hallucinated TBD fields removed entirely:
    //   - cacheSize        Was actually devConst_->waveformMemorySize at +0xC
    //                       (verified 0x1d9d4e: mov rax,[r15+0x8]; cmp ecx,[rax+0xc])
    //   - channelIndex     Was actually config_->cacheType at +0x19
    //                       (verified 0x1da355: movzx ecx,BYTE PTR [r15+0x19])
    //   - baseGrainSize    Was actually devConst_->waveformAlignment at +0x14
    //                       (verified 0x1da35d: mov eax,[devConst+0x14])
    //   - channelGrains    Was actually devConst_ uint32_t[2] at +0x18 indexed
    //                       by cacheType (cachePageCount/maxBlocks union view)
    //                       (verified 0x1da360: mov edx,[devConst+rcx*4+0x18])
    //   - seqCount         Was actually config_->deviceIndex at +0x24
    //                       (verified 0x1d7beb: cmp [config+0x24],0)
    bool appendMode() const     { return isHirzel; }
    uint8_t splitIndex() const  { return cacheType; }
    int syncVersion() const     { return numChannelGroups; }

    ~AWGCompilerConfig();

    static std::string getAwgDeviceTypeString(AwgDeviceType type);
    static AwgDeviceType getAwgDeviceTypeFromString(std::string const& str);
    std::string getChannelGroupingModeString() const;
};

} // namespace zhinst
