// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Node — AST node for the intermediate representation
//
// Field layout fully confirmed from:
//   - Node::Node(NodeType, int, int) at 0x12ace0 (simple ctor)
//   - Node::Node(int, int, vector<optional<string>>&, int, NodeType,
//         PlayConfig, PlayConfig, AsmRegister, int, AsmRegister, int,
//         shared_ptr<Node>, vector<shared_ptr<Node>>&, shared_ptr<Node>,
//         weak_ptr<Node>, int, uint, bool, bool, int) at 0x26c4a0 (full ctor)
//   - Node::~Node() at 0x12afe0
//   - Node::type2str(NodeType) at 0x269970
//   - Node::toString() at 0x264440
//   - Node::waveAtCurrentDeviceIndex() at 0x1c7de0
//   - Node::toJson() at 0x264b90
//   - Node::fromJson() at 0x268280
//   - Node::installPointers() at 0x269020
//
// Field names confirmed from toJson/fromJson JSON keys.
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/json.hpp>

#include "zhinst/asm/asm_register.hpp"
#include "zhinst/waveform/play_config.hpp"

namespace zhinst {

// Forward declaration
class Node;

// NodeType — each value corresponds to a different AST node kind.
// Confirmed from Node::type2str() jump table at 0x269970.
// The jump table handles (value-1) for values 1..0x40, then explicit
// comparisons for higher powers of two.
//! \brief Discriminator carried by `Node::type` identifying the
//! sequencer action represented by an IR node.
//!
//! Values are powers of two so they can be combined into bitmasks
//! by tree-traversal predicates that filter on multiple kinds.
//! Every enumerator maps 1:1 to the canonical string emitted by
//! `Node::type2str()` (and accepted by `Node::str2type()`); the
//! mapping is also the JSON `"type"` key used by `toJson()` /
//! `fromJson()`.
enum class NodeType : int {
    //! `"load"` — load a waveform into the playback cache.
    Load               = 0x0001,   // "load"
    //! `"play"` — play one or more waveforms.
    Play               = 0x0002,   // "play"
    //! `"branch"` — conditional branch with alternatives in `Node::branches`.
    Branch             = 0x0004,   // "branch"
    //! `"loop"` — loop boundary with body link in `Node::loop`.
    Loop               = 0x0008,   // "loop"
    //! `"setvar"` — bind a length register before a variable-length play.
    SetVar             = 0x0010,   // "setvar"
    //! `"rate"` — global sample-rate marker; payload in `Node::globalRate`.
    Rate               = 0x0020,   // "rate"
    //! `"lock"` — lock a per-device waveform slot for the subtree.
    Lock               = 0x0040,   // "lock"
    //! `"unlock"` — release a slot previously locked by `Lock`.
    Unlock             = 0x0080,   // "unlock"
    //! `"sync_cervino"` — Cervino-family synchronisation barrier.
    SyncCervino        = 0x0100,   // "sync_cervino"
    //! `"table"` — execute a wavetable entry; payload in `Node::tableIndex`.
    Table              = 0x0200,   // "table"
    //! `"prefetch"` — prefetch placeholder inserted during the prefetch pass.
    Prefetch           = 0x0400,   // "prefetch" (placeholder during prefetch pass)
    //! `"lock_placeholder"` — placeholder later resolved into `Lock`.
    LockPlaceholder    = 0x0800,   // "lock_placeholder"
    //! `"setprecomp"` — set per-subtree default pre-compensation flags
    //! (`Node::defaultPrecompFlags`).
    SetPrecomp         = 0x1000,   // "setprecomp"
    //! `"sync_hirzel"` — Hirzel-family synchronisation barrier.
    SyncHirzel         = 0x2000,   // "sync_hirzel"
    //! `"plainload"` — load without prefetch annotations.
    PlainLoad          = 0x4000,   // "plainload"
    //! `"awg_ready"` — barrier produced at AWG-ready boundaries.
    AwgReady           = 0x8000,   // "awg_ready"
    //! `"setvar_placeholder"` — placeholder later resolved into `SetVar`.
    SetVarPlaceholder  = 0x10000,  // "setvar_placeholder"
    // 0x20000 — unused. (Earlier reconstruction had a spurious
    // "PrecompFlags = 0x20000" here; the binary only uses
    // SetPrecomp = 0x1000 for setPrecompClear-emitted nodes.
    // Removed in IF-199 cleanup.)
    SyncPlaceholderCervino = 0x40000, // "sync_placeholder_cervino"
    UnlockPlaceholder  = 0x80000,  // "unlock_placeholder"
    Placeholder        = 0x100000, // "placeholder" (generic prefetch placeholder)
    Wait               = 0x200000, // "wait" — wait node type
    // All other values → "unknnown" (sic — typo in binary)
};

// Allow bitwise operations and comparison with int (used in reconstructed code)
//! \name NodeType bitmask & raw-int operators
//! \brief Mixed-mode comparison (`==`, `!=`) of `NodeType` against raw
//! `int` and bitwise composition (`|`, `&`) of two `NodeType` values.
//! \details `NodeType` enumerators are powers of two so masks built
//! with `operator|` can be tested with `operator&` to filter a tree
//! walk on multiple kinds at once.  The `int` comparison overloads
//! exist because several reconstructed call sites still hold the
//! discriminator as a raw integer (e.g. after a JSON round-trip).
//! @{

//! \brief Returns `true` when `a`'s integer value equals `b`.
//! \param a NodeType operand. \param b Raw integer value.
//! \return `true` iff `int(a) == b`.
inline bool operator==(NodeType a, int b) { return static_cast<int>(a) == b; }
//! \brief Returns `true` when `a` equals `b`'s integer value.
//! \param a Raw integer value. \param b NodeType operand.
//! \return `true` iff `a == int(b)`.
inline bool operator==(int a, NodeType b) { return a == static_cast<int>(b); }
//! \brief Returns `true` when `a`'s integer value differs from `b`.
//! \param a NodeType operand. \param b Raw integer value.
//! \return `true` iff `int(a) != b`.
inline bool operator!=(NodeType a, int b) { return static_cast<int>(a) != b; }
//! \brief Returns `true` when `a` differs from `b`'s integer value.
//! \param a Raw integer value. \param b NodeType operand.
//! \return `true` iff `a != int(b)`.
inline bool operator!=(int a, NodeType b) { return a != static_cast<int>(b); }
//! \brief Bitwise OR of two `NodeType` values, returning a combined
//! mask cast back to `NodeType` for tree-walk predicates.
//! \param a First mask operand. \param b Second mask operand.
//! \return `NodeType(int(a) | int(b))`.
inline NodeType operator|(NodeType a, NodeType b) { return static_cast<NodeType>(static_cast<int>(a) | static_cast<int>(b)); }
//! \brief Bitwise AND of two `NodeType` values, returning the
//! intersection cast back to `NodeType` (zero / empty mask when no
//! bits overlap).
//! \param a First mask operand. \param b Second mask operand.
//! \return `NodeType(int(a) & int(b))`.
inline NodeType operator&(NodeType a, NodeType b) { return static_cast<NodeType>(static_cast<int>(a) & static_cast<int>(b)); }
//! @}

// ============================================================================
// Node struct — complete field layout (0x110 bytes)
//
// Every offset confirmed from constructor stores & destructor teardown order.
// Field names confirmed from toJson/fromJson JSON key strings in .rodata.
//
// Offset  Size  Type                             Name (JSON key)
// ------  ----  ----                             ----
// 0x00      8   shared_ptr control block ptr     _sp_ptr        \  These two form
// 0x08      8   shared_ptr weak count ptr        _sp_ctrl       /  a shared_from_this base
//                                                                  (weak_ptr<Node> embedded)
// 0x10      4   int                              nodeId         — from TLS counter (JSON: "nodeId")
// 0x14      4   int                              asmId          — device/asm index (JSON: "asmId")
// 0x18     16   (zero-initialized)               _reserved      — two 8-byte zero fields
// 0x28     24   vector<optional<string>>          wavesPerDev    — waveform names per device (JSON: "wavesPerDev")
// 0x40      4   int                              deviceIndex    — index into wavesPerDev; -1 = none (JSON: "deviceIndex")
// 0x44      4   NodeType (int)                   type           — (JSON: "type")
// 0x48     32   PlayConfig                       config         — primary play config (JSON: "config")
// 0x68     32   PlayConfig                       currentCwvf    — current CWVF config (JSON: "currentCwvf")
// 0x88      8   AsmRegister                      lengthReg      — length register (JSON: "lengthReg")
// 0x90      4   int                              length         — (JSON: "length")
// 0x94      8   AsmRegister                      indexOffsetReg — index offset register (JSON: "indexOffsetReg")
// 0x9C      4   int                              tableIndex     — -1 = none (JSON: "tableIndex")
// 0xA0     24   vector<weak_ptr<Node>>            play           — play references (JSON: "play")
// 0xB8     16   shared_ptr<Node>                 next           — next sibling in chain (JSON: "next")
// 0xC8     24   vector<shared_ptr<Node>>          branches       — child branches (JSON: "branches")
// 0xE0     16   shared_ptr<Node>                 loop           — loop/else branch link (JSON: "loop")
// 0xF0     16   weak_ptr<Node>                   parent         — parent link (JSON: "parent")
// 0x100     4   int                              globalRate     — rate for Rate nodes (JSON: "globalRate")
// 0x104     4   unsigned int                     defaultPrecompFlags — (JSON: "defaultPrecompFlags")
// 0x108     1   bool                             loopBodyRunsAtLeastOnce — (JSON: "loopBodyRunsAtLeastOnce")
// 0x109     1   bool                             branchMaySkipAllBodies  — (JSON: "branchMaySkipAllBodies")
// 0x10A     2   (padding)
// 0x10C     4   int                              trig           — trigger field (JSON: "trig")
// ----------
// 0x110         Total size
// ============================================================================

//! \brief Lowered intermediate-representation node produced by the frontend.
//!
//! `Node` is the central data structure of the post-lowering IR: every
//! sequencer action — load, play, branch, loop, lock, prefetch placeholder,
//! and friends — is represented by a `Node` whose `type` is a value from
//! `NodeType`.  Nodes are linked into a tree via `next` (sibling chain),
//! `branches` (alternative children for `Branch` nodes), `loop`
//! (loop-body / else-branch link), and a back-pointer `parent` weak-ref;
//! waveform usage per device is tracked in `wavesPerDev` with the active
//! slot selected by `deviceIndex`.
//!
//! Tree edits go through the static helpers `insertBefore`, `updateParent`,
//! `remove`, `swap`, and `clone`, which keep the parent/next/branches
//! invariants consistent.  `toJson` / `fromJson` / `installPointers`
//! serialise the IR for the on-disk node-graph cache; `installPointers`
//! must be called after `fromJson` to rebuild the cross-node references
//! that JSON stores as integer IDs.
class Node : public std::enable_shared_from_this<Node> {
public:
    // --- No default constructor ---
    //
    // The binary defines no `Node()` default constructor — only the
    // simple 3-arg ctor (`Node(NodeType, int asmId, int numWaveSlots)`
    // at 0x12ace0) and the full 20-arg ctor (at 0x26c4a0).
    // `Node::fromJson` uses the 20-arg ctor; every other call site uses
    // the 3-arg ctor.  No container in source default-constructs a
    // `Node` either.  (IF-327 resolved: an earlier reconstruction added
    // a synthesized `Node()` delegating to `Node(NodeType{0}, 0, -1)`,
    // but the `-1` for `numWaveSlots` would size a vector to SIZE_MAX
    // on first invocation; the ctor existed only because the recon
    // assumed a default ctor was needed, not because the binary has
    // one.  Confirmed absent via `nm -a ./_seqc_compiler.so | grep
    // zhinst4NodeC[12]`.)

