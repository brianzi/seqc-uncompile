// =============================================================================
// seqcc — SeqC toolchain driver
//
// Sub-phase T1 (scaffolding): this file currently handles only --help and
// --version.  Pipeline-stage flags, IR dumps, and the differential test
// hook arrive in sub-phases T2..T10.  See tools/seqcc/DESIGN.md and
// TODO.md "Phase T".
//
// argv[0] dispatch (T7+):
//   seqcc    — full driver
//   seqas    — alias for `seqcc -x asm`
//   seqdump  — ELF inspector
// =============================================================================

#include "CLI11.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

// Distinct from zhinst::LaboneVersion (which is only defined when the
// pybind11 module is built) so seqcc never pulls in pybind.  Bumped manually
// alongside major driver milestones.
//
// TODO(IF-293): unify with zhinst::LaboneVersion::fullVersionWithBuild once
// those globals are moved out of `#ifdef ZHINST_HAS_PYBIND11`.  See
// reconstructed/notes/incidental_findings.md "IF-293".
constexpr const char* kSeqccVersion = "0.1.0-T1";

enum class Personality { Seqcc, Seqas, Seqdump };

Personality detectPersonality(std::string_view argv0) {
    // Strip any path component.
    auto slash = argv0.find_last_of("/\\");
    std::string_view name = (slash == std::string_view::npos)
                                ? argv0
                                : argv0.substr(slash + 1);
    if (name == "seqas")   return Personality::Seqas;
    if (name == "seqdump") return Personality::Seqdump;
    return Personality::Seqcc;
}

const char* personalityName(Personality p) {
    switch (p) {
        case Personality::Seqas:   return "seqas";
        case Personality::Seqdump: return "seqdump";
        case Personality::Seqcc:   return "seqcc";
    }
    return "seqcc";
}

}  // namespace

int main(int argc, char** argv) {
    Personality pers = detectPersonality(argc > 0 ? argv[0] : "seqcc");

    CLI::App app{
        "seqcc — SeqC toolchain driver (T1 scaffolding; pipeline flags "
        "arrive in T2+)",
        personalityName(pers)};

    // CLI11 provides its own --help; we add --version explicitly so it
    // works before any sub-phase wires up real options.
    bool showVersion = false;
    app.add_flag("--version", showVersion,
                 "Print driver version and exit.");

    // Accept (and currently ignore) the canonical input-file argv so that
    // smoke tests written against the final flag surface continue to
    // parse during T1.  Real handling lands in T2.
    std::vector<std::filesystem::path> inputs;
    app.add_option("inputs", inputs, "Input file(s) (unused in T1).")
        ->type_name("FILE");

    CLI11_PARSE(app, argc, argv);

    if (showVersion) {
        std::cout << "seqcc " << kSeqccVersion << '\n';
        return 0;
    }

    // No real work yet; future sub-phases dispatch here on `pers`.
    if (!inputs.empty()) {
        std::cerr << personalityName(pers)
                  << ": pipeline not yet implemented (Phase T1 scaffolding); "
                  << "see tools/seqcc/DESIGN.md\n";
        return 2;
    }

    std::cerr << personalityName(pers)
              << ": no input.  Run with --help for usage.\n";
    return 1;
}
