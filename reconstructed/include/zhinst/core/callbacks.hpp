// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Classes: zhinst::CancelCallback, zhinst::ProgressCallback
//
// CancelCallback: pure virtual interface, no vtable/typeinfo in binary
//   (implementations live in the Python binding layer)
//   Used via weak_ptr in Compiler, Prefetch, WavetableIR, AsmOptimize
//
// ProgressCallback: virtual base with default (empty) implementations
//   dtor @0x129960, setProgress @0x129980
//   vtable @0xb03858, typeinfo @0xb03880
//   Size: 8 bytes (vtable pointer only)
// ============================================================================
#pragma once

namespace zhinst {

// ---------------------------------------------------------------------------
// CancelCallback — cancellation interface
//
// No own symbols in the binary. Used only through weak_ptr<CancelCallback>
// which is locked to shared_ptr, then isCancelled() is called via vtable.
// The vtable/typeinfo and implementations are in the Python binding module.
//
// Usage pattern in binary:
//   if (auto lock = cancelCb_.lock()) {
//       if (lock->isCancelled()) { /* abort */ }
//   }
// ---------------------------------------------------------------------------
//! \brief Pure-virtual cancellation hook polled by long-running compile
//!        passes.
//!
//! The compiler holds a `weak_ptr<CancelCallback>` and locks it at safe
//! points during expensive passes (front-end lowering, prefetch
//! scheduling, wavetable assembly, assembler optimisation); when
//! `isCancelled()` returns true the pass aborts cleanly and the compile
//! returns an error.  The concrete implementation that decides when to
//! cancel is supplied by the embedding application.
class CancelCallback {
public:
    virtual ~CancelCallback() = default;
    virtual bool isCancelled() = 0;
};

// ---------------------------------------------------------------------------
// ProgressCallback — progress reporting interface
//
// Size: 8 bytes (vtable only). Default implementations are no-ops.
// vtable @0xb03858: [dtor @0x129960, deleting_dtor @0x129970, setProgress @0x129980]
//
// Created via make_shared (alloc 0x18 = 8 obj + 16 control block overhead).
// Used via weak_ptr in Compiler at +0xF0.
// ---------------------------------------------------------------------------
//! \brief Progress-reporting hook called by the compiler with values in
//!        `[0.0, 1.0]`; the default implementation is a no-op.
//!
//! Both the destructor and `setProgress` ship as empty defaults, so
//! derived classes only need to override `setProgress` to receive
//! updates.  The compiler keeps a `weak_ptr<ProgressCallback>` and
//! invokes it at coarse pass boundaries; concrete implementations
//! are supplied by the embedding application.
class ProgressCallback {
public:
    virtual ~ProgressCallback();               // 0x129960 (empty)
    virtual void setProgress(double progress);  // 0x129980 (empty default)
};

} // namespace zhinst
