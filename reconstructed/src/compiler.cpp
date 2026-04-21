// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Compiler — main SeqC compilation orchestrator
//
// Key methods: compile() is the master pipeline (~13KB), runPrefetcher()
// orchestrates the waveform prefetch pass (~2.8KB).
// ============================================================================

#include "zhinst/compiler.hpp"
#include "zhinst/seqc_ast_node.hpp"
#include "zhinst/expression.hpp"

#include <deque>
#include <sstream>
#include <boost/algorithm/string/replace.hpp>

// Forward declarations for flex/bison parser
extern "C" {
    int seqc_lex_init_extra(void* extra, void** scanner);  // 0x2ca0c0
    void* seqc__scan_string(const char* str, void* scanner);  // 0x2c9dd0
    int seqc_parse(void* ctx, void** result, void* scanner);  // 0x2ca2a0
    void seqc__delete_buffer(void* buf, void* scanner);  // 0x2c9a90
    int seqc_lex_destroy(void* scanner);  // 0x2ca120
}

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
    // Create AsmCommands (allocated as 0x80 bytes)
    // asmCommands_ = std::make_shared<AsmCommands>(...);

    // Create WaveformGenerator (allocated as 0xE0 bytes)
    // waveformGen_ = std::make_shared<WaveformGenerator>(...);

    // Create CustomFunctions (allocated as 0x200 bytes)
    // customFunctions_ = std::make_shared<CustomFunctions>(
    //     config, deviceConstants, wavetable_, waveformGen_, asmCommands_, ...);

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

    void* buf = seqc__scan_string(source.c_str(), scanner);

    Expression* rawResult = nullptr;
    seqc_parse(ctx, reinterpret_cast<void**>(&rawResult), scanner);

    seqc__delete_buffer(buf, scanner);
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

    // Step 5: Init static resources
    // StaticResources::init(*config_, *deviceConstants_);      // 0x11f6c5

    // Step 6: Convert to SeqC AST
    // auto seqcAst = toSeqCAst(expr);                          // 0x11f7b0

    // Step 7: (Optional debug) Print SeqC AST
    // if (config_->debugFlags & 0x04)                          // 0x11f81a
    //     printSeqCAst(*seqcAst);

    // Step 8: Frontend lowering
    // FrontEndLoweringFacade::lower(resources, *seqcAst,       // 0x11f911
    //     messages_, asmCommands_, customFunctions_,
    //     waveformGen_, wavetable_, config_->numChannelGroups);

    // Step 9: Check for errors
    if (messages_.hadCompilerError()) {                         // 0x11fb0d
        // Bail out
        return {};
    }

    // Step 10: Build assembly preamble
    // resources->newLabel() → asmCommands_->asmLabel()         // 0x11fb50
    // asmCommands_->asmLoadPlaceholder()                       // 0x11fd78

    // Step 11: Linearize nodes into assembly list
    // Build deque<shared_ptr<Node>> from the lowered tree       // 0x11ffa4
    // For each node: emit assembler instructions                // 0x120498
    // Append end/wwvf/nop

    // Step 12: Pre-waveform optimization
    // AsmOptimize optimizer(...);
    // asmList_ = optimizer.optimizePreWaveform(asmList_);       // 0x120879

    // Step 13: Serialize/deserialize round-trip (validation)
    // auto serialized = asmList_.serialize();                   // 0x120966
    // asmList_ = AsmList::deserialize(serialized);

    // Step 14: Run prefetcher
    // runPrefetcher(wavetableIR, asmList_, asmCommands_,        // 0x120d60
    //     placeholder, *deviceConstants_, *config_);

    // Step 15: Post-waveform optimization
    // asmList_ = optimizer.optimizePostWaveform(asmList_);      // 0x120e2d

    // Step 16: Insert unsyncCervino (platform-specific)
    // asmCommands_->unsyncCervino(...)                          // 0x120f2b

    // Step 17: (Optional debug) Print final assembly
    // if (config_->debugFlags & 0x08)                           // 0x1212db
    //     asmList_.print();

    // Step 18: Final error check
    if (messages_.hadCompilerError()) {                          // 0x1212e4
        return {};
    }

    // Step 19: Build output vector<Assembler>
    std::vector<AssemblerInstr> result;                          // 0x121345
    // Reserve and copy from asmList_ entries
    // Each AsmList::Asm entry's AssemblerInstr is copied to the output

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
    // Step 1: (Optional) Serialize WavetableIR to JSON file
    // if (config[0x68] == 1):                                   // 0x11e022
    //     auto json = wavetableIR->toJson();
    //     writeFile(config.string_50, boost::json::serialize(json));

    // Step 2: (Optional) Reload WavetableIR from JSON (round-trip)
    // if (config[0x28] == 1):                                   // 0x11e09a
    //     auto json = wavetableIR->toJson();
    //     wavetableIR = WavetableIR::fromJson(json, deviceConstants, ...);

    // Step 3: Construct Prefetch object
    // Creates warning callback wrapping CompilerMessageCollection::warningMessage
    // Prefetch prefetch(config, deviceConstants, asmCommands,    // 0x11e1a9
    //     rootNode, wavetableIR, warningFunc, cancelCallback_);

    // Step 4: preparePlays()                                     // 0x11e367

    // Step 5: getUsedWavesForDevice → setUsedWaveforms          // 0x11e36c
    // int deviceIndex = config[0x24];
    // auto waves = prefetch.getUsedWavesForDevice(deviceIndex);
    // wavetableIR->setUsedWaveforms();

    // Step 6: assignWaveIndexImplicit                            // 0x11e3fc
    // wavetableIR->assignWaveIndexImplicit();

    // Step 7: alignWaveformSizes                                 // 0x11e40b
    // wavetableIR->alignWaveformSizes();

    // Step 8: assignWaveformAllocationSizes                      // 0x11e413
    // wavetableIR->assignWaveformAllocationSizes();

    // Step 9: (Conditional) determineFixedWaves                  // 0x11e418
    // if (config[0x19] == 1)
    //     prefetch.determineFixedWaves();

    // Step 10: updateWaveforms                                   // 0x11e432
    // wavetableIR->updateWaveforms(config[0x19] & config[0x18], flag);

    // Step 11: placeLoads                                        // 0x11e44f
    // prefetch.placeLoads();

    // Step 12: (Optional debug) Print tree                       // 0x11e454
    // if (config.debugFlags & 0x08)
    //     prefetch.print(nullptr, 0);

    // Step 13: fillInPlaceholders                                // 0x11e4fc
    // prefetch.fillInPlaceholders(asmList);

    // Step 14: Copy result AsmList                               // 0x11e501

    // Step 15-16: Store channel info from prefetch               // 0x11e5bd
    // this->channelCount_ = prefetch.getUsedChannels();
    // this->channelMode_ = prefetch.getUsedFourChannelMode();

    // Exception handling: catches zhinst::Exception → rethrows as
    // CompilerException. Catches std::exception → reports via
    // CompilerMessageCollection::errorMessage.
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
const void* Compiler::getNodeAccessList() const {
    // Returns pointer into customFunctions_ sub-object
    // return reinterpret_cast<const char*>(customFunctions_.get()) + 0x150;
    return nullptr;  // placeholder
}

// 0x123570
const void* Compiler::getNodeToModeMap() const {
    // Returns pointer into customFunctions_ sub-object
    // return reinterpret_cast<const char*>(customFunctions_.get()) + 0x128;
    return nullptr;  // placeholder
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
// FrontendLoweringState, dispatches to SeqCAstNode virtual method (vtable[0]).
void FrontEndLoweringFacade::lower(
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
    //    Contains: asmCommands, customFunctions, waveformGen, wavetable, channelGrouping

    // 2. Create empty FrontendLoweringState on stack (vector<string>)

    // 3. Virtual dispatch: ast.lower(output, resources, context, state)
    //    This is the core compilation — the AST node polymorphically
    //    generates assembly instructions via the context.

    // 4. Copy results from output buffer to return value

    // 5. Cleanup locals
}

}  // namespace zhinst
