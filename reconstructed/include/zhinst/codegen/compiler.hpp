// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Compiler — main SeqC compilation orchestrator
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "zhinst/asm/asm_list.hpp"
#include "zhinst/core/callbacks.hpp"
#include "zhinst/core/compiler_message.hpp"
#include "zhinst/ast/expression.hpp"
#include "zhinst/ast/seqc_ast_node.hpp"
#include "zhinst/ast/seqc_parser_context.hpp"
#include "zhinst/runtime/custom_functions.hpp"

namespace zhinst {

// Forward declarations for types used only as pointers/refs in this header
// (types fully included via transitive headers do not need forward decls here)
class WavetableIR;  // not transitively included

// ---- FrontEndLoweringFacade ----
// Thin static facade that dispatches to SeqCAstNode virtual method

//! \brief Free-function entry point that lowers a parsed SeqC AST into
//! the form consumed by the back end.
namespace FrontEndLoweringFacade {

// ---- LowerResult ----
// sret return type of FrontEndLoweringFacade::lower() — 32 bytes (2 shared_ptrs).
// Binary @0x1c1fb6: writes shared_ptr<Node> (from FrontendLoweringState.result)
// into sret[0], and shared_ptr<EvalResults> (evaluate sret output) into sret[1].
// Caller Compiler::compile @0x11f92f stores sret[0] into Compiler+0x28 (ast_).
//! \brief Pair returned from `FrontEndLoweringFacade::lower`.
//!
//! Carries the lowered AST root (`astResult`, stored back into
//! `Compiler::ast_`) alongside the `EvalResults` produced by
//! evaluating that AST under the supplied resource environment.
struct LowerResult {
    std::shared_ptr<Node>        astResult;    // +0x00 (from FrontendLoweringState.result)
    std::shared_ptr<EvalResults> evalResult;   // +0x10 (from evaluate virtual sret)
};

// 0x1c1da0 — ~1KB
// Packs arguments into a FrontendLoweringContext, creates an empty
// FrontendLoweringState, dispatches to SeqCAstNode vtable[0] virtual
// (the 3-arg evaluate: Resources, Context&, State&), and returns
// {state.result, evaluate_output} as a LowerResult.
LowerResult lower(std::shared_ptr<Resources> resources,
                   SeqCAstNode& ast,
                   CompilerMessageCollection& messages,
                   std::shared_ptr<AsmCommands> asmCommands,
                   std::shared_ptr<CustomFunctions> customFunctions,
                   std::shared_ptr<WaveformGenerator> waveformGen,
                   std::shared_ptr<WavetableFront> wavetable,
                   int loopUnrollLimit);

}  // namespace FrontEndLoweringFacade

// ---- CompileResult ----
// sret return type of Compiler::compile() — 40 bytes (24 + 16)
// Binary at 0x121421: writes shared_ptr<WavetableIR> into sret+0x18
//! \brief Final output of `Compiler::compile`.
//!
//! `asmList` holds the optimised, post-prefetch instruction stream
//! (one `Assembler` per machine instruction) ready for ELF emission;
//! `wavetableIR` is the resolved wavetable whose contents go into the
//! `.waveforms`, `.wf_*`, and `.wavemem` ELF sections.
struct CompileResult {
    std::vector<Assembler> asmList;          // +0x00 (24 bytes)
    std::shared_ptr<WavetableIR> wavetableIR;    // +0x18 (16 bytes)
};

// ---- Compiler (class layout) ----
//
// Offset  Size  Type                                     Name
// +0x00   8     AWGCompilerConfig const*                 config_
// +0x08   8     DeviceConstants const*                   deviceConstants_
// +0x10   4     int32_t                                  lineNr_
// +0x14   2     uint16_t                                 flags_
// +0x16   2     (padding)
// +0x18   8     uint64_t                                 reserved18_
// +0x20   4     uint32_t                                 channelCount_
// +0x24   1     uint8_t                                  usedFourChannelMode_
// +0x25   1     uint8_t                                  usedSampleRate_
// +0x26   2     (padding)
// +0x28   16    shared_ptr<Node>                         ast_
// +0x38   32    CompilerMessageCollection                messages_
// +0x58   24    vector<string>                           sourceFiles_
// +0x70   24    vector<string>                           sourceLines_
// +0x88   24    AsmList                                  asmList_
// +0xA0   16    shared_ptr<WavetableFront>               wavetable_
// +0xB0   16    shared_ptr<AsmCommands>                  asmCommands_
// +0xC0   16    shared_ptr<WaveformGenerator>            waveformGen_
// +0xD0   16    shared_ptr<CustomFunctions>              customFunctions_
// +0xE0   16    weak_ptr<CancelCallback>                 cancelCallback_
// +0xF0   16    weak_ptr<ProgressCallback>               progressCallback_
// +0x100  56    SeqcParserContext                        parserContext_
// sizeof(Compiler) = 0x138

// ---- Compiler ----

//! \brief Inner orchestrator that drives the full SeqC compilation
//! pipeline for a single source string.
//!
//! Constructed once per compilation by `AWGCompilerImpl`, holds the
//! per-compile mutable state (parser context, message collection,
//! source line cache, accumulated `AsmList`, AST root, wavetable, and
//! the supporting `AsmCommands` / `WaveformGenerator` / `CustomFunctions`
//! services), and exposes `compile()` as the master entry point.
//!
//! `compile()` performs the canonical pipeline: line-ending
//! normalisation → flex/bison parse → `toSeqCAst` → front-end lowering
//! via `FrontEndLoweringFacade::lower` → pre-waveform optimisation →
//! optional serialise/deserialise round-trip → `runPrefetcher` →
//! post-waveform optimisation → Cervino unsync pass → output
//! `CompileResult`.  `OptimizeException` thrown by either optimiser
//! pass is caught, framed as a compiler error via `messages_`, and
//! re-thrown.
//!
//! Cancellation and progress reporting are routed through the
//! `weak_ptr` callbacks supplied by the embedding `AWGCompilerImpl`;
//! `setLineNr` propagates the current line number into `AsmCommands`
//! and `WavetableFront` so emitted instructions can be tagged with the
//! correct source location.
class Compiler {
public:
    // Constructor — stores config/device ptrs, creates AsmCommands,
    // WaveformGenerator, CustomFunctions as shared_ptrs
    Compiler(const AWGCompilerConfig& config,
             const DeviceConstants& deviceConstants,
             std::shared_ptr<WavetableFront> wavetable);        // 0x11d080

