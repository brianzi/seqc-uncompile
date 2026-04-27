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

#include "asm_register.hpp"
#include "play_config.hpp"

namespace zhinst {

// Forward declaration
class Node;

// NodeType — each value corresponds to a different AST node kind.
// Confirmed from Node::type2str() jump table at 0x269970.
// The jump table handles (value-1) for values 1..0x40, then explicit
// comparisons for higher powers of two.
enum class NodeType : int {
    Load               = 0x0001,   // "load"
    Play               = 0x0002,   // "play"
    Branch             = 0x0004,   // "branch"
    Loop               = 0x0008,   // "loop"
    SetVar             = 0x0010,   // "setvar"
    Rate               = 0x0020,   // "rate"
    Lock               = 0x0040,   // "lock"
    Unlock             = 0x0080,   // "unlock"
    SyncCervino        = 0x0100,   // "sync_cervino"
    Table              = 0x0200,   // "table"
    Prefetch           = 0x0400,   // "prefetch" (placeholder during prefetch pass)
    LockPlaceholder    = 0x0800,   // "lock_placeholder"
    SetPrecomp         = 0x1000,   // "setprecomp"
    SyncHirzel         = 0x2000,   // "sync_hirzel"
    PlainLoad          = 0x4000,   // "plainload"
    AwgReady           = 0x8000,   // "awg_ready"
    SetVarPlaceholder  = 0x10000,  // "setvar_placeholder"
    PrecompFlags       = 0x20000,  // "precomp_flags"
    SyncPlaceholderCervino = 0x40000, // "sync_placeholder_cervino"
    UnlockPlaceholder  = 0x80000,  // "unlock_placeholder"
    Placeholder        = 0x100000, // "placeholder" (generic prefetch placeholder)
    Wait               = 0x200000, // "wait" — wait node type
    // All other values → "unknnown" (sic — typo in binary)
};

// Allow bitwise operations and comparison with int (used in reconstructed code)
inline bool operator==(NodeType a, int b) { return static_cast<int>(a) == b; }
inline bool operator==(int a, NodeType b) { return a == static_cast<int>(b); }
inline bool operator!=(NodeType a, int b) { return static_cast<int>(a) != b; }
inline bool operator!=(int a, NodeType b) { return a != static_cast<int>(b); }
inline NodeType operator|(NodeType a, NodeType b) { return static_cast<NodeType>(static_cast<int>(a) | static_cast<int>(b)); }
inline NodeType operator&(NodeType a, NodeType b) { return static_cast<NodeType>(static_cast<int>(a) & static_cast<int>(b)); }

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

class Node : public std::enable_shared_from_this<Node> {
public:
    // --- Default constructor (needed for make_shared<Node>()) ---
    Node();

    // --- Simple constructor: Node(NodeType type, int asmId, int numWaveSlots)
    //     Address: 0x12ace0
    //     Allocates `numWaveSlots` optional<string> entries in wavesPerDev vector,
    //     sets nodeId from TLS counter, asmId from param, type at +0x44,
    //     deviceIndex = -1, lengthReg/indexOffsetReg = AsmRegister(-1), tableIndex = -1,
    //     globalRate = -1 (as 8-byte store 0x00000000FFFFFFFF).
    Node(NodeType type, int asmId, int numWaveSlots);

    // --- Full constructor (20 params): 0x26c4a0
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

    ~Node();  // 0x12afe0

    // Node::type2str(NodeType) — static, returns string name
    // Address: 0x269970
    static std::string type2str(NodeType t);

    // Node::str2type(string const&) — static, reverse of type2str
    // Address: 0x26ac00
    // Uses lazy-init static unordered_map; throws out_of_range if not found.
    static NodeType str2type(const std::string& s);

    // Node::toString() — "Node (asm id %d, type %s)"
    // Address: 0x264440
    //   Reads this->asmId (+0x14) and this->type (+0x44).
    std::string toString();

    // Node::waveAtCurrentDeviceIndex() const — 0x1c7de0
    //   Returns wavesPerDev[deviceIndex] if deviceIndex >= 0, else empty optional.
    //   Reads deviceIndex at +0x40, wavesPerDev.data() at +0x28.
    std::optional<std::string> waveAtCurrentDeviceIndex() const;

    // ---- Tree management methods ----

    // Node::last(node) — static, 0x1d5cb0
    //   Follows node->next chain until null, returns last node.
    static std::shared_ptr<Node> last(std::shared_ptr<Node> node);

    // Node::insertBefore(newNode) — 0x1cd860
    //   Inserts newNode before this node in the sibling chain.
    //   Sets newNode->next = shared_from_this(),
    //   copies this->parent to newNode->parent,
    //   calls updateParent(parent, this, newNode),
    //   then sets this->parent = newNode.
    void insertBefore(std::shared_ptr<Node> newNode);

    // Node::updateParent(parent, oldChild, newChild) — static, 0x1d2f50
    //   Replaces oldChild with newChild in parent's links (next, loop,
    //   or branches vector). Then sets newChild->parent = parent (as weak_ptr).
    //   If replacing with nullptr in branches vector, erases the element.
    static void updateParent(std::shared_ptr<Node> parent,
                             std::shared_ptr<Node> oldChild,
                             std::shared_ptr<Node> newChild);

