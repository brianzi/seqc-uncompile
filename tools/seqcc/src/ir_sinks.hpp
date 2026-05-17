// =============================================================================
// seqcc — IR sink bag populated by `SeqcDriver::compile`.
//
// Standalone driver-side struct: owns the IR handles directly.  Prior
// to T10a this struct wrapped the recon-side
// `zhinst::CompileSeqcIntrospection` (populated by a friend-access
// `fillIntrospection()` helper).  T10a retired that scaffold along
// with `compileSeqcWithIR()` once the owned `SeqcDriver` became the
// only seqcc compile path — the driver now captures handles directly
// via the public `AWGCompiler::compiler() → Compiler::{ast(),
// wavetable(), asmList()}` accessors.
// =============================================================================
#pragma once

#include "zhinst/asm/asm_list.hpp"

#include <memory>
#include <string>

namespace zhinst {
class Node;
class WavetableFront;
}  // namespace zhinst

namespace seqcc {

//! Bag of IR handles captured during a driver compile.  All fields are
//! optional / can be empty when the corresponding pipeline stage did
//! not run (e.g. a compile that threw before `stepLower`, or the
//! `--to=asm-pre` front-end-only path which skips the back end).
//!
//! Lifetime contract: the `shared_ptr` fields keep the underlying
//! recon-side objects alive after the `AWGCompiler` destructor has
//! run, so downstream dump emission may run after the driver's
//! `compile()` returns.  String fields are plain owning copies.
struct IRSinks {
    //! Root of the lowered SeqC AST after the front-end lowering /
    //! optimisation passes.  Empty `shared_ptr` when the compile
    //! failed before `stepLower` produced an AST, or when the driver
    //! short-circuited before `stepInnerCompile`.
    std::shared_ptr<zhinst::Node const> loweredAst;

    //! Front-end wavetable tracker (registered waveforms, per-waveform
    //! IR, name index) at compile completion.  Empty `shared_ptr` only
    //! when capture was skipped (currently only the `--to=asm-pre`
    //! front-end-only path); a normal compile always has one because
    //! the wavetable is constructed by the enclosing
    //! `AWGCompilerImpl` and shared in by `shared_ptr` copy.
    std::shared_ptr<zhinst::WavetableFront const> wavetable;

    //! Snapshot of the embedded `Compiler::asmList_` at the moment
    //! the driver captured it — taken before the wavetable-aware
    //! rewrite so it matches the `AsmList` contents the
    //! `Node::toJson(idMap)` id-densification is computed against.
    //! Allocated freshly per capture so it survives the recon-side
    //! `AWGCompiler` unwind.  Empty `shared_ptr` when the compile
    //! failed before producing one of any size; an `AsmList` with
    //! zero entries still yields a non-empty handle so callers can
    //! distinguish "no compile" from "compile produced empty list".
    std::shared_ptr<zhinst::AsmList const> asmList;

    //! Pretty-printed assembler listing cached by the driver after
    //! `stepInnerCompile`.  Populated by `SeqcDriver::compile`
    //! regardless of `--to=`; consumed by `renderStagePrimary` when
    //! `--to=asm` so the dump is available even when the back end
    //! was short-circuited (no ELF, no `.asm` section to read).
    //! Empty when the driver was not invoked (e.g. unit tests that
    //! build an `IRSinks` from scratch) or when the front-end-only
    //! `--to=asm-pre` path was taken (`preprefetchAsmText` carries
    //! that case instead).
    std::string assemblerText;

    //! T6.2: pre-prefetch AsmList serialised via `AsmList::serialize()`
    //! at the binary's natural round-trip cut (after `stepOptPre`,
    //! before `stepPrefetch`).  Populated only when the driver took
    //! the `--to=asm-pre` front-end-only path.  Consumed by
    //! `renderStagePrimary("asm-pre")`.  Empty otherwise.
    //!
    //! One-way diagnostic dump only.  The round-trip resume path was
    //! demoted to Q3 — see IF-308 for the rationale.
    std::string preprefetchAsmText;
};

}  // namespace seqcc
