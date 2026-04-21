// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveformIR — extends Waveform for the IR/backend stage
//
// Total size: 0xE0 bytes (Waveform base 0xD8 + 8 bytes of IR-specific fields)
//
// Confirmed from:
//   - WaveformIR::WaveformIR(shared_ptr<WaveformFront>) at 0x114da0
//   - WaveformIR::WaveformIR(shared_ptr<Waveform>) at 0x2a9240
//   - WaveformIR::toJsonElement(SampleFormat) at 0x2c5440
//   - __shared_ptr_emplace<Waveform>::__on_zero_shared_weak alloc=0xF0 at 0x2a9410
//     (0xF0 - 0x18 control block = 0xD8 for Waveform; IR uses 0xE0)
// ============================================================================
#pragma once

#include "waveform.hpp"
#include "signal.hpp"    // for SampleFormat
#include <memory>
#include <string>
#include <functional>    // for std::less

namespace boost { namespace property_tree {
    template<typename Key, typename Data, typename KeyCompare> class basic_ptree;
} }

namespace zhinst {

struct WaveformFront;

// ============================================================================
// WaveformIR layout — 0xE0 bytes
//
// Offset  Size  Type              Name          Notes
// ------  ----  ----              ----          -----
// 0x00-0xD7     Waveform          (base)        0xD8 bytes
// 0xD8     2    uint16_t          irField1      init=0 in ctor (was frontField1 area)
// 0xDA     1    bool              irBool1       init=0 in ctor
// 0xDB     1    (padding)
// 0xDC     4    int32_t           irField2      copied from source->file->field24
//                                               (DeviceConstants-related field)
// 0xE0          END
// ============================================================================
struct WaveformIR : Waveform {
    uint16_t irField1;      // +0xD8  init=0
    bool irBool1;           // +0xDA  init=0
    // +0xDB: 1 byte padding
    int32_t irField2;       // +0xDC  from source->deviceConstants->someField

    // TODO: these fields are referenced in prefetch .cpp files but exact offsets
    // within WaveformIR are not yet confirmed. They may be aliases for existing fields.
    int numPages = 0;       // offset TBD — number of cache pages
    bool fixed_ = false;    // offset TBD — whether waveform is fixed/locked
    int playCount = 0;      // offset TBD — number of plays referencing this waveform
    bool irBool0 = false;   // offset TBD — IR boolean flag (distinct from irBool1)
    int channelCount = 0;   // offset TBD — number of channels
    uint32_t address = 0;   // offset TBD — waveform memory address
    int waveformOffset = 0; // offset TBD — offset within waveform table
    int sizeInBytes = 0;    // offset TBD — total size in bytes
    bool used = false;      // offset TBD — whether waveform is used (isUsed accessor)

    bool isUsed() const { return used; }
    int getSampleCount() const { return sizeInBytes; } // TODO: real calculation TBD

    // Construct from WaveformFront — 0x114da0
    explicit WaveformIR(std::shared_ptr<WaveformFront> source);

    // Construct from Waveform — 0x2a9240
    explicit WaveformIR(std::shared_ptr<Waveform> source);

    // Serialize to a property tree element — 0x2c5440
    // Returns a ptree (sret via rdi)
    using ptree = boost::property_tree::basic_ptree<std::string, std::string, std::less<std::string>>;
    ptree toJsonElement(SampleFormat format) const;
};

} // namespace zhinst
