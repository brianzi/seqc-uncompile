// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Binary: /home/brian/zhinst/seqc_compiler/_seqc_compiler.so
// Class:  zhinst::RawWave hierarchy — all methods
//
// Conversion functions used:
//   double2awg(double, uint)  @0x299630 — full marker encoding (Cervino + multi-bit Hirzel)
//   double2awg1m(double, uint) @0x299680 — 1-bit marker encoding (Hirzel, single marker bit)
//   double2awg16(double)       @0x299700 — no-marker encoding (Hirzel, pure sample)
// ============================================================================

#include "zhinst/waveform/rawwave.hpp"

#include <algorithm>
#include <cstring>

namespace zhinst {

// ==========================================================================
// RawWavePlaceHolder
// ==========================================================================

// D1 (non-deleting) destructor — 0x296f90
// D0 (deleting) destructor — 0x296fc0
//   Sets vtable, frees buffer_ vector data if non-null.
//   D0 additionally calls operator delete(this, 0x28).
RawWavePlaceHolder::~RawWavePlaceHolder() = default;

// size() const — 0x297010
//   Returns byteSize_ at offset +0x08.
size_t RawWavePlaceHolder::size() const {  // 0x297010
    return byteSize_;
}

// ptr() const — 0x297020
//   Lazily grows buffer_ to byteSize_, zero-filling any new bytes.
//   Returns buffer_.data().
//
//   Logic from disassembly:
//     currentSize = buffer_.end - buffer_.begin  (r12 = rdi - r14)
//     needed = byteSize_ - currentSize           (r13)
//     if needed <= 0: shrink end pointer or return
//     if capacity sufficient: memset gap, advance end
//     else: realloc with vector growth (2x or byteSize_, whichever larger),
//           memcpy old data, memset new region
const char* RawWavePlaceHolder::ptr() const {  // 0x297020
    size_t currentSize = buffer_.size();
    if (currentSize < byteSize_) {
        buffer_.resize(byteSize_, 0);
    } else if (currentSize > byteSize_) {
        buffer_.resize(byteSize_);
    }
    return buffer_.data();
}

// ==========================================================================
// RawWaveHirzel16
// ==========================================================================

// Constructor — 0x297140
//   Parameters:
//     rdi = this
//     rsi = const vector<double>& samples   (r15)
//     rdx = const vector<uint8_t>& markers  (r12)
//     rcx = const MarkerBitsPerChannel& markerBits (r13)
//
//   Steps:
//   1. Set vtable to RawWaveHirzel16 (@0xb07820)
//   2. Initialize data_ vector (zero-init 24 bytes at +0x08)
//   3. Resize data_ to samples.size() elements (via __append)
//   4. Scan markerBits to determine marker mode:
//      - OR all bytes masked with 0x03 (SSE2 vectorized: 32-byte chunks,
//        then 4-byte chunks, then scalar remainder)
//      - result == 0: no markers → use double2awg16()
//      - result == 1: 1-bit marker → use double2awg1m()
//      - result > 1:  multi-bit markers → use double2awg()
//   5. Convert each sample using the selected function.
RawWaveHirzel16::RawWaveHirzel16(                                // 0x297140
    std::vector<double> const& samples,
    std::vector<uint8_t> const& markers,
    MarkerBitsPerChannel const& markerBits)
    : data_()
{
    size_t numSamples = samples.size();
    if (numSamples > 0) {
        data_.resize(numSamples);
    }

    // Determine marker mode by OR-ing all markerBits entries masked with 0x03.
    // The binary uses an SSE2-vectorized scan (32-byte unrolled, 4-byte mid,
    // scalar tail), but the semantic is:
    uint8_t markerMode = 0;
    for (size_t i = 0; i < markerBits.size(); ++i) {
        markerMode |= (markerBits[i] & 0x03);
    }

    if (markerMode == 0) {
        // No markers — pure 16-bit sample encoding
        for (size_t i = 0; i < numSamples; ++i) {
            data_[i] = util::wave::double2awg16(samples[i]);     // 0x299700
        }
    } else if (markerMode == 1) {
        // Single marker bit — 1-marker encoding
        for (size_t i = 0; i < numSamples; ++i) {
            data_[i] = util::wave::double2awg1m(                 // 0x299680
                samples[i], markers[i]);
        }
    } else {
        // Multi-bit markers — full marker encoding
        for (size_t i = 0; i < numSamples; ++i) {
            data_[i] = util::wave::double2awg(                   // 0x299630
                samples[i], markers[i]);
        }
    }
}

// D1 (non-deleting) destructor — 0x2973d0
// D0 (deleting) destructor — 0x297400
//   Sets vtable, frees data_ vector if non-null.
//   D0 additionally calls operator delete(this, 0x20).
RawWaveHirzel16::~RawWaveHirzel16() = default;

// size() const — 0x297450
//   Returns data_.end() - data_.begin() in bytes.
//   (mov 0x10(%rdi),%rax; sub 0x8(%rdi),%rax)
size_t RawWaveHirzel16::size() const {  // 0x297450
    return reinterpret_cast<const char*>(data_.data() + data_.size())
         - reinterpret_cast<const char*>(data_.data());
    // Equivalently: data_.size() * sizeof(uint16_t)
}

// ptr() const — 0x297460
//   Returns data_.data() as char*.
//   (mov 0x8(%rdi),%rax)
const char* RawWaveHirzel16::ptr() const {  // 0x297460
    return reinterpret_cast<const char*>(data_.data());
}

// ==========================================================================
// RawWaveCervino
// ==========================================================================

// No explicit constructor symbol — constructed inline in Signal::getRawData()
// at 0x293f46. The inline construction:
//   1. operator new(0x20)
//   2. Set vtable to @0xb07868
//   3. Zero-init data_ (24 bytes at +0x08)
//   4. Resize data_ to samples.size()
//   5. Loop: data_[i] = double2awg(samples[i], markers[i])

// D1 (non-deleting) destructor — 0x2975b0
// D0 (deleting) destructor — 0x2975e0
//   Sets vtable, frees data_ vector if non-null.
//   D0 additionally calls operator delete(this, 0x20).
RawWaveCervino::~RawWaveCervino() = default;

// size() const — 0x297630
//   Returns data_.end() - data_.begin() in bytes.
size_t RawWaveCervino::size() const {  // 0x297630
    return data_.size() * sizeof(uint16_t);
}

// ptr() const — 0x297640
//   Returns data_.data() as char*.
const char* RawWaveCervino::ptr() const {  // 0x297640
    return reinterpret_cast<const char*>(data_.data());
}

// ==========================================================================
// Signal::getRawData(SampleFormat) factory — 0x293ec0
//
// Located in signal.cpp. Creates the appropriate RawWave subclass:
//
//   if (reserveOnly_):
//     RawWavePlaceHolder — new(0x28), byteSize_ = channels_ * length_ * 2
//     buffer_ left empty (lazy allocation in ptr())
//
//   if (format == SampleFormat::Hirzel16):
//     RawWaveHirzel16 — new(0x20), ctor @0x297140
//     Passes: this->samples_, this->markers_, this->markerBits_
//
//   else (Cervino, format == 0):
//     RawWaveCervino — new(0x20), inline construction
//     Converts each sample via double2awg(sample, marker)
//
// Note: The Cervino path uses markers_ (this+0x18) for marker data,
// same as Hirzel16 path. The difference is the conversion function.
// ==========================================================================

} // namespace zhinst
