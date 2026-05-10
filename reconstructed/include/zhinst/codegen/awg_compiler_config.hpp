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

#include "zhinst/core/types.hpp"
#include "zhinst/waveform/waveform_ir.hpp"   // for SampleFormat

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
// 0x20    4     int32_t                     awgIndex            AWG core index within device; used to build node paths
//                                                               (qachannels/<idx>, generators/<idx>) and validate channel ownership
// 0x24    4     int                         deviceIndex         Device index for waveform lookup (Prefetch::moveLoadsToFront)
// 0x28    8     (unknown)                   serializeRoundTrip
// 0x30    24    std::string                 debugDumpPath           Dtor frees if heap-allocated; dtor checks +0x48 flag
// 0x48    1     bool                        debugDumpEnabled     If true, debugDumpPath was allocated
//         7     (pad)
// 0x50    24    std::string                 debugJsonPath           Dtor frees if heap-allocated; dtor checks +0x68 flag
// 0x68    1     bool                        debugJsonEnabled     If true, debugJsonPath was allocated
//         7     (pad)
// 0x70    24    std::vector<std::string>    includePaths        vector: begin @+0x70, end @+0x78, cap @+0x80
// 0x88    8     (unknown)                   optimizationFlags
// 0x90    4     uint32_t                    debugFlags          Bitmask: 0x02=print old AST, 0x04=print SeqC AST,
//                                                               0x08=print tree/assembly (verified: testb at 0x11f379)
// 0x94    4     int32_t                     numCores            Number of AWG cores
// 0x98    4     int32_t                     loopUnrollLimit     Passed to FrontEndLoweringFacade::lower() (verified 0x11f8d4)
// 0x9C    4     (pad)
// 0xA0    4     int32_t                     wavetableSize       sign-extended to size_t, passed to WavetableFront
// 0xA4    4     (pad)
// 0xA8    24    boost::filesystem::path     searchPath          Path passed to WavetableFront; dtor frees at +0xA8/+0xB8
//
// Note: libc++ std::string is 24 bytes (SSO). Layout: [capacity_or_sso_flag | size | data_ptr_or_inline]
//       Actually in libc++ SSO: byte 0 = short flag/len, bytes 1-22 = inline data, OR
//       byte 0 bit0=1 means long: [cap|size|ptr] at offsets 0,8,16.
//       The dtor checks bit0 of the first byte to decide if heap-allocated.
// ============================================================================

