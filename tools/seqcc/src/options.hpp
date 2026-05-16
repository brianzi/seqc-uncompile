// =============================================================================
// seqcc — CLI option model
//
// Translates the seqcc flag surface into the inputs required by
// zhinst::compileSeqc():
//   - a JSON config string (sequencer, samplerate, filename, wavepath,
//     waveforms)
//   - device-id string (e.g. "HDAWG8")
//   - awg index
//   - device-options string
//   - output ELF path
//
// Flag surface evolution:
//   T2  — single `-mtune=KEY=VALUE` channel for everything (kwarg-style).
//   T3a — promoted dedicated flags for sequencer, samplerate, filename,
//         wave-path, waveforms; `-mtune` kept as escape hatch only;
//         singular repeatable `-mdevopt=FEATURE`; `-mtune=index=` rejected
//         (was silently a no-op in T2; see IF-296).
//   T5+ — pipeline-stage flags (-S/-E/--from=/--to=, -O, -f).
// =============================================================================

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace seqcc {

//! Parsed, validated CLI options for a single seqcc invocation.
//!
//! Constructed in `main.cpp`; consumed by `runCompile()` in
//! `compile.cpp`.  Each first-class flag corresponds to one field;
//! the catch-all `tune` vector carries `-mtune=KEY=VALUE` escape-hatch
//! kwargs for keys that don't yet have a dedicated flag.
struct Options {
    //! Single input .seqc file (multi-input deferred — the underlying
    //! `compileSeqc()` entry point is single-source).
    std::filesystem::path input;

    //! Output ELF path.  Defaults to `<input-stem>.elf` when unset.
    std::filesystem::path output;

    //! Device type, e.g. "HDAWG8", "SHFQA4".  Set via `-march=<dev>`.
    //! Mandatory.
    std::string devtype;

    //! Device options accumulated from repeatable `-mdevopt=FEATURE`
    //! flags.  Joined with newlines when building the binding's
    //! `options` positional string — matches the binding's
    //! `boost::to_upper_copy(options)` + line-by-line consumer.
    //!
    //! For backwards compatibility with T2, the deprecated
    //! `-mdevopts=PACKED` flag (newline-separated, plural) also
    //! populates this vector by splitting on `\n`.
    std::vector<std::string> devopts;

    //! Sequencer selector, set via `--sequencer={auto|qa|sg}`.
    //! Empty == use compileSeqc()'s default (Auto).
    //! Required for SHFQA / SHFQC / SHFSG; ignored for other devices.
    std::string sequencer;

    //! Sample rate in Hz, set via `--samplerate=<Hz>`.  HDAWG only;
    //! the binding errors with "'samplerate' is relevant for HDAWG only"
    //! for any other device.
    //!
    //! `std::optional` so we can distinguish "not set" (omit the JSON
    //! key entirely) from "explicitly set to 0".
    std::optional<double> samplerate;

    //! Source-filename hint recorded in the ELF info report.  Set via
    //! `--filename=<path>`; defaults to the basename of `input` when
    //! unset (mirrors the binding's behaviour for the `filename` kwarg).
    std::string filename;

    //! Waveform search directory, set via `--wave-path=<dir>`.  Empty
    //! falls back to `compileSeqc()`'s built-in default
    //! (`ZiFolder::Data / "awg" / "waves"`).
    std::string wavePath;

    //! CSV waveform names, set via `--waveforms=<list>`.  The list is a
    //! `;`-separated string passed verbatim to compileSeqc(), which
    //! splits it internally.
    std::string waveforms;

    //! Escape-hatch JSON kwargs: each `-mtune=KEY=VALUE` adds one entry.
    //! Keys with dedicated flags above (`sequencer`, `samplerate`, ...)
    //! are still accepted here for forward compatibility, but a
    //! dedicated flag takes precedence if both are set.
    //!
    //! Reserved keys that are *rejected* in appendTune():
    //!   - `index` — was silently a no-op in T2 (IF-296); now requires
    //!     `--index=N`.
    std::vector<std::pair<std::string, std::string>> tune;

    //! Zero-based AWG core index, set via `--index=N`.  Defaults to 0.
    unsigned long awgIndex{0};
};

//! Build the JSON config string that compileSeqc() expects.
//!
//! Walks dedicated fields first (sequencer, samplerate, filename,
//! wavePath, waveforms), then folds in any `-mtune=KEY=VALUE` kwargs
//! whose key isn't already covered.  Numeric values use full double
//! precision; strings are JSON-escaped.  Invalid numeric `-mtune`
//! values throw `std::invalid_argument`.
std::string buildJsonConfig(Options const& opts);

//! Join `opts.devopts` with newlines for the binding's `options`
//! positional.  Returns an empty string when the vector is empty.
std::string buildDevoptsString(Options const& opts);

}  // namespace seqcc
