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
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
    SetPrecomp         = 0x1000,   // "setprecomp"
    SyncHirzel         = 0x2000,   // "sync_hirzel"  (NEW — not in previous enum)
    PlainLoad          = 0x4000,   // "plainload"    (was: Prefetch)
    AwgReady           = 0x8000,   // "awg_ready"    (NEW)
    // All other values → "unknnown" (sic — typo in binary)
};

// ============================================================================
// Node struct — complete field layout (0x110 bytes)
//
// Every offset confirmed from constructor stores & destructor teardown order.
//
// Offset  Size  Type                             Name
// ------  ----  ----                             ----
// 0x00      8   shared_ptr control block ptr     _sp_ptr        \  These two form
// 0x08      8   shared_ptr weak count ptr        _sp_ctrl       /  a shared_from_this base
//                                                                  (weak_ptr<Node> embedded)
// 0x10      4   int                              id             — from TLS counter
// 0x14      4   int                              deviceIndex
// 0x18     16   (zero-initialized)               _reserved      — two 8-byte zero fields
// 0x28     24   vector<optional<string>>          waves          — waveform names per device
// 0x40      4   int                              waveformIndex  — index into waves; -1 = none
// 0x44      4   NodeType (int)                   type
// 0x48     32   PlayConfig                       playConfig1
// 0x68     32   PlayConfig                       playConfig2
// 0x88      8   AsmRegister                      reg1           — stored as 8 bytes at 0x88
// 0x90      4   int                              regVal
// 0x94      8   AsmRegister                      reg2           — stored as 8 bytes at 0x94
// 0x9C      4   int                              tableIndex     — -1 = none
// 0xA0     16   vector<weak_ptr<Node>>            weakRefs       — dtor calls __release_weak
//                                                                  on each element
// 0xB0      8   (zero, part of weakRefs capacity?)
// 0xB8     16   shared_ptr<Node>                 sharedNode1    — e.g. someNodePtr param
// 0xC8     24   vector<shared_ptr<Node>>          children
// 0xE0     16   shared_ptr<Node>                 sharedNode2    — e.g. anotherNodePtr param
// 0xF0     16   weak_ptr<Node>                   parent         — parentWeak param
//                                                                  (ptr + ctrl, dtor releases weak)
// 0x100     4   int                              intField1      — e.g. rate
// 0x104     4   unsigned int                     uintField      — e.g. precompFlags
// 0x108     1   bool                             boolField1
// 0x109     1   bool                             boolField2
// 0x10A     2   (padding)
// 0x10C     4   int                              intField2
// ----------
// 0x110         Total size
// ============================================================================

class Node : public std::enable_shared_from_this<Node> {
public:
    // --- Simple constructor: Node(NodeType type, int numWaveSlots, int extra)
    //     Address: 0x12ace0
    //     Allocates `numWaveSlots` optional<string> entries in waves vector,
    //     sets id from TLS counter, deviceIndex from extra, type at +0x44,
    //     waveformIndex = -1, reg1/reg2 = AsmRegister(-1), tableIndex = -1,
    //     intField1 = -1 (as 8-byte store 0x00000000FFFFFFFF).
    Node(NodeType type, int numWaveSlots, int deviceIndex);

    // --- Full constructor (20 params): 0x26c4a0
    Node(int id, int deviceIndex,
         const std::vector<std::optional<std::string>>& waves,
         int waveformIndex,
         NodeType type,
         PlayConfig playConfig1,
         PlayConfig playConfig2,
         AsmRegister reg1,
         int regVal,
         AsmRegister reg2,
         int tableIndex,
         std::shared_ptr<Node> nextSibling,
         const std::vector<std::shared_ptr<Node>>& children,
         std::shared_ptr<Node> elseBranch,
         std::weak_ptr<Node> parent,
         int intField1,
         unsigned int uintField,
         bool boolField1,
         bool boolField2,
         int intField2);

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
    //   Reads this->deviceIndex (+0x14) and this->type (+0x44).
    std::string toString();

    // Node::waveAtCurrentDeviceIndex() const — 0x1c7de0
    //   Returns waves[waveformIndex] if waveformIndex >= 0, else empty optional.
    //   Reads waveformIndex at +0x40, waves.data() at +0x28.
    std::optional<std::string> waveAtCurrentDeviceIndex() const;

    // ---- Tree management methods ----

    // Node::last(node) — static, 0x1d5cb0
    //   Follows node->nextSibling chain until null, returns last node.
    static std::shared_ptr<Node> last(std::shared_ptr<Node> node);

    // Node::insertBefore(newNode) — 0x1cd860
    //   Inserts newNode before this node in the sibling chain.
    //   Sets newNode->nextSibling = shared_from_this(),
    //   copies this->parent to newNode->parent,
    //   calls updateParent(parent, this, newNode),
    //   then sets this->parent = newNode.
    void insertBefore(std::shared_ptr<Node> newNode);