    // Node::remove(node) — static, 0x1d4440
    //   Removes node from the tree. If node has next, splices it into
    //   parent's slot. Then recursively removes loop and each branch.
    static void remove(std::shared_ptr<Node> node);

    // Node::swap(a, b) — static, 0x1d2720
    //   Precondition: b->parent.get() == a.get() (a is b's parent).
    //   Walks up from a through Loop/Branch ancestors, copies asmId to b.
    //   Then: updateParent(parentOfA, a, b)
    //         updateParent(b, b->next, a)
    //         updateParent(a, b, b->next)
    //   Net effect: a and b exchange structural positions.
    //   Throws error 0xa4 if precondition violated.
    static void swap(const std::shared_ptr<Node>& a,
                     const std::shared_ptr<Node>& b);

    // Node::clone() const — 0x1d5d40
    //   Allocates new Node via simple ctor, copies: deviceIndex, wavesPerDev,
    //   play, lengthReg, indexOffsetReg, tableIndex, trig.
    //   Does NOT copy: configs, next/loop/branches/parent, globalRate, etc.
    std::shared_ptr<Node> clone() const;

    // ---- Serialization methods ----

    // Node::toJson(idMap) const — 0x264b90
    //   Serializes this Node to a boost::json::value (object).
    //   The idMap remaps internal nodeId values to serializable indices.
    //   Pointer fields serialized as integer IDs (-1 = null).
    boost::json::value toJson(const std::unordered_map<int,int>& idMap) const;

    // Node::fromJson(json) — static, 0x268280
    //   Deserializes scalar fields from JSON. Pointer fields left empty —
    //   reconnected later by installPointers().
    static std::shared_ptr<Node> fromJson(const boost::json::value& json);

    // Node::installPointers(nodeMap, json) — 0x269020
    //   Reconnects pointer fields (play, next, branches, loop, parent)
    //   after deserialization using the nodeMap (int ID → shared_ptr<Node>).
    void installPointers(const std::unordered_map<int, std::shared_ptr<Node>>& nodeMap,
                         const boost::json::value& json);

    // ---- Static thread_local counter for unique node IDs ----
    // Mangled: _ZN6zhinst4Node10idCounter_E
    // BSS-template offset 0x44 in the shared library's TLS module block.
    // Read+post-incremented by Node(NodeType,int,int) to assign nodeId.
    static thread_local int idCounter_;

    // ---- Fields (confirmed offsets, names from JSON keys) ----

    // +0x00, +0x08: inherited from enable_shared_from_this<Node>
    //   (weak_ptr<Node> member — pointer + control block)

    int nodeId;             // +0x10  — assigned from TLS counter (node_id++)
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
    std::weak_ptr<Node> loadRef;       // +0x18 (16 bytes: Node* + ctrl block*)

    std::vector<std::optional<std::string>> wavesPerDev;  // +0x28 (24 bytes)

    int deviceIndex = -1;   // +0x40  — index into wavesPerDev; -1 = invalid
    NodeType type;          // +0x44

    PlayConfig config;      // +0x48  (0x20 bytes) — primary play config
    PlayConfig currentCwvf; // +0x68  (0x20 bytes) — current CWVF config

    AsmRegister lengthReg;       // +0x88  (8 bytes: int value + bool valid)
    int length = 0;              // +0x90
    AsmRegister indexOffsetReg;  // +0x94  (8 bytes)
    int tableIndex = -1;         // +0x9C

    // +0xA0: vector of weak_ptr<Node> (24 bytes)
    // Destructor iterates and calls __release_weak() on each control block.
    // JSON key: "play". In installPointers, reconnected from "play" array.
    std::vector<std::weak_ptr<Node>> play;  // +0xA0

    // +0xB8: shared_ptr<Node> — next sibling in chain (16 bytes)
    //   last() follows this chain; insertBefore sets newNode->next = this;
    //   remove() splices this link; updateParent checks parent->next == oldChild.
    std::shared_ptr<Node> next;  // +0xB8

    // +0xC8: vector<shared_ptr<Node>> — child branches (24 bytes)
    //   Used when parent NodeType == Branch (0x4). updateParent scans this vector.
    std::vector<std::shared_ptr<Node>> branches;  // +0xC8

    // +0xE0: shared_ptr<Node> — loop/else-branch link (16 bytes)
    //   updateParent checks parent->loop == oldChild as alternative to next.
    //   remove() recursively removes this if present.
    std::shared_ptr<Node> loop;  // +0xE0

    // +0xF0: weak_ptr<Node> — parent link (16 bytes)
    //   Destructor calls __release_weak() on the control block at +0xF8.
    //   insertBefore/updateParent set newChild->parent = parentNode.
    std::weak_ptr<Node> parent;  // +0xF0

    int globalRate = -1;                     // +0x100  (rate for Rate nodes)
    unsigned int defaultPrecompFlags = 0;    // +0x104
    bool loopBodyRunsAtLeastOnce = false;    // +0x108
    bool branchMaySkipAllBodies = false;     // +0x109
    // +0x10A: 2 bytes padding
    int trig = 0;                            // +0x10C
    // Total size: 0x110
};

} // namespace zhinst
