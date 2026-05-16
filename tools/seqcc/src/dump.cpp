// =============================================================================
// seqcc — dump emitters.  See dump.hpp for scope.
// =============================================================================

#include "dump.hpp"

#include "elf_reader.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace seqcc {

namespace {

struct KindInfo {
    char const* extension;  //!< file extension without the dot
    char const* elfSection; //!< ELF section to extract; nullptr ⇒ not ELF-sourced
    bool fromInfoJson;      //!< true ⇒ payload is the compileSeqc info JSON
    bool implemented;       //!< false ⇒ T3b stub (e.g. ast-lowered)
};

KindInfo const& kindInfo(std::string const& kind) {
    static std::unordered_map<std::string, KindInfo> const table = {
        // T3b: ELF-section extraction.  `.asm` and `.waveforms` are
        // emitted uncompressed by compileSeqc() since the config's
        // compressSource flag is zero-initialised — verified via
        // reconstructed/notes/elf_format.md.
        {"asm",            {"asm",  ".asm",       false, true}},
        {"waveforms",      {"json", ".waveforms", false, true}},
        {"wavemem",        {"json", ".wavemem",   false, true}},
        // T3b: info-JSON channel (compileSeqc packed return prefix).
        {"compile-report", {"json", nullptr,      true,  true}},
        // T3c: needs recon accessor or driver-side AWGCompiler bypass.
        {"ast-lowered",    {"json", nullptr,      false, false}},
        // Reserved for future sub-phases (T4: text-only IRs).
        {"ast-raw",        {"txt",  nullptr,      false, false}},
        {"ast-seqc",       {"seqc", nullptr,      false, false}},
        {"prefetch",       {"txt",  nullptr,      false, false}},
        {"cache",          {"txt",  nullptr,      false, false}},
        {"resources",      {"txt",  nullptr,      false, false}},
    };
    auto it = table.find(kind);
    if (it == table.end()) {
        throw std::invalid_argument(
            "unknown --dump kind: \"" + kind + "\" (try seqcc --help)");
    }
    return it->second;
}

void writeFileSv(std::filesystem::path const& path, std::string_view data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("cannot open dump output: " + path.string());
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out) {
        throw std::runtime_error("write failed: " + path.string());
    }
}

}  // namespace

std::vector<std::string> knownDumpKinds() {
    // Stable order mirrors the pipeline: source → AST → asm → wavetable
    // → report.  `all` (T6) will iterate this list.
    return {"ast-lowered", "asm", "waveforms", "wavemem", "compile-report"};
}

DumpSpec parseDumpSpec(std::string const& token) {
    DumpSpec spec;
    auto colon = token.find(':');
    spec.kind = (colon == std::string::npos) ? token : token.substr(0, colon);
    // Validate kind (throws on unknown).
    (void)kindInfo(spec.kind);
    if (colon != std::string::npos) {
        spec.explicitPath = token.substr(colon + 1);
    }
    return spec;
}

std::filesystem::path resolveDumpPath(DumpSpec const& spec,
                                      std::filesystem::path const& dumpDir,
                                      std::filesystem::path const& outputElf) {
    if (!spec.explicitPath.empty()) return spec.explicitPath;

    std::filesystem::path stem = outputElf.stem();
    if (stem.empty()) stem = "out";
    KindInfo const& info = kindInfo(spec.kind);
    std::filesystem::path filename = stem;
    filename += ".";
    filename += spec.kind;
    filename += ".";
    filename += info.extension;

    if (!dumpDir.empty()) return dumpDir / filename;
    std::filesystem::path parent = outputElf.parent_path();
    if (parent.empty()) parent = ".";
    return parent / filename;
}

void emitDumps(std::vector<DumpSpec> const& specs,
               std::filesystem::path const& dumpDir,
               std::filesystem::path const& outputElf,
               std::string_view elfBytes,
               std::string_view infoJson,
               DumpFormat /*formatHint*/) {
    if (specs.empty()) return;

    // Parse the ELF once if any spec needs a section out of it.
    bool needElf = std::any_of(specs.begin(), specs.end(),
                               [](DumpSpec const& s) {
                                   return kindInfo(s.kind).elfSection != nullptr;
                               });
    ElfSections sections;
    if (needElf) {
        sections = parseElfSections(elfBytes);
        if (!sections.valid) {
            throw std::runtime_error(
                "seqcc: produced ELF could not be parsed for --dump=");
        }
    }

    for (DumpSpec const& spec : specs) {
        KindInfo const& info = kindInfo(spec.kind);
        if (!info.implemented) {
            std::cerr << "seqcc: --dump=" << spec.kind
                      << " is not implemented in this driver release "
                         "(see TODO.md Phase T, T3c).\n";
            continue;
        }
        std::filesystem::path path = resolveDumpPath(spec, dumpDir, outputElf);

        std::string_view payload;
        if (info.fromInfoJson) {
            payload = infoJson;
        } else if (info.elfSection != nullptr) {
            payload = sections.get(info.elfSection, elfBytes);
            if (payload.empty()) {
                std::cerr << "seqcc: ELF has no \"" << info.elfSection
                          << "\" section; --dump=" << spec.kind
                          << " skipped.\n";
                continue;
            }
        }
        writeFileSv(path, payload);
    }
}

}  // namespace seqcc
