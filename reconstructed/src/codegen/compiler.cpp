// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Compiler — main SeqC compilation orchestrator
//
// Key methods: compile() is the master pipeline (~13KB), runPrefetcher()
// orchestrates the waveform prefetch pass (~2.8KB).
// ============================================================================

#include "zhinst/codegen/compiler.hpp"
#include "zhinst/core/compiler_message.hpp"
#include "zhinst/ast/frontend_lowering.hpp"
#include "zhinst/ast/seqc_ast_node.hpp"
#include "zhinst/ast/seqc_parser_context.hpp"
#include "zhinst/ast/expression.hpp"
#include "zhinst/ast/node.hpp"
#include "zhinst/ast/eval_results.hpp"
#include "zhinst/asm/asm_commands.hpp"
#include "zhinst/waveform/waveform_generator.hpp"

#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string/replace.hpp>
#include "zhinst/asm/asm_optimize.hpp"
#include "zhinst/waveform/wavetable_ir.hpp"
#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/codegen/prefetch.hpp"
#include "zhinst/asm/address_impl.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/runtime/resources.hpp"

#include "zhinst/ast/seqc_parser_fwd.hpp"
#include "zhinst/asm/asm_parser_fwd.hpp"

namespace zhinst {

// Free functions used in pipeline
extern std::shared_ptr<SeqCAstNode> toSeqCAst(std::shared_ptr<Expression> expr);
extern void printSeqCAst(const SeqCAstNode& ast);

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
    , usedFourChannelMode_(0)
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

    // Wire SeqcParserContext error callback → messages_.parserMessage(lineNr, msg)
    // Binary: Compiler::Compiler+0x2cb calls setErrorCallback with lambda:
    //   [this](int lineNr, const string& msg) { messages_.parserMessage(lineNr, msg); }
    // The lambda body disassembles to: mov 0x8(%rdi),%rdi; mov (%rsi),%esi;
    //   add $0x38,%rdi; jmp CompilerMessageCollection::parserMessage
    // (0x38 = offset of messages_ in Compiler; rsi=int& lineNr, rdx=string&)
    parserContext_.setErrorCallback(
        [this](int lineNr, const std::string& msg) {
            messages_.parserMessage(lineNr, msg);
        });

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
    // T5b.2 — clear lifted cross-step state so a fresh compile() starts
    // from a clean slate.  The binary does not clear these fields here
    // because they do not exist in the binary; this is a recon-local
    // extension that the seqcc driver depends on (the driver calls
    // step methods in sequence and must not see stale state from a
    // previous compile).
    compileExpr_.reset();
    compileSeqcAst_.reset();
    compileLowerResult_ = FrontEndLoweringFacade::LowerResult{};
    compileStaticResources_.reset();
    compileResources_.reset();
    compilePlaceholderAsm_ = AsmList::Asm{};
    compileRootNode_.reset();
    compileWavetableIR_.reset();
    compileOptimizer_.reset();
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
    auto* ctx = &parserContext_;

    // Reset parser context at this+0x100                               // @0x11d9d7
    ctx->reset();  // 0x247cc0

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

    // Check for syntax errors                                           // @0x11da44
    // Binary: calls hadSyntaxError() at parse+148; throws CompilerException
    // "Syntax error while parsing seqC" (rodata 0x8ffdec) if true.
    if (ctx->hadSyntaxError()) {
        throw CompilerException("Syntax error while parsing seqC");
    }

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
    // @0x122640: sets cout formatting, prints label, then recursive tree dump.
    // ~3.5KB of debug printing code. Reconstructed from call targets:
    //   str(EOperationType) @0x122689, str(EOperator) @0x122706,
    //   str(ECommandType) @0x122a10, str(VarType) @0x122b13,
    //   ostream << int @0x122796, recursive call @0x12290b.
    if (!expr) return;                                                         // @0x122670

