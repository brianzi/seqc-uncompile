// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// RawWave hierarchy — abstract base + 3 subclasses for waveform encoding
//
// Vtable layout (common to all subclasses):
//   slot 0: ~Dtor() [D1 — non-deleting]
//   slot 1: ~Dtor() [D0 — deleting]
//   slot 2: size() const -> size_t
//   slot 3: ptr() const -> const char*
//
// Class hierarchy:
//   RawWave (abstract base, typeinfo @0xb07800, name @0x95de19)
//     +-- RawWavePlaceHolder (vtable @0xb077c8, typeinfo @0xb077e8, size 0x28)
//     +-- RawWaveHirzel16   (vtable @0xb07820, typeinfo @0xb07840, size 0x20)
//     +-- RawWaveCervino    (vtable @0xb07868, typeinfo @0xb07888, size 0x20)
// ============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zhinst {

// Forward declarations
using MarkerBitsPerChannel = std::vector<uint8_t>;

namespace util { namespace wave {
    uint16_t double2awg(double sample, unsigned int marker);     // 0x299630
    uint16_t double2awg1m(double sample, unsigned int marker);   // 0x299680
    uint16_t double2awg16(double sample);                        // 0x299700
    // SHA-256 of file contents at filePath. Returns 8 uint32 words
    // (256 bits). Used by CachedParser::getHash for waveform-cache
    // keys. Empty vector on file-open failure.                       // 0x299760
    std::vector<unsigned int> hash(const std::string& filePath);
}}

// ==========================================================================
// RawWave — abstract base class
//
// Layout (base portion, 0x08 bytes):
//   +0x00: vptr (8 bytes)
//
// Pure virtual interface:
//   virtual ~RawWave() = 0;
//   virtual size_t size() const = 0;
//   virtual const char* ptr() const = 0;
// ==========================================================================
class RawWave {
public:
    virtual ~RawWave() = default;
    virtual size_t size() const = 0;
    virtual const char* ptr() const = 0;
};

// ==========================================================================
// RawWavePlaceHolder — placeholder for reserve-only signals
//
// Layout (0x28 = 40 bytes):
//   +0x00: vptr                          (8 bytes)
//   +0x08: size_t byteSize_              (8 bytes) — total byte count
//   +0x10: std::vector<char> buffer_     (24 bytes: ptr, end, cap)
//
// vtable @0xb077c8 (adjusted +0x10):
//   slot 0 @0x296f90: ~RawWavePlaceHolder() [D1]
//   slot 1 @0x296fc0: ~RawWavePlaceHolder() [D0, deleting, size=0x28]
//   slot 2 @0x297010: size() — returns byteSize_ (+0x08)
//   slot 3 @0x297020: ptr()  — resizes buffer_ to byteSize_, zero-fills gap, returns buffer_.data()
//
// Behavior:
//   size() returns the pre-computed byte size (channels * length * 2).
//   ptr() lazily allocates a zero-filled buffer of that size and returns it.
//   The buffer uses std::vector<char> growth semantics (realloc + memset).
// ==========================================================================
class RawWavePlaceHolder : public RawWave {
public:
    ~RawWavePlaceHolder() override;                              // 0x296f90 (D1), 0x296fc0 (D0)
    size_t size() const override;                                // 0x297010
    const char* ptr() const override;                            // 0x297020

    size_t byteSize_;                                            // +0x08
    mutable std::vector<char> buffer_;                           // +0x10
};

// ==========================================================================
// RawWaveHirzel16 — 16-bit encoded waveform for Hirzel devices
//
// Layout (0x20 = 32 bytes):
//   +0x00: vptr                                  (8 bytes)
//   +0x08: std::vector<uint16_t> data_           (24 bytes: ptr, end, cap)
//
// vtable @0xb07820 (adjusted +0x10):
//   slot 0 @0x2973d0: ~RawWaveHirzel16() [D1]
//   slot 1 @0x297400: ~RawWaveHirzel16() [D0, deleting, size=0x20]
//   slot 2 @0x297450: size() — returns data_.end - data_.begin (byte count)
//   slot 3 @0x297460: ptr()  — returns data_.data() (as char*)
//
// Constructor @0x297140:
//   Converts double samples to uint16_t via one of three paths:
//   1. No markers used (all markerBits & 0x03 == 0): double2awg16(sample)
//   2. Only 1-bit marker (markerBits OR == 1): double2awg1m(sample, marker)
//   3. Multi-bit markers (markerBits OR > 1): double2awg(sample, marker)
//
//   The marker detection scans the entire MarkerBitsPerChannel vector,
//   OR-ing all bytes masked with 0x03 using SIMD (SSE2 vectorized loop).
// ==========================================================================
class RawWaveHirzel16 : public RawWave {
public:
    RawWaveHirzel16(std::vector<double> const& samples,          // 0x297140
                    std::vector<uint8_t> const& markers,
                    MarkerBitsPerChannel const& markerBits);
    ~RawWaveHirzel16() override;                                 // 0x2973d0 (D1), 0x297400 (D0)
    size_t size() const override;                                // 0x297450
    const char* ptr() const override;                            // 0x297460

    std::vector<uint16_t> data_;                                 // +0x08
};

// ==========================================================================
// RawWaveCervino — 16-bit encoded waveform for Cervino devices
//
// Layout (0x20 = 32 bytes):
//   +0x00: vptr                                  (8 bytes)
//   +0x08: std::vector<uint16_t> data_           (24 bytes: ptr, end, cap)
//
// vtable @0xb07868 (adjusted +0x10):
//   slot 0 @0x2975b0: ~RawWaveCervino() [D1]
//   slot 1 @0x2975e0: ~RawWaveCervino() [D0, deleting, size=0x20]
//   slot 2 @0x297630: size() — returns data_.end - data_.begin (byte count)
//   slot 3 @0x297640: ptr()  — returns data_.data() (as char*)
//
// No explicit constructor symbol — constructed inline in Signal::getRawData():
//   Converts each double sample via double2awg(sample, marker) into data_.
// ==========================================================================
class RawWaveCervino : public RawWave {
public:
    ~RawWaveCervino() override;                                  // 0x2975b0 (D1), 0x2975e0 (D0)
    size_t size() const override;                                // 0x297630
    const char* ptr() const override;                            // 0x297640

    std::vector<uint16_t> data_;                                 // +0x08
};

} // namespace zhinst
