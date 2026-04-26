// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Compiler — main SeqC compilation orchestrator
//
// Key methods: compile() is the master pipeline (~13KB), runPrefetcher()
// orchestrates the waveform prefetch pass (~2.8KB).
// ============================================================================

#include "zhinst/compiler.hpp"
#include "zhinst/frontend_lowering.hpp"
#include "zhinst/seqc_ast_node.hpp"
#include "zhinst/seqc_parser_context.hpp"
#include "zhinst/expression.hpp"
#include "zhinst/node.hpp"
#include "zhinst/eval_results.hpp"
#include "zhinst/asm_commands.hpp"
#include "zhinst/waveform_generator.hpp"

#include <deque>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string/replace.hpp>
#include "zhinst/asm_optimize.hpp"
#include "zhinst/wavetable_ir.hpp"
#include "zhinst/prefetch.hpp"
#include "zhinst/address_impl.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/resources.hpp"

// Forward declarations for flex/bison parser.
// These are compiled as C++ (LANGUAGE CXX in CMake), so signatures must
// match the actual definitions in seqc_lexer.c / seqc_parser.tab.c exactly.
namespace zhinst { class SeqcParserContext; class Expression; }
struct yy_buffer_state;
int seqc_lex_init_extra(zhinst::SeqcParserContext* extra, void** scanner);
yy_buffer_state* seqc__scan_string(const char* str, void* scanner);
int seqc_parse(zhinst::SeqcParserContext* ctx, zhinst::Expression** result, void* scanner);
void seqc__delete_buffer(yy_buffer_state* buf, void* scanner);
int seqc_lex_destroy(void* scanner);