    // --- Simple constructor: Node(NodeType type, int asmId, int numWaveSlots)
    //     Address: 0x12ace0
    //     Allocates `numWaveSlots` optional<string> entries in wavesPerDev vector,
    //     sets nodeId from TLS counter, asmId from param, type at +0x44,
    //     deviceIndex = -1, lengthReg/indexOffsetReg = AsmRegister(-1), tableIndex = -1,
    //     globalRate = -1 (as 8-byte store 0x00000000FFFFFFFF).
    //! \brief Constructs a node tagged `type` for sequencer `asmId`,
    //! reserving `numWaveSlots` waveform-slot entries in
    //! `wavesPerDev`.
    //! \details Assigns a fresh `nodeId` from the thread-local
    //! counter, sets `deviceIndex = -1`, registers
    //! (`lengthReg` / `indexOffsetReg`) and `tableIndex` to invalid,
    //! and `globalRate = -1`.  Tree links remain empty.
    //! \param type         Node-type tag stored at `+0x44`.
    //! \param asmId        Owning sequencer / assembler id stored at
    //!                     `+0x14`.
    //! \param numWaveSlots Number of `optional<string>` waveform
    //!                     slots to reserve in `wavesPerDev`.
    Node(NodeType type, int asmId, int numWaveSlots);

