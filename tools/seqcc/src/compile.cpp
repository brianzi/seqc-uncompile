// =============================================================================
// seqcc — compile driver implementation (T2)
// =============================================================================

#include "compile.hpp"

#include "dump.hpp"
#include "ir_sinks.hpp"
#include "stage.hpp"

#ifdef SEQCC_USE_OWNED_DRIVER
#  include "driver.hpp"
#endif

#include "zhinst/codegen/compile_seqc.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace seqcc {

namespace {

std::string slurpFile(std::filesystem::path const& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open input file: " + path.string() +
                                 " (" + std::strerror(errno) + ")");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeFile(std::filesystem::path const& path, std::string const& data) {
    // Create parent directories if needed — gcc creates output dirs
    // only when -o explicitly names one; we match that by requiring
    // the parent to exist already (so a typo in -o doesn't silently
    // create a stray tree).
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("cannot open output file: " + path.string() +
                                 " (" + std::strerror(errno) + ")");
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out) {
        throw std::runtime_error("write failed: " + path.string());
    }
}

// T5: write the primary stage artifact to either `path` or, when the
// caller passed the gcc-style `-` sentinel, to stdout.  Stdout is
// switched to binary mode by reopening with std::ios::binary on the
// streambuf level isn't portable; we rely on POSIX semantics where
// stdout is always byte-transparent.  Callers should avoid `-o -`
// for `--to=link` (binary ELF in a terminal is unhelpful) but we
// don't reject it — pipelines like `seqcc -E foo.seqc -o - | jq .`
// are the motivating use case.
void writePrimary(std::filesystem::path const& path, std::string const& data) {
    if (path == "-") {
        std::cout.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!std::cout) {
            throw std::runtime_error("write to stdout failed");
        }
        return;
    }
    writeFile(path, data);
}

// T5: pick the default `-o` value when none was supplied.  Routes on
// the stage's own default extension from `knownStages()` so the
// mapping lives in one place.
std::filesystem::path defaultOutputFor(std::filesystem::path const& input,
                                       std::string const& stage) {
    StageInfo const* info = findStage(stage);
    // Caller already validated `stage` at CLI parse time.
    std::filesystem::path out = input;
    if (info && !info->defaultExt.empty()) {
        // Strip the input's extension before appending the new one so
        // `foo.seqc` becomes `foo.elf` / `foo.asm` / `foo.ast-lowered.json`,
        // not `foo.seqc.elf`.
        out.replace_extension();
        out += info->defaultExt;
    } else {
        out.replace_extension(".out");
    }
    return out;
}

}  // namespace