    std::cout << label << " " << str(expr->operationType);                     // @0x122689..1226b0
    if (expr->operator_ != EOperator::eNONE) {
        std::cout << " op=" << str(expr->operator_);                           // @0x122706
    }
    std::cout << " line=" << expr->lineNumber;                                 // @0x122788..122796
    if (expr->commandType != ECommandType::eNOCMD) {
        std::cout << " cmd=" << str(expr->commandType);                        // @0x122a10
    }
    if (expr->varType != VarType_Unset) {
        std::cout << " vt=" << str(expr->varType);                             // @0x122b13
    }
    if (!expr->name.empty()) {
        std::cout << " name=\"" << expr->name << "\"";                         // @0x122aa7..122ad5
    }
    std::cout << "\n";

    // Recurse into children                                                   // @0x12290b
    for (size_t i = 0; i < expr->children.size(); ++i) {
        std::string childLabel = label + "  ";
        printAST(expr->children[i], childLabel);                               // @0x12290b
    }
}

// ============================================================================
// compile — master pipeline
// ============================================================================

// 0x11f150 (~13KB)
//
// T5b.3: the body has been split into the 9 named step methods below;
// `compile()` is now the canonical sequence.  Behaviour is identical
// to the pre-T5b.3 monolithic body (verified via diff_test_fast 1612
// + test_seqcc_diff 46 + test_seqcc_ab 15).
CompileResult Compiler::compile(const std::string& source) {
    if (auto earlyOut = stepParse(source)) {
        return std::move(*earlyOut);
    }
    stepToSeqCAst();
    stepLower();
    stepBuildAsmPreamble();
    stepOptPre();
    stepPrefetch();
    stepOptPost();
    stepUnsyncCervino();
    return stepProject();
}

// ============================================================================
// T5b.3 — pipeline step methods
// ============================================================================
//
// These are the named partitions of the original `Compiler::compile()`
// body.  Each step boundary lands on a verified binary address (see
// IF-306 for the cross-reference table).  Step methods read from /
// write to the lifted `compile*_` members so intermediate state is
// observable between them; the seqcc driver (T5b.5) calls them
// individually after T5b.4 promotes them to public visibility.

// ----------------------------------------------------------------------------
// stepParse — steps 1-3b (entry @0x11f150, parse @0x11f27e, early-out @0x11f5b1)
// ----------------------------------------------------------------------------
std::optional<CompileResult> Compiler::stepParse(const std::string& source) {
    // --- 1. Reset messages, set up callbacks ---
    messages_.reset();                                          // 0x11f179

    // T5b.2 — also clear lifted cross-step state from any previous
    // run.  The binary has no equivalent because these fields are
    // recon-local (see header for the rationale).
    compileExpr_.reset();
    compileSeqcAst_.reset();
    compileLowerResult_ = FrontEndLoweringFacade::LowerResult{};
    compileStaticResources_.reset();
    compileResources_.reset();
    compilePlaceholderAsm_ = AsmList::Asm{};
    compileRootNode_.reset();
    compileWavetableIR_.reset();
    compileOptimizer_.reset();

    // Lock progress callback
    // auto progress = progressCallback_.lock();
    // if (progress) progress->setProgress(0.0);

    // Reset TLS counters
    // TLS+0x40 = 0 (Asm nextID)
    // TLS+0x44 = 0 (Node nodeId, conditional)

    // --- 2. Normalize line endings ---
    std::string normalized = unifyLineEndings(source);          // 0x11f268

    // --- 3. Parse → shared_ptr<Expression> ---
    compileExpr_ = parse(normalized);                           // 0x11f27e

    // --- 3b. Early-out for empty input (parser returned null Expression*) ---
    // Binary @0x11f283: test r14, r14; je 0x11f557 — when the raw parse
    // result is null, jump to a short alternate path that allocates an
    // empty WavetableIR and returns immediately, without running label /
    // placeholder / trailer emit.  The returned CompileResult has an empty
    // asmList; downstream `AWGCompilerImpl::writeToStream` then sees an
    // empty opcode vector and throws ZIAWGCompilerException(EmptyInput =
    // "nothing to write, empty input").  See IF-155.
    if (!compileExpr_) {                                        // 0x11f28a..0x11f28d
        // Binary @0x11f5b1: same allocate_shared<WavetableIR>(...) as the
        // non-null path at 0x11f348.
        compileWavetableIR_ = std::allocate_shared<WavetableIR>(
            std::allocator<WavetableIR>(),
            *wavetable_,
            *deviceConstants_,
            detail::AddressImpl<uint32_t>(config_->addressImpl),
            static_cast<size_t>(config_->wavetableSize),
            config_->searchPath,
            cancelCallback_);
        return CompileResult{std::vector<Assembler>{}, compileWavetableIR_};
    }

    // --- 4. (Optional debug) Print old AST ---
    // if (config_->debugFlags & 0x02)                          // 0x11f376
    //     printAST(compileExpr_, "...");

    return std::nullopt;
}