    // --- Full constructor (20 params): 0x26c4a0
    //! \brief Field-by-field constructor used by `fromJson` paths
    //! that already have every scalar in hand; takes ownership of
    //! the supplied `shared_ptr` tree links.
    //! \param nodeId                 Pre-assigned identifier; bypasses
    //!                               the thread-local counter.
    //! \param asmId                  Owning sequencer/assembler id.
    //! \param wavesPerDev            Per-device waveform slot vector.
    //! \param deviceIndex            Currently-selected device index
    //!                               (or `-1`).
    //! \param type                   Node-type tag.
    //! \param config                 Captured `play` config payload.
    //! \param currentCwvf            Captured `currentCwvf` payload.
    //! \param lengthReg              Register backing the length
    //!                               operand.
    //! \param length                 Constant length value (when
    //!                               `lengthReg` is invalid).
    //! \param indexOffsetReg         Register backing the wave-index
    //!                               offset operand.
    //! \param tableIndex             Command-table index (or `-1`).
    //! \param next                   `next` sibling link.
    //! \param branches               `branches` vector for `Branch`
    //!                               nodes.
    //! \param loop                   `loop` body link for `Loop`
    //!                               nodes.
    //! \param parent                 Weak link back to the parent.
    //! \param globalRate             Global sample rate annotation
    //!                               (or `-1`).
    //! \param defaultPrecompFlags    Default precompensation flags
    //!                               for the subtree.
    //! \param loopBodyRunsAtLeastOnce Loop-body always-runs hint.
    //! \param branchMaySkipAllBodies Branch-may-skip hint.
    //! \param trig                   Trigger annotation copied from
    //!                               the source.
    Node(int nodeId, int asmId,
         const std::vector<std::optional<std::string>>& wavesPerDev,
         int deviceIndex,
         NodeType type,
         PlayConfig config,
         PlayConfig currentCwvf,
         AsmRegister lengthReg,
         int length,
         AsmRegister indexOffsetReg,
         int tableIndex,
         std::shared_ptr<Node> next,
         const std::vector<std::shared_ptr<Node>>& branches,
         std::shared_ptr<Node> loop,
         std::weak_ptr<Node> parent,
         int globalRate,
         unsigned int defaultPrecompFlags,
         bool loopBodyRunsAtLeastOnce,
         bool branchMaySkipAllBodies,
         int trig);

