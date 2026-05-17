// =============================================================================
// seqcc — pipeline-stage selector (T5).
//
// Canonical list of stages the driver knows about, and which of them
// are reachable from the current driver release.  The "supported"
// flag controls whether `--to=<stage>` accepts the name or rejects
// it with a "not yet implemented" diagnostic.
//
// Stage names mirror DESIGN.md §3 and reconstructed/notes/pipeline.md.
// Adding a new entry here is the only place a stage's existence is
// declared; CLI validation and `--print-stages` both consult this
// table.
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace seqcc {

//! Outputs a stage emits when it is selected as the primary `-o`
//! target.  Mostly informational; the driver dispatch is
//! per-name, not per-extension.
struct StageInfo {
    //! Canonical name accepted by `--to=<name>`.
    std::string name;

    //! Default extension for the per-stage output file when `-o`
    //! is absent.  Includes the dot, e.g. ".elf", ".asm",
    //! ".ast-lowered.json".  An empty string means "no default;
    //! `-o <path>` is required when this stage is selected"
    //! (currently unused — every supported stage has a default).
    std::string defaultExt;

    //! True when the driver can actually emit this stage's IR
    //! today.  False entries are listed by `--print-stages` as
    //! "(unsupported)" and rejected by `--to=` with a clear
    //! diagnostic.  Flipping false→true requires both an emitter
    //! in `compile.cpp` (or `dump.cpp`) and any necessary capture
    //! plumbing into `IRSinks` populated by `SeqcDriver`.
    bool supported;

    //! Short human-readable description for `--print-stages` and
    //! `--help`.
    std::string description;
};

//! Canonical stage table, ordered by pipeline position.  Read-only;
//! callers should not mutate the returned vector.
//!
//! \note "link" is always last and always supported — it represents
//! the default `-c` (compile-to-ELF) behaviour and is the implicit
//! `--to=` value when none is given.
std::vector<StageInfo> const& knownStages();

//! Look up a stage by name.  Returns `nullptr` for unknown names.
//! Callers typically reject unknown stages at CLI parse time via
//! `CLI::IsMember(stageNames())`, so a `nullptr` return here
//! indicates a driver bug rather than user error.
StageInfo const* findStage(std::string const& name);

//! Just the names, in canonical order — for `CLI::IsMember(...)`.
std::vector<std::string> stageNames();

//! Pretty-print the stage table to `os`, one line per stage with
//! the supported flag and description.  Used by `--print-stages`.
void printStageTable(std::ostream& os);

}  // namespace seqcc