// ----------------------------------------------------------------------------
// stepToSeqCAst — steps 5-6 (@0x11f66f .. @0x11f7b0)
// ----------------------------------------------------------------------------
void Compiler::stepToSeqCAst() {
    // --- 5/5b/5c/5d. Build per-compile resource scaffolding (0x11f66f .. 0x11f76e) ---
    // Factored into setupResources() so the T6 seqcc --from=asm driver
    // can call it independently before re-entering at stepOptPre.
    setupResources();

    // --- 6. Convert to SeqC AST (0x11f7b0) ---
    compileSeqcAst_ = toSeqCAst(compileExpr_);
}

// ----------------------------------------------------------------------------
// setupResources — T6 (IF-307) factor of stepToSeqCAst steps 5/5b/5c/5d.
// Code move only; called from the top of stepToSeqCAst so the binary
// pipeline path is unchanged.  Also a public entry point for the seqcc
// driver, which calls it directly when handling --from=<stage>.
// ----------------------------------------------------------------------------
void Compiler::setupResources() {
    // --- 5. Construct StaticResources with warning callback (0x11f66f) ---
    // then init with device-specific constants.
    // Binary: allocate_shared<StaticResources>(alloc,
    //   bind(&CompilerMessageCollection::warningMessage, &messages_, _1, -1))
    auto warningCb = [this](std::string const& msg) {
        messages_.warningMessage(msg, -1);
    };
    compileStaticResources_ = std::make_shared<StaticResources>(
        std::function<void(std::string const&)>(warningCb));
    compileStaticResources_->init(*config_, *deviceConstants_);  // 0x11f6c5

    // --- 5b. Wrap in GlobalResources (adds TLS register/label counters) (0x11f6df) ---
    compileResources_ = std::make_shared<GlobalResources>(
        std::static_pointer_cast<Resources>(compileStaticResources_));

    // --- 5c. Store resources into customFunctions_->resources_ (0x11f6f2) ---
    customFunctions_->resources_ = std::static_pointer_cast<Resources>(compileResources_);

    // --- 5d. Clear reserved field (binary writes 0 to [r15+0x18]) (0x11f76e) ---
    reserved18_ = 0;
}

// ----------------------------------------------------------------------------
// setAsmList / setPlaceholderAsm — T6 (IF-307) driver-only seeders for
// --from=asm.  No binary counterpart.
// ----------------------------------------------------------------------------
void Compiler::setAsmList(AsmList list) {
    asmList_ = std::move(list);
}

void Compiler::setPlaceholderAsm(AsmList::Asm asm_) {
    compilePlaceholderAsm_ = std::move(asm_);
}

