// =============================================================================
// seqcc — SeqC toolchain driver
//
// Sub-phase T3a (option-surface cleanup):
//   seqcc -march=<dev>
//         [--sequencer={auto|qa|sg}]
//         [--samplerate=<Hz>]
//         [--filename=<path>]
//         [--wave-path=<dir>] [--waveforms=<a;b;c>]
//         [--index=N]
//         [-mdevopt=<feat> ...]   (repeatable; T2's -mdevopts=<packed> also works)
//         [-mtune=<key>=<val> ...]  (escape hatch for kwargs without a flag)
//         [-o OUT]
//         INPUT.seqc
//
// Output is still byte-identical to a Python `compile_seqc(...)` call
// with the equivalent kwargs.  T3b will add `--dump=<ir>` flags for
// AST / asm / wavetable-IR / info-report dumps by switching to direct
// AWGCompiler use; T3a keeps the compileSeqc() entry point unchanged.
//
// Pipeline-stage flags (-S/-E/--from=/--to=, -O, -f), -x, and the
// seqas/seqdump personalities still arrive in T5..T10.  See
// tools/seqcc/DESIGN.md and TODO.md "Phase T".
// =============================================================================

#include "CLI11.hpp"

#include "compile.hpp"
#include "options.hpp"

#include <array>
#include <climits>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace seqcc {
// Defined in options.cpp; declared inline here to avoid widening the
// public header with a CLI-validation helper.
bool tuneKeyIsReserved(std::string const& k);
}  // namespace seqcc