    // Node::updateParent(parent, oldChild, newChild) — static, 0x1d2f50
    //   Replaces oldChild with newChild in parent's links (nextSibling, elseBranch,
    //   or children vector). Then sets newChild->parent = parent (as weak_ptr).
    //   If replacing with nullptr in children vector, erases the element.
    static void updateParent(std::shared_ptr<Node> parent,
                             std::shared_ptr<Node> oldChild,
                             std::shared_ptr<Node> newChild);

    // Node::remove(node) — static, 0x1d4440
    //   Removes node from the tree. If node has nextSibling, splices it into
    //   parent's slot. Then recursively removes elseBranch and each child.
    static void remove(std::shared_ptr<Node> node);

    // Node::swap(a, b) — static, 0x1d2720
    //   Precondition: b->parent.get() == a.get() (a is b's parent).
    //   Walks up from a through Loop/Branch ancestors, copies deviceIndex to b.
    //   Then: updateParent(parentOfA, a, b)
    //         updateParent(b, b->nextSibling, a)
    //         updateParent(a, b, b->nextSibling)
    //   Net effect: a and b exchange structural positions.
    //   Throws error 0xa4 if precondition violated.
    static void swap(const std::shared_ptr<Node>& a,
                     const std::shared_ptr<Node>& b);

    // Node::clone() const — 0x1d5d40
    //   Allocates new Node via simple ctor, copies: waveformIndex, waves,
    //   weakRefs, reg1, reg2, tableIndex, intField2.
    //   Does NOT copy: playConfigs, sharedNode1/2, children, parent, etc.
    std::shared_ptr<Node> clone() const;

    // ---- Fields (confirmed offsets) ----

    // +0x00, +0x08: inherited from enable_shared_from_this<Node>
    //   (weak_ptr<Node> member — pointer + control block)

    int id;                 // +0x10  — assigned from TLS counter (node_id++)
    int deviceIndex;        // +0x14  — device index

    // +0x18: 16 bytes zeroed in simple ctor (purpose unknown, possibly
    //        reserved or part of enable_shared_from_this padding)
    uint64_t _reserved0 = 0;  // +0x18
    uint64_t _reserved1 = 0;  // +0x20

    std::vector<std::optional<std::string>> waves;  // +0x28 (24 bytes: begin/end/cap)

    int waveformIndex = -1;   // +0x40  — index into waves; -1 = invalid
    NodeType type;            // +0x44

    PlayConfig playConfig1;   // +0x48  (0x20 bytes)
    PlayConfig playConfig2;   // +0x68  (0x20 bytes)

    AsmRegister reg1;         // +0x88  (8 bytes: int value + bool valid)
    int regVal = 0;           // +0x90
    AsmRegister reg2;         // +0x94  (8 bytes)
    int tableIndex = -1;      // +0x9C

    // +0xA0: vector of weak_ptr<Node> (24 bytes: begin/end/cap)
    // Destructor iterates and calls __release_weak() on each control block.
    std::vector<std::weak_ptr<Node>> weakRefs;  // +0xA0

    // +0xB8: shared_ptr<Node> — next sibling in tree (16 bytes: ptr + ctrl)
    //   last() follows this chain; insertBefore sets newNode->nextSibling = this;
    //   remove() splices this link; updateParent checks parent->nextSibling == oldChild.
    std::shared_ptr<Node> nextSibling;  // +0xB8

    // +0xC8: vector<shared_ptr<Node>> — child nodes (24 bytes: begin/end/cap)
    //   Used when parent NodeType == Branch (0x4). updateParent scans this vector.
    std::vector<std::shared_ptr<Node>> children;  // +0xC8

    // +0xE0: shared_ptr<Node> — auxiliary/else-branch link (16 bytes)
    //   updateParent checks parent->elseBranch == oldChild as alternative to nextSibling.
    //   remove() recursively removes this if present.
    std::shared_ptr<Node> elseBranch;  // +0xE0

    // +0xF0: weak_ptr<Node> — parent link (16 bytes: ptr + ctrl)
    //   Destructor calls __release_weak() on the control block at +0xF8.
    //   insertBefore/updateParent set newChild->parent = parentNode.
    std::weak_ptr<Node> parent;  // +0xF0

    int intField1 = -1;          // +0x100  (rate for Rate nodes)
    unsigned int uintField = 0;  // +0x104  (precompFlags for SetPrecomp nodes)
    bool boolField1 = false;     // +0x108
    bool boolField2 = false;     // +0x109
    // +0x10A: 2 bytes padding
    int intField2 = 0;           // +0x10C
    // Total size: 0x110
};

} // namespace zhinst