// ----------------------------------------------------------------------------
// stepLower — steps 7-9 (@0x11f7da .. @0x11fb0d)
// ----------------------------------------------------------------------------
void Compiler::stepLower() {
    // The binary (libc++ ABI) handles null seqcAst gracefully: the libc++
    // shared_ptr does not assert on null dereference. In the recon (libstdc++),
    // operator* asserts non-null. Guard here: skip steps 7-8 if null.
    //
    // The null-seqcAst path is reached only when the parser returned an
    // empty AST (typically empty source); a *syntax error* during parse
    // would have already thrown CompilerException("Syntax error while
    // parsing seqC") inside parse() at compiler.cpp:175-177, gated on
    // SeqcParserContext::hadSyntaxError_ (a separate flag from
    // messages_.hadError_; see IF-233 for the rationale).  So when we
    // reach this point with a null seqcAst, parse-time errors are not
    // the cause; lowering is simply skipped and step 9 below
    // (messages_.hadCompilerError()) gates only post-parse errors
    // (AsmCommands, evaluation, etc.) reported via errorMessage().
    // Binary: at +1644 tests seqcAst raw ptr; if null, continues past refcount
    // incr into FrontEndLoweringFacade::lower (libc++ handles null shared_ptr
    // deref differently from libstdc++).
    if (compileSeqcAst_) {
        // --- 7. (Optional debug) Print SeqC AST (0x11f7da) ---
        if (config_->debugFlags & 0x04) {
            printSeqCAst(*compileSeqcAst_);
        }

        // --- 8. Frontend lowering (0x11f911) ---
        // Binary reads config_->unknown_98 (offset 0x98) as loopUnrollLimit int
        compileLowerResult_ = FrontEndLoweringFacade::lower(
            std::static_pointer_cast<Resources>(compileResources_),
            *compileSeqcAst_,
            messages_,
            asmCommands_,
            customFunctions_,
            waveformGen_,
            wavetable_,
            config_->loopUnrollLimit);                                   // [config+0x98]

        // --- 8b. Store lowered AST into Compiler.ast_ (0x11f92f) ---
        ast_ = std::move(compileLowerResult_.astResult);
    }

    // compileSeqcAst_ retained on the Compiler until reset(); the binary
    // releases here, but holding it through end-of-compile is benign —
    // the SeqC AST is small and read-only after lowering.        // 0x11faec

    // --- 9. Check for errors (0x11fb0d) ---
    if (messages_.hadCompilerError()) {
        throw CompilerException("Compiler error while evaluating sequence");
    }
}

