// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Compiler — main SeqC compilation orchestrator
//
// Layout: ~0x138 bytes
//   +0x00: AWGCompilerConfig const*    config_
//   +0x08: DeviceConstants const*      deviceConstants_
//   +0x10: int32_t                     lineNr_
//   +0x14: uint16_t                    flags_
//   +0x18: (reserved, 16 bytes)
//   +0x28: shared_ptr<Node>            ast_
//   +0x38: CompilerMessageCollection   messages_ (0x20 bytes)
//   +0x58: vector<string>              sourceFiles_
//   +0x70: vector<string>              sourceLines_
//   +0x88: AsmList                     asmList_
//   +0xA0: shared_ptr<WavetableFront>  wavetable_
//   +0xB0: shared_ptr<AsmCommands>     asmCommands_
//   +0xC0: shared_ptr<WaveformGenerator> waveformGen_
//   +0xD0: shared_ptr<CustomFunctions>  customFunctions_
//   +0xE0: weak_ptr<CancelCallback>    cancelCallback_
//   +0xF0: weak_ptr<ProgressCallback>  progressCallback_
//   +0x100: SeqcParserContext           parserContext_ (~0x38 bytes)
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "asm_list.hpp"
#include "compiler_message.hpp"

namespace zhinst {

// Forward declarations
class AWGCompilerConfig;
class AsmCommands;
class CancelCallback;
class CustomFunctions;
class DeviceConstants;
class Expression;
class Node;
class ProgressCallback;
class Resources;
class SeqCAstNode;
class SeqcParserContext;
class WavetableFront;
class WavetableIR;
class WaveformGenerator;

// ---- FrontEndLoweringFacade ----
// Thin static facade that dispatches to SeqCAstNode virtual method

namespace FrontEndLoweringFacade {

// 0x1c1da0 — ~1KB
// Packs arguments into a FrontendLoweringContext, creates an empty
// FrontendLoweringState, dispatches to SeqCAstNode::lower() virtual,
// returns result.
void lower(std::shared_ptr<Resources> resources,
           SeqCAstNode& ast,
           CompilerMessageCollection& messages,
           std::shared_ptr<AsmCommands> asmCommands,
           std::shared_ptr<CustomFunctions> customFunctions,
           std::shared_ptr<WaveformGenerator> waveformGen,
           std::shared_ptr<WavetableFront> wavetable,
           int channelGrouping);

}  // namespace FrontEndLoweringFacade

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
    std::vector<AssemblerInstr> compile(const std::string& source);  // 0x11f150

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

    // Returns pointer into Resources sub-object at *(this+0xD0)+0x150
    const void* getNodeAccessList() const;                         // 0x123550

    // Returns pointer into Resources sub-object at *(this+0xD0)+0x128
    const void* getNodeToModeMap() const;                          // 0x123570

    // Returns {channelCount (int at +0x20), mode (byte at +0x24)}
    std::vector<int> getChannelInfo() const;                       // 0x123590

    // Returns byte at this+0x25
    bool usedDeviceSampleRate() const;                             // 0x1235e0

    // Copies messages from CompilerMessageCollection
    std::vector<CompilerMessage> getCompileMessages() const;       // 0x1235f0

    // Sets lineNr at +0x10, propagates to AsmCommands and WavetableFront
    void setLineNr(int nr);                                        // 0x123640

    // Builds vector<int> from asmList entries: groups of 4 ints per instruction
    std::vector<int> getLineMap(int offset) const;                 // 0x123660

private:
    const AWGCompilerConfig* config_;              // +0x00
    const DeviceConstants* deviceConstants_;        // +0x08
    int32_t lineNr_;                               // +0x10
    uint16_t flags_;                               // +0x14
    uint8_t pad16_[2];                             // +0x16
    uint64_t reserved18_;                          // +0x18
    uint32_t channelCount_;                        // +0x20
    uint8_t channelMode_;                          // +0x24
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
    // SeqcParserContext at +0x100 (~0x38 bytes, contains std::function)
    char parserContext_[0x38];                     // +0x100 (opaque)
    // +0x138 END
};

}  // namespace zhinst
