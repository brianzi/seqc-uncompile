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

#include "zhinst/asm/assembler.hpp"
#include "zhinst/ast/node.hpp"

namespace zhinst {

// Offset  Size  Type                Name
// +0x00   8     T*                  begin pointer
// +0x08   8     T*                  end pointer
// +0x10   8     T*                  capacity end pointer
// sizeof(AsmList) = 0x18
//! \brief Sequence of assembled instructions.
//!
//! `AsmList` is a thin wrapper around a `std::vector<AsmList::Asm>` that
//! holds the instruction stream of a single sequencer.  Instructions are
//! appended in emission order by `AsmCommands` and friends, optimised
//! in place by `AsmOptimize`, then serialised to the ELF `.asm` and
//! `.text` sections.  In addition to the standard container interface
//! the class provides `serialize()` / `deserialize()` (round-trip text
//! form including embedded JSON for placeholder nodes), `print()`
//! (human-readable disassembly), `maxRegister()` (highest register seen,
//! used by the register allocator), and a node-anchored `insert()`
//! overload used by the prefetch pass to splice instructions at
//! placeholder positions.
class AsmList {
public:
    //! \brief One instruction record inside an `AsmList`.
    //!
    //! Pairs an `Assembler` instruction with the bookkeeping data the
    //! later compiler stages need: a per-instance `sequenceId` for
    //! stable identity across optimisation passes, a `wavetableFront`
    //! index that doubles as the source-line number for diagnostic
    //! commands (see `lineNumber()`), an optional `node` link back to
    //! the IR `Node` that produced the instruction (used to recover
    //! placeholder positions during prefetch), and a `noOpt` flag that
    //! marks the entry as exempt from optimiser rewrites.
    // Offset  Size  Type              Name            Notes
    // +0x00   4     int               sequenceId      from createUniqueID(false)
    // +0x04   4     (padding)
    // +0x08   0x80  Assembler         assembler       instruction data
    // +0x88   4     int               wavetableFront  waveform table context index
    // +0x8C   4     (padding)
    // +0x90   16    shared_ptr<Node>  node            AST node (may be null)
    // +0xA0   1     bool              noOpt           derived: (cmd-3) < 3u
    // +0xA1   7     (padding to 0xA8)
    // sizeof(AsmList::Asm) = 0xA8
    struct Asm {
        int sequenceId = 0;               // +0x00
        // +0x04: 4 bytes padding
        Assembler assembler;         // +0x08  (0x80 bytes)
        int wavetableFront = 0;           // +0x88  (dual-purpose; see lineNumber())
        // +0x8C: 4 bytes padding
        std::shared_ptr<Node> node;       // +0x90  (16 bytes: ptr + ctrl)
        bool noOpt = false;       // +0xA0
        // +0xA1..+0xA7: padding to 0xA8

        // The +0x88 int is dual-purpose depending on command type:
        //   - For most assembler commands it stores AsmCommands::wavetableFrontIndex_
        //     (set by emitEntry et al.) — accessed via `wavetableFront`.
        //   - For MESSAGE / ERROR_MSG instructions it holds a source line number
        //     (read by AsmOptimize::reportUserMessages at 0x280c02:
        //     `mov eax, [r13+0x88]`) — accessed via `lineNumber()`.
        // Same 4 bytes; accessor methods (not a separate field) so layout is stable.
        int& lineNumber()       { return wavetableFront; }
        int  lineNumber() const { return wavetableFront; }

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

        bool operator==(const Asm& other) const {
            return sequenceId == other.sequenceId
                && wavetableFront == other.wavetableFront
                && noOpt == other.noOpt;
        }
    };

    AsmList() = default;
    AsmList(std::vector<Asm> v) : entries(std::move(v)) {}
    AsmList& operator=(std::vector<Asm> v) { entries = std::move(v); return *this; }

    // --- Destructor: 0x11d5b0
    //   Iterates elements from end to begin, destroying each
    //   (release shared_ptr, destroy Assembler), then frees buffer.
    ~AsmList();

    // --- append(entry): 0x15d180
    //   Copy-appends an Asm record. Fast path: placement new if capacity
    //   allows; slow path: vector reallocation.
    //   Copies: sequenceId (int), Assembler (copy ctor), wavetableFront (int),
    //   node (shared_ptr copy + atomic inc), noOpt (bool).
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
    //                      suffix if noOpt && specific opcode check.
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

    // --- Container-like forwarding (code often uses AsmList directly as a container) ---
    using iterator = std::vector<Asm>::iterator;
    using const_iterator = std::vector<Asm>::const_iterator;
    iterator begin() { return entries.begin(); }
    iterator end() { return entries.end(); }
    const_iterator begin() const { return entries.begin(); }
    const_iterator end() const { return entries.end(); }
    const_iterator cbegin() const { return entries.cbegin(); }
    const_iterator cend() const { return entries.cend(); }
    size_t size() const { return entries.size(); }
    bool empty() const { return entries.empty(); }
    void reserve(size_t n) { entries.reserve(n); }
    void push_back(const Asm& e) { entries.push_back(e); }
    void push_back(Asm&& e) { entries.push_back(std::move(e)); }
    template<typename... Args>
    iterator insert(const_iterator pos, Args&&... args) { return entries.insert(pos, std::forward<Args>(args)...); }
    iterator erase(const_iterator pos) { return entries.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return entries.erase(first, last); }
    void clear() { entries.clear(); }

    // Insert range before the entry whose node matches the given placeholder.
    // Used by Prefetch::placeSingleCommand to splice instructions at placeholder positions.
    void insert(std::shared_ptr<Node> const& placeholder, AsmList& source) {
        insert(placeholder, source.begin(), source.end());
    }
    void insert(std::shared_ptr<Node> const& placeholder, const_iterator first, const_iterator last) {
        // Find the entry whose node == placeholder
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (it->node == placeholder) {
                entries.insert(it, first, last);
                return;
            }
        }
        // If not found, append at end
        entries.insert(entries.end(), first, last);
    }
    Asm& operator[](size_t i) { return entries[i]; }
    const Asm& operator[](size_t i) const { return entries[i]; }
    Asm& front() { return entries.front(); }
    const Asm& front() const { return entries.front(); }
    Asm& back() { return entries.back(); }
    const Asm& back() const { return entries.back(); }

    bool operator==(const AsmList& other) const { return entries == other.entries; }
    friend void swap(AsmList& a, AsmList& b) { std::swap(a.entries, b.entries); }

    // Implicit conversion from single Asm — prefetch code assigns Asm to AsmList
    AsmList(const Asm& single) { entries.push_back(single); }
    AsmList(Asm&& single) { entries.push_back(std::move(single)); }
    AsmList& operator=(const Asm& single) { entries.clear(); entries.push_back(single); return *this; }
    AsmList& operator=(Asm&& single) { entries.clear(); entries.push_back(std::move(single)); return *this; }

    // --- Storage: exactly std::vector<Asm> ---
    std::vector<Asm> entries;
};

// Free function alias — used in AsmCommandsImpl{Cervino,Hirzel}
// Maps to AsmList::Asm::createUniqueID(false).
inline int nextSequenceId() { return AsmList::Asm::createUniqueID(false); }

} // namespace zhinst
