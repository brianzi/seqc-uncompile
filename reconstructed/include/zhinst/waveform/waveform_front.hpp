// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveformFront — waveform metadata for the compiler frontend
//
// WaveformFront extends Waveform (base class, 0xD8 bytes).
// ============================================================================
#pragma once

#include "zhinst/waveform/waveform.hpp"
#include "zhinst/ast/value.hpp"

#include <memory>
#include <string>
#include <vector>

namespace zhinst {

// Offset  Size  Type              Name            Notes
// +0x00   0xD8  Waveform (base)
// +0xD8   4     int               useCount_       init=1
// +0xDC   1     bool              dirty_          "isModified"
// +0xDD   1     bool              hasDuplicate_
// +0xDE   2     (padding)
// +0xE0   24    vector<Value>     genArgs_        element size 0x28
// sizeof(WaveformFront) = 0xF8
//! Front-end extension of `Waveform` that adds the bookkeeping
//! needed during SeqC compilation: how often the waveform is
//! referenced, whether it has been modified since its last
//! placement, whether a duplicate was detected, and the original
//! generator-function arguments (for cache lookup and re-evaluation).
//!
//! `WaveformGenerator` returns instances of this type; the IR
//! lowering pass converts each one into a `WaveformIR`.
struct WaveformFront : Waveform {
    int  useCount_;                    //!< Reference count tracking how many SeqC variables / call sites reference this waveform. Initialised to 1 on construction; the front-end uses non-zero use counts to decide which waveforms reach the lowering pass. +0xD8
    bool dirty_;                       //!< Set when the waveform's sample data has been mutated since the last `getWaveformForFront` lookup. Exposed via the `isModified()` / `setModified()` accessors. Drives the dedup gate in `WavetableManager::getWaveformForFront`: a modified waveform is no longer a valid cache hit for an identical generator call. +0xDC
    bool hasDuplicate_;                //!< Set on both entries when two waveforms are registered under the same name (see `WavetableManager::newWaveformFromFile`). Read by later passes that need to diagnose ambiguous-name conditions. +0xDD
    // +0xDE..+0xDF: 2 bytes padding
    std::vector<Value> genArgs_;       //!< Argument vector captured from the SeqC source for the call that produced this waveform (e.g. `gauss(64, 32, 16)` → three `Value`s). Used by re-evaluation paths and by the symbol-renaming audit; each `Value` is 0x28 bytes. +0xE0
    // +0xF8: END

    /*! \brief Construct a fresh front-end waveform bound to a name,
     *  source-file kind, and device constants snapshot.
     *
     *  \details Initialises the `Waveform` base with empty signal
     *  payload, `waveIndex == -1`, and `minLengthSamples` taken
     *  from `dc.maxWaveformLength`, then sets the front-extension
     *  fields to their starting values (`useCount_ = 1`,
     *  `dirty_` and `hasDuplicate_` cleared, `genArgs_` empty).
     *
     *  \param name  User-visible waveform identifier.
     *  \param type  Source-file kind (CSV / RAW / GEN).
     *  \param dc    Device-constants snapshot. Stored by pointer; the
     *               object must outlive the waveform.
     */
    WaveformFront(const std::string& name, Waveform::File::Type type,
                  const DeviceConstants& dc);

    /*! \brief Copy-with-rename constructor used when the front-end
     *  needs a private mutable waveform that shares the source's
     *  initial sample state but carries its own identity.
     *
     *  \details Delegates to the `Waveform` copy-rename
     *  constructor (which deep-copies the base-class signal,
     *  generator descriptor, file pointer, etc., then overwrites
     *  the name with `newName`), then resets `useCount_` to 1,
     *  clears `dirty_`, copies `hasDuplicate_` from the source,
     *  and element-wise copies the source's `genArgs_` vector.
     *
     *  \param source   Source waveform. Must be non-null.
     *  \param newName  New user-visible name for the copy.
     */
    WaveformFront(std::shared_ptr<WaveformFront> source, std::string const& newName);  // 0x2a2510

    //! \brief Tears down the front-extension `genArgs_` vector and
    //! then runs the `Waveform` base destructor. Called via the
    //! `shared_ptr` deleter when the last reference is dropped.
    // Implicit destructor (invoked at 0x2a1300 via __on_zero_shared).
    ~WaveformFront();

    /*! \brief Format a human-readable single-line description of
     *  this waveform for verbose-mode debug dumps.
     *
     *  \details Emits `"Name: <name> (<TYPE>) <length> samples & <channels> channels"`
     *  where `<TYPE>` is one of `CSV`, `RAW`, `GEN`, or `UNDEF`.
     *
     *  \return The formatted description.
     */
    std::string toString() const;  // 0x2c5120

    // --- Convenience accessors that forward to existing fields ---
    // (these forward to Waveform-base or our own bools; they exist so legacy
    //  call sites continue to compile while we audit each call's intent).

    // hasDuplicate ↔ hasDuplicate_ (+0xDD)
    //! \brief Mark (or unmark) this waveform as one of two entries
    //! that were registered under the same name.
    //! \param v New duplicate flag.
    void setHasDuplicate(bool v) { hasDuplicate_ = v; }
    //! \brief Read the duplicate flag set by `setHasDuplicate()`.
    //! \return `true` if this waveform shares its name with another
    //! registration in the same wavetable.
    bool hasDuplicate() const    { return hasDuplicate_; }

    // isModified ↔ dirty_ (+0xDC)
    //! \brief Read the modification flag tracked in `dirty_`.
    //! \return `true` if the waveform's sample data has been mutated
    //! since registration; such waveforms are not eligible as
    //! deduplication cache hits in `WavetableManager::getWaveformForFront`.
    bool isModified() const      { return dirty_; }
    //! \brief Set or clear the modification flag.
    //! \param v New `dirty_` value.
    void setModified(bool v)     { dirty_ = v; }

    // funDescrName ↔ Waveform::funDescrName (+0x50, JSON key "genFunc")
    //! \brief Read the generator-function descriptor name from the
    //! base-class `funDescrName` field (JSON key `"genFunc"`).
    //! \return Reference to the stored descriptor string.
    std::string const& funDescrName() const { return Waveform::funDescrName; }
    //! \brief Replace the generator-function descriptor name on the
    //! base-class `funDescrName` field.
    //! \param s New descriptor; moved into place.
    void setFunDescrName(std::string s)     { Waveform::funDescrName = std::move(s); }

    //! \brief Attach a source-file descriptor, replacing any prior
    //! `Waveform::file` shared pointer.
    //! \param f New file descriptor (may be null to detach).
    void setFile(std::shared_ptr<Waveform::File> f) { file = std::move(f); }
    //! \brief Rename the waveform by overwriting the base-class
    //! `name` field. Callers using this directly bypass
    //! `WavetableManager::updateWave` and so do **not** update the
    //! manager's name index — prefer `WavetableFront::updateWave`
    //! when the waveform is registered in a wavetable.
    //! \param n New user-visible name.
    void setName(const std::string& n)              { name = n; }
    //! \brief Bind this waveform to an explicit device wavetable
    //! slot by overwriting the base-class `waveIndex` field.
    //! Most call sites should go through
    //! `WavetableFront::assignWaveIndex` instead, which validates
    //! against an already-assigned index.
    //! \param idx Target wave-index slot, or `-1` to clear.
    void setWaveIndex(int idx)                      { waveIndex = idx; }
};

} // namespace zhinst
