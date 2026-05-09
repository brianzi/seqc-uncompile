// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// zhinst::Signal — waveform signal container
// Struct size: 0x58 (88 bytes)
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

namespace boost { namespace json { class value; } }

namespace zhinst {

// Forward declarations for getRawData() return type hierarchy
// See rawwave.hpp for full definitions
class RawWave;              // abstract base, typeinfo @0xb07800
class RawWavePlaceHolder;   // vtable @0xb077c8, size 0x28
class RawWaveCervino;       // vtable @0xb07868, size 0x20
class RawWaveHirzel16;      // vtable @0xb07820, size 0x20

// Typedef used throughout the signal API
using MarkerBitsPerChannel = std::vector<uint8_t>;

// Sample encoding format for getRawData()
enum class SampleFormat : int {
    Cervino   = 0,
    Hirzel16  = 1
};

// Tag type for reserve-only construction (empty struct for overload dispatch)
//! Empty tag type used to disambiguate the `Signal` constructor that
//! reserves storage without populating samples from the constructors
//! that copy or move sample data.
struct ReserveOnly {};

// ==========================================================================
// Signal layout (0x58 bytes):
//   +0x00: std::vector<double>  samples_       (24 bytes)
//   +0x18: std::vector<uint8_t> markers_       (24 bytes)
//   +0x30: std::vector<uint8_t> markerBits_    (24 bytes)
//   +0x48: uint16_t             channels_      (2 bytes)
//   +0x4A: bool                 reserveOnly_   (1 byte)
//   +0x4B: (padding)            5 bytes
//   +0x50: uint64_t             length_        (8 bytes)
//   +0x58: END
// ==========================================================================
//! In-memory representation of an interleaved multi-channel waveform
//! signal: floating-point samples plus per-sample marker bytes and a
//! per-channel marker-bits descriptor.
//!
//! `Signal` is the workhorse value type used throughout waveform
//! generation, parsing, and serialisation. `samples_` carries
//! `length_ * channels_` doubles in interleaved order; `markers_`
//! carries one byte per sample; `markerBits_` records, for each
//! channel, how many low-order bits of `markers_` are actually in
//! use. `reserveOnly_` flags signals whose sample buffer is
//! intentionally empty (the storage is reserved in the output ELF
//! but populated at runtime).
//!
//! `getRawData()` produces the device-specific encoding (a
//! `RawWave` subclass) selected by `SampleFormat`. JSON
//! round-tripping is provided via `toJson()` / `fromJson()`.
class Signal {
public:
    // --- Constructors ---
    Signal() : Signal(0) {}  // default ctor delegates to length-based ctor
    explicit Signal(size_t length);                                              // 0x25dd60
    Signal(size_t numSamples, double value, uint8_t marker, uint16_t channels); // 0x25eac0
    Signal(size_t length, MarkerBitsPerChannel const& markerBits);              // 0x25f1a0
    Signal(std::vector<double> samples, std::vector<uint8_t> markers,
           std::vector<uint8_t> markerBits, uint16_t channels,
           bool reserveOnly, size_t length);                                    // 0x2a8940
    Signal(ReserveOnly const& tag, size_t length,
           MarkerBitsPerChannel const& markerBits);                             // 0x25ef50
    Signal(std::vector<double> const& samples,
           std::vector<uint8_t> const& markers,
           MarkerBitsPerChannel const& markerBits);                             // 0x106340
    Signal(std::vector<double> const& samples, uint16_t channels);              // 0x25db90

    // --- Copy/Destroy ---
    Signal(Signal const& other);                                                // 0x1150e0
    ~Signal();                                                                  // 0x106520

    // --- Mutators ---
    void append(double sample, uint8_t marker);                                 // 0x25de80
    void append(Signal& other);                                                 // 0x25f310
    void resizeSamples(size_t newLength);                                       // 0x1dff70
    void checkAllocation();                                                     // 0x246950

    // --- Serialization ---
    boost::json::value toJson() const;                                          // 0x2a3e40
    static Signal fromJson(boost::json::value const& json);                     // 0x2a65d0

    // --- Data access ---
    std::unique_ptr<RawWave> getRawData(SampleFormat format) const;              // 0x293ec0

    // Convenience accessors — used in wavetable/waveform code
    std::vector<double> const& data() const { return samples_; }
    uint16_t channels() const { return channels_; }
    size_t length() const { return length_; }
    // Binary: granularity / maxLength / minLength / bitsPerSample are NOT methods
    // on Signal in the binary (verified — no such symbols exist). Earlier
    // call sites that appeared to call sig.granularity() etc. were actually
    // reading DeviceConstants fields (grainSize @+0x44 = grainSize,
    // maxWaveformLength @+0x40 = maxWaveformLength, bitsPerSample @+0x50)
    // or Waveform::minLengthSamples @+0x70 ("minLengthSamples" in JSON). Those
    // call sites have been rewritten to use the real fields directly.

    // --- Comparison ---
    bool operator==(Signal const& other) const;                                 // 0x2a9750

    // --- Fields ---
    std::vector<double>  samples_;       // +0x00
    std::vector<uint8_t> markers_;       // +0x18
    std::vector<uint8_t> markerBits_;    // +0x30
    uint16_t             channels_;      // +0x48
    bool                 reserveOnly_;   // +0x4A
    uint64_t             length_;        // +0x50
};

static_assert(sizeof(Signal) == 0x58, "Signal must be 0x58 bytes (no strings, ABI-safe)");

} // namespace zhinst
