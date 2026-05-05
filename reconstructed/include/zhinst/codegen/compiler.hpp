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

namespace FrontEndLoweringFacade {

// ---- LowerResult ----
// sret return type of FrontEndLoweringFacade::lower() — 32 bytes (2 shared_ptrs).
// Binary @0x1c1fb6: writes shared_ptr<Node> (from FrontendLoweringState.result)
// into sret[0], and shared_ptr<EvalResults> (evaluate sret output) into sret[1].
// Caller Compiler::compile @0x11f92f stores sret[0] into Compiler+0x28 (ast_).
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