    ~Compiler();                                                 // 0x103660

    // ---- Main entry point ----

    // Master compilation pipeline (~13KB):
    // 1. Reset messages, set up callbacks
    // 2. unifyLineEndings → parse → toSeqCAst
    // 3. FrontEndLoweringFacade::lower
    // 4. Build assembly list, pre-waveform optimize
    // 5. Serialize/deserialize round-trip
    // 6. runPrefetcher
    // 7. Post-waveform optimize, unsyncCervino
    // 8. Build output vector<Assembler>
    /*! \brief Run the full compilation pipeline on a single SeqC source
     *  string and produce an assembled instruction list together with
     *  the populated wavetable IR.
     *
     *  \details The pipeline executes in this order:
     *    1. Reset `messages_` and lock the cancel / progress callbacks.
     *    2. `unifyLineEndings` — normalise `\r\n` / `\r` to `\n`.
     *    3. `parse` — flex/bison parse of the normalised source into an
     *       `Expression` tree.  An empty/parse-failure tree short-
     *       circuits to an empty `CompileResult` carrying only an empty
     *       `WavetableIR` (downstream `AWGCompilerImpl::writeToStream`
     *       then surfaces the standard "empty input" error).
     *    4. Construct `StaticResources` (with a warning callback bound
     *       to `messages_.warningMessage`), initialise it with
     *       `config_` and `*deviceConstants_`, then wrap it in
     *       `GlobalResources` and publish that as
     *       `customFunctions_->resources_`.
     *    5. `toSeqCAst` — convert the parser AST into the SeqC AST.
     *    6. `FrontEndLoweringFacade::lower` — lower the SeqC AST into
     *       an evaluation tree, populating `lowerResult.astResult`
     *       (stored back into `ast_`) and `lowerResult.evalResult`.
     *    7. After lowering, `messages_.hadCompilerError()` is checked;
     *       if true the function throws `CompilerException` with
     *       message "Compiler error while evaluating sequence".
     *    8. Build the assembly preamble: a `start` label, a load
     *       placeholder, an `Entry`-style root `Node` either grafted
     *       on top of `ast_` (when the program has a `main`) or on top
     *       of `lowerResult.evalResult->node_`.  Walk the resulting
     *       node tree and back-fill `parent` pointers via BFS.
     *    9. Append the placeholder, the lowered evaluator's
     *       `assemblers_`, and the trailer triple
     *       (`wwvf` + `nop` + `end`) to `asmList_`.
     *   10. Construct an `AsmOptimize`, prepare its register / label
     *       resources from `asmList_`, then run
     *       `optimizePreWaveform`.  Any `OptimizeException` is caught,
     *       framed as an error message via
     *       `messages_.errorMessage(e.what(), e.lineNumber())`, and
     *       re-thrown.
     *   11. Optionally serialise `asmList_` to a debug-dump file and/or
     *       round-trip it through `serialize` / `deserialize` when the
     *       corresponding `config_` flags are set.
     *   12. Allocate a `WavetableIR` from the current `WavetableFront`,
     *       then call `runPrefetcher` to populate it and rewrite
     *       `asmList_` with the prefetch-aware version.
     *   13. Run `optimizePostWaveform` (same exception handling as
     *       step 10).
     *   14. On UHFLI / UHFQA targets, prepend the
     *       `AsmCommands::unsyncCervino` sequence to `asmList_`.
     *   15. Final error check: if `messages_.hadCompilerError()`
     *       throws `CompilerException` with message "Compiler error
     *       while assembling output file".
     *   16. Project `asmList_` into a `vector<Assembler>` skipping
     *       entries whose command is `Assembler::INVALID`.
     *   17. Publish the final `usedSampleRate_` flag from
     *       `staticResources` so that
     *       `AWGCompilerImpl::usedDeviceSampleRate()` reports it.
     *
     *  Cancellation requests posted through `cancelCallback_` are
     *  observed at `AsmOptimize` and `Prefetch` boundaries; debug
     *  flags on `config_` enable AST and final-assembly dumps at
     *  steps 5, 6, and 14.
     *
     *  \param source   Raw SeqC source as supplied by the caller.
     *                  May contain `\r\n` / `\r` line endings.
     *  \return         A `CompileResult` with the assembled instruction
     *                  list (`std::vector<Assembler>`) and the populated
     *                  `WavetableIR`.  An empty source produces a
     *                  result with an empty instruction vector and an
     *                  otherwise-default `WavetableIR`.
     *  \throws CompilerException  When `messages_.hadCompilerError()`
     *                  is set after lowering or after assembly.
     *  \throws OptimizeException  Re-thrown after framing as an error
     *                  message when either `optimizePreWaveform` or
     *                  `optimizePostWaveform` raises one.
     */
    CompileResult compile(const std::string& source);  // 0x11f150

