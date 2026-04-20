// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveformFront — waveform metadata for the compiler frontend
//
// WaveformFront extends Waveform (base class, 0xD8 bytes).
// Total size: 0xF8 (248 bytes). Confirmed from allocate_shared: alloc 0x110
// minus 0x18 shared_ptr control block header = 0xF8.
//
// Field layout confirmed from:
//   - WaveformFront::WaveformFront(shared_ptr<WaveformFront>, string) at 0x2a2510
//   - WaveformFront::toString() const at 0x2c5120
//   - __shared_ptr_emplace<WaveformFront>::__on_zero_shared() at 0x2a1300
//   - WaveformIR::WaveformIR(shared_ptr<WaveformFront>) at 0x114da0
//   - AsmCommands::genPlayConfig() at 0x2789a0
//   - AsmCommands::asmPrefetch() at 0x278720
//   - AsmCommands::asmPlay() at 0x278b40
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "value.hpp"
#include "signal.hpp"

namespace zhinst {

// Forward declarations
struct DeviceConstants;

// ============================================================================
// Waveform::File::Type — waveform source format enum
// Confirmed from toString() output: "CSV", "RAW", "GEN", "UNDEF"
// ============================================================================
namespace Waveform {
    namespace File {
        enum class Type : int {
            CSV  = 0,
            RAW  = 1,
            GEN  = 2,
            // Other values → "UNDEF" in toString()
        };
    }
}

// ============================================================================
// Waveform base class — 0xD8 bytes
//
// CORRECTED Phase 3c: Signal is 0x58 bytes (not 48). The fields previously
// listed as +0xB0 markers, +0xC8 sampleWidth, +0xD0 numSamples are actually
// inside Signal (markerBits_, channels_, length_ respectively).
//
// Offset  Size  Type                    Name              Notes
// ------  ----  ----                    ----              -----
// 0x00    24    std::string             name              waveform name (SSO)
// 0x18     4    int (File::Type enum)   waveformType      0=CSV, 1=RAW, 2=GEN
// 0x1C     4    (padding)
// 0x20    24    std::string             secondaryName     additional name/path
// 0x38    16    shared_ptr<File>        file              source file reference
// 0x48     1    bool                    used              set by asmPlay/asmPrefetch
// 0x49     3    (padding)
// 0x4C     4    int                     field4C           unknown
// 0x50    24    std::string             thirdString       additional string field
// 0x68     4    uint32_t                playWord          packed PlayConfig
// 0x6C     4    int32_t                 playIndex         -1 = compute playWord
// 0x70     4    int                     seqRegWidth       from DeviceConstants.sequencerReg.width (+0x40)
// 0x74     4    int                     field74           init=0
// 0x78     8    DeviceConstants*        deviceConstants   pointer to device config
// 0x80    0x58  Signal                  signal            see signal.hpp (0x58 bytes)
// 0xD8          END of Waveform base
//
// Signal internal layout at Waveform+0x80:
//   +0x80: Signal.samples_     (vector<double>)    — sample data
//   +0x98: Signal.markers_     (vector<uint8_t>)   — per-sample marker bytes
//   +0xB0: Signal.markerBits_  (vector<uint8_t>)   — per-channel marker bit ORs
//   +0xC8: Signal.channels_    (uint16_t)          — "channels" in toString()
//   +0xCA: Signal.reserveOnly_ (bool)
//   +0xD0: Signal.length_      (uint64_t)          — "numSamples" in toString()
// ============================================================================

// ============================================================================
// WaveformFront — extends Waveform, total 0xF8 bytes
//
// Offset  Size  Type                    Name              Notes
// ------  ----  ----                    ----              -----
// 0x00-0xD7     Waveform base           (see above)
// 0xD8     4    int                     frontField1       init=1 in copy ctor
// 0xDC     1    bool                    frontBool1        init=0 in copy ctor
// 0xDD     1    bool                    frontBool2        copied from source
// 0xDE     2    (padding)
// 0xE0    24    vector<Value>           values            per-parameter values (each 0x28 bytes)
// 0xF8          END
//
// Destructor: destroys vector<Value> at +0xE0 (releasing any heap strings
// in Value elements), then tail-calls Waveform::~Waveform().
// ============================================================================
struct WaveformFront {
    // --- Waveform base (0xD8 bytes) ---
    std::string name;                    // +0x00  (24 bytes)
    Waveform::File::Type waveformType;   // +0x18  (4 bytes)
    // +0x1C: 4 bytes padding
    std::string secondaryName;           // +0x20  (24 bytes)
    // +0x38: shared_ptr<Waveform::File> (16 bytes) — not reconstructed
    char _file[16];                      // +0x38  placeholder
    bool used;                           // +0x48
    char _pad49[3];                      // +0x49
    int field4C;                         // +0x4C
    std::string thirdString;             // +0x50  (24 bytes)
    uint32_t playWord;                   // +0x68
    int32_t playIndex;                   // +0x6C  (-1 = not computed)
    int seqRegWidth;                     // +0x70  (from DeviceConstants.sequencerReg.width)
    int field74;                         // +0x74
    DeviceConstants* deviceConstants;    // +0x78
    Signal signal;                       // +0x80  (0x58 bytes — see signal.hpp)
    // +0xD8: END of Waveform base

    // --- WaveformFront extension (0x20 bytes) ---
    int frontField1;                     // +0xD8  (init=1)
    bool frontBool1;                     // +0xDC  (init=0)
    bool frontBool2;                     // +0xDD  (copied from source)
    // +0xDE: 2 bytes padding
    std::vector<Value> values;           // +0xE0  (24 bytes, each Value is 0x28)
    // +0xF8: END

    // --- Methods ---

    // WaveformFront(shared_ptr<WaveformFront> source, string const& newName) — 0x2a2510
    //   Copy-with-rename constructor. Copies Waveform base via
    //   Waveform::Waveform(shared_ptr<Waveform>, string), then initializes
    //   extension fields: frontField1=1, frontBool1=0, copies frontBool2 and
    //   values vector from source.
    WaveformFront(std::shared_ptr<WaveformFront> source, std::string const& newName);

    // ~WaveformFront — implicit (at 0x2a1300 via __on_zero_shared)
    //   Destroys vector<Value> at +0xE0, then calls Waveform::~Waveform().
    ~WaveformFront();

    // toString() const — 0x2c5120
    //   Returns "Name: <name> (<type>) <signal.length_> samples & <signal.channels_> channels"
    //   where type is "CSV"/"RAW"/"GEN"/"UNDEF".
    std::string toString() const;

    // --- Accessors used by AsmCommands ---

    // Name is directly at +0x00 (std::string).
    // Signal at +0x80 contains samples, markers, markerBits, channels, length.
    // signal.channels_ at +0xC8 determines channel count for PlayConfig.
    // signal.markerBits_ at +0xB0 provides per-channel marker bit ORs.
    // playWord at +0x68, playIndex at +0x6C used in asmPlay for packed config.
    // used at +0x48 set to true when waveform is referenced by play/prefetch.
};

} // namespace zhinst
