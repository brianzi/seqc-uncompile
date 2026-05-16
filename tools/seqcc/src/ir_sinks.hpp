// =============================================================================
// seqcc — IR sink bag wrapping the recon-side `CompileSeqcIntrospection`.
//
// Lets `dump.hpp` declare its signature without pulling in the recon
// header (and its `boost::json` dependency) for every TU that includes
// `dump.hpp`.  The recon header is only included by `compile.cpp` and
// `dump.cpp` where the sink is actually populated / consumed.
//
// T5b.5: promoted from a thin alias to a wrapping struct so the driver
// can carry the cached pretty-printed assembler text alongside the
// recon IR handles — needed so `--to=asm` can short-circuit after
// `stepInnerCompile` (assembler text is populated by then) without
// running `stepAssembleOpcodes` / `stepCheckLimits` / `writeToStream`
// just to parse the `.asm` section back out of the produced ELF.
// =============================================================================
#pragma once

#include "zhinst/codegen/compile_seqc.hpp"

#include <string>

namespace seqcc {

//! Bag of IR handles captured during a driver compile.  Wraps the
//! recon-side `CompileSeqcIntrospection` (for AST / wavetable / asmList
//! handles) and adds driver-only fields populated by the driver
//! between step calls — currently just the pretty-printed assembler
//! text, used to service `--to=asm` without producing a full ELF.
struct IRSinks : public zhinst::CompileSeqcIntrospection {
    //! Pretty-printed assembler listing as cached by
    //! `AWGCompilerImpl::assemblerText_` after `stepInnerCompile`.
    //! Populated by `SeqcDriver::compile` regardless of `--to=`;
    //! consumed by `renderStagePrimary` when `--to=asm`.  Empty when
    //! the driver was not invoked (e.g. unit tests that build an
    //! `IRSinks` from scratch).
    std::string assemblerText;
};

}  // namespace seqcc
