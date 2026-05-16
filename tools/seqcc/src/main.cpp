// =============================================================================
// seqcc — SeqC toolchain driver
//
// Sub-phase T2 (default compile path):
//   seqcc -march=<dev> [-mdevopts=<opts>] [-mtune=k=v ...]
//         [--index=N] [-o OUT] INPUT.seqc
//
// Drives the existing zhinst::compileSeqc() free function with a JSON
// config built from -mtune flags.  Output is byte-identical to a
// Python `compile_seqc(...)` call with the equivalent kwargs.
//
// Pipeline-stage flags (-S/-E/--from=/--to=, -O, -f), -x, --dump=,
// --info=, and the seqas/seqdump personalities arrive in T3..T10.
// See tools/seqcc/DESIGN.md and TODO.md "Phase T".
// =============================================================================

#include "CLI11.hpp"

#include "compile.hpp"
#include "options.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

// Distinct from zhinst::LaboneVersion (which is only defined when the
// pybind11 module is built) so seqcc never pulls in pybind.  Bumped manually
// alongside major driver milestones.
//
// TODO(IF-293): unify with zhinst::LaboneVersion::fullVersionWithBuild once
// those globals are moved out of `#ifdef ZHINST_HAS_PYBIND11`.  See
// reconstructed/notes/incidental_findings.md "IF-293".
constexpr const char* kSeqccVersion = "0.2.0-T2";

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

// Parse a single `KEY=VALUE` token into the tune vector.  CLI11 calls
// this once per occurrence of -mtune via a custom validator.  Empty
// key or missing '=' is a parse error.
void appendTune(seqcc::Options& opts, std::string const& kv) {
    auto eq = kv.find('=');
    if (eq == std::string::npos || eq == 0) {
        throw CLI::ValidationError(
            "-mtune expects KEY=VALUE (got: \"" + kv + "\")");
    }
    opts.tune.emplace_back(kv.substr(0, eq), kv.substr(eq + 1));
}

// Rewrite gcc-style single-dash long flags (-march, -mtune, -mdevopts)
// into the canonical double-dash form CLI11 expects.  We do this in a
// preprocessing pass over argv rather than registering both forms with
// CLI11 because CLI11 rejects single-dash long names outright
// ("Long names strings require 2 dashes").
//
// Returns a vector<string> owning the rewritten tokens; pointers into
// it are then collected into a parallel argv-style vector for
// CLI11_PARSE.  We deliberately do NOT rewrite `-o` or `-h` (those are
// legitimate single-char options) — only the gcc -m* family.
std::vector<std::string> rewriteGccLongFlags(int argc, char** argv) {
    static const std::array<std::string_view, 3> kRewrite{{
        "-march", "-mtune", "-mdevopts"
    }};
    std::vector<std::string> out;
    out.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        std::string_view a = argv[i];
        bool rewritten = false;
        for (auto prefix : kRewrite) {
            if (a == prefix ||
                (a.size() > prefix.size() && a.substr(0, prefix.size()) == prefix
                 && a[prefix.size()] == '=')) {
                out.emplace_back("-" + std::string(a));
                rewritten = true;
                break;
            }
        }
        if (!rewritten) out.emplace_back(a);
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    Personality pers = detectPersonality(argc > 0 ? argv[0] : "seqcc");

    // gcc-style -march/-mtune/-mdevopts use a single dash but multi-char
    // names.  CLI11 rejects those, so rewrite to --march/etc. first.
    std::vector<std::string> rewritten = rewriteGccLongFlags(argc, argv);
    std::vector<char*> rewrittenArgv;
    rewrittenArgv.reserve(rewritten.size());
    for (auto& s : rewritten) rewrittenArgv.push_back(s.data());
    int rewrittenArgc = static_cast<int>(rewrittenArgv.size());
    char** rewrittenArgvPtr = rewrittenArgv.data();

    CLI::App app{
        "seqcc — SeqC toolchain driver.  T2: default `.seqc → ELF` path.",
        personalityName(pers)};

    bool showVersion = false;
    app.add_flag("--version", showVersion,
                 "Print driver version and exit.");

    seqcc::Options opts;

    app.add_option("-o,--output", opts.output,
                   "Output ELF path (default: <input>.elf).")
        ->type_name("FILE");

    app.add_option("--march", opts.devtype,
                   "Target device type, e.g. HDAWG8, SHFQA4, SHFSG8.  "
                   "Also accepts the gcc-style single-dash form -march=...")
        ->type_name("DEVTYPE");

    app.add_option("--mdevopts", opts.devopts,
                   "Device options string (case-insensitive, "
                   "newline-separated).  Single-dash -mdevopts=... also "
                   "accepted.")
        ->type_name("STR");

    // -mtune=KEY=VALUE — repeatable.  We capture each occurrence as a
    // raw string and parse it in `appendTune()` so we control the
    // KEY=VALUE shape (CLI11's built-in pair-of-strings option would
    // require a different syntax).
    std::vector<std::string> tuneRaw;
    app.add_option("--mtune", tuneRaw,
                   "JSON config key/value: -mtune=KEY=VALUE.  "
                   "Recognised keys: sequencer, samplerate, filename, "
                   "wavepath, waveforms, index.  Repeatable.  Single-dash "
                   "-mtune=... also accepted.")
        ->type_name("KEY=VALUE");

    app.add_option("--index", opts.awgIndex,
                   "AWG core index (zero-based; default 0).  "
                   "Equivalent to -mtune=index=N.");

    std::vector<std::filesystem::path> inputs;
    app.add_option("inputs", inputs, "Input .seqc file (exactly one).")
        ->type_name("FILE");

    CLI11_PARSE(app, rewrittenArgc, rewrittenArgvPtr);

    if (inputs.size() > 1) {
        std::cerr << "seqcc: multiple inputs not supported (got "
                  << inputs.size() << ").\n";
        return 2;
    }
    if (!inputs.empty()) opts.input = inputs.front();

    if (showVersion) {
        std::cout << "seqcc " << kSeqccVersion << '\n';
        return 0;
    }

    // T2 only handles the default seqcc personality.  seqas/seqdump
    // dispatch lands in T7.
    if (pers != Personality::Seqcc) {
        std::cerr << personalityName(pers)
                  << ": personality not yet implemented (Phase T2); "
                     "use `seqcc` for now.\n";
        return 2;
    }

    for (auto const& kv : tuneRaw) {
        try {
            appendTune(opts, kv);
        } catch (CLI::ValidationError const& e) {
            std::cerr << "seqcc: " << e.what() << '\n';
            return 2;
        }
    }

    if (opts.input.empty()) {
        std::cerr << "seqcc: no input file.  Run with --help for usage.\n";
        return 1;
    }

    return seqcc::runCompile(opts);
}
