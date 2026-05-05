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
