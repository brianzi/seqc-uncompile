// =============================================================================
// seqcc — CLI option model
//
// Translates gcc-style flags (-march, -mtune=k=v, -o, --wave-path,
// --waveforms) into the inputs required by zhinst::compileSeqc():
//   - a JSON config string (sequencer, samplerate, filename, wavepath,
//     waveforms)
//   - device-id string (e.g. "HDAWG8")
//   - awg index
//   - device-options string
//   - output ELF path
//
// Sub-phase T2.  Pipeline-stage flags (-S/-E/--from=/--to=, -O, -f)
// arrive in T5+ and will extend this struct.
// =============================================================================

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace seqcc {

//! Parsed, validated CLI options for a single seqcc invocation.
//!
//! Constructed by `parseOptions()`; consumed by `runCompile()`.
struct Options {
    //! Single input .seqc file (multi-input deferred to a later phase —
    //! the underlying compileSeqc() entry point is single-source).
    std::filesystem::path input;

    //! Output ELF path.  Defaults to input basename + ".elf" when unset.
    std::filesystem::path output;

    //! Device type, e.g. "HDAWG8", "SHFQA4".  Set via `-march=<dev>`.
    //! Mandatory.
    std::string devtype;

    //! Newline-separated device options string (case-insensitive),
    //! set via `-mdevopts=...` (gcc-style; matches the binding's
    //! `options` positional).  Empty by default.
    std::string devopts;

    //! JSON config keys collected from `-mtune=<key>=<value>`.  Mirrors
    //! the `**kwargs` channel of the Python binding: each entry becomes
    //! a top-level field in the JSON config that compileSeqc() parses.
    //!
    //! Recognised keys (see reconstructed/src/codegen/compile_seqc.cpp):
    //!   sequencer    string  "qa" | "sg" | "auto" (default)
    //!   samplerate   double  HDAWG only
    //!   filename     string  recorded in the ELF info report
    //!   wavepath     string  search directory for CSV waveforms
    //!   waveforms    string  ';'-separated list of CSV waveform names
    //!   index        double  AWG core index (also settable via --index)
    //!
    //! Stored as raw (string, string) pairs; numeric coercion happens
    //! in `buildJsonConfig()` so this struct stays trivially printable
    //! for `--dump-options` (a debugging flag landing in T3).
    std::vector<std::pair<std::string, std::string>> tune;

    //! Zero-based AWG core index, set via `--index=N` (T2 convenience
    //! flag — also expressible as `-mtune=index=N`).  Defaults to 0.
    unsigned long awgIndex{0};
};

//! Build the JSON config string that compileSeqc() expects.
//!
//! Walks `opts.tune` and emits a flat JSON object.  Numeric keys
//! (`samplerate`, `index`) are coerced to double via `std::stod`;
//! everything else is emitted as a string.  Invalid numeric values
//! throw `std::invalid_argument`.
std::string buildJsonConfig(Options const& opts);

}  // namespace seqcc
