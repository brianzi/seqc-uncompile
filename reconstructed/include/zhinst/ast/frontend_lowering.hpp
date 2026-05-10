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
//   +0x48  4   int32_t                               loopUnrollLimit
//   +0x4C  4   (padding)
// ============================================================================
//! \brief Read-only environment passed to every AST `evaluate()` override.
//!
//! `FrontendLoweringContext` bundles the compiler subsystems an
//! `evaluate()` implementation may need to consult: the message collector
//! for diagnostics, the assembler-instruction registry for opcode lookup,
//! the registry of SeqC built-in functions, the waveform code generator,
//! and the wavetable bookkeeping front-end.  `loopUnrollLimit` caps how
//! deeply loop bodies may be unrolled by `repeat()` and `for(...)`
//! evaluations.
//!
//! The context is constructed once per compile, lives on the stack of the
//! `lower()` driver, and is shared by reference across the entire AST
//! traversal.
struct FrontendLoweringContext {
    CompilerMessageCollection*                 messages;          //!< Non-owning sink for warnings / errors emitted during the AST walk. +0x00
    std::shared_ptr<AsmCommands>               asmCommands;       //!< Assembler-command registry consulted for opcode emission. +0x08
    std::shared_ptr<CustomFunctions>           customFunctions;   //!< SeqC built-in / user-function resolver. +0x18
    std::shared_ptr<WaveformGenerator>         waveformGen;       //!< Factory for waveform-generator built-ins (`zeros`, `sin`, ...). +0x28
    std::shared_ptr<WavetableFront>            wavetable;         //!< Front-end wavetable that records every waveform reference. +0x38
    int32_t                                    loopUnrollLimit;   //!< Iteration cap applied when `repeat()` / `for(...)` loop bodies are constant-folded. +0x48
    // 4 bytes padding to 0x50

    //! Releases the four owned subsystem `shared_ptr`s
    //! (assembler-command registry, custom-functions table,
    //! waveform generator, wavetable front-end) in reverse
    //! declaration order.  The non-owning `messages`
    //! pointer is left untouched.
    ~FrontendLoweringContext();  // 0x1233b0
};

static_assert(sizeof(FrontendLoweringContext) == 0x50,
              "FrontendLoweringContext must be exactly 0x50 (80) bytes");

// ============================================================================
// FrontendLoweringState — 0x38 bytes
//
// Layout (from dtor @0x1c2190, lower() @0x1c1da0, and evaluate accesses):
//   +0x00  16  shared_ptr<Node>                    result (lowered AST root)
//   +0x10  8   (zeroed — possibly bool or padding)
//   +0x18  24  vector<string>                      labelStack (accumulator)
//   +0x30  1   uint8_t                             inLoop_   (set by loop evaluates)
//   +0x31  1   uint8_t                             inSwitch_ (set by SeqCSwitchCase)
//   +0x32  6   (padding to 0x38)
//
// The shared_ptr at +0x00 holds the lowered AST root node. After the
// virtual dispatch on SeqCAstNode in lower(), this is copied into the
// first half of lower()'s sret return struct, and then stored into
// Compiler+0x28 (declared as shared_ptr<Node> ast_).
//
// Pointee type is Node. Evidence: lower() @0x1c1fb6 copies [rbp-0x90]
// (state.result, 16B) into sret[0]; caller Compiler::compile @0x11f92f
// stores sret[0] into Compiler+0x28 which is declared as
// shared_ptr<Node> ast_.
//
// Size is 0x38: SeqCCaseEntry::evaluate @0x21aa40 accesses [r8+0x31] to
// check inSwitch_ flag. SeqCSwitchCase, SeqCForLoop et al. set
// inLoop_/inSwitch_ around their body evaluations.
// ============================================================================
//! \brief Mutable accumulator threaded through the AST `evaluate()` walk.
//!
//! `FrontendLoweringState` collects the output of the lowering traversal:
//! `result` is the lowered IR root that grows as statements are processed,
//! `labelStack` tracks the active break/continue label scope for
//! `SeqCBreakStatement` / `SeqCContinueStatement`, and `inLoop_` /
//! `inSwitch_` are scope flags that loop and switch evaluations toggle
//! around their body so nested control-flow statements can validate they
//! appear in a legal context.
struct FrontendLoweringState {
    std::shared_ptr<Node>                      result;      //!< Lowered AST root accumulated by the walk; copied into `Compiler::ast_` after `lower()` returns. +0x00
    uint64_t                                   inFunctionDef_{};    //!< Function-definition nesting counter; zero outside any function body. +0x10
    std::vector<std::string>                   labelStack;     //!< Active break / continue label scope used by `SeqCBreakStatement` and `SeqCContinueStatement`. +0x18
    uint8_t                                    inLoop_{};   //!< Non-zero while the walk is inside a loop body; consulted by `break` / `continue` validation. +0x30
    uint8_t                                    inSwitch_{}; //!< Non-zero while the walk is inside a `switch` body; consulted by `SeqCCaseEntry`. +0x31
    // +0x32..0x37 padding to 0x38

    //! Releases the lowered-AST root `shared_ptr` and the
    //! `labelStack` vector (with its element strings) in
    //! reverse declaration order.  Scope-flag bytes have
    //! no destruction work.
    ~FrontendLoweringState();  // 0x1c2190
};

static_assert(sizeof(FrontendLoweringState) == 0x38,
              "FrontendLoweringState must be exactly 0x38 (56) bytes");

}  // namespace zhinst
