// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveformIR — extends Waveform for the IR/backend stage
//
// Total size: 0xE0 bytes (Waveform base 0xD8 + 8 bytes of IR-specific fields)
// Confirmed by __on_zero_shared_weak deallocating 0xF8 bytes (= 0x18 ctrl + 0xE0).
// __on_zero_shared jumps directly to ~Waveform — meaning NO IR field has a
// non-trivial destructor (so no std::string/vector/shared_ptr at +0xD8..+0xDF).
//
// Confirmed from:
//   - WaveformIR::WaveformIR(shared_ptr<WaveformFront>) at 0x114da0
//   - WaveformIR::WaveformIR(shared_ptr<Waveform>)     at 0x2a9240
//   - WaveformIR::toJsonElement(SampleFormat)          at 0x2c5440
//   - __shared_ptr_emplace<WaveformIR>::__on_zero_shared_weak alloc=0xF8 at 0x114d90
//
// IR-specific layout (ctor evidence at 0x114e49..0x114e63 / 0x2a92e9..0x2a9303):
//   mov WORD  PTR [rbx+0xd8], 0x0   ; zeroes BOTH +0xD8 and +0xD9 as a word
//   mov BYTE  PTR [rbx+0xda], 0x0
//   mov DWORD PTR [rbx+0xdc], <eax>  ; copied from source->file->field24
//
// Per-byte usage evidence (single-byte access patterns confirm two bools):
//   +0xD8 byte writes:  0x1c9850 (Prefetch::prepareTree),
//                       0x1d4e13 (Prefetch::createLoad)
//                       Both set BYTE = 1 when marking referenced waveforms.
//   +0xD9 byte writes:  0x1cb9bf, 0x1cbab1, 0x1cbb17 (Prefetch::determineFixedWaves)
//                       Set BYTE = 1 when waveform fits memory constraints.
//                       Also read in WavetableIR::allocateWaveformsForFifo
//                       (0x2aa723 phase-1, 0x2acfcd phase-2) to partition fixed/free.
//   +0xDA byte writes:  0x2a9e47, 0x2aa88b, 0x2ad023, 0x2ad0ac
//                       Set/cleared inside WavetableIR::allocateWaveforms.
//   +0xDC dword reads:  0x293a88, 0x293bea, 0x293dad (ElfWriter::addWaveform),
//                       0x10e1c7 (writeWavesToElfAbsolute)
//                       Used as a size/offset in ELF writing computations.
//
// Net layout:
//   +0xD8  bool      markedForLoad  ("used"/"referenced" — set by prefetch passes)
//   +0xD9  bool      fixed_         (placement-fixed; partitions FIFO allocation)
//   +0xDA  bool      crossesCacheLine_  (allocation flag: waveform straddles
//                                        a cache-line boundary; set true for
//                                        fillers, copied from MemoryAllocator
//                                        block flags bit 8 for normal waves,
//                                        cleared on reload-without-crossing)
//   +0xDB  1 byte    (padding to align +0xDC int32)
//   +0xDC  int32_t   elfAlignment_       (per-waveform allocation size, from DeviceConstants;
//                                    used by ElfWriter::addWaveform)
//   +0xE0  END
//
// Speculative fields removed (verified absent — would need destructors,
// or would not fit in 8-byte IR area, or duplicate Waveform base fields):
//   - numPages        → was actually Signal::channels_ at +0xC8 (uint16) or
//                        ambiguous; see real access sites (prefetch_placesingle.cpp
//                        comments at +0xC8 are reading Signal+0x48 = channels_)
//   - playCount       → was Signal::length_ at +0xD0 (size_t)
//   - channelCount    → Signal::channels_ at +0xC8 (use signal.channels())
//   - irBool0         → same field as fixed_ (+0xD9) under a different name
//   - address         → Waveform::addressValue at +0x4C (already present)
//   - waveformOffset  → Waveform::addressValue at +0x4C (same field)
//   - sizeInBytes     → Waveform::allocationByteSize at +0x74 (already present)
//   - used (separate) → Waveform::used at +0x48 (already present)
// ============================================================================
#pragma once

#include "zhinst/waveform/waveform.hpp"
#include "zhinst/waveform/signal.hpp"    // for SampleFormat
#include <memory>
#include <string>
#include <functional>    // for std::less

namespace boost { namespace property_tree {
    template<typename Key, typename Data, typename KeyCompare> class basic_ptree;
} }

