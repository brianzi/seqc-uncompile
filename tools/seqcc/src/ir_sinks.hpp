// =============================================================================
// seqcc — IR sink bag wrapping the recon-side `CompileSeqcIntrospection`.
//
// Lets `dump.hpp` declare its signature without pulling in the recon
// header (and its `boost::json` dependency) for every TU that includes
// `dump.hpp`.  The recon header is only included by `compile.cpp` and
// `dump.cpp` where the sink is actually populated / consumed.
// =============================================================================
#pragma once

#include "zhinst/codegen/compile_seqc.hpp"

namespace seqcc {

//! Bag of IR handles captured from `compileSeqcWithIR()`.  Currently
//! a thin alias for the recon-side struct; promoted to a wrapper if
//! the driver ever needs to carry additional driver-only state
//! alongside the handles.
using IRSinks = zhinst::CompileSeqcIntrospection;

}  // namespace seqcc