    // ---- Helpers called by compile() ----

    // Normalize \r\n and \r to \n using boost::replace_all_copy
    std::string unifyLineEndings(const std::string& input) const;  // 0x11d720

    // Parse source via flex/bison (seqc_parse), returns AST
    std::shared_ptr<Expression> parse(const std::string& source);  // 0x11d9b0

    // Debug: print AST to stdout
    void printAST(std::shared_ptr<Expression> expr,
                  const std::string& label);                       // 0x122640

    // Reset the message collection
    void reset();                                                  // 0x11dfe0

    // Prefetch orchestrator: construct Prefetch, run waveform pipeline
    /*! \brief Run the waveform / prefetch sub-pipeline against the
     *  populated `WavetableFront` and rewrite `asmList_` with the
     *  prefetch-aware instruction list.
     *
     *  \details Construct a `Prefetch` over `ast_` and `wavetableIR`
     *  with a warning callback bound to `messages_.warningMessage`,
     *  then drive the waveform pipeline:
     *    1. `Prefetch::preparePlays` — walk the AST, prepare per-play
     *       state, count branches, and define play sizes.
     *    2. `Prefetch::getUsedWavesForDevice(deviceIndex)` →
     *       `WavetableIR::setUsedWaveforms` to register which
     *       waveforms the prefetcher actually needs.
     *    3. `WavetableIR::assignWaveIndexImplicit` — assign indices to
     *       waveforms that did not receive an explicit
     *       `assignWaveIndex` annotation.
     *    4. `WavetableIR::alignWaveformSizes` — round each waveform's
     *       sample count up to the device's grain size.
     *    5. `WavetableIR::assignWaveformAllocationSizes` — compute the
     *       per-waveform allocation footprint (samples × channel
     *       count × format).
     *    6. `Prefetch::determineFixedWaves` — only when
     *       `config.cacheType == 1` (fixed-cache mode): mark the
     *       waveforms that the prefetcher must pin.
     *    7. `WavetableIR::updateWaveforms(useCache, hasDIO)` — finalise
     *       the waveform records (`useCache` is true when caching is
     *       enabled and the target is a Hirzel-generation device;
     *       `hasDIO` is the device's DIO presence flag).
     *    8. `Prefetch::placeLoads` — schedule cache loads against the
     *       prepared play tree.
     *    9. `Prefetch::fillInPlaceholders(asmList)` — produce the
     *       final assembly list with the original `placeholder`
     *       expanded into the scheduled load instructions; the result
     *       replaces `asmList_`.
     *   10. Cache `Prefetch::getUsedChannels` and
     *       `getUsedFourChannelMode` into `channelCount_` and
     *       `usedFourChannelMode_` for downstream ELF section emission.
     *
     *  Optional debug branches: serialise / round-trip `wavetableIR`
     *  through JSON when the corresponding `config` flags are set;
     *  call `Prefetch::print` when `config.debugFlags & 0x08` is set.
     *
     *  \param wavetableIR     Freshly-allocated, empty `WavetableIR`
     *                         that this call populates.
     *  \param asmList         The `AsmList` produced by
     *                         `optimizePreWaveform`; consumed by
     *                         `fillInPlaceholders` to produce the
     *                         post-prefetch list assigned to
     *                         `asmList_`.
     *  \param asmCommands     Shared `AsmCommands` used to construct
     *                         the per-load instructions.
     *  \param placeholder     The load-placeholder `AsmList::Asm`
     *                         emitted earlier in `compile()`; the
     *                         anchor that `fillInPlaceholders` expands.
     *  \param deviceConstants Per-device geometry and feature flags
     *                         (DIO presence is read here).
     *  \param config          The active `AWGCompilerConfig`
     *                         (`deviceIndex`, `cacheType`, `isHirzel`,
     *                         and the debug flags are read here).
     */
    void runPrefetcher(std::shared_ptr<WavetableIR> wavetableIR,
                       const AsmList& asmList,
                       std::shared_ptr<AsmCommands> asmCommands,
                       AsmList::Asm placeholder,
                       const DeviceConstants& deviceConstants,
                       const AWGCompilerConfig& config);           // 0x11dff0