    //! \brief Releases the embedded `shared_ptr` / `weak_ptr`
    //! members in declaration-reverse order.
    ~Node();  // 0x12afe0

    // Node::type2str(NodeType) — static, returns string name
    // Address: 0x269970
    //! \brief Returns the canonical string name of `t` (the same
    //! name used as the `"type"` JSON key).
    //! \param t NodeType discriminator to render.
    //! \return Canonical string name of `t`, or the literal
    //! `"unknown"` for any value not in the recognised set.
    static std::string type2str(NodeType t);

    // Node::str2type(string const&) — static, reverse of type2str
    // Address: 0x26ac00
    // Uses lazy-init static unordered_map; throws out_of_range if not found.
    //! \brief Inverse of `type2str`: looks up `s` in a lazily-built
    //! `unordered_map`.
    //! \param s Canonical type name to look up (case-sensitive,
    //! exact match against the names emitted by `type2str`).
    //! \return The matching NodeType.
    //! \throws std::out_of_range when `s` is not a recognised type
    //! name.
    static NodeType str2type(const std::string& s);

    // Node::toString() — "Node (asm id %d, type %s)"
    // Address: 0x264440
    //   Reads this->asmId (+0x14) and this->type (+0x44).
    //! \brief Renders a single-line debug representation:
    //! `"Node (asm id <asmId>, type <type2str(type)>)"`.
    //! \return Single-line debug string with this node's `asmId`
    //! and `type` interpolated in.
    std::string toString();

