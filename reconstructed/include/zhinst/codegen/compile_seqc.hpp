// ============================================================================
// compile_seqc.hpp
//
// Public declaration of the `compileSeqc()` free function.  The
// original binary did not ship a header for this function (the Python
// binding and any other consumer used a local forward declaration);
// this header exists so seqcc-side code and the pybind layer can
// include a single canonical signature.
//
// History.  T3c added a parallel additive entry point
// `compileSeqcWithIR()` plus a `CompileSeqcIntrospection` carrier
// struct and a `fillIntrospection()` friend helper, used by the early
// seqcc driver to capture mid-pipeline IR handles into the driver
// without exposing them through the public API.  T10a retired all of
// that scaffold once the owned `SeqcDriver` became the only seqcc
// compile path — the driver now captures handles directly via the
// public `AWGCompiler::compiler() → Compiler::{ast(), wavetable(),
// asmList()}` accessors and the recon-side surface is back to the
// original-binary footprint for this entry point.
// ============================================================================
#pragma once

#include <string>

namespace zhinst {

//! \brief Original-binary entry point.  Unchanged signature; see
//!        compile_seqc.cpp for full semantics.
std::string compileSeqc(std::string const& jsonConfig,
                        std::string sourceCode,
                        std::string deviceId,
                        unsigned long awgIndex,
                        std::string const& options);

}  // namespace zhinst