namespace zhinst {

struct WaveformFront;

//! Back-end representation of a waveform: extends `Waveform` with
//! the placement and prefetch flags that the IR / wavetable passes
//! need but the front-end parsed form does not.
//!
//! Added flags mark whether prefetch determined the waveform is
//! actually referenced (`markedForLoad`), whether wavetable
//! placement has pinned it (`fixed_`), and whether its allocated
//! slot straddles a cache-line boundary (`crossesCacheLine_`).
//! `elfAlignment_` carries the per-waveform allocation size used
//! when emitting the `.wf_<name>` ELF section. Constructed from
//! either a `WaveformFront` or an existing `Waveform`, or in place
//! by the wavetable manager from a name and device-constants
//! reference.
struct WaveformIR : Waveform {
    //! \brief Set by the prefetch passes when this waveform is actually referenced (`true`) and so will be included in the cache plan.
    bool markedForLoad;     // +0xD8  "used"/"referenced" by prefetch passes
    //! \brief Placement-fixed flag; partitions waveforms into the fixed and the FIFO-allocated halves of the wavetable.
    bool fixed_;            // +0xD9  placement-fixed; partitions FIFO alloc
    //! \brief `true` when this waveform's allocated slot straddles a cache-line boundary
    //!  (always `true` for filler waveforms; for normal waveforms set by
    //!  `WavetableIR::allocateWaveforms` from bit 8 of the
    //!  `MemoryAllocator` block flags, and cleared on reload into a non-straddling slot).
    bool crossesCacheLine_; // +0xDA  set true for filler waveforms; for
                            //         normal waveforms = bit 8 of the
                            //         MemoryAllocator block.flags written by
                            //         WavetableIR::allocateWaveforms; cleared
                            //         when a waveform is reloaded into a slot
                            //         that does not straddle a cache line.
    // +0xDB: 1 byte padding
    //! \brief Per-waveform allocation size (sourced from `DeviceConstants`) consumed by `ElfWriter::addWaveform`.
    int32_t elfAlignment_;       // +0xDC  per-waveform allocation size (from DC)

    // --- Convenience accessors (these forward to the appropriate base/Signal field;
    //     they are NOT separate storage — they exist so legacy call sites continue
    //     to compile while we audit each call's intended target) ---
    //! \brief Forwarder for `Waveform::used` (whether the waveform is referenced).
    //! \return  `true` iff the underlying `Waveform::used` flag is set.
    bool isUsed() const { return used; }                // Waveform::used at +0x48
    //! \brief Forwarder for `Waveform::allocationByteSize`.
    //! \return  The waveform's allocated size in bytes.
    int  getAllocationByteSize() const { return allocationByteSize; } // Waveform::allocationByteSize +0x74

    //! \brief Construct a back-end IR node from a parsed front-end
    //!        waveform, copying the front-end fields and zeroing the
    //!        IR-specific flags.
    //! \param source  Parsed front-end waveform to adopt.
    // Construct from WaveformFront — 0x114da0
    explicit WaveformIR(std::shared_ptr<WaveformFront> source);

    //! \brief Construct a back-end IR node by adopting an existing
    //!        `Waveform`'s base subobject; IR-specific flags start
    //!        zeroed and `elfAlignment_` is copied from the source's
    //!        backing-file alignment field.
    //! \param source  Existing waveform whose base subobject is adopted.
    // Construct from Waveform — 0x2a9240
    explicit WaveformIR(std::shared_ptr<Waveform> source);

    //! \brief Construct a placeholder IR waveform for the wavetable
    //!        manager: only `name`, `waveformType`, and
    //!        `deviceConstants_` are populated; `waveIndex` is set to
    //!        `-1` and every other field (including the IR flags) is
    //!        zeroed.
    //! \param name  Waveform name.
    //! \param type  Backing-file kind tag.
    //! \param dc    Device-constants reference stored on the waveform.
    // Construct in-place from (name, file type, device constants).
    // The body of this ctor is inlined into the
    //   allocate_shared<WaveformIR>(allocator, name, File::Type, DC&)
    // dispatcher at 0x2aa170-0x2aa20f, which is itself called by
    // WavetableManager<WaveformIR>::newWaveform (0x2aa004). The dispatcher
    // initializes Waveform::name (+0), Waveform::waveformType (+0x18) and
    // Waveform::deviceConstants_ (+0x78), zeroes everything else, and sets
    // Waveform::waveIndex (+0x6C / dispatcher offset +0x84) to -1.
    WaveformIR(const std::string& name,
               Waveform::File::Type type,
               const DeviceConstants& dc);

    //! \brief Property-tree element type used by `toJsonElement`.
    // Serialize to a property tree element — 0x2c5440
    // Returns a ptree (sret via rdi)
    using ptree = boost::property_tree::basic_ptree<std::string, std::string, std::less<std::string>>;
    //! \brief Render this waveform as a JSON property-tree element
    //!        suitable for inclusion in the `.waveforms` ELF section,
    //!        encoding the samples in the requested `format`.
    //! \param format  Sample-encoding format selector.
    //! \return  Property-tree element ready to embed in the waveforms JSON.
    ptree toJsonElement(SampleFormat format) const;
};

} // namespace zhinst
