// ============================================================================
// compile_seqc.hpp
//
// Public declaration of the `compileSeqc()` free function and the
// T3c-added `compileSeqcWithIR()` introspection variant.  The original
// binary did not ship a header for `compileSeqc()` (the Python binding
// and any other consumer used a local forward declaration); this
// header exists so the optional `CompileSeqcIntrospection` struct
// can be shared between the implementation TU and the seqcc driver
// without each side redeclaring it.
//
// NOTE: `compileSeqcWithIR()` and `CompileSeqcIntrospection` are
// purely additive — neither exists in the original binary.  They were
// introduced for the `seqcc` toolchain driver (Phase T, T3c) and are
// expected to be retired when the seqcc-owned compilation driver
// lands.  The existing `compileSeqc()` signature is unchanged so the
// pybind binding remains binary-compatible.
// ============================================================================
#pragma once

#include <memory>
#include <string>
#include <utility>

namespace zhinst {

class Node;

//! \brief Optional out-bag populated by `compileSeqcWithIR()` to give
//!        tooling read-only access to internal compiler IR that would
//!        otherwise be destroyed when the function returns.
//!
//! \details Fields are populated only when the corresponding compile
//! stage runs to completion.  When the compile throws, the struct is
//! left in its default-constructed state (all handles empty).
//!
//! Adding new fields here is backward-compatible — the struct is
//! returned by value from a function the original binary doesn't
//! export, and there are no external consumers.
struct CompileSeqcIntrospection {
    //! \brief Root of the lowered SeqC AST after the front-end
    //!        lowering / optimisation passes.  Empty when the
    //!        compile failed before producing an AST.
    std::shared_ptr<Node const> loweredAst;

    // Future T4 hooks (not yet populated):
    // std::shared_ptr<AsmList const>     asmList;
    // std::shared_ptr<WavetableIR const> wavetableIR;
    // std::shared_ptr<CustomFunctions const> resources;
};

//! \brief Original-binary entry point.  Unchanged signature; see
//!        compile_seqc.cpp for full semantics.
std::string compileSeqc(std::string const& jsonConfig,
                        std::string sourceCode,
                        std::string deviceId,
                        unsigned long awgIndex,
                        std::string const& options);

//! \brief T3c additive variant: runs the same pipeline as
//!        `compileSeqc()` and additionally returns IR handles that
//!        survive the compile's destructor scope.
//!
//! \details The ELF half of the return is byte-identical to what
//! `compileSeqc()` produces for the same inputs — both functions
//! share the same internal implementation.  Callers requesting no
//! introspection should keep using `compileSeqc()`.
//!
//! \return  `pair.first` is the packed `JSON + '\0' + ELF` string
//!          (identical layout to `compileSeqc()`'s return value).
//!          `pair.second` carries the captured IR handles; empty
//!          fields indicate the corresponding stage didn't run.
std::pair<std::string, CompileSeqcIntrospection>
compileSeqcWithIR(std::string const& jsonConfig,
                  std::string sourceCode,
                  std::string deviceId,
                  unsigned long awgIndex,
                  std::string const& options);

}  // namespace zhinst
