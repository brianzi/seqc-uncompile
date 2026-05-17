// =============================================================================
// seqcc — `--dump=KIND[:PATH]` registry
//
// T3b ships three dump kinds that source their content from the ELF
// produced by the driver (`asm`, `waveforms`) or from the info-JSON
// it returns alongside the ELF (`compile-report`).  These require no
// access to live `AWGCompiler` state and therefore no recon-side
// edits — see AGENTS.md "Tooling vs reconstructed code".
//
// T3c added `ast-lowered`, sourced from `IRSinks` populated by
// `SeqcDriver::compile`.  T4a added `wavetable`, same source.
// T10a: `IRSinks` is now a standalone driver-side struct (no
// `CompileSeqcIntrospection` dependency); the driver captures handles
// directly via the public `AWGCompiler::compiler() →
// Compiler::{ast(), wavetable(), asmList()}` accessors.
// =============================================================================
#pragma once

#include "ir_sinks.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace seqcc {

struct Options;  // fwd from options.hpp

//! One `--dump=KIND[:path]` request parsed from the command line.
struct DumpSpec {
    std::string kind;                    //!< e.g. "asm", "waveforms"
    std::filesystem::path explicitPath;  //!< empty ⇒ derive from kind + opts
};

//! Format-hint mode for dumps that have a canonical format
//! different from their kind's default.  Currently advisory only —
//! all T3b kinds emit their native format regardless.
enum class DumpFormat { Auto, Json, Text };

//! All implemented kinds, in pipeline order.  Returned list is the
//! canonical order used by `--dump=all` (when that lands in T6).
std::vector<std::string> knownDumpKinds();

//! Returns true when at least one spec in `specs` needs IR sinks
//! (currently: `ast-lowered`, `wavetable`).  Historical: pre-T10a
//! the driver used this to decide between `compileSeqc()` and
//! `compileSeqcWithIR()`.  Post-T10a sinks are always populated by
//! the owned driver so this is informational only — kept for
//! diagnostic / future use.
bool needsIRSinks(std::vector<DumpSpec> const& specs);

//! Parse one `KIND[:PATH]` token.  Throws `std::invalid_argument`
//! on an unknown kind.
DumpSpec parseDumpSpec(std::string const& token);

//! Resolve where a dump for `spec` should land given the global
//! `--dump-dir` (`dumpDir`, may be empty) and the compile output
//! path (`outputElf`, used to derive the basename when neither
//! `dumpDir` nor `spec.explicitPath` says otherwise).
//!
//! Precedence:
//!   1. `spec.explicitPath` — used verbatim (relative resolved
//!      against current working directory).
//!   2. `dumpDir / "<basename>.<ext>"` if `dumpDir` non-empty.
//!   3. `<outputElf parent> / "<basename>.<ext>"` otherwise.
//!
//! `basename` is the stem of `outputElf` (e.g. `foo` from `foo.elf`)
//! suffixed with `.<kind>` (e.g. `foo.asm`).  Extension is chosen
//! per kind (`json` for compile-report and waveforms; `asm` for asm).
std::filesystem::path resolveDumpPath(DumpSpec const& spec,
                                      std::filesystem::path const& dumpDir,
                                      std::filesystem::path const& outputElf);

//! Emit every requested dump.  Inputs are the ELF bytes (for
//! section-extracting dumps), the info-JSON returned by the driver
//! (for `compile-report`), and the `IRSinks` populated by
//! `SeqcDriver::compile` (for IR-introspection dumps such as
//! `ast-lowered` / `wavetable`).  Throws on I/O errors; callers
//! should catch and report.
void emitDumps(std::vector<DumpSpec> const& specs,
               std::filesystem::path const& dumpDir,
               std::filesystem::path const& outputElf,
               std::string_view elfBytes,
               std::string_view infoJson,
               IRSinks const& sinks,
               DumpFormat formatHint);

//! T5: render the primary-output bytes for `stage`.  Used when
//! `--to=<stage>` (or its `-S`/`-E` aliases) selects a non-link
//! stage as the main `-o` target.
//!
//! Stage semantics:
//!   - "link"  — returns `elfBytes` verbatim.
//!   - "lower" — returns the JSON-serialised lowered AST from
//!               `sinks` (always populated by `SeqcDriver`).
//!               Empty string if the sink has no AST (compile
//!               failed before `stepLower`).
//!   - "asm"   — returns the ELF `.asm` section payload.  Empty if
//!               absent.
//!
//! Unknown / unsupported stage names throw `std::invalid_argument`
//! — they should have been rejected at CLI parse time, so reaching
//! here is a driver bug.  The CLI-parse-time check is the user-
//! visible diagnostic.
std::string renderStagePrimary(std::string const& stage,
                               std::string_view elfBytes,
                               IRSinks const& sinks);

}  // namespace seqcc