    // Node::waveAtCurrentDeviceIndex() const — 0x1c7de0
    //   Returns wavesPerDev[deviceIndex] if deviceIndex >= 0, else empty optional.
    //   Reads deviceIndex at +0x40, wavesPerDev.data() at +0x28.
    //! \brief Returns `wavesPerDev[deviceIndex]` when
    //! `deviceIndex >= 0`, otherwise `std::nullopt`.
    //! \return The waveform slot for the currently-selected device,
    //! or `std::nullopt` when no device is selected.
    std::optional<std::string> waveAtCurrentDeviceIndex() const;

    // ---- Tree management methods ----

    // Node::last(node) — static, 0x1d5cb0
    //   Follows node->next chain until null, returns last node.
    //! \brief Walks the `next` chain from `node` and returns the
    //! tail (the last node whose `next` is null).
    //! \param node Starting node; may be null (returns null).
    //! \return The tail of the `next` chain rooted at `node`.
    static std::shared_ptr<Node> last(std::shared_ptr<Node> node);

    // Node::insertBefore(newNode) — 0x1cd860
    //   Inserts newNode before this node in the sibling chain.
    //   Sets newNode->next = shared_from_this(),
    //   copies this->parent to newNode->parent,
    //   calls updateParent(parent, this, newNode),
    //   then sets this->parent = newNode.
    //! \brief Splices `newNode` immediately before `*this` in the
    //! sibling chain so the resulting order is
    //! `... -> newNode -> *this -> ...`, transferring `*this`'s
    //! parent link to `newNode` and re-pointing `*this->parent` at
    //! `newNode`.
    //! \param newNode Node to splice in immediately before `*this`.
    void insertBefore(std::shared_ptr<Node> newNode);

    // Node::updateParent(parent, oldChild, newChild) — static, 0x1d2f50
    //   Replaces oldChild with newChild in parent's links (next, loop,
    //   or branches vector). Then sets newChild->parent = parent (as weak_ptr).
    //   If replacing with nullptr in branches vector, erases the element.
    //! \brief Replaces `oldChild` with `newChild` in whichever of
    //! `parent`'s links currently holds it (`next`, the `branches`
    //! vector when `parent->type == Branch`, or `loop`), and sets
    //! `newChild->parent = parent` if `newChild` is non-null.
    //! \details Passing `newChild == nullptr` erases the entry when
    //! the old slot was a branches-vector element; for `next` /
    //! `loop` slots the link is simply cleared.  No-op if `parent`
    //! is null.
    //! \param parent   Parent node whose link is being rewritten;
    //!                 no-op when null.
    //! \param oldChild Existing child link to find inside `parent`.
    //! \param newChild Replacement (may be null to clear/erase the
    //!                 slot).
    static void updateParent(std::shared_ptr<Node> parent,
                             std::shared_ptr<Node> oldChild,
                             std::shared_ptr<Node> newChild);