// ----------------------------------------------------------------------------
// stepBuildAsmPreamble — steps 10-11d (@0x11fb1a .. @0x1205b8)
// ----------------------------------------------------------------------------
void Compiler::stepBuildAsmPreamble() {
    // --- 10. Build assembly preamble (0x11fb1a) ---
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
    compilePlaceholderAsm_ = asmCommands_->asmLoadPlaceholder();             // 0x11fd78

    // --- 10e. Build root node from lowered AST (0x11fd7d) ---
    bool hasMainAndAst = compileResources_->hasMain() && (ast_ != nullptr); // 0x11fd84

    // Create a wrapper Node(NodeType::Entry, placeholderAsm.sequenceId,
    //                       config_->numChannelGroups)
    // Both branches create the same kind of node — the difference is in
    // how the lowered AST is grafted.
    compileRootNode_ = std::make_shared<Node>(
        NodeType::Load,
        compilePlaceholderAsm_.sequenceId,
        config_->numChannelGroups);                                          // 0x11fdc8 / 0x11fe8e

    if (hasMainAndAst) {
        // Graft the lowered AST as compileRootNode_'s next chain          // 0x11fe43
        compileRootNode_->next = ast_;
        ast_ = compileRootNode_;
    } else {
        // Use evalResult's node tree                                       // 0x11ff09
        if (compileLowerResult_.evalResult) {
            compileRootNode_->next = compileLowerResult_.evalResult->node_;
        } else {
        }
        ast_ = compileRootNode_;
    }

    // --- 11. Walk node tree setting parent pointers (0x11ff85) ---
    // Use a deque for BFS traversal
    {
        std::deque<std::shared_ptr<Node>> worklist;
        worklist.push_back(compileRootNode_);

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

    // --- 11b. Append placeholder Asm to asmList_ (0x120471) ---
    asmList_.append(compilePlaceholderAsm_);

    // --- 11c. Insert EvalResults assemblers into asmList_ (0x120549) ---
    if (compileLowerResult_.evalResult) {
        auto& assemblers = compileLowerResult_.evalResult->assemblers_;

        asmList_.entries.insert(
            asmList_.entries.end(),
            assemblers.begin(),
            assemblers.end());
    }

    // --- 11d. Append wwvf + nop + end trailer (0x120589) ---
    // Binary inserts 3 contiguous AsmList::Asm entries in order: wwvf, nop, end
    // (verified from stack layout: wwvf@-0x390, nop@-0x2e8, end@-0x240)
    {
        auto endAsm = asmCommands_->end();                                   // 0x120589
        auto wwvfAsm = asmCommands_->wwvf();                                // 0x12059f
        auto nopAsm = asmCommands_->nop();                                   // 0x1205b8

        asmList_.append(wwvfAsm);
        asmList_.append(nopAsm);
        asmList_.append(endAsm);
    }
}

// ----------------------------------------------------------------------------
// stepOptPre — step 12 + 13 + 13b + 13c (@0x120707 .. @0x120c92)
// ----------------------------------------------------------------------------
void Compiler::stepOptPre() {
    // --- 12. Pre-waveform optimization (0x120707) ---
    // Construct AsmOptimize with error/info callbacks bound to messages_
    auto errorCb = [this](std::string const& msg, int line) {
        messages_.errorMessage(msg, line);
    };
    auto infoCb = [this](std::string const& msg, int line) {
        messages_.infoMessage(msg, line);
    };
    auto cancelLocked = cancelCallback_.lock();
    compileOptimizer_ = std::make_unique<AsmOptimize>(
        std::function<void(const std::string&, int)>(errorCb),
        std::function<void(const std::string&, int)>(infoCb),
        static_cast<uint32_t>(deviceConstants_->registerDepth),             // +0x28
        static_cast<uint32_t>(config_->optimizationFlags),                       // +0x88
        cancelLocked);

    compileOptimizer_->prepareResources(asmList_);                        // 0x120857
    try {
        asmList_ = compileOptimizer_->optimizePreWaveform(asmList_);      // 0x120879
    } catch (OptimizeException const& e) {
        // Binary @0x121d09: catches OptimizeException, calls
        // messages_.errorMessage(e.what(), e.lineNumber()) so that the
        // standard "Compiler Error (line: N): " prefix is added by
        // CompilerMessage::str().  See IF-165.
        messages_.errorMessage(std::string(e.what()), e.lineNumber());
        throw;
    }

    // --- 13. Conditional serialize to file (if debugDumpEnabled) (0x120953) ---
    if (config_->debugDumpEnabled) {
        auto serialized = asmList_.serialize();
        // Binary writes serialized data to config_->debugDumpPath.        // @0x120953
        std::ofstream ofs(config_->debugDumpPath, std::ios::binary);
        if (ofs)
            ofs.write(serialized.data(), serialized.size());
    }

    // --- 13b. Conditional serialize/deserialize round-trip (0x1209a1) ---
    if (config_->serializeRoundTrip == 1) {
        auto serialized = asmList_.serialize();
        asmList_.deserialize(serialized);
    }

    // --- 13c. Construct WavetableIR from WavetableFront (0x120c92) ---
    compileWavetableIR_ = std::allocate_shared<WavetableIR>(
        std::allocator<WavetableIR>(),
        *wavetable_,
        *deviceConstants_,
        detail::AddressImpl<uint32_t>(config_->addressImpl),
        static_cast<size_t>(config_->wavetableSize),
        config_->searchPath,
        cancelCallback_);
}

// ----------------------------------------------------------------------------
// stepPrefetch — step 14 (@0x120d60)
// ----------------------------------------------------------------------------
void Compiler::stepPrefetch() {
    // --- 14. Run prefetcher (0x120d60) ---
    runPrefetcher(compileWavetableIR_, asmList_, asmCommands_,
                  compilePlaceholderAsm_, *deviceConstants_, *config_);
}

// ----------------------------------------------------------------------------
// stepOptPost — step 15 (@0x120e2d)
// ----------------------------------------------------------------------------
void Compiler::stepOptPost() {
    // --- 15. Post-waveform optimization (0x120e2d) ---
    try {
        asmList_ = compileOptimizer_->optimizePostWaveform(asmList_);
    } catch (OptimizeException const& e) {
        // Binary @0x121d09: catches OptimizeException, calls
        // messages_.errorMessage(e.what(), e.lineNumber()).  See IF-165.
        messages_.errorMessage(std::string(e.what()), e.lineNumber());
        throw;
    }
}

// ----------------------------------------------------------------------------
// stepUnsyncCervino — step 16 (@0x120f2b)
// ----------------------------------------------------------------------------
void Compiler::stepUnsyncCervino() {
    // --- 16. Insert unsyncCervino (platform-specific) (0x120f2b) ---
    // Binary at 0x120f08: only called when deviceType == UHFLI(1) or UHFQA(4)
    if (deviceConstants_->deviceType == static_cast<uint32_t>(AwgDeviceType::UHFLI) ||
        deviceConstants_->deviceType == static_cast<uint32_t>(AwgDeviceType::UHFQA)) {
        AsmList unsyncEntries = asmCommands_->unsyncCervino();
        // Insert unsync entries at the FRONT of asmList_ (binary 0x120f5c
        // calls vector::insert at begin(), not push_back)
        auto insertPos = asmList_.begin();
        for (auto& entry : unsyncEntries) {
            insertPos = asmList_.insert(insertPos, std::move(entry));
            ++insertPos;  // advance past just-inserted element
        }
    }
}

// ----------------------------------------------------------------------------
// stepProject — steps 17-19b (@0x1212db .. @0x121421)
// ----------------------------------------------------------------------------
CompileResult Compiler::stepProject() {
    // --- 17. (Optional debug) Print final assembly (0x1212db) ---
    if (config_->debugFlags & 0x08) {
        asmList_.print(true, std::cout, true);
    }

    // --- 18. Final error check ---
    if (messages_.hadCompilerError()) {                          // 0x1212e4
        throw CompilerException("Compiler error while assembling output file");
    }

    // --- 19. Build output vector<Assembler> (0x121345) ---
    std::vector<Assembler> result;
    result.reserve(asmList_.size());
    for (size_t _i = 0; _i < asmList_.entries.size(); _i++) {
        auto& entry = asmList_.entries[_i];
        if (entry.assembler.cmd == Assembler::INVALID)  // @0x1213a2: skip dead instrs
            continue;
        result.push_back(entry.assembler);
    }

    // --- 19b. Cache device-sample-rate flag from StaticResources (0x1213c8) ---
    // Binary @0x1213c8: mov -0x110(%rbp),%rax; movzbl 0xd8(%rax),%eax;
    //                   mov %al,0x25(%r15)
    // i.e. compiler_.usedSampleRate_ = staticResources->usedSampleRate_;
    // This is the only writer of Compiler::usedSampleRate_ in the binary.
    // StaticResources::usedSampleRate_ is the primary — set true inside
    // StaticResources::getValue("DEVICE_SAMPLE_RATE").
    // The Compiler-side field is a cache read by usedDeviceSampleRate(),
    // which is consulted by AWGCompilerImpl when emitting the
    // ".required_sample_rate" ELF section.
    usedSampleRate_ = compileStaticResources_->usedSampleRate();

    return CompileResult{std::move(result), compileWavetableIR_};  // 0x121421: sret+0x18 = wavetableIR (recon retains the member; binary moves out)
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
    // --- 1. (Optional) Serialize WavetableIR to JSON file (0x11e022) ---
    // if (config.debugJsonEnabled) {
    //     auto json = wavetableIR->toJson();
    //     // writeFile(config.debugJsonPath, boost::json::serialize(json));
    // }

    // --- 2. (Optional) Reload WavetableIR from JSON (round-trip) (0x11e09a) ---
    // if (config.serializeRoundTrip == 1) {
    //     auto json = wavetableIR->toJson();
    //     wavetableIR = WavetableIR::fromJson(json, deviceConstants,
    //         detail::AddressImpl<uint32_t>(config.addressImpl),
    //         static_cast<size_t>(config.wavetableSize),
    //         config.searchPath, cancelCallback_);
    // }

    // --- 3. Construct Prefetch object (0x11e1a9) ---
    auto warningCb = [this](std::string const& msg) {
        messages_.warningMessage(msg, -1);
    };

    Prefetch prefetch(config, deviceConstants, asmCommands,
                      ast_, wavetableIR,
                      std::function<void(std::string const&)>(warningCb),
                      cancelCallback_);

    // --- 4. preparePlays() (0x11e367) ---
    prefetch.preparePlays();

    // --- 5. getUsedWavesForDevice → setUsedWaveforms (0x11e36c) ---
    auto waves = prefetch.getUsedWavesForDevice(
        static_cast<size_t>(config.deviceIndex));
    wavetableIR->setUsedWaveforms(waves);

    // --- 6. assignWaveIndexImplicit (0x11e3fc) ---
    wavetableIR->assignWaveIndexImplicit();

    // --- 7. alignWaveformSizes (0x11e40b) ---
    wavetableIR->alignWaveformSizes();

    // --- 8. assignWaveformAllocationSizes (0x11e413) ---
    wavetableIR->assignWaveformAllocationSizes();

    // --- 9. (Conditional) determineFixedWaves (0x11e418) ---
    if (config.cacheType == 1) {
        prefetch.determineFixedWaves();
    }

    // --- 10. updateWaveforms (0x11e432) ---
    wavetableIR->updateWaveforms(config.cacheType != 0 && config.isHirzel,
                                  deviceConstants.hasDIO);

    // --- 11. placeLoads (0x11e44f) ---
    prefetch.placeLoads();

    // --- 12. (Optional debug) Print tree (0x11e454) ---
    if (config.debugFlags & 0x08) {
        prefetch.print(nullptr, 0);
    }

    // --- 13. fillInPlaceholders (0x11e4fc) ---

    asmList_ = prefetch.fillInPlaceholders(asmList);

    // --- 15-16. Store channel info from prefetch (0x11e5bd) ---
    channelCount_ = prefetch.getUsedChannels();
    usedFourChannelMode_ = prefetch.getUsedFourChannelMode();  // binary 0x11e5e0: direct store
}

// ============================================================================
// Getters / Setters
// ============================================================================

// 0x123480
void Compiler::setCancelCallback(std::weak_ptr<CancelCallback> cb) {
    cancelCallback_ = cb;                                         // @0x12349e-1234b1
    waveformGen_->setCancelCallback(std::move(cb));               // @0x1234b6-1234e7
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
    result.push_back(static_cast<int>(usedFourChannelMode_));
    return result;
}

// 0x1235e0
bool Compiler::usedDeviceSampleRate() const {
    return usedSampleRate_ != 0;
}

bool Compiler::hadSyntaxError() const {
    // Binary reads byte at Compiler+0x100+0x03 = parserContext_[3]
    // which is SeqcParserContext::hadSyntaxError flag
    return parserContext_.hadSyntaxError();
}

// 0x1235f0
std::vector<CompilerMessage> Compiler::getCompileMessages() const {
    return messages_.messages();
}

// 0x123640
void Compiler::setLineNr(int nr) {
    lineNr_ = nr;                                                 // @0x123644
    asmCommands_->setWavetableFrontIndex(nr);                     // @0x12364e
    wavetable_->setLineNr(nr);                                    // @0x123659 (tail call)
}

// 0x123660
std::vector<int> Compiler::getLineMap(int offset) const {
    std::vector<int> result;
    int counter = 0;
    int seq = 1;

    // Iterate asmList_ entries (stride 0xA8)                       // @0x123660
    for (auto& entry : asmList_) {
        if (entry.assembler.cmd == Assembler::INVALID)
            continue;
        if (entry.assembler.cmd == Assembler::LABEL) {
            seq++;
            continue;
        }
        result.push_back(counter + offset);
        result.push_back(counter);
        result.push_back(seq);
        result.push_back(entry.lineNumber());  // +0x88 in AsmList::Asm
        counter++;
        seq++;
    }

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
// Return type is LowerResult (32B sret = 2 shared_ptrs). Evidence:
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
    int loopUnrollLimit)
{
    // 1. Build FrontendLoweringContext on stack (holds shared_ptrs + int)
    FrontendLoweringContext context;
    context.messages = &messages;
    context.asmCommands = std::move(asmCommands);
    context.customFunctions = std::move(customFunctions);
    context.waveformGen = std::move(waveformGen);
    context.wavetable = std::move(wavetable);
    context.loopUnrollLimit = loopUnrollLimit;

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
