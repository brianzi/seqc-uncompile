// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmEntry / AsmList — output instruction records
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "assembler.hpp"
#include "node.hpp"

namespace zhinst {

// AsmEntry (aka AsmList::Asm) — a single output record produced by
// AsmCommands methods. Size: 0xA8 bytes.
//
// Layout:
//   +0x00  int sequenceId           — unique ID from thread-local counter
//   +0x08  AssemblerInstr assembler — instruction data (0x80 bytes)
//   +0x88  int wavetableFront       — waveform table context
//   +0x90  shared_ptr<Node> node    — AST node (16 bytes; may be null)
//   +0xA0  bool isWaveformCmd       — derived from opcode: (cmd-3) < 3u
struct AsmEntry {
    int sequenceId = 0;               // +0x00
    // 4 bytes padding
    AssemblerInstr assembler;         // +0x08
    int wavetableFront = 0;           // +0x88
    std::shared_ptr<Node> node;       // +0x90
    bool isWaveformCmd = false;       // +0xA0
};

// Thread-local sequence counter — every AsmEntry gets a unique sequential ID.
// Lives at TLS offset 0x40 in the original binary.
int nextSequenceId();

// AsmList — container for AsmEntry records.
// Exact type TBD (Phase 3a); likely wraps a vector.
class AsmList {
public:
    using Asm = AsmEntry;

    // Asm enum seen in Compiler::runPrefetcher — purpose TBD
    // enum Asm { ... };

    std::vector<AsmEntry> entries;
};

} // namespace zhinst
