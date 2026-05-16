// =============================================================================
// seqcc — compile driver
//
// Thin wrapper around zhinst::compileSeqc() that takes a parsed
// `Options` struct, runs the compiler, unpacks the `info\0elf` packed
// return value, and writes the ELF to disk.  Used by the `seqcc`
// personality in T2.
// =============================================================================

#pragma once

#include "options.hpp"

namespace seqcc {

//! Run the full compile pipeline for the given options.  Returns the
//! process exit code: 0 on success, non-zero on any failure (with a
//! diagnostic already written to stderr).
//!
//! Behaviour mirrors a single `compile_seqc(...)` Python call:
//!   - reads `opts.input`
//!   - builds the JSON config from `opts.tune`
//!   - calls `zhinst::compileSeqc(jsonConfig, src, opts.devtype,
//!     opts.awgIndex, opts.devopts)`
//!   - splits the packed return value at the first NUL byte
//!   - writes the trailing ELF payload to `opts.output`
//!   - discards the info JSON (a future T-phase will expose it via
//!     `--info=<path>`)
int runCompile(Options const& opts);

}  // namespace seqcc
