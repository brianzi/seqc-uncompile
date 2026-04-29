// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveformFront — waveform metadata for the compiler frontend
//
// WaveformFront extends Waveform (base class, 0xD8 bytes).
// Total size: 0xF8 (248 bytes). Confirmed:
//   - __on_zero_shared_weak at 0x2a13a0 deletes 0x110 bytes
//     (= 0x18 control block + 0xF8 object) → object is 0xF8.
//   - __on_zero_shared at 0x2a1300 destroys vector<Value> at +0xE0..+0xF8
//     (element stride 0x28), then jumps to ~Waveform — proving NO additional
//     non-trivially-destructible field exists between +0xDD and +0xE0.
//
// Confirmed from:
//   - WaveformFront::WaveformFront(shared_ptr<WaveformFront>, string) at 0x2a2510
//   - WaveformFront::toString() const at 0x2c5120
//   - __shared_ptr_emplace<WaveformFront>::__on_zero_shared() at 0x2a1300
//   - WaveformIR::WaveformIR(shared_ptr<WaveformFront>) at 0x114da0
//
// Ctor field initialization (0x2a25b9..0x2a2671):
//   mov DWORD PTR [rbx+0xd8], 0x1     ; useCount_ = 1
//   mov BYTE  PTR [rbx+0xdc], 0x0     ; dirty_  = false
//   mov BYTE  PTR [rbx+0xdd], cl      ; hasDuplicate_  = source->hasDuplicate_
//   <padding +0xDE..+0xDF>
//   <copy-construct vector<Value> at +0xE0 from source +0xE0>
//
// Layout:
//   +0x00..+0xD7  Waveform base (0xD8 bytes)
//   +0xD8         int        useCount_   (init=1)
//   +0xDC         bool       dirty_    (init=0; source comments call it "isModified")
//   +0xDD         bool       hasDuplicate_    (copied from source; "hasDuplicate")
//   +0xDE..+0xDF  padding
//   +0xE0         vector<Value> genArgs_     (24 bytes; element size 0x28)
//   +0xF8         END
//
// Speculative fields removed (verified absent by ctor + dtor):
//   - isAllocated   → was Waveform::used at +0x48 (already present)
//   - channels      → was Waveform::signal.channels_ at +0xC8 (signal+0x48)
//   - sampleLength  → was Waveform::signal.length_ at +0xD0 (signal+0x50)
//   - fileType      → was Waveform::waveformType at +0x18 (already present)
//   - isModified    → was dirty_ at +0xDC (this very struct's field)
//   - funDescrName_ → was Waveform::funDescrName at +0x50 (already present)
//   - args_         → no usage anywhere; pure hallucination
// ============================================================================
#pragma once

#include "waveform.hpp"
#include "value.hpp"

#include <memory>
#include <string>
#include <vector>

namespace zhinst {

struct WaveformFront : Waveform {
    int  useCount_;                    // +0xD8  (init=1)
    bool dirty_;                     // +0xDC  (init=0; "isModified" in source comments)
    bool hasDuplicate_;                     // +0xDD  (copied; "hasDuplicate")
    // +0xDE..+0xDF: 2 bytes padding
    std::vector<Value> genArgs_;           // +0xE0  (each Value is 0x28 bytes)
    // +0xF8: END

    // WaveformFront(name, fileType, devConst) — INLINED into the
    // WavetableManager<WaveformFront>::newWaveformFromFile dispatcher at
    // 0x29b110-0x29b24f. No standalone symbol in the binary.
    //
    // Sets, on the freshly-allocated 0xF8-byte object:
    //   Waveform::name              = name
    //   Waveform::waveformType      = type
    //   Waveform::waveIndex         = -1
    //   Waveform::minLengthSamples       = dc.maxWaveformLength (dc+0x40)
    //   Waveform::deviceConstants   = &dc
    //   useCount_                 = 1                          ← differs from IR
    //   dirty_, hasDuplicate_      = 0
    //   genArgs_                      = empty vector<Value>
    //   (all other Waveform/Signal fields zero / empty)
    //
    // We provide a body so reconstructed TUs that say
    // make_shared<WaveformFront>(name, type, dc) can link.
    WaveformFront(const std::string& name, Waveform::File::Type type,
                  const DeviceConstants& dc);

    // WaveformFront(shared_ptr<WaveformFront> source, string const& newName) — 0x2a2510
    WaveformFront(std::shared_ptr<WaveformFront> source, std::string const& newName);

    // ~WaveformFront — implicit (at 0x2a1300 via __on_zero_shared)
    ~WaveformFront();

    // toString() const — 0x2c5120
    std::string toString() const;

    // --- Convenience accessors that forward to existing fields ---
    // (these forward to Waveform-base or our own bools; they exist so legacy
    //  call sites continue to compile while we audit each call's intent).

    // hasDuplicate ↔ hasDuplicate_ (+0xDD)
    void setHasDuplicate(bool v) { hasDuplicate_ = v; }
    bool hasDuplicate() const    { return hasDuplicate_; }

    // isModified ↔ dirty_ (+0xDC)
    bool isModified() const      { return dirty_; }
    void setModified(bool v)     { dirty_ = v; }

    // funDescrName ↔ Waveform::funDescrName (+0x50, JSON key "genFunc")
    std::string const& funDescrName() const { return Waveform::funDescrName; }
    void setFunDescrName(std::string s)     { Waveform::funDescrName = std::move(s); }

    void setFile(std::shared_ptr<Waveform::File> f) { file = std::move(f); }
    void setName(const std::string& n)              { name = n; }
    void setWaveIndex(int idx)                      { waveIndex = idx; }
};

} // namespace zhinst
