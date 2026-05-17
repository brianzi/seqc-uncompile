// =============================================================================
// seqcc — stage table (T5).
// =============================================================================
#include "stage.hpp"

#include <algorithm>
#include <iomanip>
#include <ostream>

namespace seqcc {

namespace {

// Canonical pipeline-stage table.  Order mirrors `Compiler::compile()`
// (DESIGN.md §3): parse → astgen → lower → opt-pre → prefetch →
// opt-post → assemble → link.  Only "lower", "asm-pre", "asm", and
// "link" are reachable from the current driver release:
//   - "lower" (alias `-E`) pulls the lowered AST out of
//     `CompileSeqcIntrospection::loweredAst` (T3c plumbing) and
//     emits it as JSON via the existing `emitAstLoweredJson()`.
//   - "asm-pre" (T6.2) emits the pre-prefetch AsmList via
//     `AsmList::serialize()` — the binary's natural round-trip cut
//     point (after `stepOptPre`, before `stepPrefetch`).  This is
//     the input format consumed by `--from=asm`.
//   - "asm" (alias `-S`) extracts the `.asm` section from the
//     produced ELF via the existing `parseElfSections()` reader.
//   - "link" is the default: emit the compiled ELF as-is.
// Other names are listed so `--print-stages` shows a complete
// pipeline diagram; selecting one via `--to=` is rejected at parse
// time (per `supported = false`) with the same "not yet implemented"
// diagnostic the dump module already uses for stub kinds.
std::vector<StageInfo> const& stageTable() {
    static std::vector<StageInfo> const table = {
        {"parse",      "",                   false,
         "Lex + bison parse → expression tree."},
        {"astgen",     "",                   false,
         "Build SeqC AST from parsed expressions."},
        {"lower",      ".ast-lowered.json",  true,
         "Frontend lowering → Node IR.  Alias: -E."},
        {"opt-pre",    "",                   false,
         "Pre-waveform AsmOptimize passes."},
        {"prefetch",   "",                   false,
         "Waveform prefetch + WavetableIR build."},
        {"opt-post",   "",                   false,
         "Post-waveform AsmOptimize passes."},
        {"asm-pre",    ".seqasm",            true,
         "Pre-prefetch AsmList serialised via AsmList::serialize() "
         "(natural round-trip cut).  Input format for --from=asm."},
        {"asm",        ".asm",               true,
         "Assembled .asm text (from ELF .asm section).  Alias: -S."},
        {"assemble",   "",                   false,
         "AsmList → encoded opcode bytes."},
        {"link",       ".elf",               true,
         "Default: full ELF compile."},
    };
    return table;
}

}  // namespace

std::vector<StageInfo> const& knownStages() {
    return stageTable();
}

StageInfo const* findStage(std::string const& name) {
    auto const& t = stageTable();
    auto it = std::find_if(t.begin(), t.end(),
                           [&](StageInfo const& s) {
                               return s.name == name;
                           });
    return (it == t.end()) ? nullptr : &*it;
}

std::vector<std::string> stageNames() {
    auto const& t = stageTable();
    std::vector<std::string> out;
    out.reserve(t.size());
    for (auto const& s : t) out.push_back(s.name);
    return out;
}

void printStageTable(std::ostream& os) {
    auto const& t = stageTable();
    // Two columns: name (left-aligned 12), status (10), description.
    os << "Pipeline stages (in execution order):\n";
    for (auto const& s : t) {
        os << "  " << std::left << std::setw(12) << s.name
           << std::setw(15)
           << (s.supported ? "(supported)" : "(unsupported)")
           << s.description << '\n';
    }
    os << "\nSelect with --to=<stage>.  -S is sugar for --to=asm; "
          "-E is sugar for --to=lower.\n";
}

}  // namespace seqcc
