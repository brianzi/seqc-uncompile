// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Node method implementations
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/node.hpp"

#include <sstream>
#include <unordered_map>

namespace zhinst {

// ============================================================================
// TLS counter for unique node IDs
// The counter is at offset +0x44 from the TLS block base (accessed via
// __tls_get_addr). The simple constructor reads and post-increments it.
// ============================================================================
static thread_local int node_id_counter = 0;  // TLS offset 0x44

// ============================================================================
// Simple constructor — 0x12ace0
//
// Allocates numWaveSlots optional<string> entries, sets id from TLS counter,
// zeros all pointer/vector fields, sets scalar defaults.
// ============================================================================
Node::Node(NodeType type, int numWaveSlots, int deviceIndex)
    : id(node_id_counter++)         // +0x10 — TLS counter, post-increment
    , deviceIndex(deviceIndex)      // +0x14
    , waves(numWaveSlots)           // +0x28 — vector of nullopt optional<string>
    , waveformIndex(-1)             // +0x40
    , type(type)                    // +0x44
    , playConfig1{}                 // +0x48 — zero-init (channelMask = -1 per binary)
    , playConfig2{}                 // +0x68
    , reg1{-1, false}              // +0x88 — AsmRegister(-1)
    , regVal(0)                     // +0x90
    , reg2{-1, false}              // +0x94
    , tableIndex(-1)                // +0x9C
    , weakRefs()                    // +0xA0 — empty
    , nextSibling(nullptr)          // +0xB8
    , children()                    // +0xC8 — empty
    , elseBranch(nullptr)           // +0xE0
    , parent()                      // +0xF0 — empty weak_ptr
    , intField1(-1)                 // +0x100
    , uintField(0)                  // +0x104
    , boolField1(false)             // +0x108
    , boolField2(false)             // +0x109
    , intField2(0)                  // +0x10C
{
    _reserved0 = 0;  // +0x18
    _reserved1 = 0;  // +0x20
}

// ============================================================================
// Full constructor (20 params) — 0x26c4a0
//
// Unlike the simple ctor, id is passed directly (NOT from TLS counter).
// weakRefs (+0xA0) is always initialized empty — no constructor parameter.
// ============================================================================
Node::Node(int id, int deviceIndex,
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
           int intField2)
    : id(id)
    , deviceIndex(deviceIndex)
    , waves(waves)                              // deep copy via __init_with_size
    , waveformIndex(waveformIndex)
    , type(type)
    , playConfig1(playConfig1)                  // POD copy (0x20 bytes)
    , playConfig2(playConfig2)
    , reg1(reg1)
    , regVal(regVal)
    , reg2(reg2)
    , tableIndex(tableIndex)
    , weakRefs()                                // always empty — no parameter
    , nextSibling(std::move(nextSibling))       // refcount incremented
    , children(children)                        // deep copy, each refcount++
    , elseBranch(std::move(elseBranch))
    , parent(std::move(parent))                 // weak refcount incremented
    , intField1(intField1)
    , uintField(uintField)
    , boolField1(boolField1)
    , boolField2(boolField2)
    , intField2(intField2)
{
    _reserved0 = 0;
    _reserved1 = 0;
}

// ============================================================================
// Destructor — 0x12afe0
//
// Destroys fields in reverse offset order:
// 1. parent (+0xF0): __release_weak on control block
// 2. elseBranch (+0xE0): shared_ptr decref → dispose → release_weak
// 3. children (+0xC8): reverse-iterate, decref each shared_ptr, free buffer
// 4. nextSibling (+0xB8): shared_ptr decref
// 5. weakRefs (+0xA0): reverse-iterate, __release_weak each, free buffer
// 6. waves (+0x28): reverse-iterate, destroy long strings, free buffer
// 7. enable_shared_from_this base: __release_weak if non-null
// ============================================================================
Node::~Node() = default;  // compiler-generated matches binary behavior

// ============================================================================
// type2str(NodeType) — 0x269970 (static)
//
// Switch on int value. Values 1..0x40 use a jump table (indexed by value-1).
// Higher values use explicit comparisons.
// ============================================================================
std::string Node::type2str(NodeType t) {  // 0x269970
    switch (static_cast<int>(t)) {
    case 0x0001: return "load";
    case 0x0002: return "play";
    case 0x0004: return "branch";
    case 0x0008: return "loop";
    case 0x0010: return "setvar";
    case 0x0020: return "rate";
    case 0x0040: return "lock";
    case 0x0080: return "unlock";
    case 0x0100: return "sync_cervino";
    case 0x0200: return "table";
    case 0x1000: return "setprecomp";
    case 0x2000: return "sync_hirzel";
    case 0x4000: return "plainload";
    case 0x8000: return "awg_ready";
    default:     return "unknnown";  // sic — typo matches binary
    }
}

// ============================================================================
// str2type(string const&) — 0x26ac00 (static)
//
// Reverse of type2str. Uses a lazily-initialized static unordered_map
// with __cxa_guard_acquire/release for thread safety. 14 entries.
// Throws std::out_of_range if not found.
// ============================================================================
NodeType Node::str2type(const std::string& s) {  // 0x26ac00
    static const std::unordered_map<std::string, NodeType> map = {
        {"load",          NodeType::Load},
        {"play",          NodeType::Play},
        {"branch",        NodeType::Branch},
        {"loop",          NodeType::Loop},
        {"setvar",        NodeType::SetVar},
        {"rate",          NodeType::Rate},
        {"lock",          NodeType::Lock},
        {"unlock",        NodeType::Unlock},
        {"sync_cervino",  NodeType::SyncCervino},
        {"table",         NodeType::Table},
        {"setprecomp",    NodeType::SetPrecomp},
        {"sync_hirzel",   NodeType::SyncHirzel},
        {"plainload",     NodeType::PlainLoad},
        {"awg_ready",     NodeType::AwgReady},
    };
    return map.at(s);  // throws std::out_of_range if not found
}

// ============================================================================
// toString() — 0x264440
//
// Returns "Node (asm id <deviceIndex>, type <type2str(type)>)"
// Note: uses deviceIndex (not id) as the printed "asm id".
// Non-const method (confirmed from mangled name).
// ============================================================================
std::string Node::toString() {  // 0x264440
    std::ostringstream ss;
    ss << "Node (asm id " << deviceIndex << ", type " << type2str(type) << ")";
    return ss.str();
}

// ============================================================================
// waveAtCurrentDeviceIndex() const — 0x1c7de0
//
// Returns waves[waveformIndex] if waveformIndex >= 0 and the optional is
// engaged, otherwise returns nullopt.
// Each optional<string> is 0x20 bytes; engaged flag at +0x18 within element.
// ============================================================================
std::optional<std::string> Node::waveAtCurrentDeviceIndex() const {  // 0x1c7de0
    if (waveformIndex < 0)
        return std::nullopt;
    return waves[waveformIndex];
}

// ============================================================================
// clone() const — 0x1d5d40
//
// Creates a new Node via simple ctor (type, waves.size(), deviceIndex),
// then copies a subset of fields. Does NOT copy playConfigs, tree links
// (nextSibling, elseBranch, children, parent), intField1, uintField, or bools.
// ============================================================================
std::shared_ptr<Node> Node::clone() const {  // 0x1d5d40
    auto n = std::make_shared<Node>(type,
                                    static_cast<int>(waves.size()),
                                    deviceIndex);
    n->waveformIndex = waveformIndex;
    n->waves = waves;
    n->weakRefs = weakRefs;
    n->reg1 = reg1;
    n->reg2 = reg2;
    n->tableIndex = tableIndex;
    n->intField2 = intField2;
    return n;
}

// ============================================================================
// last(node) — static, 0x1d5cb0
//
// Follows node->nextSibling chain until null, returns the tail node.
// ============================================================================
std::shared_ptr<Node> Node::last(std::shared_ptr<Node> node) {  // 0x1d5cb0
    while (node->nextSibling) {
        node = node->nextSibling;
    }
    return node;
}

// ============================================================================
// updateParent(parent, oldChild, newChild) — static, 0x1d2f50
//
// Replaces oldChild with newChild in parent's links. Checks in order:
// 1. parent->nextSibling == oldChild → replace with newChild
// 2. If parent is Branch type (0x4), scan parent->children for oldChild
//    → replace or erase (if newChild == nullptr)
// 3. parent->elseBranch == oldChild → replace with newChild
// Then sets newChild->parent = parent (as weak_ptr).
// ============================================================================
void Node::updateParent(std::shared_ptr<Node> parent,
                        std::shared_ptr<Node> oldChild,
                        std::shared_ptr<Node> newChild) {  // 0x1d2f50
    if (!parent)
        return;

    // Check nextSibling
    if (parent->nextSibling == oldChild) {
        parent->nextSibling = newChild;
    }
    // Check children (only for Branch type)
    else if (static_cast<int>(parent->type) == 0x4) {
        auto& ch = parent->children;
        for (auto it = ch.begin(); it != ch.end(); ++it) {
            if (*it == oldChild) {
                if (newChild) {
                    *it = newChild;
                } else {
                    ch.erase(it);
                }
                break;
            }
        }
    }
    // Check elseBranch
    else if (parent->elseBranch == oldChild) {
        parent->elseBranch = newChild;
    }

    // Set parent link on new child
    if (newChild) {
        newChild->parent = parent;
    }
}

// ============================================================================
// insertBefore(newNode) — 0x1cd860
//
// Inserts newNode immediately before this node in the sibling chain.
// After: ... → newNode → this → ...
// ============================================================================
void Node::insertBefore(std::shared_ptr<Node> newNode) {  // 0x1cd860
    newNode->nextSibling = shared_from_this();
    auto par = parent.lock();
    newNode->parent = parent;
    updateParent(par, shared_from_this(), newNode);
    parent = newNode;  // this node's parent becomes newNode
}

// ============================================================================
// remove(node) — static, 0x1d4440
//
// Removes node from the tree. If node has a nextSibling, splices it into
// parent's slot (so the chain stays connected). Then recursively removes
// elseBranch and each child in the children vector.
// ============================================================================
void Node::remove(std::shared_ptr<Node> node) {  // 0x1d4440
    auto par = node->parent.lock();

    if (node->nextSibling) {
        // Splice nextSibling into parent's slot
        updateParent(par, node, node->nextSibling);
        node->nextSibling = nullptr;
    } else {
        // No sibling — just remove from parent
        updateParent(par, node, nullptr);
    }

    // Recursively remove elseBranch
    if (node->elseBranch) {
        remove(node->elseBranch);
        node->elseBranch = nullptr;
    }

    // Recursively remove each child
    for (auto& child : node->children) {
        if (child) {
            remove(child);
        }
    }
    node->children.clear();
}

// ============================================================================
// swap(a, b) — static, 0x1d2720
//
// Precondition: b->parent must equal a (i.e., a is b's parent node).
// If this is not the case, throws ZIAWGCompilerException with error 0xa4.
//
// Walks up from `a` through Loop/Branch ancestors to find the first
// non-Loop/non-Branch ancestor node. Copies that ancestor's deviceIndex
// to `b` if it's > 0.
//
// Then performs three updateParent calls:
//   1. updateParent(parentOfA, a, b)        — put b in a's former slot
//   2. updateParent(b, b_nextSibling, a)    — inside b, replace b's old
//                                             nextSibling with a
//   3. updateParent(a, b, b_nextSibling)    — inside a, replace b with
//                                             b's old nextSibling
//
// Net effect: a and b exchange their structural positions — b takes a's
// place in the parent, and a becomes a child/sibling inside b.
// ============================================================================
void Node::swap(const std::shared_ptr<Node>& a,        // 0x1d2720
                const std::shared_ptr<Node>& b) {
    // --- Verify b->parent.get() == a.get() --- (0x1d273a..0x1d2788)
    Node* bNode = b.get();                             // 0x1d273a: r15 = *rbx

    {
        // 0x1d273d: rdi = bNode->parent.ctrl (+0xF8)
        auto bParentCtrl = bNode->parent.lock();       // 0x1d2749: lock()

        if (bParentCtrl) {
            // 0x1d2753: r12 = bNode->parent.ptr (+0xF0) — raw parent pointer
            Node* bParentRaw = bParentCtrl.get();
            // 0x1d275a: r13 = *r14 = a.get()
            Node* aRaw = a.get();

            // 0x1d2783: cmp r12, r13
            if (bParentRaw != aRaw) {                  // 0x1d2786: jne → throw
                goto throw_error;                      // 0x1d2788
            }
        } else {
            // 0x1d27c3: b has no parent ctrl block
            if (a.get() != nullptr) {                  // 0x1d27c3: cmpq $0, (*r14)
                goto throw_error;                      // 0x1d27c7: jne → 0x1d2788
            }
        }
    }

    {
        // --- Walk up from a through Loop/Branch ancestors --- (0x1d27c9..0x1d2868)
        // 0x1d27c9: current = a (copy shared_ptr, addref)
        std::shared_ptr<Node> current = a;             // rbp-0x38 / rbp-0x30

        // Loop: 0x1d27f0
        while (true) {
            Node* cur = current.get();                 // 0x1d27f0: r15 = rbp-0x38
            int t = static_cast<int>(cur->type);       // 0x1d27f4: eax = r15->0x44

            if (t == 8 || t == 4) {                    // 0x1d27f8/0x1d27fd: Loop or Branch
                // 0x1d2802: rdi = cur->parent.ctrl (+0xF8)
                auto parentLocked = cur->parent.lock();// 0x1d280e: call lock()
                if (!parentLocked) {                   // 0x1d2813/0x1d2816
                    // 0x1d2830: set current = {nullptr, nullptr}
                    current.reset();
                    // Next iteration reads cur->type from nullptr — but the
                    // binary just falls through with ecx=0,eax=0 which makes
                    // current = {0, 0} and loops to 0x1d27f0 where it would
                    // dereference null. This path is unreachable in practice
                    // (Loop/Branch always have parents). To match binary behavior:
                    break;
                }
                // 0x1d2818: get cur->parent.ptr (+0xF0)
                // 0x1d2834: current.ptr = cur->parent.ptr
                //           current.ctrl = parentLocked (swap old ctrl out)
                current = parentLocked;                // release old, take new
                // 0x1d2840..0x1d2868: release old ctrl, continue loop
                continue;
            }
            // 0x1d2800: type != 8 && type != 4 → break
            break;
        }

        // --- Copy deviceIndex to b if > 0 --- (0x1d286a..0x1d2878)
        Node* ancestor = current.get();                // r15 at this point
        int devIdx = ancestor->deviceIndex;            // 0x1d286a: eax = r15->0x14
        if (devIdx > 0) {                              // 0x1d286e: test eax,eax; jle
            bNode->deviceIndex = devIdx;               // 0x1d2872: (*rbx)->0x14 = eax
        }

        // --- Lock a's parent --- (0x1d2878..0x1d28ad)
        Node* aNode = a.get();                         // 0x1d2878: r15 = *r14
        std::shared_ptr<Node> parentOfA;               // rbp-0x80 / rbp-0x78
        {
            // 0x1d2882: rdi = aNode->parent.ctrl (+0xF8)
            auto locked = aNode->parent.lock();        // 0x1d288e: call lock()
            if (locked) {                              // 0x1d2897
                // 0x1d289c: parentOfA.ptr = aNode->parent.ptr (+0xF0)
                // 0x1d28a3: parentOfA.ctrl = locked
                parentOfA = locked;
            }
            // else: parentOfA stays null (0x1d28a9: xor eax; xor ecx)
        }

        // --- Save b's nextSibling --- (0x1d28ad..0x1d28cc)
        // 0x1d28ad: rsi = *rbx = b.get()
        // 0x1d28b0: rdx = b->nextSibling.ctrl (+0xC0), addref
        // 0x1d28b7: xmm0 = movups b+0xB8 (ptr+ctrl of nextSibling)
        std::shared_ptr<Node> bNextSibling = bNode->nextSibling;  // rbp-0x50/rbp-0x48

        // === updateParent call 1 === (0x1d2933)
        // Args: parentOfA (rbp-0x90 copy), a (rbp-0x130 copy), b (rbp-0x120 copy)
        // 0x1d28cc..0x1d2910: copy parentOfA, a, b to temporaries with addref
        // 0x1d2915..0x1d2930: lea args into r15, r12, r13
        // 0x1d2933: call updateParent
        updateParent(parentOfA, a, b);
        // 0x1d2938..0x1d29c0: destroy temp copies (3 shared_ptr destructors)

        // === updateParent call 2 === (0x1d2a2d)
        // Args: b (rbp-0x110 copy), bNextSibling (rbp-0x100 copy), a (rbp-0xf0 copy)
        // 0x1d29c5..0x1d2a0a: copy b, bNextSibling, a to temporaries with addref
        // 0x1d2a0f..0x1d2a2a: lea args
        // 0x1d2a2d: call updateParent
        updateParent(b, bNextSibling, a);
        // 0x1d2a32..0x1d2abf: destroy temp copies (3 shared_ptr destructors)

        // === updateParent call 3 === (0x1d2b27)
        // Args: a (rbp-0xe0 copy), b (rbp-0xd0 copy), bNextSibling (rbp-0xc0 copy)
        // 0x1d2abf..0x1d2b04: copy a, b, bNextSibling to temporaries with addref
        // 0x1d2b09..0x1d2b24: lea args
        // 0x1d2b27: call updateParent
        updateParent(a, b, bNextSibling);
        // 0x1d2b2c..0x1d2c3d: destroy all remaining locals:
        //   call3 tmps (rbp-0xb8, rbp-0xc8, rbp-0xd8)
        //   bNextSibling (rbp-0x48)
        //   parentOfA (rbp-0x78)
        //   current (rbp-0x30)

        return;  // 0x1d2c3d..0x1d2c4e: epilogue, ret
    }

throw_error:
    // 0x1d2788..0x1d2c9b: allocate exception, format message, throw
    {
        // 0x1d2788: __cxa_allocate_exception(0x60)
        // 0x1d2795: rdi = &errMsg (global ErrorMessages at 0x95de60)
        // 0x1d279c: esi = 0xa4 — ErrorMessageT index
        // 0x1d27a1: call errMsg[0xa4] → get error string
        // 0x1d2c60: ErrorMessages::format(ErrorMessageT(0xa2), errorString)
        // 0x1d2c7f: ZIAWGCompilerException(formatted_string)
        // 0x1d2c9b: __cxa_throw(exception, typeinfo, dtor)
        std::string errStr = errMsg[static_cast<ErrorMessageT>(0xa4)];
        std::string formatted = ErrorMessages::format(
            static_cast<ErrorMessageT>(0xa2), errStr);
        throw ZIAWGCompilerException(std::move(formatted));
    }
}

} // namespace zhinst
