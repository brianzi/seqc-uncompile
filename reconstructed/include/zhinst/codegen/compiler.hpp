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
    std::shared_ptr<Node>        astResult;    //!< Lowered AST root produced by the walk; stored into `Compiler::ast_` after `lower()` returns. +0x00 (from FrontendLoweringState.result)
    std::shared_ptr<EvalResults> evalResult;   //!< Top-level `EvalResults` from evaluating the AST under the supplied resource environment. +0x10 (from evaluate virtual sret)
};

// 0x1c1da0 — ~1KB
// Packs arguments into a FrontendLoweringContext, creates an empty
// FrontendLoweringState, dispatches to SeqCAstNode vtable[0] virtual
// (the 3-arg evaluate: Resources, Context&, State&), and returns
// {state.result, evaluate_output} as a LowerResult.
/*! \brief Lower a parsed SeqC AST into the back-end node tree plus
 *  its top-level evaluation result.
 *
 *  \details Builds a stack-local `FrontendLoweringContext` populated
 *  with the supplied resource environment (`asmCommands`,
 *  `customFunctions`, `waveformGen`, `wavetable`, `loopUnrollLimit`)
 *  and the diagnostics sink `messages`, plus an empty
 *  `FrontendLoweringState`, then dispatches the top-level virtual
 *  `SeqCAstNode::evaluate(resources, context, state)`.  That call
 *  walks the AST polymorphically: each statement / expression node
 *  emits assembler placeholders into `context.asmCommands`, populates
 *  `state.result` with the lowered back-end `Node` graph, and
 *  threads its own per-node `EvalResults` up the call chain.  The
 *  facade returns the pair `{state.result, top-level EvalResults}`,
 *  which `Compiler::compile` stores into `Compiler::ast_` (root) and
 *  uses to drive the rest of the pipeline.
 *
 *  \param resources       The static / dynamic resource environment
 *                         (variables, registers, device constants)
 *                         visible to every AST node during lowering.
 *  \param ast             The root of the parsed SeqC AST.  Mutated
 *                         only via the virtual `evaluate` it
 *                         dispatches to (the AST itself is not
 *                         rewritten).
 *  \param messages        Diagnostics sink for warnings and errors
 *                         emitted by the lowering pass.
 *  \param asmCommands     Sink for emitted assembler placeholders.
 *                         The lowering pass appends to this; the
 *                         back end consumes it after the facade
 *                         returns.
 *  \param customFunctions Resolver for built-in / user functions
 *                         invoked from SeqC (`playWave`, `wait`,
 *                         math built-ins, etc.).
 *  \param waveformGen     Factory for waveform-generator built-ins
 *                         (`zeros`, `sin`, …) called from SeqC.
 *  \param wavetable       Front-end wavetable that records every
 *                         waveform reference for later wave-table
 *                         construction by `WavetableIR`.
 *  \param loopUnrollLimit Iteration cap that the lowering pass
 *                         applies when constant-folding `repeat (N)`
 *                         loops; sourced from
 *                         `AWGCompilerConfig::loopUnrollLimit`.
 *  \return                A `LowerResult` carrying the lowered
 *                         back-end `Node` root (`astResult`) and
 *                         the top-level `EvalResults`
 *                         (`evalResult`).
 */
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
    std::vector<Assembler> asmList;          //!< Optimised, post-prefetch instruction stream emitted by the pipeline; consumed by the ELF writer. +0x00 (24 bytes)
    std::shared_ptr<WavetableIR> wavetableIR;    //!< Resolved wavetable populated by the prefetch pass; produces the ELF `.waveforms`, `.wf_*`, and `.wavemem` sections. +0x18 (16 bytes)
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
    /*! \brief Construct a `Compiler` ready to compile SeqC source for a
     *  single AWG sequencer.
     *
     *  \details The constructor only wires up the long-lived collaborator
     *  graph; no SeqC source is parsed and no waveform memory is touched
     *  until `compile()` is called.  Specifically it:
     *   1.  Stores `config` and `deviceConstants` as raw pointers (the
     *       caller owns their lifetimes; the typical owner is the
     *       enclosing `AWGCompilerImpl`).
     *   2.  Default-initialises `messages_`, `lineNr_`, the channel-mode
     *       counters, and the cancel / progress `weak_ptr`s.
     *   3.  Creates `asmCommands_` (`std::make_shared<AsmCommands>`),
     *       binding its error callback to `messages_.errorMessage`.
     *   4.  Creates `waveformGen_` (`std::make_shared<WaveformGenerator>`),
     *       binding its warning callback to `messages_.warningMessage`.
     *   5.  Creates `customFunctions_` with the freshly-built
     *       `asmCommands_`, `waveformGen_`, and the same warning
     *       callback.
     *   6.  Wires `parserContext_`'s syntax-error callback to
     *       `messages_.parserMessage(lineNr, msg)` so flex/bison
     *       diagnostics flow into the same message bag the rest of the
     *       pipeline appends to.
     *
     *  \param config           Compilation configuration (sequencer
     *                          index, debug flags, cache mode, etc.).
     *                          Captured by raw pointer; must outlive
     *                          this `Compiler`.
     *  \param deviceConstants  Per-device geometry and feature flags
     *                          (DIO presence, waveform geometry,
     *                          opcode set).  Also captured by raw
     *                          pointer; must outlive this `Compiler`.
     *  \param wavetable        Shared `WavetableFront` that this
     *                          `Compiler` reads from and writes back
     *                          via the AST and `WaveformGenerator`.
     */
    Compiler(const AWGCompilerConfig& config,
             const DeviceConstants& deviceConstants,
             std::shared_ptr<WavetableFront> wavetable);        // 0x11d080

    //! \brief Destroy the `Compiler` and release all owned shared state.
    //!
    //! All owned `shared_ptr` members (`asmCommands_`, `waveformGen_`,
    //! `customFunctions_`, `wavetable_`) are released in reverse
    //! construction order.  No additional cleanup is performed; the
    //! defaulted destructor is sufficient because every member type is
    //! itself RAII-managed.
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
     *   1.  Reset `messages_` and lock the cancel / progress callbacks.
     *   2.  `unifyLineEndings` — normalise `\r\n` / `\r` to `\n`.
     *   3.  `parse` — flex/bison parse of the normalised source into an
     *       `Expression` tree.  An empty/parse-failure tree short-
     *       circuits to an empty `CompileResult` carrying only an empty
     *       `WavetableIR` (downstream `AWGCompilerImpl::writeToStream`
     *       then surfaces the standard "empty input" error).
     *   4.  Construct `StaticResources` (with a warning callback bound
     *       to `messages_.warningMessage`), initialise it with
     *       `config_` and `*deviceConstants_`, then wrap it in
     *       `GlobalResources` and publish that as
     *       `customFunctions_->resources_`.
     *   5.  `toSeqCAst` — convert the parser AST into the SeqC AST.
     *   6.  `FrontEndLoweringFacade::lower` — lower the SeqC AST into
     *       an evaluation tree, populating `lowerResult.astResult`
     *       (stored back into `ast_`) and `lowerResult.evalResult`.
     *   7.  After lowering, `messages_.hadCompilerError()` is checked;
     *       if true the function throws `CompilerException` with
     *       message "Compiler error while evaluating sequence".
     *   8.  Build the assembly preamble: a `start` label, a load
     *       placeholder, an `Entry`-style root `Node` either grafted
     *       on top of `ast_` (when the program has a `main`) or on top
     *       of `lowerResult.evalResult->node_`.  Walk the resulting
     *       node tree and back-fill `parent` pointers via BFS.
     *   9.  Append the placeholder, the lowered evaluator's
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
    /*! \brief Normalise line-ending conventions in a SeqC source string
     *  to LF (`\n`).
     *
     *  \details Performs a single full-buffer scan-and-replace.  When
     *  the input contains `\r\n` the carriage-return / line-feed pairs
     *  are collapsed to a single `\n`; otherwise, when standalone `\r`
     *  characters are present they are each rewritten to `\n`.  When
     *  the input already uses `\n` exclusively the original string is
     *  returned by value (a copy), so the caller may treat the result
     *  as owning a normalised buffer in every case.
     *
     *  \param input  Raw SeqC source as supplied by the caller.
     *  \return       The same source with all line endings collapsed
     *                to `\n`.
     */
    std::string unifyLineEndings(const std::string& input) const;  // 0x11d720

    // Parse source via flex/bison (seqc_parse), returns AST
    /*! \brief Parse a normalised SeqC source string into the parser-side
     *  expression tree.
     *
     *  \details Resets `parserContext_` and runs the flex/bison
     *  pipeline (`seqc_lex_init_extra` → `seqc__scan_string` →
     *  `seqc_parse` → cleanup) against the supplied buffer.  The raw
     *  `Expression*` produced by the bison parser is wrapped in a
     *  `shared_ptr<Expression>` and returned to the caller; on the way
     *  out the source is split at LF boundaries and stored in
     *  `sourceLines_` so later phases can attach line context to error
     *  messages.  Any syntax error reported by the parser becomes an
     *  entry in `messages_` (via the parser-context callback wired in
     *  the constructor) and additionally raises a `CompilerException`
     *  here; lexer-init failure also raises `CompilerException`.
     *
     *  \param source  Normalised (LF-only) SeqC source.  Caller is
     *                 responsible for running `unifyLineEndings`
     *                 first if their buffer may contain `\r`.
     *  \return        Owning handle to the parser AST root.  Never
     *                 null on success.
     *  \throws CompilerException  When the lexer fails to initialise
     *                 ("Failed to initialize lexer") or the parser
     *                 reported a syntax error ("Syntax error while
     *                 parsing seqC").
     */
    std::shared_ptr<Expression> parse(const std::string& source);  // 0x11d9b0

    // Debug: print AST to stdout
    /*! \brief Recursively dump a parser AST sub-tree to `std::cout`
     *  (debug aid).
     *
     *  \details Writes a single line per node containing the operation
     *  type, optional operator, source line number, optional command
     *  type, optional `VarType`, and optional `name`, then descends
     *  into the children.  Intended for manual inspection during
     *  development; never invoked by the production pipeline.  A
     *  `nullptr` `expr` is silently ignored so callers may dump a
     *  potentially-empty parse result without guarding.
     *
     *  \param expr   Sub-tree root to dump.  May be null.
     *  \param label  Prefix written before the root node's line; used
     *                to mark which compilation phase produced the
     *                tree (e.g. `"parse"`, `"after lower"`).
     */
    void printAST(std::shared_ptr<Expression> expr,
                  const std::string& label);                       // 0x122640

    // Reset the message collection
    /*! \brief Drop every message accumulated by the previous
     *  `compile()` invocation.
     *
     *  \details Clears `messages_` (the `CompilerMessageCollection`)
     *  in place, preserving its callback wiring; collaborator state
     *  such as `parserContext_`, `asmList_`, `sourceLines_`, and the
     *  cancel / progress `weak_ptr`s are not touched.  Typically
     *  called by the embedding `AWGCompilerImpl` between two
     *  consecutive `compile()` calls so that older diagnostics do not
     *  leak into the new run.
     */
    void reset();                                                  // 0x11dfe0

    // Prefetch orchestrator: construct Prefetch, run waveform pipeline
    /*! \brief Run the waveform / prefetch sub-pipeline against the
     *  populated `WavetableFront` and rewrite `asmList_` with the
     *  prefetch-aware instruction list.
     *
     *  \details Construct a `Prefetch` over `ast_` and `wavetableIR`
     *  with a warning callback bound to `messages_.warningMessage`,
     *  then drive the waveform pipeline:
     *   1.  `Prefetch::preparePlays` — walk the AST, prepare per-play
     *       state, count branches, and define play sizes.
     *   2.  `Prefetch::getUsedWavesForDevice(deviceIndex)` →
     *       `WavetableIR::setUsedWaveforms` to register which
     *       waveforms the prefetcher actually needs.
     *   3.  `WavetableIR::assignWaveIndexImplicit` — assign indices to
     *       waveforms that did not receive an explicit
     *       `assignWaveIndex` annotation.
     *   4.  `WavetableIR::alignWaveformSizes` — round each waveform's
     *       sample count up to the device's grain size.
     *   5.  `WavetableIR::assignWaveformAllocationSizes` — compute the
     *       per-waveform allocation footprint (samples × channel
     *       count × format).
     *   6.  `Prefetch::determineFixedWaves` — only when
     *       `config.cacheType == 1` (fixed-cache mode): mark the
     *       waveforms that the prefetcher must pin.
     *   7.  `WavetableIR::updateWaveforms(useCache, hasDIO)` — finalise
     *       the waveform records (`useCache` is true when caching is
     *       enabled and the target is a Hirzel-generation device;
     *       `hasDIO` is the device's DIO presence flag).
     *   8.  `Prefetch::placeLoads` — schedule cache loads against the
     *       prepared play tree.
     *   9.  `Prefetch::fillInPlaceholders(asmList)` — produce the
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

    /*! \brief Install a `CancelCallback` that the compiler will poll at
     *  long-running cooperative cancellation points.
     *
     *  \details Stores `cb` into `cancelCallback_` and propagates the
     *  same `weak_ptr` into `waveformGen_->cancelCallback_` (so that
     *  expensive waveform builders may eventually poll the same
     *  hook).  `AsmOptimize` and `Prefetch` already poll
     *  `cancelCallback_` at their cooperative checkpoints; on a
     *  positive cancellation answer they raise an exception that
     *  propagates back out of `compile()`.  The previous callback (if
     *  any) is overwritten unconditionally; passing an empty
     *  `weak_ptr` therefore detaches cancellation polling on both
     *  this `Compiler` and its `WaveformGenerator`.
     *
     *  \param cb  Weak handle to the user-installed cancel hook.  May
     *             be empty to detach.
     */
    void setCancelCallback(std::weak_ptr<CancelCallback> cb);      // 0x123480

    /*! \brief Install a `ProgressCallback` that receives periodic
     *  progress notifications from `compile()`.
     *
     *  \details Stores `cb` into the `progressCallback_` `weak_ptr`.
     *  The pipeline currently invokes the callback only at the major
     *  phase boundaries observable in `compile()`; finer-grained
     *  notifications are TBD.  As with `setCancelCallback`, passing an
     *  empty `weak_ptr` detaches the previously-installed callback.
     *
     *  \param cb  Weak handle to the user-installed progress hook.
     *             May be empty to detach.
     */
    void setProgressCallback(std::weak_ptr<ProgressCallback> cb);  // 0x123510

    // Returns pointer to customFunctions_->nodeList_ (vector<NodeMapItem> at +0x150)
    /*! \brief Return the list of LabOne data-server nodes the most-
     *  recent `compile()` run referenced.
     *
     *  \details Each `NodeMapItem` records a node path string together
     *  with its inferred access mode (subscribed, polled, etc.).  The
     *  list is populated by `CustomFunctions` as it lowers SeqC
     *  built-ins that touch the data server (`getNode`, `setNode`,
     *  `playWave` with feedback, ...).  The returned pointer aliases
     *  internal storage of `customFunctions_`; it is invalidated by
     *  the next `compile()` or by destruction of this `Compiler`.
     *
     *  \return  Pointer to the live `nodeList_`, or `nullptr` if
     *           `customFunctions_` was never set up (only happens if
     *           the constructor failed before completion).
     */
    const std::vector<NodeMapItem>* getNodeAccessList() const;             // 0x123550

    // Returns pointer to customFunctions_->accessModeMap_ (unordered_map at +0x128)
    /*! \brief Return the per-node access-mode map populated during the
     *  most-recent `compile()` run.
     *
     *  \details Companion to `getNodeAccessList`: the map keys are the
     *  `NodeMapItem`s appearing in that list, and each value is the
     *  set of `AccessMode`s observed for the node across all SeqC
     *  built-ins that referenced it.  The returned pointer aliases
     *  internal storage of `customFunctions_` and follows the same
     *  invalidation rules.
     *
     *  \return  Pointer to the live `accessModeMap_`, or `nullptr`
     *           when `customFunctions_` is missing.
     */
    const std::unordered_map<NodeMapItem, std::set<AccessMode>>* getNodeToModeMap() const;  // 0x123570

    // Returns {channelCount (int at +0x20), mode (byte at +0x24)}
    /*! \brief Report the channel count and four-channel-mode flag the
     *  most-recent `compile()` settled on for the target sequencer.
     *
     *  \details Returns a fresh two-element `std::vector<int>`:
     *  element 0 is the active channel count and element 1 is the
     *  four-channel-mode flag (non-zero on devices that pack two
     *  logical sequencers into one four-channel emission).  Both
     *  fields are written by the prefetch / channel-assignment phase
     *  inside `compile()`; calling this getter before the first
     *  `compile()` run yields `{0, 0}`.
     *
     *  \return  Two-element vector `{channelCount, fourChannelMode}`.
     */
    std::vector<int> getChannelInfo() const;                       // 0x123590

    // Returns byte at this+0x25
    /*! \brief Return whether the most-recent `compile()` run actually
     *  consulted the device-supplied sample rate.
     *
     *  \details The pipeline records this flag whenever a SeqC
     *  built-in resolves a duration in samples by multiplying by the
     *  device sample rate.  Callers (the LabOne data-server bindings)
     *  use the answer to decide whether the compiled program must be
     *  re-emitted when the device's sample-rate setting changes.
     *
     *  \return  `true` when the compiled program references the
     *           device sample rate, `false` otherwise.
     */
    bool usedDeviceSampleRate() const;                             // 0x1235e0

    // Access parserContext_.hadSyntaxError() — used by AWGCompilerImpl
    // Binary reads byte at Compiler+0x100+0x03 directly.
    /*! \brief Report whether the most-recent `parse()` call observed a
     *  syntax error.
     *
     *  \details Forwards to `parserContext_.hadSyntaxError()`; the
     *  flag is set by the bison error handler when the parser cannot
     *  reduce the input.  `parse()` itself raises a
     *  `CompilerException` when this flag is set, so a `true` answer
     *  is only observable when the caller catches that exception and
     *  inspects the `Compiler` afterwards.
     *
     *  \return  `true` when the previous `parse()` produced a syntax
     *           error.
     */
    bool hadSyntaxError() const;                                   // inline, delegates to parserContext_

    // Copies messages from CompilerMessageCollection
    /*! \brief Return a copy of every diagnostic emitted by the most-
     *  recent `compile()` run.
     *
     *  \details Each `CompilerMessage` carries a severity (info /
     *  warning / error), a one-based source line number, and the
     *  human-readable text.  Messages from all phases (parser, AST
     *  lowering, optimisation, prefetch, output emission) are
     *  collected into the same bag and returned in the order they
     *  were appended.  Returning by value lets the caller inspect or
     *  display the messages without holding any lock against further
     *  `compile()` activity.
     *
     *  \return  All collected diagnostics in append order.
     */
    std::vector<CompilerMessage> getCompileMessages() const;       // 0x1235f0

    // Sets lineNr at +0x10, propagates to AsmCommands and WavetableFront
    /*! \brief Set the current source-line number used to annotate
     *  subsequently-emitted assembly entries.
     *
     *  \details Writes `nr` into three places, in order:
     *   1.  `lineNr_` (this `Compiler`).
     *   2.  `asmCommands_->setWavetableFrontIndex(nr)` — every
     *       subsequent assembly entry produced through `AsmCommands`
     *       reads this for its `lineNumber` field.
     *   3.  `wavetable_->setLineNr(nr)` (tail call) — `WavetableFront`
     *       caches the value as `manager_->numDefs_` so newly-created
     *       waveform records carry the same line annotation.
     *
     *  \param nr  One-based source line to associate with the next
     *             assembly entries.
     */
    void setLineNr(int nr);                                        // 0x123640

    // Builds vector<int> from asmList entries: groups of 4 ints per instruction
    /*! \brief Build the flat line-map projection of `asmList_` for a
     *  given starting offset.
     *
     *  \details Walks every entry of `asmList_` and emits four `int`s
     *  per non-skipped instruction:
     *    - `counter + offset`  — instruction address (PC) including
     *      the caller-supplied base offset.
     *    - `counter`            — zero-based instruction index.
     *    - `seq`                — running label sequence number;
     *      bumped each time a `LABEL` entry is encountered.
     *    - `entry.lineNumber()` — original SeqC source line.
     *
     *  Entries whose command is `Assembler::INVALID` (`cmd == -1`)
     *  are skipped entirely; `LABEL` entries advance `seq` but do not
     *  themselves produce output.  The result is consumed by the
     *  ELF-emission layer to build the `.linenr` section.
     *
     *  \param offset  Base instruction address to add to every
     *                 emitted PC value.  Typically the device's
     *                 `addressImpl` constant (e.g. `0x80000000` on
     *                 HDAWG / SHF).
     *  \return        Flat `int` vector with `4 *
     *                 emitted-instruction-count` entries.
     */
    std::vector<int> getLineMap(int offset) const;                 // 0x123660