//! \brief All compile-time configuration for an `AWGCompiler` invocation.
//!
//! Bundles the device identity (`deviceType`, `isHirzel`,
//! `cacheType`, `addressImpl`, `awgIndex`, `deviceIndex`,
//! `numChannelGroups`, `channelsPerGroup`), the runtime sample-rate
//! (`deviceSampleRate`, NaN meaning "use the device's default"), the
//! waveform-output policy (`sampleFormat`, `wavetableSize`,
//! `searchPath`, `includePaths`), and a set of debug toggles
//! (`debugDumpEnabled` / `debugDumpPath`, `debugJsonEnabled` /
//! `debugJsonPath`, `debugFlags`, `serializeRoundTrip`,
//! `loopUnrollLimit`).  A reference is held by `Compiler` for the
//! lifetime of one compilation; consumers throughout the back end
//! query individual fields rather than copying the whole struct.
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
    int32_t awgIndex;                   // 0x20 — AWG core index (node paths + channel validation)
    int32_t deviceIndex;                // 0x24 — Device index for waveform lookup
    uint64_t serializeRoundTrip;                // 0x28 — no reconstructed consumer; may be set by AWGCompilerImpl
    std::string debugDumpPath;              // 0x30 — 24 bytes, conditionally owned
    bool debugDumpEnabled;               // 0x48
    char pad_49[7];                     // 0x49
    std::string debugJsonPath;              // 0x50 — 24 bytes, conditionally owned
    bool debugJsonEnabled;               // 0x68
    char pad_69[7];                     // 0x69
    std::vector<std::string> includePaths; // 0x70 — begin/end/cap at 0x70/0x78/0x80
    uint64_t optimizationFlags;                // 0x88 — no reconstructed consumer; adjacent to debugFlags
    uint32_t debugFlags;                // 0x90 — bitmask: 0x02=old AST, 0x04=SeqC AST, 0x08=tree/asm
    int32_t numCores = 0;               // 0x94 — number of AWG cores (binary default is 0)
    int32_t loopUnrollLimit;            // 0x98 — passed to FrontEndLoweringFacade::lower() as last arg
                                        //        (verified: mov eax,[rax+0x98] at 0x11f8d4 in compile())
    uint8_t pad_9c;                     // 0x9C — padding
    bool compressSource;                // 0x9D — if true, compress source sections in ELF (verified at 0x108f48)
    uint8_t pad_9e[2];                  // 0x9E — padding
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
    //   appendMode    : was actually `isHirzel` test at +0x18 — DROPPED (callers
    //                   now read config_->isHirzel directly)
    //   splitIndex    : same field as `cacheType` at +0x19 (verified
    //                   0x1d6c4d in Prefetch::clampToCache:
    //                   movzx edx, BYTE PTR [rax+0x19])
    //   syncVersion   : same field as `numChannelGroups` at +0x1C (verified
    //                   0x1d7bb5 in Prefetch::placeSingleCommand and
    //                   0x270b1c in getChannelGroupingModeString:
    //                   cmp DWORD PTR [rax+0x1c], <imm>)
    //
    // Hallucinated placeholder fields removed entirely:
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
    // Remaining forwarding accessors: all dropped (Cluster E).
    // Callers now use fields directly: cacheType, numChannelGroups.

    //! \brief Release the heap allocations owned by the
    //!        config struct.
    //!
    //! \details Member subobjects' destructors handle the actual
    //! frees in reverse declaration order: `searchPath`'s
    //! `boost::filesystem::path` (which owns a `std::string`),
    //! `includePaths`'s vector buffer plus each element string,
    //! `debugJsonPath`, and `debugDumpPath`.  In the binary the
    //! two debug-path strings are only freed when their
    //! respective `*Enabled` flags are set; the libstdc++ recon
    //! relies on the SSO short-string check inside
    //! `std::string::~string` to achieve the same effect for
    //! short or never-initialised strings.
    ~AWGCompilerConfig();

    //! \brief Map an `AwgDeviceType` enum value to its short
    //!        device-family display string.
    //!
    //! \details Recognises the bit-flag enum values
    //! `1`→`"UHFLI"`, `2`→`"HDAWG"`, `4`→`"UHFQA"`,
    //! `8`→`"SHFQA"`, `16`→`"SHFSG"`, `32`→`"SHFQC (SG)"`,
    //! `64`→`"SHFLI"`, `128`→`"GHFLI"`, `256`→`"VHFLI"`.  Any
    //! other value (including `0` and combinations of multiple
    //! flags) returns the empty string.  The strings are short
    //! enough to fit in libc++'s SSO inline storage and the
    //! binary constructs them with single-instruction immediate
    //! moves rather than rodata loads.
    //!
    //! Used by `AWGCompilerImpl::compileString` to format the
    //! device-name argument of the `ErrorMessageT(0xDA)` /
    //! `ErrorMessageT(0xDB)` "unsupported device" diagnostics.
    //!
    //! \param type  Device-type enum value.
    //! \return  Short device-family name, or empty when `type`
    //!          is not one of the recognised single-bit values.
    static std::string getAwgDeviceTypeString(AwgDeviceType type);
    //! \brief Inverse of `getAwgDeviceTypeString`, but keyed by
    //!        the internal **codename** rather than the
    //!        marketing string.
    //!
    //! \details Performs case-insensitive
    //! `boost::algorithm::iequals` comparisons against the
    //! internal Zurich Instruments project codenames:
    //! `"cervino"`→`UHFLI`, `"hirzel"`→`HDAWG`,
    //! `"klausen"`→`UHFQA`, `"grimsel_qa"`→`SHFQA`,
    //! `"grimsel_sg"`→`SHFSG`, `"grimsel_qc_sg"`→`SHFQC (SG)`,
    //! `"grimsel_li"`→`SHFLI`, `"gurnigel"`→`GHFLI`,
    //! `"maloja"`→`VHFLI`.  These are the names used in
    //! firmware build identifiers and internal device
    //! manifests, not the customer-facing product names that
    //! `getAwgDeviceTypeString` produces.  An unknown
    //! codename raises a `ZIAWGCompilerException` formatted
    //! with `ErrorMessageT(0xD9)` carrying `str`.
    //!
    //! \binarynote The inverse function for the
    //!             marketing-string form (e.g. `"HDAWG"` →
    //!             `2`) is **not** provided — callers needing
    //!             that lookup must do their own table walk.
    //!
    //! \param str  Device codename, case-insensitive.
    //! \return  The corresponding `AwgDeviceType` enum value.
    //! \throws ZIAWGCompilerException  When `str` does not
    //!         match any known codename.
    static AwgDeviceType getAwgDeviceTypeFromString(std::string const& str);
    //! \brief Render the per-instance channel-grouping mode as
    //!        a short display string.
    //!
    //! \details Only meaningful for HDAWG (`deviceType == 2`);
    //! every other device type returns the empty string.  For
    //! HDAWG the `numChannelGroups` field is mapped to:
    //! `1`→`"4x2"` (four 2-channel groups), `2`→`"2x4"` (two
    //! 4-channel groups), `4`→`"1x8"` (one 8-channel group).
    //! Any other `numChannelGroups` value returns the empty
    //! string.  The mode strings describe how the eight HDAWG
    //! output channels are partitioned across the AWG cores.
    //!
    //! Used in compiler diagnostics and the JSON arguments
    //! section to describe the active grouping at compile
    //! time.
    //!
    //! \return  Channel-grouping mode string, or empty for
    //!          non-HDAWG devices and unsupported group counts.
    std::string getChannelGroupingModeString() const;
};

} // namespace zhinst
