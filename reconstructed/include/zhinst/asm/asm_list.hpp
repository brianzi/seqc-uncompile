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

        //! \brief Aliased view of `wavetableFront` reused as
        //!        a **source line number** for `MESSAGE` and
        //!        `ERROR_MSG` pseudo-instructions.
        //!
        //! \details The `+0x88` int is overloaded by command
        //! type: ordinary instructions store the
        //! waveform-table context index propagated from
        //! `AsmCommands::wavetableFrontIndex_`, while
        //! diagnostic pseudo-instructions store the source
        //! line they originated from.  Both views alias the
        //! same 4 bytes; the accessor exists purely for
        //! call-site clarity at message-reporting sites
        //! (e.g. `AsmOptimize::reportUserMessages`).
        //!
        //! \return  Mutable / const reference to the
        //!          source-line view of the dual-purpose
        //!          field.
        int& lineNumber()       { return wavetableFront; }
        //! \copydoc lineNumber()
        int  lineNumber() const { return wavetableFront; }

        //! \brief Release the entry's `node` and tear down
        //!        the embedded `Assembler`.
        //!
        //! \details Member-subobject dtors run in reverse
        //! declaration order: `node` (shared_ptr release,
        //! atomic-decrement of the control block) then
        //! `assembler` (frees its inline string and vector
        //! buffers).  The trivial `sequenceId`,
        //! `wavetableFront`, and `noOpt` fields require no
        //! special teardown.
        ~Asm();

        //! \brief Render this entry's IR `node` as a JSON
        //!        string.
        //!
        //! \details Calls `node->toJson(idMap)` to build a
        //! `boost::json::value`, then
        //! `boost::json::serialize`.  Used during
        //! `AsmList::serialize()` to embed placeholder-node
        //! state inline in the `.asm` text dump so the
        //! corresponding `deserialize()` can reconstruct
        //! the IR side of the instruction stream.  Caller
        //! must ensure `node` is non-null.
        //!
        //! \param idMap  Mapping from raw entry sequence
        //!               IDs to the dense IDs assigned in
        //!               `serialize()`'s pass 1; used to
        //!               rewrite intra-list references in
        //!               the JSON output.
        //! \return  Serialised JSON form of `node`.
        std::string serializeNodeToJsonString(
            const std::unordered_map<int,int>& idMap) const;

        //! \brief Allocate the next process-unique
        //!        instruction sequence ID, or reset the
        //!        counter.
        //!
        //! \details Backed by a thread-local `nextID`
        //! counter (TLS offset `0x40`).  When `reset` is
        //! `false`, returns the current value and
        //! post-increments; when `true`, resets the
        //! counter to `0` and returns `0`.  Always inlined
        //! in the binary; no standalone symbol exists.
        //! `nextSequenceId()` is the conventional alias
        //! for the post-increment form.
        //!
        //! \param reset  When `true`, zero the counter
        //!               instead of post-incrementing it.
        //! \return  The pre-increment counter value, or
        //!          `0` when resetting.
        static int createUniqueID(bool reset);

        //! \brief Compare two entries by their identity
        //!        bookkeeping fields only.
        //!
        //! \details Compares `sequenceId`, `wavetableFront`,
        //! and `noOpt` — the embedded `Assembler` and IR
        //! `node` are deliberately *not* part of the
        //! comparison.  The intent is identity / liveness
        //! comparison across passes that may rewrite the
        //! instruction in place.
        //!
        //! \param other  Entry to compare against.
        //! \return  `true` when the identity bookkeeping
        //!          fields match.
        bool operator==(const Asm& other) const {
            return sequenceId == other.sequenceId
                && wavetableFront == other.wavetableFront
                && noOpt == other.noOpt;
        }
    };

    //! \brief Default-construct an empty list.
    AsmList() = default;
    //! \brief Construct from a pre-built `std::vector<Asm>`,
    //!        adopting its storage by move.
    //!
    //! \param v  Source vector whose storage is moved into
    //!           the new list.
    AsmList(std::vector<Asm> v) : entries(std::move(v)) {}
    //! \brief Replace the entire entry vector by move-assign
    //!        from a raw `std::vector<Asm>`.
    //!
    //! \param v  Source vector whose storage is moved into
    //!           this list.
    //! \return  `*this`.
    AsmList& operator=(std::vector<Asm> v) { entries = std::move(v); return *this; }

    //! \brief Destroy the list and every `Asm` it contains.
    //!
    //! \details Iterates the entries from end to begin
    //! tearing each down (releasing its `node` shared_ptr
    //! and destroying the embedded `Assembler`) before
    //! freeing the vector buffer.
    ~AsmList();

    //! \brief Copy-append an instruction record at the end
    //!        of the list.
    //!
    //! \details Performs the standard `vector::push_back`
    //! placement-new on the fast path (when capacity
    //! suffices) or grows the buffer otherwise.  Copies
    //! every field of `entry` including the `node`
    //! shared_ptr (atomic-increment of its control block).
    //!
    //! \param entry  Instruction record to append.
    void append(const Asm& entry);

    //! \brief Render the list as human-readable disassembly.
    //!
    //! \details For each entry: when `showHeader` is `true`,
    //! prefixes `"<wavetableFront> (<sequenceId>): "`; when
    //! the opcode is real, appends `Assembler::str(true)`
    //! followed by a newline; when the opcode is the
    //! placeholder sentinel `-1`, behaviour depends on
    //! `showNode` and `node` — non-null nodes render as
    //! `"// placeholder: <node->toString()>\n"`, null
    //! nodes render as `"// <empty command>\n"` when
    //! `showNode` is `true`, and as a bare newline
    //! otherwise.
    //!
    //! \param showNode    Whether to print placeholder
    //!                    bodies for opcode-`-1` entries.
    //! \param os          Output stream.
    //! \param showHeader  Whether to prefix each line with
    //!                    the `wavetableFront`/`sequenceId`
    //!                    pair.
    void print(bool showNode, std::ostream& os, bool showHeader) const;

    //! \brief Serialise the list to its round-trip text form.
    //!
    //! \details Two-pass:
    //! - Pass 1 builds a dense `idMap`
    //!   (`unordered_map<int,int>`) assigning sequential
    //!   IDs to entries while skipping opcode-`4` entries
    //!   and placeholder entries with a null `node`.
    //! - Pass 2 emits one line per entry: real opcodes
    //!   render via `Assembler::str(true)` (with an optional
    //!   `" #disableOpt"` suffix when `noOpt` is set on a
    //!   qualifying opcode), and non-null placeholders
    //!   render as `"placeholder # " +
    //!   serializeNodeToJsonString(idMap)`.
    //!
    //! Used to round-trip the assembler stream through the
    //! ELF `.asm` section so that `deserialize` can later
    //! rebuild an equivalent in-memory list.
    //!
    //! \return  Multi-line serialised text form.
    std::string serialize() const;

    //! \brief Rebuild the list in place from text produced
    //!        by `serialize()`.
    //!
    //! \details Delegates to `parseStringToAsmList(str)`
    //! and move-assigns the parsed entries over the
    //! current contents.  Returns `*this` for chaining.
    //! Any error / warning text produced by the parser is
    //! discarded by this overload — call
    //! `parseStringToAsmList` directly to capture it.
    //!
    //! \param str  Text-form input to parse.
    //! \return  `*this`.
    AsmList& deserialize(const std::string& str);

    //! \brief Parse assembler text into a fresh `AsmList`,
    //!        returning any diagnostic output alongside.
    //!
    //! \details Uses the legacy `AWGAssembler` text parser
    //! against the device-constants for `AwgDeviceType` 2
    //! (HDAWG) — the device family is hardcoded because
    //! the text form is device-agnostic.  Each parsed
    //! expression becomes one `Asm` entry with a fresh
    //! `sequenceId` from `createUniqueID(false)`.
    //!
    //! \binarynote The device-type parameter is hardwired
    //!             to HDAWG (`AwgDeviceType(2)`) regardless
    //!             of which device originally produced the
    //!             text — round-tripping non-HDAWG
    //!             instructions relies on the encoded form
    //!             being device-portable.
    //!
    //! \param str  Assembler text to parse.
    //! \return  `(parsed_list, diagnostic_text)` pair; the
    //!          diagnostic string is empty on success.
    static std::tuple<AsmList, std::string> parseStringToAsmList(const std::string& str);

    //! \brief Highest register number referenced anywhere
    //!        in the list.
    //!
    //! \details Walks every entry calling
    //! `Assembler::highestRegisterNumber()` (which encodes
    //! a packed `int64`: bit 32 is a validity flag, the
    //! low 32 bits are the register number).  Returns `0`
    //! on an empty list.  Used by the register allocator
    //! to size its scratch tables.
    //!
    //! \return  Maximum register number seen across all
    //!          entries.
    int maxRegister() const;

    // --- Container-like forwarding (code often uses AsmList directly as a container) ---
    using iterator = std::vector<Asm>::iterator;
    using const_iterator = std::vector<Asm>::const_iterator;
    //! \brief Iterator to the first entry.
    //! \return  Mutable iterator at the list's beginning.
    iterator begin() { return entries.begin(); }
    //! \brief Iterator past the last entry.
    //! \return  Mutable iterator one past the list's end.
    iterator end() { return entries.end(); }
    //! \brief Const iterator to the first entry.
    //! \return  Const iterator at the list's beginning.
    const_iterator begin() const { return entries.begin(); }
    //! \brief Const iterator past the last entry.
    //! \return  Const iterator one past the list's end.
    const_iterator end() const { return entries.end(); }
    //! \brief Const iterator to the first entry (explicit
    //!        const overload).
    //! \return  Const iterator at the list's beginning.
    const_iterator cbegin() const { return entries.cbegin(); }
    //! \brief Const iterator past the last entry (explicit
    //!        const overload).
    //! \return  Const iterator one past the list's end.
    const_iterator cend() const { return entries.cend(); }
    //! \brief Number of entries currently in the list.
    //! \return  Entry count.
    size_t size() const { return entries.size(); }
    //! \brief Whether the list contains no entries.
    //! \return  `true` when the list is empty.
    bool empty() const { return entries.empty(); }
    //! \brief Pre-allocate storage for at least `n` entries.
    //! \param n  Minimum capacity to reserve.
    void reserve(size_t n) { entries.reserve(n); }
    //! \brief Copy-append an entry (vector forwarding).
    //! \param e  Entry to append.
    void push_back(const Asm& e) { entries.push_back(e); }
    //! \brief Move-append an entry (vector forwarding).
    //! \param e  Entry to append; moved from.
    void push_back(Asm&& e) { entries.push_back(std::move(e)); }
    //! \brief Forward to `std::vector::insert` for any
    //!        argument shape (single value, count+value,
    //!        iterator range, initializer list).
    //!
    //! \param pos    Position before which to insert.
    //! \param args   Forwarded arguments selecting the
    //!               underlying `std::vector::insert`
    //!               overload.
    //! \return  Iterator to the first inserted entry (or
    //!          `pos` when nothing was inserted).
    template<typename... Args>
    iterator insert(const_iterator pos, Args&&... args) { return entries.insert(pos, std::forward<Args>(args)...); }
    //! \brief Erase the entry at `pos`; returns iterator to
    //!        the next entry.
    //! \param pos  Position of the entry to erase.
    //! \return  Iterator to the entry that followed `pos`.
    iterator erase(const_iterator pos) { return entries.erase(pos); }
    //! \brief Erase entries in `[first, last)`; returns
    //!        iterator to the entry after `last`.
    //! \param first  Begin of the range to erase.
    //! \param last   End of the range to erase (exclusive).
    //! \return  Iterator to the entry that followed the
    //!          erased range.
    iterator erase(const_iterator first, const_iterator last) { return entries.erase(first, last); }
    //! \brief Remove every entry, leaving the list empty.
    void clear() { entries.clear(); }

    //! \brief Splice the entirety of `source`'s entries into
    //!        this list immediately before the entry whose
    //!        IR node matches `placeholder`.
    //!
    //! \details Convenience wrapper that calls
    //! `insert(placeholder, source.begin(), source.end())`.
    //! Used by the prefetch pass to expand a placeholder
    //! node into the full instruction sequence the pass
    //! produced for it.
    //!
    //! \param placeholder  IR node identifying the entry
    //!                     before which to splice.
    //! \param source       List whose entries are spliced
    //!                     in (read from begin to end).
    void insert(std::shared_ptr<Node> const& placeholder, AsmList& source) {
        insert(placeholder, source.begin(), source.end());
    }
    //! \brief Splice the iterator range `[first, last)` into
    //!        this list immediately before the entry whose
    //!        IR node matches `placeholder`.
    //!
    //! \details Linear scan over `entries` looking for the
    //! first entry whose `node` shared_ptr compares equal
    //! to `placeholder`; the range is inserted at that
    //! position.  When no entry matches, the range is
    //! appended at the end of the list — chosen
    //! deliberately so prefetch-pass output is never
    //! silently dropped.
    //!
    //! \param placeholder  IR node identifying the entry
    //!                     before which to splice.
    //! \param first        Begin of the range to splice in.
    //! \param last         End of the range to splice in
    //!                     (exclusive).
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
    //! \brief Indexed mutable access to an entry.
    //! \param i  Zero-based entry index.
    //! \return  Mutable reference to the entry at `i`.
    Asm& operator[](size_t i) { return entries[i]; }
    //! \brief Indexed const access to an entry.
    //! \param i  Zero-based entry index.
    //! \return  Const reference to the entry at `i`.
    const Asm& operator[](size_t i) const { return entries[i]; }
    //! \brief Reference to the first entry (UB on empty
    //!        list).
    //! \return  Mutable reference to the first entry.
    Asm& front() { return entries.front(); }
    //! \copydoc front()
    const Asm& front() const { return entries.front(); }
    //! \brief Reference to the last entry (UB on empty
    //!        list).
    //! \return  Mutable reference to the last entry.
    Asm& back() { return entries.back(); }
    //! \copydoc back()
    const Asm& back() const { return entries.back(); }

    //! \brief Element-wise equality on the underlying entry
    //!        vector (uses `Asm::operator==`'s identity
    //!        comparison — see its docs).
    //!
    //! \param other  List to compare against.
    //! \return  `true` when both lists have the same length
    //!          and pairwise-equal entries.
    bool operator==(const AsmList& other) const { return entries == other.entries; }
    //! \brief ADL `swap` — exchanges the two lists' entry
    //!        vectors in O(1).
    friend void swap(AsmList& a, AsmList& b) { std::swap(a.entries, b.entries); }

    // Implicit conversion from single Asm — prefetch code assigns Asm to AsmList
    //! \brief Implicit single-entry conversion: build a
    //!        one-element list from a single `Asm`.
    //!
    //! \details The prefetch pass and other call sites
    //! frequently treat a single instruction as a list of
    //! one; this ctor (and the matching assignments below)
    //! avoid forcing those sites to construct a vector
    //! explicitly.
    //!
    //! \param single  Entry to seed the new list with.
    AsmList(const Asm& single) { entries.push_back(single); }
    //! \copydoc AsmList(const Asm&)
    AsmList(Asm&& single) { entries.push_back(std::move(single)); }
    //! \brief Replace the list with a single-entry list
    //!        copied from `single`.
    //! \param single  Entry to seed the list with.
    //! \return  `*this`.
    AsmList& operator=(const Asm& single) { entries.clear(); entries.push_back(single); return *this; }
    //! \brief Replace the list with a single-entry list
    //!        moved from `single`.
    //! \param single  Entry to seed the list with; moved
    //!                from.
    //! \return  `*this`.
    AsmList& operator=(Asm&& single) { entries.clear(); entries.push_back(std::move(single)); return *this; }

    // --- Storage: exactly std::vector<Asm> ---
    std::vector<Asm> entries;
};

// Free function alias — used in AsmCommandsImpl{Cervino,Hirzel}
// Maps to AsmList::Asm::createUniqueID(false).
//! \brief Allocate the next process-unique instruction
//!        sequence ID.
//!
//! \details Convenience alias for
//! `AsmList::Asm::createUniqueID(false)`; used pervasively
//! by `AsmCommandsImplCervino` / `AsmCommandsImplHirzel`
//! when stamping freshly built `Asm` records with their
//! identity.
//!
//! \return  Post-incremented value of the thread-local
//!          sequence counter.
inline int nextSequenceId() { return AsmList::Asm::createUniqueID(false); }

} // namespace zhinst
