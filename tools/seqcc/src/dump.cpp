// =============================================================================
// seqcc — dump emitters.  See dump.hpp for scope.
// =============================================================================

#include "dump.hpp"

#include "elf_reader.hpp"

#include "zhinst/ast/node.hpp"
#include "zhinst/waveform/wavetable_front.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace seqcc {

namespace {

struct KindInfo {
    char const* extension;  //!< file extension without the dot
    char const* elfSection; //!< ELF section to extract; nullptr ⇒ not ELF-sourced
    bool fromInfoJson;      //!< true ⇒ payload is the compileSeqc info JSON
    bool fromIRSinks;       //!< true ⇒ payload sourced from `IRSinks` (T3c+)
    bool implemented;       //!< false ⇒ stub kind held for future sub-phases
};

KindInfo const& kindInfo(std::string const& kind) {
    static std::unordered_map<std::string, KindInfo> const table = {
        // T3b: ELF-section extraction.  `.asm` and `.waveforms` are
        // emitted uncompressed by compileSeqc() since the config's
        // compressSource flag is zero-initialised — verified via
        // reconstructed/notes/elf_format.md.
        {"asm",            {"asm",  ".asm",       false, false, true}},
        {"waveforms",      {"json", ".waveforms", false, false, true}},
        {"wavemem",        {"json", ".wavemem",   false, false, true}},
        // T3b: info-JSON channel (compileSeqc packed return prefix).
        {"compile-report", {"json", nullptr,      true,  false, true}},
        // T3c: sourced from compileSeqcWithIR()'s introspection sink.
        {"ast-lowered",    {"json", nullptr,      false, true,  true}},
        // T4a: sourced from the introspection sink's `wavetable`
        // field; rendered via `WavetableFront::toString()` (text
        // dump, one block per registered waveform).  JSON variant
        // is deferred — `WavetableFront::toJson()` doesn't exist
        // publicly; only the private `WavetableManager<>::toJson()`
        // does, and reaching it would require another friend grant
        // outside our single-friend principle on `AWGCompiler`.
        {"wavetable",      {"txt",  nullptr,      false, true,  true}},
        // Reserved for future sub-phases (T4: text-only IRs).
        {"ast-raw",        {"txt",  nullptr,      false, false, false}},
        {"ast-seqc",       {"seqc", nullptr,      false, false, false}},
        {"prefetch",       {"txt",  nullptr,      false, false, false}},
        {"cache",          {"txt",  nullptr,      false, false, false}},
        {"resources",      {"txt",  nullptr,      false, false, false}},
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

//! Build an identity `asmId → asmId` map by walking every node
//! reachable from `root` along `next`, `loop`, and `branches`.
//! Used to feed `Node::toJson(idMap)` when seqcc has no AsmList in
//! hand — the proper map would come from
//! `AsmList::serialize()`'s pass-1 (mapping `sequenceId` to a
//! sequential index for asm-line cross-references).  An identity
//! map produces JSON whose `asmId` cross-references point at the
//! original ids rather than re-densified ones; that's still a
//! useful debugging artifact, just not byte-identical to the
//! pybind path.
void collectAsmIdsIdentity(std::shared_ptr<zhinst::Node const> const& node,
                           std::unordered_set<zhinst::Node const*>& seen,
                           std::unordered_map<int,int>& idMap) {
    if (!node) return;
    if (!seen.insert(node.get()).second) return;
    idMap[node->asmId] = node->asmId;
    collectAsmIdsIdentity(node->next, seen, idMap);
    collectAsmIdsIdentity(node->loop, seen, idMap);
    for (auto const& b : node->branches) {
        collectAsmIdsIdentity(b, seen, idMap);
    }
}

//! Serialise a lowered-AST root to JSON (pretty-printed-ish via
//! boost::json's default formatter).  Returns an empty string when
//! the sink doesn't carry an AST.
//!
//! \note The `idMap` passed to `Node::toJson()` would normally come
//! from `AsmList::serialize()` pass-1 (mapping `sequenceId` to a
//! dense sequential index).  Seqcc doesn't currently capture the
//! AsmList into the introspection sink, so we feed an identity map
//! built by walking the AST.  The resulting JSON's `asmId` cross-
//! references therefore point at the original ids rather than the
//! densified ones; structurally identical, ids just differ.  Full
//! parity requires capturing the AsmList alongside the AST (queued
//! in TODO.md Phase T).
std::string emitAstLoweredJson(IRSinks const& sinks) {
    if (!sinks.loweredAst) {
        return {};
    }
    std::unordered_map<int,int> idMap;
    std::unordered_set<zhinst::Node const*> seen;
    collectAsmIdsIdentity(sinks.loweredAst, seen, idMap);
    try {
        boost::json::value jv = sinks.loweredAst->toJson(idMap);
        return boost::json::serialize(jv);
    } catch (std::out_of_range const&) {
        // Defensive: an asmId reachable through a path we don't
        // traverse (e.g. parent weak_ptr) could still trip the map
        // lookup.  Surface a diagnostic rather than crashing.
        return {};
    }
}

//! Serialise the front-end wavetable as text via
//! `WavetableFront::toString()` — one block per registered
//! waveform.  Returns an empty string when the sink doesn't carry
//! a wavetable (compile failed before one was built) or when the
//! wavetable has no waveforms (a valid outcome for sequences that
//! play none, distinguished from "sink empty" only by inspecting
//! `sink.wavetable` itself; the dump pipeline treats both as
//! "skip with diagnostic", which is fine for a debug artifact).
//!
//! JSON variant deferred — see `kindInfo` table comment.
std::string emitWavetableText(IRSinks const& sinks) {
    if (!sinks.wavetable) {
        return {};
    }
    return sinks.wavetable->toString();
}

}  // namespace

std::vector<std::string> knownDumpKinds() {
    // Stable order mirrors the pipeline: source → AST → wavetable
    // → asm → report.  `all` (T6) will iterate this list.
    return {"ast-lowered", "wavetable", "asm", "waveforms", "wavemem",
            "compile-report"};
}

bool needsIRSinks(std::vector<DumpSpec> const& specs) {
    return std::any_of(specs.begin(), specs.end(),
                       [](DumpSpec const& s) {
                           return kindInfo(s.kind).fromIRSinks;
                       });
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
               IRSinks const& sinks,
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

    // IR-sink-sourced payloads are serialised lazily on first need
    // so a request for `ast-lowered` doesn't pay the JSON cost when
    // the sink is empty (e.g. a compile that failed mid-pipeline).
    std::string astLoweredCache;
    bool astLoweredEvaluated = false;
    std::string wavetableCache;
    bool wavetableEvaluated = false;

    for (DumpSpec const& spec : specs) {
        KindInfo const& info = kindInfo(spec.kind);
        if (!info.implemented) {
            std::cerr << "seqcc: --dump=" << spec.kind
                      << " is not implemented in this driver release "
                         "(see TODO.md Phase T).\n";
            continue;
        }
        std::filesystem::path path = resolveDumpPath(spec, dumpDir, outputElf);

        std::string_view payload;
        if (info.fromIRSinks) {
            if (spec.kind == "ast-lowered") {
                if (!astLoweredEvaluated) {
                    astLoweredCache = emitAstLoweredJson(sinks);
                    astLoweredEvaluated = true;
                }
                if (astLoweredCache.empty()) {
                    std::cerr << "seqcc: --dump=ast-lowered: no AST "
                                 "available, or asm-id remap missing "
                                 "(see TODO.md Phase T); skipped.\n";
                    continue;
                }
                payload = astLoweredCache;
            } else if (spec.kind == "wavetable") {
                if (!wavetableEvaluated) {
                    wavetableCache = emitWavetableText(sinks);
                    wavetableEvaluated = true;
                }
                if (wavetableCache.empty()) {
                    std::cerr << "seqcc: --dump=wavetable: no wavetable "
                                 "captured (compile failed before "
                                 "construction, or sequence registered "
                                 "no waveforms); skipped.\n";
                    continue;
                }
                payload = wavetableCache;
            } else {
                // Defensive: a future fromIRSinks kind without a
                // handler.  Skip with a diagnostic rather than
                // silently writing nothing.
                std::cerr << "seqcc: --dump=" << spec.kind
                          << " is registered as IR-sink-sourced but has "
                             "no emitter (driver bug).\n";
                continue;
            }
        } else if (info.fromInfoJson) {
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