    // Node::remove(node) — static, 0x1d4440
    //   Removes node from the tree. If node has next, splices it into
    //   parent's slot. Then recursively removes loop and each branch.
    //! \brief Detaches `node` from the tree: splices `node->next`
    //! into `node`'s parent slot when present (otherwise simply
    //! clears the slot), then recursively `remove`s `node->loop`
    //! and every entry of `node->branches` and clears those links.
    //! \param node Node to detach; no-op when null.
    static void remove(std::shared_ptr<Node> node);

    // Node::swap(a, b) — static, 0x1d2720
    //   Precondition: b->parent.get() == a.get() (a is b's parent).
    //   Walks up from a through Loop/Branch ancestors, copies asmId to b.
    //   Then: updateParent(parentOfA, a, b)
    //         updateParent(b, b->next, a)
    //         updateParent(a, b, b->next)
    //   Net effect: a and b exchange structural positions.
    //   Throws error 0xa4 if precondition violated.
    //! \brief Exchanges the structural positions of a parent `a`
    //! and its immediate child `b`: after the call, `b` occupies
    //! `a`'s former slot and `a` becomes a child of `b`.
    //! \details Walks up from `a` through `Loop`/`Branch` ancestors,
    //! copying that ancestor's `asmId` onto `b` if positive, then
    //! issues three `updateParent` calls to rewire the links.
    //! \param a Parent of `b` whose slot will be replaced by `b`.
    //! \param b Immediate child of `a`; must satisfy
    //!          `b->parent.lock() == a`.
    //! \throws ZIAWGCompilerException (`SwapNotConnected`, error
    //! code 0xa4) if `b->parent` does not lock to `a`.
    static void swap(const std::shared_ptr<Node>& a,
                     const std::shared_ptr<Node>& b);

    // Node::clone() const — 0x1d5d40
    //   Allocates new Node via simple ctor, copies: deviceIndex, wavesPerDev,
    //   play, lengthReg, indexOffsetReg, tableIndex, trig.
    //   Does NOT copy: configs, next/loop/branches/parent, globalRate, etc.
    //! \brief Returns a partial copy of this node: `type`, `asmId`,
    //! `wavesPerDev`, `deviceIndex`, `play`, `lengthReg`,
    //! `indexOffsetReg`, `tableIndex`, and `trig` are copied; the
    //! tree links (`next`, `loop`, `branches`, `parent`), the play
    //! configs, `globalRate`, `defaultPrecompFlags`, and the bool
    //! flags are deliberately left at their default-constructed
    //! values.
    //! \return Newly-allocated detached node carrying the partial
    //! field copy described above.
    std::shared_ptr<Node> clone() const;

    // ---- Serialization methods ----

    // Node::toJson(idMap) const — 0x264b90
    //   Serializes this Node to a boost::json::value (object).
    //   The idMap remaps internal nodeId values to serializable indices.
    //   Pointer fields serialized as integer IDs (-1 = null).
    //! \brief Serialises this node to a `boost::json::value`
    //! object.  Pointer fields (`next`, `loop`, `branches`,
    //! `parent`, `play`) are emitted as integer IDs taken from
    //! `idMap`; an unmapped or null pointer becomes `-1`.
    //! \param idMap Mapping from in-memory `nodeId` to the integer
    //! ID written into the JSON output.
    //! \return JSON object representation of this node.
    boost::json::value toJson(const std::unordered_map<int,int>& idMap) const;