private:
    //! \brief Grants `AWGCompilerImpl::getJsonVersion` direct
    //!        read access to `customFunctions_` for emission of
    //!        the compile-result JSON.
    friend class AWGCompilerImpl;  // getJsonVersion reads customFunctions_ directly
    //! \brief Owning compilation's configuration (sequencer
    //!        index, debug flags, sample rate); captured by raw
    //!        pointer.
    const AWGCompilerConfig* config_;              // +0x00
    //! \brief Per-device geometry / feature table (DIO presence,
    //!        opcode set, waveform geometry); captured by raw
    //!        pointer.
    const DeviceConstants* deviceConstants_;        // +0x08
    //! \brief Current source line during the parse / lower
    //!        pipeline; written by `setLineNr` and consumed by
    //!        `messages_` when framing diagnostics.
    int32_t lineNr_;                               // +0x10
    //! \brief 2-byte slot at +0x14 zero-initialised by the
    //!        binary's `Compiler` constructor (`0x11d0ab: mov
    //!        WORD PTR [rdi+0x14], 0x0`) and never re-touched by
    //!        any reader or writer in either binary or recon.
    //!        Likely an unused flags slot retained for ABI
    //!        fidelity.
    uint16_t flags_;                               // +0x14
    //! \brief Alignment padding ahead of `reserved18_`.
    uint8_t pad16_[2];                             // +0x16
    //! \brief 8-byte slot zero-initialised by the binary's
    //!        `Compiler` constructor and re-zeroed at the start
    //!        of `compile()` (`0x11f76e`); never read by any
    //!        reconstructed-or-binary code path. Retained for
    //!        ABI fidelity.
    uint64_t reserved18_;                          // +0x18
    //! \brief Number of physical output channels selected for
    //!        the compile (1, 2, 4, or 8 depending on device).
    uint32_t channelCount_;                        // +0x20
    //! \brief Non-zero when the program has activated 4-channel
    //!        mode (HDAWG groups of four); consumed by the
    //!        `.channels` ELF section emitter.
    uint8_t usedFourChannelMode_;                   // +0x24
    //! \brief Non-zero when the program has touched a
    //!        device-specific sample-rate node, triggering the
    //!        emission of the optional `.required_sample_rate`
    //!        ELF section.
    uint8_t usedSampleRate_;                       // +0x25
    //! \brief Alignment padding ahead of the embedded
    //!        `shared_ptr` chain.
    uint8_t pad26_[2];                             // +0x26
    //! \brief Root of the lowered SeqC AST built by `compile()`;
    //!        consumed by the optimisation / prefetch / codegen
    //!        passes.
    std::shared_ptr<Node> ast_;                    // +0x28
    //! \brief Collection of pipeline diagnostics
    //!        (errors / warnings / info) emitted during the
    //!        compile; surfaced through `messages()`.
    CompilerMessageCollection messages_;           // +0x38 (0x20 bytes)
    //! \brief Source file names introduced by `#include`
    //!        directives, in order of first appearance.
    std::vector<std::string> sourceFiles_;         // +0x58
    //! \brief Verbatim source lines (one element per input line)
    //!        used by diagnostics to render the offending source
    //!        context.
    std::vector<std::string> sourceLines_;         // +0x70
    //! \brief Generated assembler list plus its companion
    //!        `WavetableIR`; produced by codegen, consumed by
    //!        the prefetcher and the final ELF emitter.
    AsmList asmList_;                              // +0x88 (0x18 bytes)
    //! \brief Shared front-side wavetable copied from the
    //!        embedding `AWGCompilerImpl`; updated in place by
    //!        the lowering / codegen passes.
    std::shared_ptr<WavetableFront> wavetable_;    // +0xA0
    //! \brief Shared assembler-command registry; bound at
    //!        construction with its error callback wired to
    //!        `messages_.errorMessage`.
    std::shared_ptr<AsmCommands> asmCommands_;     // +0xB0
    //! \brief Shared waveform-generator handle; bound at
    //!        construction with its warning callback wired to
    //!        `messages_.warningMessage`.
    std::shared_ptr<WaveformGenerator> waveformGen_;     // +0xC0
    //! \brief SeqC built-in dispatch table created at
    //!        construction and consulted on every function-call
    //!        AST node during lowering.
    std::shared_ptr<CustomFunctions> customFunctions_;   // +0xD0
    //! \brief Caller-supplied cancellation hook polled at
    //!        pipeline checkpoints; an expired `weak_ptr`
    //!        disables cancellation.
    std::weak_ptr<CancelCallback> cancelCallback_;       // +0xE0
    //! \brief Caller-supplied progress hook invoked at pipeline
    //!        milestones with a value in `[0.0, 1.0]`; an
    //!        expired `weak_ptr` disables reporting.
    std::weak_ptr<ProgressCallback> progressCallback_;   // +0xF0
    //! \brief Inline flex/bison parser context shared with the
    //!        SeqC lexer; its syntax-error callback is wired to
    //!        `messages_.parserMessage`.
    SeqcParserContext parserContext_;                // +0x100 (0x38 bytes)
    // +0x138 END
};

}  // namespace zhinst