namespace {

// Distinct from zhinst::LaboneVersion (which is only defined when the
// pybind11 module is built) so seqcc never pulls in pybind.  Bumped manually
// alongside major driver milestones.
//
// TODO(IF-293): unify with zhinst::LaboneVersion::fullVersionWithBuild once
// those globals are moved out of `#ifdef ZHINST_HAS_PYBIND11`.  See
// reconstructed/notes/incidental_findings.md "IF-293".
constexpr const char* kSeqccVersion = "0.4.0-T3c";

enum class Personality { Seqcc, Seqas, Seqdump };

Personality detectPersonality(std::string_view argv0) {
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

// Parse a single `KEY=VALUE` token into the tune vector.  Reserved
// keys (currently just `index`) are rejected with a hint pointing at
// the dedicated flag — closes IF-296's silent-no-op behaviour.
void appendTune(seqcc::Options& opts, std::string const& kv) {
    auto eq = kv.find('=');
    if (eq == std::string::npos || eq == 0) {
        throw CLI::ValidationError(
            "-mtune expects KEY=VALUE (got: \"" + kv + "\")");
    }
    std::string key = kv.substr(0, eq);
    if (seqcc::tuneKeyIsReserved(key)) {
        throw CLI::ValidationError(
            "-mtune=" + key + "= is reserved; use the dedicated flag instead "
            "(--index=N for index).  See IF-296.");
    }
    opts.tune.emplace_back(std::move(key), kv.substr(eq + 1));
}

// Rewrite gcc-style single-dash long flags (-march, -mtune, -mdevopt,
// -mdevopts) into the canonical double-dash form CLI11 expects.  See
// IF-294 for why this exists.
//
// TODO(IF-294): this hand-maintained allowlist will silently fail to
// catch new gcc-style flags added in T5+; consider switching to a
// parser that supports single-dash long names natively, or patching
// CLI11.
std::vector<std::string> rewriteGccLongFlags(int argc, char** argv) {
    static const std::array<std::string_view, 4> kRewrite{{
        "-march", "-mtune", "-mdevopt", "-mdevopts"
    }};
    std::vector<std::string> out;
    out.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        std::string_view a = argv[i];
        bool rewritten = false;
        for (auto prefix : kRewrite) {
            // Match exact "-march" or "-march=..." — but not e.g. "-marchive".
            // The trailing-char check covers both.
            if (a == prefix ||
                (a.size() > prefix.size()
                 && a.substr(0, prefix.size()) == prefix
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

// Split a legacy -mdevopts=PACKED string on newlines; append each
// non-empty line to opts.devopts.  Preserves T2's surface so existing
// scripts keep working while IF-297 is being unwound.
void appendDevoptsPacked(seqcc::Options& opts, std::string const& packed) {
    std::size_t start = 0;
    while (start <= packed.size()) {
        auto nl = packed.find('\n', start);
        std::string line = packed.substr(
            start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!line.empty()) opts.devopts.push_back(std::move(line));
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
}

}  // namespace

int main(int argc, char** argv) {
    Personality pers = detectPersonality(argc > 0 ? argv[0] : "seqcc");

    std::vector<std::string> rewritten = rewriteGccLongFlags(argc, argv);
    std::vector<char*> rewrittenArgv;
    rewrittenArgv.reserve(rewritten.size());
    for (auto& s : rewritten) rewrittenArgv.push_back(s.data());
    int rewrittenArgc = static_cast<int>(rewrittenArgv.size());
    char** rewrittenArgvPtr = rewrittenArgv.data();

    CLI::App app{
        "seqcc — SeqC toolchain driver.  T3a: cleaned-up option surface.",
        personalityName(pers)};

    bool showVersion = false;
    app.add_flag("--version", showVersion, "Print driver version and exit.");

    seqcc::Options opts;

    app.add_option("-o,--output", opts.output,
                   "Output ELF path (default: <input>.elf).")
        ->type_name("FILE");

    app.add_option("--march", opts.devtype,
                   "Target device type, e.g. HDAWG8, SHFQA4, SHFSG8.  "
                   "Also accepts the gcc-style single-dash form -march=...")
        ->type_name("DEVTYPE");

    app.add_option("--sequencer", opts.sequencer,
                   "Sequencer selector for SHF* devices: auto (default), "
                   "qa, sg.")
        ->type_name("KIND")
        ->check(CLI::IsMember({"auto", "qa", "sg"}));

    app.add_option("--samplerate", opts.samplerate,
                   "Device sample rate in Hz (HDAWG only).")
        ->type_name("HZ");

    app.add_option("--filename", opts.filename,
                   "Source-filename hint recorded in the compile report.  "
                   "Defaults to the basename of the input file.")
        ->type_name("PATH");

    app.add_option("--wave-path", opts.wavePath,
                   "Waveform CSV search directory (default: built-in "
                   "ZiData/awg/waves).")
        ->type_name("DIR");

    app.add_option("--waveforms", opts.waveforms,
                   "Semicolon-separated list of CSV waveform names to "
                   "preload from --wave-path.")
        ->type_name("A;B;C");

    app.add_option("--index", opts.awgIndex,
                   "AWG core index (zero-based; default 0).");

    // Repeatable -mdevopt=FEATURE.  Plural -mdevopts=PACKED kept as
    // back-compat alias (T2 surface).  See IF-297.
    //
    // CLI11 idiom for "repeatable, single-value, doesn't slurp the
    // trailing positional" (verified against CLI11 docs):
    //   add_option(vector) + expected(1) + take_all() + allow_extra_args(false)
    //
    //   - expected(1) constrains each occurrence to exactly one value
    //   - take_all() switches multi-option policy from Throw to TakeAll
    //     so the option may appear more than once
    //   - allow_extra_args(false) defeats CLI11's default-for-vectors
    //     greedy consumption of subsequent positionals
    //
    // The earlier `add_option_function<std::string>->expected(1, INT_MAX)`
    // pattern from T3a silently dropped all but the last occurrence;
    // logged as IF-300.
    app.add_option("--mdevopt", opts.devopts,
                   "Device feature flag; repeatable.  Example: "
                   "-mdevopt=MF -mdevopt=ME for HDAWG multi-frequency "
                   "+ multi-execution.")
        ->type_name("FEATURE")
        ->expected(1)
        ->take_all()
        ->allow_extra_args(false);

    std::vector<std::string> devoptsPacked;
    app.add_option("--mdevopts", devoptsPacked,
                   "[deprecated] Packed device options (newline-separated).  "
                   "Prefer repeatable -mdevopt=FEATURE.")
        ->type_name("STR")
        ->expected(1)
        ->take_all()
        ->allow_extra_args(false);

    std::vector<std::string> tuneRaw;
    app.add_option("--mtune", tuneRaw,
                   "Escape-hatch JSON kwarg: -mtune=KEY=VALUE.  Repeatable.  "
                   "Prefer the dedicated --sequencer / --samplerate / "
                   "--filename / --wave-path / --waveforms flags when "
                   "available.")
        ->type_name("KEY=VALUE")
        ->expected(1)
        ->take_all()
        ->allow_extra_args(false);

    std::vector<std::filesystem::path> inputs;
    app.add_option("inputs", inputs, "Input .seqc file (exactly one).")
        ->type_name("FILE");

    // --dump=KIND[:PATH], repeatable.  CLI11 idiom for "repeatable
    // single-value option that doesn't slurp positionals": bind a
    // vector target with `expected(1)` (forces exactly one value per
    // occurrence) and `allow_extra_args(false)` (defeats CLI11's
    // default for vector options to greedily consume trailing tokens
    // up to the next option-looking arg, which would swallow the
    // positional INPUT.seqc).  Each `--dump=X` then appends one
    // element to the vector.
    app.add_option("--dump", opts.dumpRequests,
                   "Emit an intermediate artifact alongside the ELF.  "
                   "KIND ∈ {compile-report, asm, waveforms, wavemem}.  "
                   "Optional :PATH overrides the auto-derived filename.  "
                   "Repeatable.  (ast-lowered and other IR kinds: TODO T3c+.)")
        ->type_name("KIND[:PATH]")
        ->expected(1)
        ->take_all()
        ->allow_extra_args(false);

    app.add_option("--dump-dir", opts.dumpDir,
                   "Directory for --dump outputs that have no explicit "
                   ":PATH (default: alongside the compile output).")
        ->type_name("DIR");

    app.add_option("--dump-format", opts.dumpFormat,
                   "Format hint for --dump outputs: auto (default), "
                   "json, text.  Currently advisory — each implemented "
                   "dump kind emits its native format regardless.")
        ->type_name("FMT")
        ->check(CLI::IsMember({"auto", "json", "text"}));

    CLI11_PARSE(app, rewrittenArgc, rewrittenArgvPtr);

    if (showVersion) {
        std::cout << "seqcc " << kSeqccVersion << '\n';
        return 0;
    }

    if (pers != Personality::Seqcc) {
        std::cerr << personalityName(pers)
                  << ": personality not yet implemented (Phase T3a); "
                     "use `seqcc` for now.\n";
        return 2;
    }

    // Fold devopt flag forms into opts.devopts.  The --mdevopt= form
    // already populated opts.devopts directly (vector target).  Then
    // splice in any packed --mdevopts= strings, newline-split per
    // the T2 surface contract.
    for (auto const& d : devoptsPacked) appendDevoptsPacked(opts, d);

    for (auto const& kv : tuneRaw) {
        try {
            appendTune(opts, kv);
        } catch (CLI::ValidationError const& e) {
            std::cerr << "seqcc: " << e.what() << '\n';
            return 2;
        }
    }

    if (inputs.size() > 1) {
        std::cerr << "seqcc: multiple inputs not supported (got "
                  << inputs.size() << ").\n";
        return 2;
    }
    if (!inputs.empty()) opts.input = inputs.front();

    if (opts.input.empty()) {
        std::cerr << "seqcc: no input file.  Run with --help for usage.\n";
        return 1;
    }

    // Auto-populate --filename hint from the input's basename if the user
    // didn't override it.  Mirrors the binding's behaviour for callers
    // that don't pass `filename=` explicitly (the compile-report info
    // section embeds *some* filename; an empty one is unusual).
    if (opts.filename.empty()) {
        opts.filename = opts.input.filename().string();
    }

    return seqcc::runCompile(opts);
}