    // Node::fromJson(json) — static, 0x268280
    //   Deserializes scalar fields from JSON. Pointer fields left empty —
    //   reconnected later by installPointers().
    //! \brief Allocates a fresh node and populates only its scalar
    //! fields from `json`; pointer fields are left empty and must
    //! be reconnected by a subsequent `installPointers` call once
    //! every node in the graph has been deserialised.
    //! \param json JSON object previously produced by `toJson`.
    //! \return Newly-allocated node with scalar fields populated;
    //! pointer fields remain empty.
    static std::shared_ptr<Node> fromJson(const boost::json::value& json);

    // Node::installPointers(nodeMap, json) — 0x269020
    //   Reconnects pointer fields (play, next, branches, loop, parent)
    //   after deserialization using the nodeMap (int ID → shared_ptr<Node>).
    //! \brief Second-pass deserialisation step: reconnects this
    //! node's pointer fields (`play`, `next`, `branches`, `loop`,
    //! `parent`) by looking up the integer IDs stored in `json`
    //! through `nodeMap`.
    //! \param nodeMap Map from the integer IDs emitted by `toJson`
    //! to the live `shared_ptr<Node>` instances produced by
    //! `fromJson`.
    //! \param json    The same JSON object originally passed to
    //! `fromJson` for this node.
    void installPointers(const std::unordered_map<int, std::shared_ptr<Node>>& nodeMap,
                         const boost::json::value& json);

    // ---- Static thread_local counter for unique node IDs ----
    // Mangled: _ZN6zhinst4Node10idCounter_E
    // BSS-template offset 0x44 in the shared library's TLS module block.
    // Read+post-incremented by Node(NodeType,int,int) to assign nodeId.
    //! \brief Thread-local monotonic counter that supplies each
    //! freshly-constructed node's `nodeId`; read and
    //! post-incremented by the `(NodeType, int, int)` constructor.
    static thread_local int idCounter_;

    // ---- Fields (confirmed offsets, names from JSON keys) ----

    // +0x00, +0x08: inherited from enable_shared_from_this<Node>
    //   (weak_ptr<Node> member — pointer + control block)

    //! \brief Unique node identifier assigned from `idCounter_`.
    int nodeId;             // +0x10  — assigned from TLS counter (node_id++)
    //! \brief Owning sequencer / assembler index this node targets.
    int asmId;              // +0x14  — asm/device index

    // +0x18, +0x20: Load reference — weak_ptr<Node>.
    //   Prefetch::assignLoad sets this to a load node (incrementing weak refcount
    //   on the load's control block). Prefetch::createLoad and other consumers
    //   resolve via loadRef.lock(). Dtor calls __release_weak on +0x20.
    //
    //   Binary layout (libc++): +0x18 = raw Node*, +0x20 = __shared_weak_count*.
    //   Build host (libstdc++) weak_ptr<T> is also 16 bytes with the same layout
    //   semantics (object_ptr + control_block_ptr), so source-level weak_ptr
    //   matches the binary's footprint at this offset range. See
    //   notes/libcpp_abi.md for ABI details.
    //
    //   Source aliases (all map here):
    //     loadNode, loadPtr, load, loadRef → +0x18..+0x20
    //! \brief Weak link to the `Load` node responsible for staging
    //! this node's waveform; set by the prefetch pass and consumed
    //! by `Prefetch::createLoad` and friends.
    std::weak_ptr<Node> loadRef;       // +0x18 (16 bytes: Node* + ctrl block*)

    //! \brief One waveform-slot name per device the node may target;
    //! the active entry is `wavesPerDev[deviceIndex]`.
    std::vector<std::optional<std::string>> wavesPerDev;  // +0x28 (24 bytes)

    //! \brief Index into `wavesPerDev` selecting the currently
    //! targeted device; `-1` means none.
    int deviceIndex = -1;   // +0x40  — index into wavesPerDev; -1 = invalid
    //! \brief `NodeType` discriminator identifying the action this
    //! node performs.
    NodeType type;          // +0x44

