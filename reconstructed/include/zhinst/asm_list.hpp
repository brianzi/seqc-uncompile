// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmList::Asm (AsmEntry) — output instruction record
// AsmList — container (std::vector<Asm>)
//
// AsmList::Asm layout confirmed from:
//   - AsmList::Asm::~Asm() at 0x122dd0
//   - AsmList::append() at 0x15d180
//   - AsmList::maxRegister() at 0x269910
//   - AsmList::print() at 0x264250
//   - AsmList::serialize() at 0x2646d0
//
// AsmList is exactly std::vector<Asm> (0x18 bytes: begin/end/capacity).
// No additional fields.
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "assembler.hpp"
#include "node.hpp"

namespace zhinst {

// ============================================================================
// AsmList class — wraps a vector of Asm records.
//
// Total size: 0x18 (24 bytes) = std::vector<Asm>
//   +0x00  data pointer (begin)
//   +0x08  end pointer
//   +0x10  capacity end pointer
// ============================================================================
class AsmList {
public:
    // ========================================================================
    // Asm (nested type) — a single output record produced by AsmCommands.
    // Size: 0xA8 bytes.
    //
    // Layout:
    //   +0x00  int             sequenceId      — from createUniqueID(false)
    //   +0x04  (4 bytes padding)
    //   +0x08  AssemblerInstr  assembler       — instruction data (0x80 bytes)
    //   +0x88  int             wavetableFront  — waveform table context index
    //   +0x8C  (4 bytes padding)
    //   +0x90  shared_ptr<Node> node           — AST node (16 bytes; may be null)
    //   +0xA0  bool            isWaveformCmd   — derived: (cmd-3) < 3u
    //   +0xA1  (7 bytes padding to 0xA8)
    // ========================================================================
    struct Asm {
        int sequenceId = 0;               // +0x00
        // +0x04: 4 bytes padding
        AssemblerInstr assembler;         // +0x08  (0x80 bytes)
        int wavetableFront = 0;           // +0x88
        // +0x8C: 4 bytes padding
        std::shared_ptr<Node> node;       // +0x90  (16 bytes: ptr + ctrl)
        bool isWaveformCmd = false;       // +0xA0
        // +0xA1..+0xA7: padding to 0xA8

        ~Asm();  // 0x122dd0

        // Asm::serializeNodeToJsonString(idMap) const — 0x2698b0
        //   Calls node->toJson(idMap), then boost::json::serialize().
        //   Returns the JSON string. Uses sret calling convention.
        std::string serializeNodeToJsonString(
            const std::unordered_map<int,int>& idMap) const;

        // createUniqueID(bool reset) — always inlined, no standalone function.
        //   Static thread-local nextID at TLS offset 0x40.
        //   If reset=true: nextID = 0.
        //   If reset=false: returns nextID++ (post-increment).
        static int createUniqueID(bool reset);
    };

    // --- Destructor: 0x11d5b0
    //   Iterates elements from end to begin, destroying each
    //   (release shared_ptr, destroy Assembler), then frees buffer.
    ~AsmList();

    // --- append(entry): 0x15d180
    //   Copy-appends an Asm record. Fast path: placement new if capacity
    //   allows; slow path: vector reallocation.
    //   Copies: sequenceId (int), Assembler (copy ctor), wavetableFront (int),
    //   node (shared_ptr copy + atomic inc), isWaveformCmd (bool).
    void append(const Asm& entry);

    // --- print(showNode, os, showHeader) const: 0x264250
    //   Iterates entries. For each:
    //     If showHeader: prints "NNN (NNN): " with wavetableFront and sequenceId.
    //     If opcode != -1: prints Assembler::str(true) + "\n".
    //     If opcode == -1 (placeholder):
    //       If showNode && node: prints "// placeholder: " + node->toString() + "\n".
    //       If showNode && !node: prints "// <empty command>\n".
    //       Else: just "\n".
    void print(bool showNode, std::ostream& os, bool showHeader) const;

    // --- serialize() const: 0x2646d0
    //   Two-pass serialization to string:
    //   Pass 1: Build idMap (unordered_map<int,int>) — assigns sequential IDs
    //           to entries, skipping opcode==4 and (opcode==-1 && node==null).
    //   Pass 2: For each entry:
    //     If opcode != -1: emit Assembler::str(true), optionally " #disableOpt"
    //                      suffix if isWaveformCmd && specific opcode check.
    //     If opcode == -1 && node: emit "placeholder # " + node.toJson(idMap) serialized.
    //   Returns the full string.
    std::string serialize() const;

    // --- deserialize(str): 0x266050
    //   Calls parseStringToAsmList(str), moves result into this.
    //   Returns *this.
    AsmList& deserialize(const std::string& str);

    // --- parseStringToAsmList(str): 0x266160 (static)
    //   Parses assembly text back into AsmList. Uses:
    //     1. getDeviceConstants(AwgDeviceType::2) — hardcoded device type
    //     2. AWGAssembler::assembleStringToExpressionsVec(str) — parse text
    //     3. Iterates expressions, builds Asm entries with createUniqueID
    //   Returns tuple<AsmList, string> (AsmList + error/warning messages).
    //   ~7000 bytes of code — complex parser.
    static std::tuple<AsmList, std::string> parseStringToAsmList(const std::string& str);

    // --- maxRegister() const: 0x269910
    //   Iterates all entries, calls Assembler::highestRegisterNumber() on each.
    //   Returns the maximum register number found (0 if empty).
    //   highestRegisterNumber() returns packed int64: bit 32 = valid flag,
    //   lower 32 = register number.
    int maxRegister() const;

    // --- Storage: exactly std::vector<Asm> ---
    std::vector<Asm> entries;
};

} // namespace zhinst
