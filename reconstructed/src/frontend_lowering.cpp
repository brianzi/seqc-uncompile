// ============================================================================
// FrontendLoweringContext / FrontendLoweringState destructors +
// constWaveform helper.
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/frontend_lowering.hpp"
#include "zhinst/compiler_message.hpp"
#include "zhinst/value.hpp"

#include <string>
#include <vector>

// Forward declarations for types not yet fully reconstructed
namespace zhinst {
class WaveformGenerator;
class EvalResults;
}  // namespace zhinst

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
// The pad10_ at +0x10 is trivially destructible.
// ============================================================================
zhinst::FrontendLoweringState::~FrontendLoweringState() = default;
// The compiler-generated default dtor destroys members in reverse order:
//   strings (vector<string>), then pad10_ (trivial), then result (shared_ptr).
// This matches the binary dtor at 0x1c2190 which destroys the vector first,
// then releases the shared_ptr.

// ============================================================================
// (anonymous namespace)::constWaveform  — 0x22c9f0
//
// Produces a constant rectangular waveform of the given length and value.
//
// 1. Allocates a shared_ptr<EvalResults> (EvalResults = 0x80 bytes,
//    zero-initialized).
// 2. Builds a vector<Value> with two elements:
//      Value(int: numSamples)
//      Value(double: value, subtype=2)
// 3. Calls WaveformGenerator::eval("rect", values) via ctx.waveformGen.
// 4. Swaps the returned shared_ptr<EvalResults> into the output,
//    releasing the initially empty one.
// 5. On exception, catches two types:
//      filter 2 → errorMessage("WaveformGenerator Exception", -1)
//      filter 1 → errorMessage("WaveformGenerator Value Exception", -1)
//    using the exception's what() string, falling back to the above
//    defaults if what() is empty.
// ============================================================================
namespace {

// TODO: Full reconstruction requires:
//   - WaveformGenerator::eval() declaration with return type shared_ptr<EvalResults>
//   - EvalResults struct layout (0x80 bytes, mostly zeroed)
//   - Exception types caught (filter 1 and 2 — likely ValueException and
//     a WaveformGenerator-specific exception)
//
// Stub left here with binary address and full behavioral documentation
// above.  The function is only called from SeqCAstNode::lower() virtual
// method overrides during waveform constant-folding.
//
// shared_ptr<EvalResults> constWaveform(int numSamples, double value,
//                                       FrontendLoweringContext& ctx)
// {
//     // 0x22c9f0 — see disassembly analysis above
// }

}  // anonymous namespace