    // ---- Getters / Setters ----

    void setCancelCallback(std::weak_ptr<CancelCallback> cb);      // 0x123480
    void setProgressCallback(std::weak_ptr<ProgressCallback> cb);  // 0x123510

    // Returns pointer to customFunctions_->nodeList_ (vector<NodeMapItem> at +0x150)
    const std::vector<NodeMapItem>* getNodeAccessList() const;             // 0x123550

    // Returns pointer to customFunctions_->accessModeMap_ (unordered_map at +0x128)
    const std::unordered_map<NodeMapItem, std::set<AccessMode>>* getNodeToModeMap() const;  // 0x123570

    // Returns {channelCount (int at +0x20), mode (byte at +0x24)}
    std::vector<int> getChannelInfo() const;                       // 0x123590

    // Returns byte at this+0x25
    bool usedDeviceSampleRate() const;                             // 0x1235e0

    // Access parserContext_.hadSyntaxError() — used by AWGCompilerImpl
    // Binary reads byte at Compiler+0x100+0x03 directly.
    bool hadSyntaxError() const;                                   // inline, delegates to parserContext_

    // Copies messages from CompilerMessageCollection
    std::vector<CompilerMessage> getCompileMessages() const;       // 0x1235f0

    // Sets lineNr at +0x10, propagates to AsmCommands and WavetableFront
    void setLineNr(int nr);                                        // 0x123640

    // Builds vector<int> from asmList entries: groups of 4 ints per instruction
    std::vector<int> getLineMap(int offset) const;                 // 0x123660

private:
    friend class AWGCompilerImpl;  // getJsonVersion reads customFunctions_ directly
    const AWGCompilerConfig* config_;              // +0x00
    const DeviceConstants* deviceConstants_;        // +0x08
    int32_t lineNr_;                               // +0x10
    uint16_t flags_;                               // +0x14
    uint8_t pad16_[2];                             // +0x16
    uint64_t reserved18_;                          // +0x18
    uint32_t channelCount_;                        // +0x20
    uint8_t usedFourChannelMode_;                   // +0x24
    uint8_t usedSampleRate_;                       // +0x25
    uint8_t pad26_[2];                             // +0x26
    std::shared_ptr<Node> ast_;                    // +0x28
    CompilerMessageCollection messages_;           // +0x38 (0x20 bytes)
    std::vector<std::string> sourceFiles_;         // +0x58
    std::vector<std::string> sourceLines_;         // +0x70
    AsmList asmList_;                              // +0x88 (0x18 bytes)
    std::shared_ptr<WavetableFront> wavetable_;    // +0xA0
    std::shared_ptr<AsmCommands> asmCommands_;     // +0xB0
    std::shared_ptr<WaveformGenerator> waveformGen_;     // +0xC0
    std::shared_ptr<CustomFunctions> customFunctions_;   // +0xD0
    std::weak_ptr<CancelCallback> cancelCallback_;       // +0xE0
    std::weak_ptr<ProgressCallback> progressCallback_;   // +0xF0
    SeqcParserContext parserContext_;                // +0x100 (0x38 bytes)
    // +0x138 END
};

}  // namespace zhinst
