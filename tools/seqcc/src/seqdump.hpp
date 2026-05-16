// =============================================================================
// seqcc — `seqdump` personality
//
// C++ port of `tests/dump_elf.py`.  Accepts a single ELF file (the
// SeqC compiler's binary output format) on the command line and
// pretty-prints its sections.  Supports filtering to a single
// section and side-by-side diffing against another ELF file.
//
// Invoked either as the `seqdump` binary (a symlink to `seqcc`) or
// as `seqcc dump …` (TODO if subcommand routing lands; T9 only wires
// the symlink path).
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace seqcc {

//! Entry point for the seqdump personality.  Takes the program's
//! `argv[0]` (used in `--help` output) plus the remaining arguments
//! (everything past `argv[0]`).  Returns a process exit code.
//!
//! Recognised flags:
//!
//!   `<elf-file>`        Positional, required (or two positionals
//!                       when `--diff` is implied).
//!   `--section=<name>`  Dump only the named section.  Repeatable.
//!   `--all`             Dump every section (default when no
//!                       `--section` is given).
//!   `--diff <other>`    Compare against another ELF, printing only
//!                       differing sections side-by-side.
//!   `--max-lines=N`     Truncate text-section dumps to N lines
//!                       (default 200, matches dump_elf.py).
//!   `--help`            Print usage and exit.
//!   `--version`         Print driver version and exit.
int seqdumpMain(std::string const& argv0,
                std::vector<std::string> args);

}  // namespace seqcc
