// =============================================================================
// seqcc — owned compilation driver (Phase T5a)
//
// SeqcDriver is the structural replacement for the direct call into
// `zhinst::compileSeqcWithIR()` that `compile.cpp::runCompile()`
// currently makes.  The goal across T5a/T5b/T6/T10a is to move the
// outer compile flow (config marshalling → AWGCompiler ownership →
// addWaveforms → compileString → writeToStream → pack) out of the
// recon-side `compileSeqcImpl()` and into this driver, so that:
//
//   - seqcc can drive the pipeline phase-by-phase once T5b lands the
//     stepwise `Compiler` / `AWGCompiler` API;
//   - `--from=<stage>` (T6) can construct a `Compiler` instance and
//     hand-populate mid-pipeline state;
//   - `compileSeqcWithIR()` + `CompileSeqcIntrospection` can retire
//     (T10a), leaving `compileSeqc()` as the sole recon-side entry
//     point.
//
// Sub-phase T5a.1 (this commit) lands the class shell.  `compile()`
// is a thin delegating wrapper around `compileSeqcWithIR()` so that
// the binary is byte-identical to the pre-T5a path.  Behaviour
// changes ship in T5a.2 (driver owns the outer flow) and T5b
// (stepwise API).
//
// **No recon edits in T5a.**
// =============================================================================

#pragma once

#include "ir_sinks.hpp"
#include "options.hpp"

#include <string>
#include <utility>

namespace seqcc {

//! Outcome of a single SeqcDriver compile.  Mirrors the layout the
//! pybind shim exposes: an opaque ELF byte stream plus an
//! info-JSON string that describes the compile (messages,
//! max-elf-size, wavemem usage), plus the structured IR handles
//! captured into `IRSinks` for downstream dump emission.
struct DriverResult {
    //! ELF bytes — the same payload that the recon-side
    //! `writeToStream()` would write.  In T5a.1 this is extracted
    //! from the packed `info\0elf` blob returned by
    //! `compileSeqcWithIR()`; in T5a.2 it will be produced directly
    //! by the driver-owned `AWGCompiler::writeToStream` call.
    std::string elf;

    //! Info-JSON string — the same payload that pybind exposes via
    //! the second element of its `(elf, info)` tuple.  In T5a.1
    //! extracted from the same packed blob; in T5a.2 assembled
    //! directly by the driver from `getCompileReport()` +
    //! `getJsonWaveformMemoryInfo()`.
    std::string infoJson;

    //! IR handles captured during the compile, for `--dump=` and
    //! `--to=<stage>` consumers.  Lifetime is bound to the
    //! `DriverResult`; the underlying `shared_ptr`s keep the IRs
    //! alive across the recon-side `AWGCompiler` destruction.
    IRSinks sinks;
};

//! seqcc-owned compilation driver.  Constructed per-invocation; not
//! re-entrant.  See the file-level comment for the multi-sub-phase
//! roadmap.
class SeqcDriver {
public:
    //! Construct a driver bound to the given parsed options.  Does
    //! not perform any I/O or compilation; that happens in
    //! `compile(source)`.
    explicit SeqcDriver(Options const& opts);

    //! Compile the given source string and return the ELF + info +
    //! IR handles.  Throws `std::exception` (or a subclass) on
    //! compile error; callers translate to the usual seqcc exit
    //! codes / diagnostics.
    //!
    //! In T5a.1 this delegates to `zhinst::compileSeqcWithIR()`
    //! with the same arguments the legacy path constructed.  In
    //! T5a.2 it will own the outer flow directly (still using
    //! `AWGCompiler` as the inner engine); the input/output
    //! contract of this method is invariant across the
    //! sub-phases.
    //!
    //! \param source  The .seqc source text (already slurped from
    //!                the input file by the caller).
    //! \param jsonConfig  The JSON-encoded config string the recon
    //!                    side expects; built by
    //!                    `buildJsonConfig(opts_)` upstream.
    //! \param devoptsStr  The newline-joined device-options string;
    //!                    built by `buildDevoptsString(opts_)`.
    DriverResult compile(std::string const& source,
                         std::string const& jsonConfig,
                         std::string const& devoptsStr);

private:
    Options const& opts_;
};

}  // namespace seqcc
