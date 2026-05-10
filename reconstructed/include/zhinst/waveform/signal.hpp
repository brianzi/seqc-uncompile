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
//! \brief Abstract base of the device-specific encoded-sample
//! container hierarchy returned by `Signal::getRawData()`.
class RawWave;              // abstract base, typeinfo @0xb07800
//! \brief Reserve-only `RawWave` subclass: records the byte size of
//! the buffer to be allocated in the output ELF without carrying any
//! actual sample data.
class RawWavePlaceHolder;   // vtable @0xb077c8, size 0x28
//! \brief Cervino-format `RawWave` subclass holding a `vector<uint16_t>`
//! of samples encoded via `util::wave::double2awg()` (combined sample
//! plus marker bits, 16 bits per AWG sample).
class RawWaveCervino;       // vtable @0xb07868, size 0x20
//! \brief Hirzel16-format `RawWave` subclass carrying the
//! double/marker/markerBits triple in the layout expected by Hirzel
//! devices.
class RawWaveHirzel16;      // vtable @0xb07820, size 0x20

//! \brief Per-channel marker-bits descriptor: `markerBits[c]` is the
//! bitwise OR of every marker byte that has ever been written for
//! channel `c`, recording which of the low marker bits are actually
//! in use by this signal.
using MarkerBitsPerChannel = std::vector<uint8_t>;

//! \brief Sample-encoding format selector for `Signal::getRawData()`,
//! choosing between Cervino-style 16-bit packed AWG samples (HDAWG
//! family) and Hirzel-style triple-vector layout (SHF family).
enum class SampleFormat : int {
    Cervino   = 0, //!< 16-bit packed AWG sample format (HDAWG and predecessors).
    Hirzel16  = 1  //!< Triple-vector format used by Hirzel16-class devices.
};

