// ============================================================================
// FrontendLoweringContext / FrontendLoweringState destructors.
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/ast/frontend_lowering.hpp"

// ============================================================================
// FrontendLoweringContext::~FrontendLoweringContext()  — 0x1233b0
//
// Destroys 4 shared_ptrs in reverse declaration order:
//   +0x38 wavetable  (ctrl @+0x40)
//   +0x28 waveformGen (ctrl @+0x30)
//   +0x18 customFunctions (ctrl @+0x20)
//   +0x08 asmCommands (ctrl @+0x10)
//
// The raw pointer at +0x00 (messages) is non-owning and not touched.
// ============================================================================
zhinst::FrontendLoweringContext::~FrontendLoweringContext() = default;
// The compiler-generated default dtor destroys members in reverse order,
// which matches the binary (wavetable, waveformGen, customFunctions,
// asmCommands).  The raw pointer and int are trivially destructible.

// ============================================================================
// FrontendLoweringState::~FrontendLoweringState()  — 0x1c2190
//
// Destroys vector<string> at +0x18, then shared_ptr at +0x00.
// The inFunctionDef_ at +0x10 is trivially destructible.
// ============================================================================
zhinst::FrontendLoweringState::~FrontendLoweringState() = default;
// The compiler-generated default dtor destroys members in reverse order:
//   labelStack (vector<string>), then inFunctionDef_ (trivial), then result (shared_ptr).
// This matches the binary dtor at 0x1c2190 which destroys the vector first,
// then releases the shared_ptr.