    //! \brief Primary play configuration (waveform indices, marker
    //! bits, channel routing) associated with this node.
    PlayConfig config;      // +0x48  (0x20 bytes) — primary play config
    //! \brief Currently-active CWVF (compressed waveform) play
    //! configuration when the node belongs to a CWVF subtree.
    PlayConfig currentCwvf; // +0x68  (0x20 bytes) — current CWVF config

    //! \brief Register backing the length operand for
    //! variable-length plays; invalid when length is a literal.
    AsmRegister lengthReg;       // +0x88  (8 bytes: int value + bool valid)
    //! \brief Literal length value used when `lengthReg` is invalid.
    int length = 0;              // +0x90
    //! \brief Register backing the wave-index offset operand;
    //! invalid when no offset register is bound.
    AsmRegister indexOffsetReg;  // +0x94  (8 bytes)
    //! \brief Command-table entry index for `Table` nodes; `-1` when
    //! the node does not reference a wavetable entry.
    int tableIndex = -1;         // +0x9C

    // +0xA0: vector of weak_ptr<Node> (24 bytes)
    // Destructor iterates and calls __release_weak() on each control block.
    // JSON key: "play". In installPointers, reconnected from "play" array.
    //! \brief Weak references to the `Play` nodes that consume the
    //! waveform staged by this node (typically populated on `Load`
    //! nodes).
    std::vector<std::weak_ptr<Node>> play;  // +0xA0

    //! \brief Strong link to the next sibling in the IR chain;
    //! `nullptr` marks the chain tail.
    // +0xB8: shared_ptr<Node> — next sibling in chain (16 bytes)
    //   last() follows this chain; insertBefore sets newNode->next = this;
    //   remove() splices this link; updateParent checks parent->next == oldChild.
    std::shared_ptr<Node> next;  // +0xB8

    //! \brief Alternative-branch children for `Branch` nodes; each
    //! entry is the head of one branch's `next` chain.
    // +0xC8: vector<shared_ptr<Node>> — child branches (24 bytes)
    //   Used when parent NodeType == Branch (0x4). updateParent scans this vector.
    std::vector<std::shared_ptr<Node>> branches;  // +0xC8

    //! \brief Body link for `Loop` nodes (and else-branch alias on
    //! `Branch` nodes) pointing at the head of the contained chain.
    // +0xE0: shared_ptr<Node> — loop/else-branch link (16 bytes)
    //   updateParent checks parent->loop == oldChild as alternative to next.
    //   remove() recursively removes this if present.
    std::shared_ptr<Node> loop;  // +0xE0

    //! \brief Weak back-link to this node's parent, maintained by
    //! `updateParent`.
    // +0xF0: weak_ptr<Node> — parent link (16 bytes)
    //   Destructor calls __release_weak() on the control block at +0xF8.
    //   insertBefore/updateParent set newChild->parent = parentNode.
    std::weak_ptr<Node> parent;  // +0xF0

    //! \brief Sample-rate value carried by `Rate` nodes; `-1` when
    //! unset.
    int globalRate = -1;                     // +0x100  (rate for Rate nodes)
    //! \brief Default precompensation flag mask applied to the
    //! subtree rooted at this node; emitted by `SetPrecomp` nodes.
    unsigned int defaultPrecompFlags = 0;    // +0x104
    //! \brief Set when the compiler can prove a `Loop` node's body
    //! executes at least once (enables hoisting optimisations).
    bool loopBodyRunsAtLeastOnce = false;    // +0x108
    //! \brief Set when at least one branch alternative may skip the
    //! entire body (forces conservative placement decisions).
    bool branchMaySkipAllBodies = false;     // +0x109
    // +0x10A: 2 bytes padding
    //! \brief Trigger annotation copied from the source statement.
    int trig = 0;                            // +0x10C
    // Total size: 0x110
};

} // namespace zhinst
