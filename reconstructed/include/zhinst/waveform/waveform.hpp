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

#include "zhinst/waveform/signal.hpp"
#include "zhinst/asm/address_impl.hpp"

namespace zhinst {

struct DeviceConstants;  // forward declaration

// ============================================================================
// Waveform::File — source file reference (0x40 bytes)
//
// Offset  Size  Type              Name
// ------  ----  ----              ----
// 0x00    24    std::string       name
// 0x18     4    FormatType        formatType
// 0x1C     4    int32_t           columnMode
// 0x20     4    int32_t           isIntegerFormat
// 0x24     4    (padding)
// 0x28     8    uint*             fileHash begin  }
// 0x30     8    uint*             fileHash end    } vector<unsigned int> (hash)
// 0x38     8    uint*             fileHash cap    }
// 0x40          END
// ============================================================================
struct WaveformFile {
    // File::Type enum — source file kind (CSV text, RAW binary, GEN generated)
    enum class Type : int {
        CSV = 0,
        RAW = 1,
        GEN = 2,
    };

    // FormatType — detected data layout within a CSV file (+0x18 field)
    //   AutoDetect    = 0: not yet determined; detection runs on first data line
    //   AwgInteger    = 1: single-column AWG integer format (hex "0x..." values)
    //   MultiColFloat = 2: multi-column floating-point (>2 columns detected)
    enum class FormatType : int32_t {
        AutoDetect    = 0,
        AwgInteger    = 1,
        MultiColFloat = 2,
    };

    std::string name;                   // +0x00
    FormatType formatType;              // +0x18
    int32_t columnMode;                    // +0x1C
    int32_t isIntegerFormat;                    // +0x20
    // +0x24: 4 bytes padding
    std::vector<unsigned int> fileHash;     // +0x28 — file hash (from CachedParser::getHash)

    // Static string converters (lazy-initialized static maps)
    static std::string typeToStr(Type type);         // 0x2a3a90
    static Type typeFromStr(std::string str);        // 0x2a63c0

    bool operator==(WaveformFile const& other) const;  // 0x2a9680

    // Constructor from filename (called by fromJson when fileName non-empty)
    explicit WaveformFile(const char* filename);     // 0x2a7ff0
    explicit WaveformFile(const std::string& filename) : WaveformFile(filename.c_str()) {}
};

// ============================================================================
// Waveform base class — 0xD8 bytes
//
// Offset  Size  Type                        Name              Notes
// ------  ----  ----                        ----              -----
// 0x00    24    std::string                 name
// 0x18     4    WaveformFile::Type          waveformType      0=CSV, 1=RAW, 2=GEN
// 0x1C     4    (padding)
// 0x20    24    std::string                 functionArgs     "functionArgs" in JSON
// 0x38    16    shared_ptr<WaveformFile>    file              source file
// 0x48     1    bool                        used              "load" in JSON
// 0x49     3    (padding)
// 0x4C     4    uint32_t                    addressValue      "globalAddress" in JSON
// 0x50    24    std::string                 funDescrName       "genFunc" in JSON
// 0x68     4    uint32_t                    playConfig          "playConfig" in JSON
// 0x6C     4    int32_t                     playIndex         "waveIndex" in JSON
// 0x70     4    int                         minLengthSamples       "minLengthSamples" in JSON
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
    std::string functionArgs;                      // +0x20
    std::shared_ptr<File> file;                     // +0x38
    bool used;                                      // +0x48
    // +0x49: padding
    uint32_t addressValue;                          // +0x4C
    std::string funDescrName;                        // +0x50
    uint32_t playConfig;                              // +0x68
    int32_t waveIndex;                              // +0x6C — "waveIndex" in JSON
    int minLengthSamples;                                // +0x70
    int allocationByteSize;                         // +0x74 — "allocationSize" in JSON
    const DeviceConstants* deviceConstants;               // +0x78
    Signal signal;                                  // +0x80

    // --- Constructors / Destructor ---

    // Default constructor — needed by various factory paths
    Waveform() = default;

    // Full 13-parameter constructor — 0x2a71e0
    Waveform(std::string name, File::Type type, std::string functionArgs,
             std::shared_ptr<File> file, bool used,
             detail::AddressImpl<uint32_t> addr, std::string genFunc,
             int playConfig, int playIndex, int minLengthSamples, int allocationByteSize,
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

    // Convenience accessor
    std::string const& getName() const { return name; }
};

} // namespace zhinst