int runCompile(Options const& opts) {
    try {
        if (opts.devtype.empty()) {
            std::cerr << "seqcc: -march=<devtype> is required (e.g. "
                         "-march=HDAWG8)\n";
            return 2;
        }
        if (opts.input.empty()) {
            std::cerr << "seqcc: no input file given\n";
            return 2;
        }

        std::string source = slurpFile(opts.input);
        std::string jsonConfig = buildJsonConfig(opts);
        std::string devoptsStr = buildDevoptsString(opts);

        // T5: validate the requested stage.  CLI-level
        // CLI::IsMember(stageNames()) already rejects unknown names;
        // this second guard rejects *known-but-unsupported* names
        // (see stage.cpp::knownStages()) with a clear diagnostic.
        StageInfo const* stage = findStage(opts.toStage);
        if (stage == nullptr) {
            std::cerr << "seqcc: unknown --to stage: " << opts.toStage << '\n';
            return 2;
        }
        if (!stage->supported) {
            std::cerr << "seqcc: --to=" << opts.toStage
                      << " is a recognised pipeline stage but is not "
                         "implemented in this driver release "
                         "(see TODO.md Phase T / `seqcc --print-stages`).\n";
            return 2;
        }

        // Parse dump specs up-front so we can decide which compile
        // entry point to use.  Errors here are user-facing and must
        // fail the run (unlike emission errors, which are logged but
        // non-fatal once an ELF exists).
        std::vector<DumpSpec> dumpSpecs;
        dumpSpecs.reserve(opts.dumpRequests.size());
        for (auto const& tok : opts.dumpRequests) {
            dumpSpecs.push_back(parseDumpSpec(tok));
        }
        // The lowered-AST primary output reuses the same sink as
        // `--dump=ast-lowered`; promote `wantSinks` accordingly so a
        // bare `-E` (no --dump=) still captures it.
        bool wantSinks = needsIRSinks(dumpSpecs) || opts.toStage == "lower";

        // Drive compileSeqc() (no IR introspection) or its T3c
        // variant (compileSeqcWithIR — captures lowered AST and any
        // future IR handles).  The ELF half is byte-identical
        // between the two entry points; the variant exists solely
        // to make IR survive the compiler's destructor scope.
        //
        // T5a.2: when `SEQCC_USE_OWNED_DRIVER` is defined at build
        // time, route through `SeqcDriver::compile()` instead — the
        // driver owns the outer flow directly and produces an
        // equivalent `(elf, infoJson, sinks)` triple.  Validation:
        // `tests/tools/test_seqcc_diff.py` (full ELF byte-equal vs
        // the pybind path).  Default OFF through T5a.2; T5a.3 flips
        // the gate ON after the A/B fixture clears.
        //
        // `elfStorage` / `infoStorage` back the `string_view`s used
        // by downstream dump emission; keep them alive in the
        // enclosing scope.
        std::string elfStorage;
        std::string infoStorage;
        IRSinks sinks;
#ifdef SEQCC_USE_OWNED_DRIVER
        {
            SeqcDriver driver(opts);
            DriverResult dr = driver.compile(source, jsonConfig, devoptsStr);
            // T5b.5: an empty ELF is legal when `--to=<stage>` short-
            // circuited the back end before `writeToStream`.  The
            // driver guarantees ELF is non-empty for `--to=link` (the
            // default), so we only treat empty-ELF as an error in
            // that case.
            if (dr.elf.empty() && opts.toStage == "link") {
                std::cerr << "seqcc: driver returned empty ELF payload\n";
                return 1;
            }
            elfStorage  = std::move(dr.elf);
            infoStorage = std::move(dr.infoJson);
            sinks       = std::move(dr.sinks);
        }
#else
        {
            std::string packed;
            if (wantSinks) {
                auto pair = zhinst::compileSeqcWithIR(
                    jsonConfig, source, opts.devtype, opts.awgIndex, devoptsStr);
                packed = std::move(pair.first);
                // T5b.5: IRSinks is now a wrapper that adds
                // `assemblerText` to the recon-side
                // `CompileSeqcIntrospection`.  Assign into the base
                // sub-object explicitly to avoid object slicing.  The
                // legacy path leaves `assemblerText` empty (its
                // consumer routes through the produced ELF instead).
                static_cast<zhinst::CompileSeqcIntrospection&>(sinks) =
                    std::move(pair.second);
            } else {
                packed = zhinst::compileSeqc(
                    jsonConfig, source, opts.devtype, opts.awgIndex, devoptsStr);
            }

            // Packed layout matches pybind_seqc.cpp::pyCompileSeqc:
            // info-JSON, a single NUL, then the ELF bytes.  An absent
            // NUL means the compiler emitted only an info report
            // (no ELF) — currently treated as a hard error.
            auto sep = packed.find('\0');
            if (sep == std::string::npos) {
                std::cerr << "seqcc: compileSeqc returned no ELF payload\n"
                          << "info:  " << packed << '\n';
                return 1;
            }
            infoStorage.assign(packed, 0, sep);
            elfStorage.assign(packed, sep + 1, std::string::npos);
            if (elfStorage.empty()) {
                std::cerr << "seqcc: compileSeqc returned empty ELF payload\n";
                return 1;
            }
        }
#endif
        // Suppress unused-warning for `wantSinks` in the owned-driver
        // build (the driver always populates sinks regardless).
        (void)wantSinks;

        // Re-bind the names downstream code reads.  These are views /
        // refs into the backing storage above and remain valid for
        // the rest of the function.
        std::string const& elf = elfStorage;
        std::string_view infoJson(infoStorage);

        // T5: route the primary `-o` output by stage.  `link` keeps
        // the T2 behaviour (write ELF); other stages render their
        // own artifact via `renderStagePrimary()`.  The ELF bytes
        // are still produced (the compile pipeline runs end-to-end)
        // — see IF-304 for the "logical vs literal stop" caveat.
        std::filesystem::path outPath = opts.output;
        if (outPath.empty()) {
            outPath = defaultOutputFor(opts.input, opts.toStage);
        }
        std::string primary = renderStagePrimary(opts.toStage, elf, sinks);
        if (primary.empty() && opts.toStage != "link") {
            // `link` can never be empty (we already rejected empty
            // ELF above).  An empty `lower` or `asm` payload usually
            // means a feature gap (lower: AST sink unpopulated, see
            // IF-302; asm: device-config disabled .asm emission).
            std::cerr << "seqcc: --to=" << opts.toStage
                      << " produced an empty payload "
                         "(see TODO.md Phase T notes).\n";
            return 1;
        }
        writePrimary(outPath, primary);

        // Emit any --dump=KIND[:PATH] artifacts.  Dump failures are
        // diagnosed but do not fail the compile (the ELF is already
        // on disk) — mirrors gcc, where `-fdump-*` write failures
        // print a warning rather than aborting -c.
        //
        // T5: dumps derive their default filenames from the *ELF*
        // basename, not the (possibly non-ELF) primary `-o` path —
        // otherwise `seqcc -E foo.seqc -o - --dump=asm` has nowhere
        // sensible to land its asm dump.  We synthesise an ELF-style
        // basename from the input.
        if (!dumpSpecs.empty()) {
            try {
                DumpFormat fmt = DumpFormat::Auto;
                if (opts.dumpFormat == "json")      fmt = DumpFormat::Json;
                else if (opts.dumpFormat == "text") fmt = DumpFormat::Text;
                std::filesystem::path dumpBase =
                    (opts.toStage == "link" && outPath != "-")
                        ? outPath
                        : defaultOutputFor(opts.input, "link");
                emitDumps(dumpSpecs, opts.dumpDir, dumpBase, elf, infoJson,
                          sinks, fmt);
            } catch (std::exception const& e) {
                std::cerr << "seqcc: dump emission failed: "
                          << e.what() << '\n';
                // Non-fatal: ELF is already written.
            }
        }
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "seqcc: " << e.what() << '\n';
        return 1;
    }
}

}  // namespace seqcc