// Tag type for reserve-only construction (empty struct for overload dispatch)
//! \brief Empty tag type used to disambiguate the `Signal` constructor
//! that reserves storage without populating samples from the
//! constructors that copy or move sample data.
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
//! \brief In-memory representation of an interleaved multi-channel
//! waveform signal: floating-point samples plus per-sample marker
//! bytes and a per-channel marker-bits descriptor.
//!
//! \details `Signal` is the workhorse value type used throughout
//! waveform generation, parsing, and serialisation. `samples_`
//! carries `length_ * channels_` doubles in interleaved order;
//! `markers_` carries one byte per sample; `markerBits_` records,
//! for each channel, which low-order marker bits are actually in
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
    //! \brief Default-construct an empty single-channel `Signal` with
    //! length 0 and a single zeroed marker-bits entry.
    Signal() : Signal(0) {}  // default ctor delegates to length-based ctor
    //! \brief Construct a single-channel `Signal` whose sample and
    //! marker buffers have capacity reserved for `length` entries but
    //! no elements yet inserted.
    //! \details `channels_` is set to 1 and `markerBits_` is
    //! initialised with one zero byte; `length_` is recorded as
    //! `length` so subsequent code that reasons by length works
    //! immediately even before samples are appended.
    //! \param length Capacity to reserve for the sample and marker buffers.
    explicit Signal(size_t length);                                              // 0x25dd60
    //! \brief Construct a multi-channel `Signal` filled with a constant
    //! sample value and a constant marker byte.
    //! \details `samples_` and `markers_` are resized to `numSamples`
    //! and filled with `value` and `marker` respectively;
    //! `markerBits_` is allocated with one zero entry per channel and
    //! the supplied `marker` is OR-ed into every entry. `length_` is
    //! computed as `numSamples / channels`.
    //! \param numSamples Total number of interleaved samples to write.
    //! \param value Sample value used to fill `samples_`.
    //! \param marker Marker byte used to fill `markers_` and folded
    //! into every `markerBits_` entry.
    //! \param channels Number of interleaved channels (must divide
    //! `numSamples`).
    Signal(size_t numSamples, double value, uint8_t marker, uint16_t channels); // 0x25eac0
    //! \brief Construct an empty `Signal` with the given length and
    //! per-channel marker-bits descriptor, reserving capacity for
    //! `markerBits.size() * length` interleaved samples.
    //! \details `channels_` is set from `markerBits.size()`. No
    //! samples or markers are populated; storage is only reserved.
    //! \param length Per-channel sample count to record in `length_`.
    //! \param markerBits Per-channel marker-bits descriptor copied
    //! into `markerBits_`.
    Signal(size_t length, MarkerBitsPerChannel const& markerBits);              // 0x25f1a0
    //! \brief Construct a `Signal` directly from prebuilt sample,
    //! marker and marker-bits buffers plus the descriptor scalars.
    //! \details Used by `fromJson()` and other deserialisation paths.
    //! `samples` and `markers` are moved in; `markerBits` is copied
    //! (the parameter is taken by value so its contents are
    //! reassigned with `assign`).
    //! \param samples Interleaved sample buffer.
    //! \param markers Per-sample marker byte buffer.
    //! \param markerBits Per-channel marker-bits descriptor.
    //! \param channels Number of interleaved channels.
    //! \param reserveOnly Whether the signal is a reserve-only
    //! placeholder whose sample buffer is intentionally empty.
    //! \param length Per-channel sample count.
    Signal(std::vector<double> samples, std::vector<uint8_t> markers,
           std::vector<uint8_t> markerBits, uint16_t channels,
           bool reserveOnly, size_t length);                                    // 0x2a8940
    //! \brief Construct a reserve-only placeholder `Signal`: the sample
    //! and marker buffers are left empty, `reserveOnly_` is set to
    //! `true`, and only the per-channel marker-bits descriptor is
    //! recorded.
    //! \details A subsequent `checkAllocation()` call (or `append`)
    //! materialises the buffer to `channels_ * length_` zeros. When
    //! `markerBits` is empty, `channels_` defaults to `1` (verified
    //! against the original binary).
    //! \param tag Disambiguating tag (no runtime effect).
    //! \param length Per-channel sample count to record.
    //! \param markerBits Per-channel marker-bits descriptor.
    Signal(ReserveOnly const& tag, size_t length,
           MarkerBitsPerChannel const& markerBits);                             // 0x25ef50
    //! \brief Copy-construct a `Signal` from prebuilt sample, marker
    //! and marker-bits buffers, deriving `channels_` and `length_`
    //! from the buffer sizes.
    //! \details `channels_` is `markerBits.size()` and `length_` is
    //! `samples.size() / channels_`. The buffers are copied (not
    //! moved). `reserveOnly_` is `false`.
    //! \param samples Interleaved sample buffer copied into the new signal.
    //! \param markers Per-sample marker byte buffer.
    //! \param markerBits Per-channel marker-bits descriptor.
    Signal(std::vector<double> const& samples,
           std::vector<uint8_t> const& markers,
           MarkerBitsPerChannel const& markerBits);                             // 0x106340
    //! \brief Construct a `Signal` from a sample buffer alone, with
    //! zeroed markers and zeroed per-channel marker-bits.
    //! \details `markers_` is sized to match `samples` and zeroed;
    //! `markerBits_` is sized to `channels` and zeroed; `length_` is
    //! `samples.size() / channels`.
    //! \param samples Interleaved sample buffer copied into the new signal.
    //! \param channels Number of interleaved channels.
    Signal(std::vector<double> const& samples, uint16_t channels);              // 0x25db90

    // --- Copy/Destroy ---
    //! \brief Deep-copy constructor: copies all three buffers and the
    //! scalar descriptor fields.
    //! \param other Source signal to copy.
    Signal(Signal const& other);                                                // 0x1150e0
    //! \brief Destructor; releases the three owned buffers.
    ~Signal();                                                                  // 0x106520

    // --- Mutators ---
    //! \brief Append a single sample together with its marker byte to
    //! the end of the signal.
    //! \details The marker byte is OR-ed into the
    //! `markerBits_[index % markerBits_.size()]` slot, and `length_`
    //! is recomputed as `samples_.size() / channels_`.
    //! \param sample Sample value to push onto `samples_`.
    //! \param marker Marker byte to push onto `markers_`.
    void append(double sample, uint8_t marker);                                 // 0x25de80
    //! \brief Append the contents of another `Signal` to this one.
    //! \details Returns immediately if `other.length_ == 0`. Calls
    //! `other.checkAllocation()` to materialise reserve-only sources
    //! before insertion. Sample and marker buffers are concatenated;
    //! marker-bits descriptors are OR-ed element-wise.
    //! \binarynote `other` is taken by non-const reference because
    //! `checkAllocation()` may mutate it.
    //! \param other Signal whose contents are appended to this one.
    void append(Signal& other);                                                 // 0x25f310
    //! \brief Resize the signal to a new per-channel length, growing
    //! with zeros or truncating in place.
    //! \details Reserve-only signals only update `length_`; otherwise
    //! both `samples_` and `markers_` are resized to
    //! `channels_ * newLength`.
    //! \param newLength Target per-channel sample count.
    void resizeSamples(size_t newLength);                                       // 0x1dff70
    //! \brief Materialise the sample and marker buffers of a
    //! reserve-only signal, sizing them to `channels_ * length_` and
    //! filling new entries with zero.
    //! \details Has no effect when `reserveOnly_` is `false`. Does
    //! not clear the `reserveOnly_` flag.
    void checkAllocation();                                                     // 0x246950

    // --- Serialization ---
    //! \brief Serialise this signal to a JSON object with keys
    //! `length`, `channels`, `reserveOnly`, `data`, `marker`,
    //! `markerBits`.
    //! \return A JSON value carrying the serialised representation.
    boost::json::value toJson() const;                                          // 0x2a3e40
    //! \brief Deserialise a `Signal` from its JSON object form.
    //! \param json JSON value expected to be an object with the keys
    //! produced by `toJson()`.
    //! \return The reconstructed signal.
    static Signal fromJson(boost::json::value const& json);                     // 0x2a65d0

    // --- Data access ---
    //! \brief Produce the device-specific raw encoding of this signal.
    //! \details Reserve-only signals always return a
    //! `RawWavePlaceHolder` whose `byteSize_` is
    //! `channels_ * length_ * 2`. Otherwise, `Hirzel16` constructs a
    //! `RawWaveHirzel16` carrying the three vectors directly, while
    //! `Cervino` (the default) packs each `(sample, marker)` pair
    //! through `util::wave::double2awg()` into a `vector<uint16_t>`.
    //! \param format Sample-encoding format to produce.
    //! \return Owning pointer to the appropriate `RawWave` subclass.
    std::unique_ptr<RawWave> getRawData(SampleFormat format) const;              // 0x293ec0

    // Convenience accessors — used in wavetable/waveform code
    //! \brief Const access to the raw interleaved-sample buffer.
    //! \return Reference to `samples_`.
    std::vector<double> const& data() const { return samples_; }
    //! \brief Number of interleaved channels recorded in this signal.
    //! \return Channel count.
    uint16_t channels() const { return channels_; }
    //! \brief Per-channel sample count recorded in this signal.
    //! \return The `length_` field (number of samples per channel).
    size_t length() const { return length_; }
    // Binary: granularity / maxLength / minLength / bitsPerSample are NOT methods
    // on Signal in the binary (verified — no such symbols exist). Earlier
    // call sites that appeared to call sig.granularity() etc. were actually
    // reading DeviceConstants fields (grainSize @+0x44 = grainSize,
    // maxWaveformLength @+0x40 = maxWaveformLength, bitsPerSample @+0x50)
    // or Waveform::minLengthSamples @+0x70 ("minLengthSamples" in JSON). Those
    // call sites have been rewritten to use the real fields directly.

    // --- Comparison ---
    //! \brief Compare two signals for equality.
    //! \details `samples_` are compared with a relative-plus-absolute
    //! tolerance of `1e-12` (`|a-b| <= |b|*epsilon + epsilon`);
    //! `markers_` and `markerBits_` are compared bytewise; the
    //! scalar descriptor fields (`channels_`, `reserveOnly_`,
    //! `length_`) are compared exactly.
    //! \param other Signal to compare against.
    //! \return `true` if the signals are equivalent under the rules above.
    bool operator==(Signal const& other) const;                                 // 0x2a9750

    // --- Fields ---
    std::vector<double>  samples_;       //!< Interleaved sample buffer; `length_ * channels_` doubles in channel-major order. +0x00
    std::vector<uint8_t> markers_;       //!< Per-sample marker byte buffer (one byte per entry of `samples_`). +0x18
    std::vector<uint8_t> markerBits_;    //!< Per-channel marker-bits descriptor: `markerBits_[c]` is the OR of every marker byte ever written for channel `c`. +0x30
    uint16_t             channels_;      //!< Number of interleaved channels in `samples_` / `markers_`. +0x48
    bool                 reserveOnly_;   //!< When `true`, `samples_` and `markers_` are intentionally empty; the buffer will be allocated on demand by `checkAllocation()`. +0x4A
    uint64_t             length_;        //!< Per-channel sample count (i.e. `samples_.size() / channels_` once materialised). +0x50
};

static_assert(sizeof(Signal) == 0x58, "Signal must be 0x58 bytes (no strings, ABI-safe)");

} // namespace zhinst