namespace zhinst {

// Forward declarations for types used in pipeline
class StaticResources;
class Prefetch;
class AsmOptimize;

// Free functions used in pipeline
extern std::shared_ptr<SeqCAstNode> toSeqCAst(std::shared_ptr<Expression> expr);
extern void printSeqCAst(SeqCAstNode& ast);

// ============================================================================
// Constructor
// ============================================================================

// 0x11d080
Compiler::Compiler(const AWGCompilerConfig& config,
                   const DeviceConstants& deviceConstants,
                   std::shared_ptr<WavetableFront> wavetable)
    : config_(&config)
    , deviceConstants_(&deviceConstants)
    , lineNr_(0)
    , flags_(0)
    , reserved18_(0)
    , channelCount_(0)
    , channelMode_(0)
    , usedSampleRate_(0)
    , ast_(nullptr)
    , messages_()
    , wavetable_(std::move(wavetable))
{
    // Create error callback for AsmCommands
    auto errorCb = [this](std::string const& msg) {
        messages_.errorMessage(msg, -1);
    };

    // Create AsmCommands
    asmCommands_ = std::make_shared<AsmCommands>(
        config, wavetable_,
        std::function<void(const std::string&)>(errorCb));

    // Create WaveformGenerator
    auto warningCb = [this](std::string const& msg) {
        messages_.warningMessage(msg, -1);
    };
    waveformGen_ = std::make_shared<WaveformGenerator>(
        wavetable_,
        std::function<void(const std::string&)>(warningCb));

    // Create CustomFunctions
    customFunctions_ = std::make_shared<CustomFunctions>(
        config, deviceConstants, wavetable_, waveformGen_, asmCommands_,
        std::function<void(const std::string&)>(warningCb));

    // Initialize SeqcParserContext at +0x100
    // Sets error callback as std::function wrapping a lambda $_0
    // that calls CompilerMessageCollection::parserMessage
    std::memset(parserContext_, 0, sizeof(parserContext_));

    // Cancel/progress callbacks start as empty weak_ptrs
}

// ============================================================================
// Destructor
// ============================================================================

// 0x103660
// Destroys in reverse order:
// - parserContext_ std::function at +0x100
// - progressCallback_ weak_ptr at +0xF0
// - cancelCallback_ weak_ptr at +0xE0
// - customFunctions_ shared_ptr at +0xD0
// - waveformGen_ shared_ptr at +0xC0
// - asmCommands_ shared_ptr at +0xB0
// - wavetable_ shared_ptr at +0xA0 (implicit)
// - asmList_ at +0x88
// - sourceLines_ vector<string> at +0x70
// - sourceFiles_ vector<string> at +0x58
// - messages_ at +0x38
// - ast_ shared_ptr<Node> at +0x28
Compiler::~Compiler() = default;

// ============================================================================
// reset
// ============================================================================

// 0x11dfe0
void Compiler::reset() {
    messages_.reset();
}

// ============================================================================
// unifyLineEndings
// ============================================================================

// 0x11d720
std::string Compiler::unifyLineEndings(const std::string& input) const {
    // Check for \r\n first (prioritized over standalone \r)
    if (input.find("\r\n") != std::string::npos) {
        return boost::replace_all_copy(input, "\r\n", "\n");
    }
    // Check for standalone \r
    if (input.find('\r') != std::string::npos) {
        return boost::replace_all_copy(input, "\r", "\n");
    }
    // No conversion needed
    return input;
}

// ============================================================================
// parse
// ============================================================================

// 0x11d9b0
std::shared_ptr<Expression> Compiler::parse(const std::string& source) {
    // Reset parser context at this+0x100
    auto* ctx = reinterpret_cast<SeqcParserContext*>(parserContext_);
    // ctx->reset();  // 0x247cc0

    void* scanner = nullptr;
    if (seqc_lex_init_extra(ctx, &scanner) != 0) {
        seqc_lex_destroy(scanner);
        throw CompilerException("Failed to initialize lexer");
    }

    auto* buf = seqc__scan_string(source.c_str(), scanner);

    Expression* rawResult = nullptr;
    seqc_parse(ctx, &rawResult, scanner);

    seqc__delete_buffer(static_cast<yy_buffer_state*>(buf), scanner);
    seqc_lex_destroy(scanner);

    // Check for syntax errors
    // if (ctx->hadSyntaxError()) {
    //     throw CompilerException("Syntax error");
    // }

    // Split source into lines and store in sourceLines_
    sourceLines_.clear();
    std::istringstream iss(source);
    std::string line;
    while (std::getline(iss, line)) {
        sourceLines_.push_back(std::move(line));
    }

    // Wrap raw pointer in shared_ptr with default_delete
    return std::shared_ptr<Expression>(rawResult);
}

// ============================================================================
// printAST (debug)
// ============================================================================

// 0x122640
// Prints AST to std::cout. Calls zhinst::str(EOperationType) for each node,
// recursively prints children. Debug-only, approximate reconstruction.
void Compiler::printAST(std::shared_ptr<Expression> expr,
                        const std::string& label) {
    // Sets cout formatting flags
    // If expr is null, returns
    // Prints expression type via str(EOperationType), then recursively
    // prints children. Uses indentation based on nesting depth.
    // ~3.5KB of debug printing code — not reconstructed in detail.
}

// ============================================================================
// compile — master pipeline
// ============================================================================

// 0x11f150 (~13KB)
std::vector<AssemblerInstr> Compiler::compile(const std::string& source) {
    // Step 1: Reset messages, set up callbacks
    messages_.reset();                                          // 0x11f179

    // Lock progress callback
    // auto progress = progressCallback_.lock();
    // if (progress) progress->setProgress(0.0);

    // Reset TLS counters
    // TLS+0x40 = 0 (Asm nextID)
    // TLS+0x44 = 0 (Node nodeId, conditional)

    // Step 2: Normalize line endings
    std::string normalized = unifyLineEndings(source);          // 0x11f268

    // Step 3: Parse → shared_ptr<Expression>
    auto expr = parse(normalized);                              // 0x11f27e

    // Step 4: (Optional debug) Print old AST
    // if (config_->debugFlags & 0x02)                          // 0x11f376
    //     printAST(expr, "...");

    // Step 5: Construct StaticResources with warning callback,            // 0x11f66f
    // then init with device-specific constants.
    // Binary: allocate_shared<StaticResources>(alloc,
    //   bind(&CompilerMessageCollection::warningMessage, &messages_, _1, -1))
    auto warningCb = [this](std::string const& msg) {
        messages_.warningMessage(msg, -1);
    };
    auto staticResources = std::make_shared<StaticResources>(
        std::function<void(std::string const&)>(warningCb));
    staticResources->init(*config_, *deviceConstants_);                  // 0x11f6c5

    // Step 5b: Wrap in GlobalResources (adds TLS register/label counters)  // 0x11f6df
    auto resources = std::make_shared<GlobalResources>(
        std::static_pointer_cast<Resources>(staticResources));

    // Step 5c: Store resources into customFunctions_->resources_            // 0x11f6f2
    customFunctions_->resources_ = std::static_pointer_cast<Resources>(resources);

    // Step 5d: Clear reserved field (binary writes 0 to [r15+0x18])        // 0x11f76e
    reserved18_ = 0;

    // Step 6: Convert to SeqC AST                                          // 0x11f7b0
    auto seqcAst = toSeqCAst(expr);

    // Step 7: (Optional debug) Print SeqC AST                              // 0x11f7da
    if (config_->debugFlags & 0x04) {
        printSeqCAst(*seqcAst);
    }

    // Step 8: Frontend lowering                                            // 0x11f911
    // Binary reads config_->unknown_98 (offset 0x98) as channelGrouping int
    auto lowerResult = FrontEndLoweringFacade::lower(
        std::static_pointer_cast<Resources>(resources),
        *seqcAst,
        messages_,
        asmCommands_,
        customFunctions_,
        waveformGen_,
        wavetable_,
        config_->channelGrouping);                                       // [config+0x98]

    // Step 8b: Store lowered AST into Compiler.ast_                        // 0x11f92f
    ast_ = std::move(lowerResult.astResult);

    // seqcAst destroyed here (shared_ptr dtor)                             // 0x11faec

    // Step 9: Check for errors                                             // 0x11fb0d
    if (messages_.hadCompilerError()) {
        return {};
    }

    // Step 10: Build assembly preamble                                     // 0x11fb1a
    // 10a: Reset wavetableFrontIndex on asmCommands
    asmCommands_->setWavetableFrontIndex(0);                                // [rbx+0x50] = 0

    // 10b: Generate "start" label and asmLoadPlaceholder
    std::string startLabel = Resources::newLabel("\nstart");                 // 0x11fb50
    auto labelAsm = asmCommands_->asmLabel(startLabel);                     // 0x11fb66

    // 10c: Initialize asmList_ with the label entry                         // 0x11fc20
    {
        std::vector<AsmList::Asm> initEntries;
        initEntries.push_back(std::move(labelAsm));
        asmList_ = AsmList(std::move(initEntries));
    }

    // 10d: Generate load placeholder
    auto placeholderAsm = asmCommands_->asmLoadPlaceholder();               // 0x11fd78

    // Step 10e: Build root node from lowered AST                            // 0x11fd7d
    std::shared_ptr<Node> rootNode;
    bool hasMainAndAst = resources->hasMain() && (ast_ != nullptr);          // 0x11fd84

    // Create a wrapper Node(NodeType::Entry, placeholderAsm.sequenceId,
    //                       config_->numChannelGroups)
    // Both branches create the same kind of node — the difference is in
    // how the lowered AST is grafted.
    rootNode = std::make_shared<Node>(
        NodeType::Load,
        config_->numChannelGroups,
        placeholderAsm.sequenceId);                                          // 0x11fdc8 / 0x11fe8e

    if (hasMainAndAst) {
        // Graft the lowered AST as rootNode's next chain                   // 0x11fe43
        rootNode->next = ast_;
        ast_ = rootNode;
    } else {
        // Use evalResult's node tree                                       // 0x11ff09
        if (lowerResult.evalResult) {
            rootNode->next = lowerResult.evalResult->node_;
        }
        ast_ = rootNode;
    }

    // Step 11: Walk node tree setting parent pointers                       // 0x11ff85
    // Use a deque for BFS traversal
    {
        std::deque<std::shared_ptr<Node>> worklist;
        worklist.push_back(rootNode);

        while (!worklist.empty()) {
            auto current = worklist.back();                                  // 0x120020
            worklist.pop_back();

            if (!current) continue;

            // Process current->next: set parent, push to worklist
            if (current->next) {                                             // 0x120129
                current->next->parent = current;                             // 0x12014b
                worklist.push_back(current->next);
            }

            // Process current->branches: push each child
            for (auto& child : current->branches) {                          // 0x120208
                if (!child) continue;
                child->parent = current;                                     // 0x120250
                worklist.push_back(child);
            }

            // Process current->loop: set parent, push
            if (current->loop) {                                             // 0x120310
                current->loop->parent = current;                             // 0x120332
                worklist.push_back(current->loop);
            }
        }
    }

    // Step 11b: Append placeholder Asm to asmList_                          // 0x120471
    asmList_.append(placeholderAsm);

    // Step 11c: Insert EvalResults assemblers into asmList_                  // 0x120549
    if (lowerResult.evalResult) {
        auto& assemblers = lowerResult.evalResult->assemblers_;
        asmList_.entries.insert(
            asmList_.entries.end(),
            assemblers.begin(),
            assemblers.end());
    }

    // Step 11d: Append end + wwvf + nop trailer                             // 0x120589
    {
        auto endAsm = asmCommands_->end();                                   // 0x120589
        auto wwvfAsm = asmCommands_->wwvf();                                // 0x12059f
        auto nopAsm = asmCommands_->nop();                                   // 0x1205b8

        // Build a combined entry with end's fields and insert 3 entries
        asmList_.append(endAsm);
        asmList_.append(wwvfAsm);
        asmList_.append(nopAsm);
    }

    // Step 12: Pre-waveform optimization                                // 0x120707
    // Construct AsmOptimize with error/info callbacks bound to messages_
    auto errorCb = [this](std::string const& msg, int line) {
        messages_.errorMessage(msg, line);
    };
    auto infoCb = [this](std::string const& msg, int line) {
        messages_.infoMessage(msg, line);
    };
    auto cancelLocked = cancelCallback_.lock();
    AsmOptimize optimizer(
        std::function<void(const std::string&, int)>(errorCb),
        std::function<void(const std::string&, int)>(infoCb),
        static_cast<uint32_t>(deviceConstants_->registerDepth),             // +0x28
        static_cast<uint32_t>(config_->unknown_88),                       // +0x88
        cancelLocked);

    optimizer.prepareResources(asmList_);                                 // 0x120857
    asmList_ = optimizer.optimizePreWaveform(asmList_);                   // 0x120879

    // Step 13: Conditional serialize to file (if string_30_owned)         // 0x120953
    if (config_->string_30_owned) {
        auto serialized = asmList_.serialize();
        // Write serialized to config_->string_30 path (debug dump)
        // TODO: implement file write if needed for diff testing
    }

    // Step 13b: Conditional serialize/deserialize round-trip              // 0x1209a1
    if (config_->unknown_28 == 1) {
        auto serialized = asmList_.serialize();
        asmList_.deserialize(serialized);
    }

    // Step 13c: Construct WavetableIR from WavetableFront                // 0x120c92
    auto wavetableIR = std::allocate_shared<WavetableIR>(
        std::allocator<WavetableIR>(),
        *wavetable_,
        *deviceConstants_,
        detail::AddressImpl<uint32_t>(config_->addressImpl),
        static_cast<size_t>(config_->wavetableSize),
        config_->searchPath,
        cancelCallback_);

    // Step 14: Run prefetcher                                            // 0x120d60
    runPrefetcher(wavetableIR, asmList_, asmCommands_,
                  placeholderAsm, *deviceConstants_, *config_);

    // Step 15: Post-waveform optimization                                // 0x120e2d
    asmList_ = optimizer.optimizePostWaveform(asmList_);

    // Step 16: Insert unsyncCervino (platform-specific)                  // 0x120f2b
    {
        AsmList unsyncEntries = asmCommands_->unsyncCervino();
        // Insert unsync entries into asmList_ before end
        for (auto& entry : unsyncEntries) {
            asmList_.push_back(std::move(entry));
        }
    }

    // Step 17: (Optional debug) Print final assembly                     // 0x1212db
    if (config_->debugFlags & 0x08) {
        asmList_.print(true, std::cout, true);
    }

    // Step 18: Final error check
    if (messages_.hadCompilerError()) {                          // 0x1212e4
        return {};
    }

    // Step 19: Build output vector<AssemblerInstr>                       // 0x121345
    std::vector<AssemblerInstr> result;
    result.reserve(asmList_.size());
    for (auto& entry : asmList_) {
        result.push_back(entry.assembler);
    }

    return result;
}

// ============================================================================
// runPrefetcher
// ============================================================================

// 0x11dff0 (~2.8KB)
void Compiler::runPrefetcher(std::shared_ptr<WavetableIR> wavetableIR,
                             const AsmList& asmList,
                             std::shared_ptr<AsmCommands> asmCommands,
                             AsmList::Asm placeholder,
                             const DeviceConstants& deviceConstants,
                             const AWGCompilerConfig& config) {
    // Step 1: (Optional) Serialize WavetableIR to JSON file              // 0x11e022
    // if (config.string_50_owned) {
    //     auto json = wavetableIR->toJson();
    //     // writeFile(config.string_50, boost::json::serialize(json));
    // }

    // Step 2: (Optional) Reload WavetableIR from JSON (round-trip)       // 0x11e09a
    // if (config.unknown_28 == 1) {
    //     auto json = wavetableIR->toJson();
    //     wavetableIR = WavetableIR::fromJson(json, deviceConstants,
    //         detail::AddressImpl<uint32_t>(config.addressImpl),
    //         static_cast<size_t>(config.wavetableSize),
    //         config.searchPath, cancelCallback_);
    // }

    // Step 3: Construct Prefetch object                                  // 0x11e1a9
    auto warningCb = [this](std::string const& msg) {
        messages_.warningMessage(msg, -1);
    };

    Prefetch prefetch(config, deviceConstants, asmCommands,
                      ast_, wavetableIR,
                      std::function<void(std::string const&)>(warningCb),
                      cancelCallback_);

    // Step 4: preparePlays()                                             // 0x11e367
    prefetch.preparePlays();

    // Step 5: getUsedWavesForDevice → setUsedWaveforms                   // 0x11e36c
    auto waves = prefetch.getUsedWavesForDevice(
        static_cast<size_t>(config.deviceIndex));
    wavetableIR->setUsedWaveforms(waves);

    // Step 6: assignWaveIndexImplicit                                    // 0x11e3fc
    wavetableIR->assignWaveIndexImplicit();

    // Step 7: alignWaveformSizes                                         // 0x11e40b
    wavetableIR->alignWaveformSizes();

    // Step 8: assignWaveformAllocationSizes                              // 0x11e413
    wavetableIR->assignWaveformAllocationSizes();

    // Step 9: (Conditional) determineFixedWaves                          // 0x11e418
    if (config.cacheType == 1) {
        prefetch.determineFixedWaves();
    }

    // Step 10: updateWaveforms                                           // 0x11e432
    wavetableIR->updateWaveforms(config.cacheType != 0 && config.isHirzel,
                                  false);

    // Step 11: placeLoads                                                // 0x11e44f
    prefetch.placeLoads();

    // Step 12: (Optional debug) Print tree                               // 0x11e454
    if (config.debugFlags & 0x08) {
        prefetch.print(nullptr, 0);
    }

    // Step 13: fillInPlaceholders                                        // 0x11e4fc
    asmList_ = prefetch.fillInPlaceholders(asmList);

    // Step 15-16: Store channel info from prefetch                       // 0x11e5bd
    channelCount_ = prefetch.getUsedChannels();
    channelMode_ = prefetch.getUsedFourChannelMode() ? 1 : 0;
}

// ============================================================================
// Getters / Setters
// ============================================================================

// 0x123480
void Compiler::setCancelCallback(std::weak_ptr<CancelCallback> cb) {
    cancelCallback_ = std::move(cb);
    // Also propagates to sub-object at *(this+0xC0)+0xB0
    // (likely WaveformGenerator's cancel callback)
}

// 0x123510
void Compiler::setProgressCallback(std::weak_ptr<ProgressCallback> cb) {
    progressCallback_ = std::move(cb);
}

// 0x123550
const std::vector<NodeMapItem>* Compiler::getNodeAccessList() const {
    if (!customFunctions_) return nullptr;
    return &customFunctions_->nodeList();
}

// 0x123570
const std::unordered_map<NodeMapItem, std::set<AccessMode>>* Compiler::getNodeToModeMap() const {
    if (!customFunctions_) return nullptr;
    return &customFunctions_->accessModeMap();
}

// 0x123590
std::vector<int> Compiler::getChannelInfo() const {
    std::vector<int> result;
    result.push_back(static_cast<int>(channelCount_));
    result.push_back(static_cast<int>(channelMode_));
    return result;
}

// 0x1235e0
bool Compiler::usedDeviceSampleRate() const {
    return usedSampleRate_ != 0;
}

bool Compiler::hadSyntaxError() const {
    // Binary reads byte at Compiler+0x100+0x03 = parserContext_[3]
    // which is SeqcParserContext::hadSyntaxError flag
    auto* ctx = reinterpret_cast<SeqcParserContext const*>(parserContext_);
    return ctx->hadSyntaxError();
}

// 0x1235f0
std::vector<CompilerMessage> Compiler::getCompileMessages() const {
    return messages_.messages();
}

// 0x123640
void Compiler::setLineNr(int nr) {
    lineNr_ = nr;
    // Propagate to AsmCommands at *(this+0xB0)+0x50
    // Also tail-calls WavetableFront::setLineNr on *(this+0xA0)
}

// 0x123660
std::vector<int> Compiler::getLineMap(int offset) const {
    std::vector<int> result;
    int counter = 0;
    int seq = 1;

    // Iterate asmList_ entries (stride 0xA8)
    // for (auto& entry : asmList_) {
    //     if (entry.assembler.cmd == -1)
    //         continue;
    //     if (entry.assembler.cmd == Assembler::LABEL) {
    //         seq++;
    //         continue;
    //     }
    //     result.push_back(counter + offset);
    //     result.push_back(counter);
    //     result.push_back(seq);
    //     result.push_back(entry.lineNumber);  // +0x88 in AsmList::Asm
    //     counter++;
    //     seq++;
    // }

    return result;
}

// ============================================================================
// FrontEndLoweringFacade
// ============================================================================

// 0x1c1da0 (~1KB)
// Thin facade: packs arguments into FrontendLoweringContext, creates empty
// FrontendLoweringState, dispatches to SeqCAstNode vtable[0] virtual
// (the 3-arg evaluate: Resources, Context&, State&).
// Returns {state.result (shared_ptr<Node>), evaluate_output (shared_ptr<EvalResults>)}.
//
// CORRECTION 2026-04-23 (Phase 15a-i): return type changed from void to
// LowerResult (32B sret = 2 shared_ptrs). Evidence:
//   - sret save at 0x1c1dba: mov [rbp-0xa0], rdi
//   - 0x1c1fb6: movups [r15], [rbp-0x90]     → sret[0] = state.result (shared_ptr<Node>)
//   - 0x1c1fcc: movups [r15+0x10], [rbp-0x50] → sret[1] = evaluate output (shared_ptr<EvalResults>)
//   - Caller @0x11f92f: movups [r15+0x28], sret[0]  → Compiler.ast_ = lowered AST root
FrontEndLoweringFacade::LowerResult FrontEndLoweringFacade::lower(
    std::shared_ptr<Resources> resources,
    SeqCAstNode& ast,
    CompilerMessageCollection& messages,
    std::shared_ptr<AsmCommands> asmCommands,
    std::shared_ptr<CustomFunctions> customFunctions,
    std::shared_ptr<WaveformGenerator> waveformGen,
    std::shared_ptr<WavetableFront> wavetable,
    int channelGrouping)
{
    // 1. Build FrontendLoweringContext on stack (holds shared_ptrs + int)
    FrontendLoweringContext context;
    context.messages = &messages;
    context.asmCommands = std::move(asmCommands);
    context.customFunctions = std::move(customFunctions);
    context.waveformGen = std::move(waveformGen);
    context.wavetable = std::move(wavetable);
    context.channelGrouping = channelGrouping;

    // 2. Create empty FrontendLoweringState on stack
    FrontendLoweringState state;

    // 3. Virtual dispatch: ast.vtable[0](sret, ast, resources, context, state)
    //    This is the core compilation — the AST node polymorphically
    //    generates assembly instructions via the context.
    //    Returns shared_ptr<EvalResults> via sret.
    auto evaluateOutput = ast.evaluate(resources, context, state);              // @0x1c1f60

    // 4. Return {state.result, evaluate_output}
    LowerResult result;
    result.astResult = std::move(state.result);
    result.evalResult = std::move(evaluateOutput);
    return result;
}

}  // namespace zhinst
