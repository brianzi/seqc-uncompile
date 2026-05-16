// =============================================================================
// seqcc — optimisation-flag plumbing (T8).
//
// Owns the canonical pass-name → bit map and the parsers for
// `-O<n>` (curated level → bitmask) and `-f[no-]<pass>` (per-bit
// toggle).  Consumed from `main.cpp` (CLI wiring) and exposed via
// `--print-passes`.
//
// The bitmask itself flows from `Options::optimizationFlags` through
// `buildJsonConfig()` into the JSON config under the key
// `"optimizationFlags"`, where it lands at
// `AWGCompilerConfig::optimizationFlags` (see IF-305 for the
// recon-side plumbing).
//
// Bit assignments verified against
// `reconstructed/src/asm/asm_optimize.cpp`:
//
//   0x01  jump-elim         — branch target collapse
//   0x02  label-cleanup     — drop unreferenced labels
//   0x04  dead-code-elim    — mark unused instructions INVALID
//   0x08  reg-zero-merge    — coalesce R0/RW0 reads
//   0x10  reg-alloc         — physical-register assignment
//
// Curated levels (gcc-style; tunable in stage.cpp::knownPasses()):
//
//   -O0  0x00   (no passes — useful for raw-output debugging)
//   -O1  0x05   (jump-elim + dead-code-elim — original "safe" set)
//   -O2  0x1F   (every defined pass — same effective output as -O3
//                on the current binary; bits 0x20..0x80 are unused
//                today and reserved for forward compatibility)
//   -O3  0xFF   (gcc parity; alias for -O2 on the current binary)
// =============================================================================
#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace seqcc {

//! One named optimisation pass.
struct PassInfo {
    std::string name;          //!< Canonical name for `-f<name>` / `-fno-<name>`.
    uint64_t    bit;           //!< Bitmask contribution (single bit).
    std::string description;   //!< Short text for `--print-passes`.
};

//! Canonical pass table, in bit-order.  `--print-passes` walks this.
std::vector<PassInfo> const& knownPasses();

//! Look up a pass by name; returns nullptr for unknown names.
PassInfo const* findPass(std::string const& name);

//! Bitmask for a curated optimisation level.  Accepts 0..3.  Any
//! value above 3 is treated as -O3 (gcc semantics for unrecognised
//! `-O<n>` levels above the highest defined one — gcc actually
//! errors on -O4+, but we choose to be permissive since the bitmask
//! is just a uint64).
uint64_t optimizationLevelMask(unsigned level);

//! Pretty-print the pass table to `os`.  Used by `--print-passes`.
void printPassTable(std::ostream& os);

//! Apply a single `-f<pass>` (enable) or `-fno-<pass>` (disable)
//! toggle to `flags`.  Throws `std::invalid_argument` on unknown
//! pass names.  Caller passes the trimmed pass name plus the
//! enable/disable bit.
void applyPassToggle(uint64_t& flags, std::string const& name,
                     bool enable);

}  // namespace seqcc
