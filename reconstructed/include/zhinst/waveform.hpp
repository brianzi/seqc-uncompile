// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Waveform base class (0xD8 bytes) + nested File struct (0x40 bytes)
//
// Source path (from debug strings):
//   /builds/labone/labone/ziAWG/ziAWGUtils/src/main/include/Waveform
//
// Confirmed from:
//   - Full constructor (0x2a71e0)
//   - Copy constructor (0x2a8ff0)
//   - Destructor (0x1152e0)
//   - toJson (0x2a33c0)
//   - fromJson (0x2a54b0)
//   - operator== (0x2a9510)
//   - getSizePerDevice (0x1d5c30)
//   - Copy-rename ctor (0x114f10)
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "signal.hpp"

namespace boost { namespace json { class value; } }

namespace zhinst {

struct DeviceConstants;

namespace detail {
    template<typename T>
    using AddressImpl = T;  // simplification — wraps a uint32_t
}

// ============================================================================
// Waveform::File — source file reference (0x40 bytes)
//
// Offset  Size  Type              Name
// ------  ----  ----              ----
// 0x00    24    std::string       name
// 0x18     4    int32_t           field18
// 0x1C     4    int32_t           field1C
// 0x20     4    int32_t           field20
// 0x24     4    (padding)
// 0x28     8    uint8_t*          data begin  }
// 0x30     8    uint8_t*          data end    } vector<uint8_t>
// 0x38     8    uint8_t*          data cap    }
// 0x40          END
// ============================================================================
struct WaveformFile {
    std::string name;                   // +0x00
    int32_t field18;                    // +0x18
    int32_t field1C;                    // +0x1C
    int32_t field20;                    // +0x20
    // +0x24: 4 bytes padding
    std::vector<uint8_t> data;          // +0x28

    // File::Type enum
    enum class Type : int {
        CSV = 0,
        RAW = 1,
        GEN = 2,
    };

    // Static string converters (lazy-initialized static maps)
    static std::string typeToStr(Type type);         // 0x2a3a90
    static Type typeFromStr(std::string str);        // 0x2a63c0

    bool operator==(WaveformFile const& other) const;  // 0x2a9680

    // Constructor from filename (called by fromJson when fileName non-empty)
    explicit WaveformFile(const char* filename);     // 0x2a7ff0
};

// ============================================================================
// Waveform base class — 0xD8 bytes
//
// Offset  Size  Type                        Name              Notes
// ------  ----  ----                        ----              -----
// 0x00    24    std::string                 name
// 0x18     4    WaveformFile::Type          waveformType      0=CSV, 1=RAW, 2=GEN
// 0x1C     4    (padding)
// 0x20    24    std::string                 secondaryName     "functionArgs" in JSON
// 0x38    16    shared_ptr<WaveformFile>    file              source file
// 0x48     1    bool                        used              "load" in JSON
// 0x49     3    (padding)
// 0x4C     4    uint32_t                    addressValue      "globalAddress" in JSON
// 0x50    24    std::string                 thirdString       "genFunc" in JSON
// 0x68     4    uint32_t                    playWord          "playConfig" in JSON
// 0x6C     4    int32_t                     playIndex         "waveIndex" in JSON
// 0x70     4    int                         seqRegWidth       "minLengthSamples" in JSON
// 0x74     4    int                         field74           "allocationSize" in JSON
// 0x78     8    DeviceConstants*            deviceConstants
// 0x80    0x58  Signal                      signal
// 0xD8          END
// ============================================================================
struct Waveform {
    using File = WaveformFile;

    std::string name;                               // +0x00
    File::Type waveformType;                        // +0x18
    // +0x1C: padding
    std::string secondaryName;                      // +0x20
    std::shared_ptr<File> file;                     // +0x38
    bool used;                                      // +0x48
    // +0x49: padding
    uint32_t addressValue;                          // +0x4C
    std::string thirdString;                        // +0x50
    uint32_t playWord;                              // +0x68
    int32_t playIndex;                              // +0x6C
    int seqRegWidth;                                // +0x70
    int field74;                                    // +0x74
    DeviceConstants* deviceConstants;               // +0x78
    Signal signal;                                  // +0x80

    // --- Constructors / Destructor ---

    // Full 13-parameter constructor — 0x2a71e0
    Waveform(std::string name, File::Type type, std::string secondaryName,
             std::shared_ptr<File> file, bool used,
             detail::AddressImpl<uint32_t> addr, std::string genFunc,
             int playWord, int playIndex, int seqRegWidth, int field74,
             DeviceConstants const& dc, Signal signal);

    // Copy-rename constructor — 0x114f10
    Waveform(std::shared_ptr<Waveform> source, std::string newName);

    // Copy constructor — 0x2a8ff0
    Waveform(Waveform const& other);

    // Destructor — 0x1152e0
    ~Waveform();

    // --- Methods ---

    boost::json::value toJson() const;              // 0x2a33c0
    static Waveform fromJson(boost::json::value const& json,
                             DeviceConstants const& dc);  // 0x2a54b0
    uint32_t getSizePerDevice() const;              // 0x1d5c30
    bool operator==(Waveform const& other) const;   // 0x2a9510
};

} // namespace zhinst
