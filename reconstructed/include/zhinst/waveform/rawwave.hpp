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
//! \brief Type-erased view onto an encoded waveform's raw byte
//!        buffer, ready to be embedded in a `.wf_<name>` ELF section.
//!
//! Concrete subclasses encode samples in a device-specific format
//! (Hirzel 16-bit, Cervino, or a deferred placeholder); callers see
//! only the byte pointer and length pair returned by `ptr()` and
//! `size()`. `ElfWriter::addWaveform()` returns one of these so the
//! caller can determine the section size after encoding without
//! knowing which subclass was selected.
class RawWave {
public:
    //! \brief Virtual destructor so deleting a `RawWave*` correctly
    //!        invokes the derived destructor (defaulted body).
    virtual ~RawWave() = default;
    //! \brief Return the byte length of the encoded waveform buffer.
    //! \return Size in bytes of the data pointed to by `ptr()`.
    virtual size_t size() const = 0;
    //! \brief Return a non-owning pointer to the encoded waveform
    //!        bytes.
    //! \return Pointer to a buffer of `size()` bytes; the storage is
    //! owned by the concrete subclass and lives for the lifetime of
    //! `*this`.
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
//! \brief `RawWave` subclass for reserve-only signals: holds a
//!        target byte size but materialises the underlying
//!        zero-filled buffer lazily on the first `ptr()` call.
//!
//! Used when a waveform's storage must be reserved in the output ELF
//! but its samples will be supplied at runtime, so allocating the
//! full buffer up front would be pointless overhead.
class RawWavePlaceHolder : public RawWave {
public:
    //! \brief Release the lazily-allocated `buffer_` and chain to
    //!        `~RawWave()`.
    ~RawWavePlaceHolder() override;                              // 0x296f90 (D1), 0x296fc0 (D0)
    //! \brief Return the pre-computed byte size of the placeholder.
    //! \return `byteSize_` — `channels * length * 2` for a 16-bit
    //! encoding.
    size_t size() const override;                                // 0x297010
    //! \brief Lazily allocate a zero-filled buffer of `byteSize_`
    //!        bytes on the first call and return its data pointer.
    //! \details `buffer_` is resized to `byteSize_` (growing the
    //! `std::vector<char>` with default-initialisation, i.e.
    //! zero-fill); subsequent calls reuse the same allocation.
    //! \return Pointer to the (now-populated) buffer.
    const char* ptr() const override;                            // 0x297020

    //! \brief Target byte size of the deferred buffer.
    size_t byteSize_;                                            // +0x08
    //! \brief Lazily-materialised zero-filled storage.
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
//! \brief `RawWave` subclass that encodes samples as 16-bit unsigned
//!        integers in the format expected by Hirzel-family devices
//!        (HDAWG and the SHF series).
//!
//! The constructor inspects the marker-bits-per-channel descriptor
//! and selects the cheapest of three encoding paths:
//! sample-only (`double2awg16`), single-bit marker
//! (`double2awg1m`), or multi-bit marker (`double2awg`).
class RawWaveHirzel16 : public RawWave {
public:
    //! \brief Encode `samples` (and optional per-sample `markers`)
    //!        into 16-bit Hirzel words and store them in `data_`.
    //! \details Selects between `double2awg16`,
    //! `double2awg1m`, or `double2awg` per-sample depending on
    //! the OR-reduction of `markerBits & 0x03` across every
    //! channel (computed with a vectorised SSE2 inner loop).
    //! \param samples     Source samples in `[-1, 1]`.
    //! \param markers     Per-sample marker bits to embed.
    //! \param markerBits  Per-channel marker-bit descriptor that
    //!                    selects the encoding path.
    RawWaveHirzel16(std::vector<double> const& samples,          // 0x297140
                    std::vector<uint8_t> const& markers,
                    MarkerBitsPerChannel const& markerBits);
    //! \brief Release the encoded `data_` buffer and chain to
    //!        `~RawWave()`.
    ~RawWaveHirzel16() override;                                 // 0x2973d0 (D1), 0x297400 (D0)
    //! \brief Return the encoded buffer's byte length.
    //! \return `data_.size() * sizeof(uint16_t)`.
    size_t size() const override;                                // 0x297450
    //! \brief Return the encoded buffer as a `const char*`.
    //! \return `reinterpret_cast<const char*>(data_.data())`.
    const char* ptr() const override;                            // 0x297460

    //! \brief Encoded waveform samples, one `uint16_t` per source
    //!        sample (markers packed into the upper bits when used).
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
//! \brief `RawWave` subclass that encodes samples as 16-bit unsigned
//!        integers in the format expected by Cervino-family devices
//!        (UHFLI / UHFQA).
//!
//! Samples are produced inline by `Signal::getRawData()`; this class
//! has no public constructor.
class RawWaveCervino : public RawWave {
public:
    //! \brief Release the encoded `data_` buffer and chain to
    //!        `~RawWave()`.
    ~RawWaveCervino() override;                                  // 0x2975b0 (D1), 0x2975e0 (D0)
    //! \brief Return the encoded buffer's byte length.
    //! \return `data_.size() * sizeof(uint16_t)`.
    size_t size() const override;                                // 0x297630
    //! \brief Return the encoded buffer as a `const char*`.
    //! \return `reinterpret_cast<const char*>(data_.data())`.
    const char* ptr() const override;                            // 0x297640

    //! \brief Encoded waveform samples (sample-plus-marker pairs
    //!        produced by `double2awg`).
    std::vector<uint16_t> data_;                                 // +0x08
};

} // namespace zhinst
