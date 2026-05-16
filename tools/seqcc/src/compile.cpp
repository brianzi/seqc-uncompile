// =============================================================================
// seqcc — compile driver implementation (T2)
// =============================================================================

#include "compile.hpp"

#include "dump.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// Forward-declared from reconstructed/src/codegen/compile_seqc.cpp.
// No public header exposes this symbol today (the Python binding uses
// the same local forward decl in pybind_seqc.cpp); we mirror that
// pattern rather than adding a header just for seqcc.
namespace zhinst {
std::string compileSeqc(std::string const& jsonConfig,
                        std::string sourceCode,
                        std::string deviceId,
                        unsigned long awgIndex,
                        std::string const& options);
}

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

        std::string packed = zhinst::compileSeqc(
            jsonConfig, source, opts.devtype, opts.awgIndex, devoptsStr);

        // Packed layout matches pybind_seqc.cpp::pyCompileSeqc: the
        // info-JSON, a single NUL, then the ELF bytes.  An absent NUL
        // means the compiler emitted only an info report (no ELF) —
        // currently treated as a hard error for seqcc.
        auto sep = packed.find('\0');
        if (sep == std::string::npos) {
            std::cerr << "seqcc: compileSeqc returned no ELF payload\n"
                      << "info:  " << packed << '\n';
            return 1;
        }
        std::string elf = packed.substr(sep + 1);
        std::string_view infoJson(packed.data(), sep);
        if (elf.empty()) {
            std::cerr << "seqcc: compileSeqc returned empty ELF payload\n";
            return 1;
        }

        std::filesystem::path outPath = opts.output;
        if (outPath.empty()) {
            outPath = opts.input;
            outPath.replace_extension(".elf");
        }
        writeFile(outPath, elf);

        // Emit any --dump=KIND[:PATH] artifacts.  Dump failures are
        // diagnosed but do not fail the compile (the ELF is already
        // on disk) — mirrors gcc, where `-fdump-*` write failures
        // print a warning rather than aborting -c.
        if (!opts.dumpRequests.empty()) {
            try {
                std::vector<DumpSpec> specs;
                specs.reserve(opts.dumpRequests.size());
                for (auto const& tok : opts.dumpRequests) {
                    specs.push_back(parseDumpSpec(tok));
                }
                DumpFormat fmt = DumpFormat::Auto;
                if (opts.dumpFormat == "json")      fmt = DumpFormat::Json;
                else if (opts.dumpFormat == "text") fmt = DumpFormat::Text;
                emitDumps(specs, opts.dumpDir, outPath, elf, infoJson, fmt);
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
