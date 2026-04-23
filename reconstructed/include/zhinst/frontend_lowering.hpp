// ============================================================================
// FrontendLoweringContext / FrontendLoweringState — stack-local structures
// used by FrontEndLoweringFacade::lower() during AST lowering.
//
// FrontendLoweringContext: holds references to all compiler subsystems
//   needed during lowering.  Dtor @0x1233b0 destroys 4 shared_ptrs.
//
// FrontendLoweringState: accumulates lowering output.
//   Dtor @0x1c2190 destroys a vector<string> and one shared_ptr.
//
// constWaveform (anonymous namespace) @0x22c9f0: utility that produces
//   a constant rectangular waveform via WaveformGenerator::eval("rect",...).
//
// See also: FrontEndLoweringFacade::lower() declared in compiler.hpp.
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zhinst {

// Forward declarations
class AsmCommands;
class CompilerMessageCollection;
class CustomFunctions;
class EvalResults;      // 0x80 bytes — see eval_results.hpp
class Node;
class WaveformGenerator;
class WavetableFront;

// ============================================================================
// FrontendLoweringContext — 0x50 bytes (padded from 0x4C)
//
// Layout (from dtor @0x1233b0 and lower() @0x1c1da0):
//   +0x00  8   CompilerMessageCollection*          messages   (raw, non-owning)
//   +0x08  16  shared_ptr<AsmCommands>              asmCommands
//   +0x18  16  shared_ptr<CustomFunctions>           customFunctions
//   +0x28  16  shared_ptr<WaveformGenerator>         waveformGen
//   +0x38  16  shared_ptr<WavetableFront>            wavetable
//   +0x48  4   int32_t                               channelGrouping
//   +0x4C  4   (padding)
// ============================================================================
struct FrontendLoweringContext {
    CompilerMessageCollection*                 messages;          // +0x00
    std::shared_ptr<AsmCommands>               asmCommands;       // +0x08
    std::shared_ptr<CustomFunctions>           customFunctions;   // +0x18
    std::shared_ptr<WaveformGenerator>         waveformGen;       // +0x28
    std::shared_ptr<WavetableFront>            wavetable;         // +0x38
    int32_t                                    channelGrouping;   // +0x48
    // 4 bytes padding to 0x50

    ~FrontendLoweringContext();  // 0x1233b0
};

static_assert(sizeof(FrontendLoweringContext) == 0x50,
              "FrontendLoweringContext must be exactly 0x50 (80) bytes");

// ============================================================================
// FrontendLoweringState — 0x30 bytes
//
// Layout (from dtor @0x1c2190 and lower() @0x1c1da0):
//   +0x00  16  shared_ptr<Node>                    result (lowered AST root)
//   +0x10  8   (zeroed — possibly bool or padding)
//   +0x18  24  vector<string>                      strings (accumulator)
//
// The shared_ptr at +0x00 holds the lowered AST root node. After the
// virtual dispatch on SeqCAstNode in lower(), this is copied into the
// first half of lower()'s sret return struct, and then stored into
// Compiler+0x28 (which was previously labeled ast_ = shared_ptr<Node>,
// confirming the pointee type).
//
// CORRECTION 2026-04-23 (Phase 15a-i): Pointee type resolved as Node
// (was shared_ptr<void>/TBD). Evidence: lower() @0x1c1fb6 copies
// [rbp-0x90] (state.result, 16B) into sret[0]; caller Compiler::compile
// @0x11f92f stores sret[0] into Compiler+0x28, which is declared as
// shared_ptr<Node> ast_ — type must match.
// ============================================================================
struct FrontendLoweringState {
    std::shared_ptr<Node>                      result;    // +0x00 (lowered AST root)
    uint64_t                                   pad10_{};  // +0x10 (zeroed in lower())
    std::vector<std::string>                   strings;   // +0x18

    ~FrontendLoweringState();  // 0x1c2190
};

static_assert(sizeof(FrontendLoweringState) == 0x30,
              "FrontendLoweringState must be exactly 0x30 (48) bytes");

}  // namespace zhinst
