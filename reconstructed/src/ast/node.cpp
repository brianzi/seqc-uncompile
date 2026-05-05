// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Node method implementations
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/ast/node.hpp"
#include "zhinst/core/error_messages.hpp"

#include <sstream>
#include <unordered_map>

namespace zhinst {

// ============================================================================
// TLS counter for unique node IDs
// Mangled: _ZN6zhinst4Node10idCounter_E
// BSS-template offset +0x44 in the shared library's TLS module block.
// The simple constructor reads and post-increments it.
// ============================================================================
thread_local int Node::idCounter_ = 0;  // TLS offset 0x44

// ============================================================================
// Default constructor addition.
//
// Not defined in the binary (no symbol `_ZN6zhinst4NodeC1Ev` present).
// Caller: asm_commands. Likely used for default-init in a Node-valued
// container or POD-style aggregate. Delegates to the 3-arg ctor with
// neutral defaults (NodeType=Invalid/0, no wave slots, asmId=-1).
//
// All scalar fields get the same defaults as the 3-arg ctor; the 3-arg
// ctor in turn handles vector/weak_ptr/shared_ptr default-init.
// ============================================================================
Node::Node()
    : Node(NodeType{0}, 0, -1)
{}

// ============================================================================
// Simple constructor — 0x12ace0
//
// Allocates numWaveSlots optional<string> entries, sets nodeId from TLS counter,
// zeros all pointer/vector fields, sets scalar defaults.
// ============================================================================
Node::Node(NodeType type, int asmId, int numWaveSlots)
    : nodeId(idCounter_++)              // +0x10 — TLS counter, post-increment
    , asmId(asmId)                      // +0x14
    , wavesPerDev(numWaveSlots)         // +0x28 — vector of nullopt optional<string>
    , deviceIndex(-1)                   // +0x40
    , type(type)                        // +0x44
    , config{}                          // +0x48 — zero-init
    , currentCwvf{}                     // +0x68
    , lengthReg{-1, false}             // +0x88 — AsmRegister(-1)
    , length(0)                         // +0x90
    , indexOffsetReg{-1, false}        // +0x94
    , tableIndex(-1)                    // +0x9C
    , play()                            // +0xA0 — empty
    , next(nullptr)                     // +0xB8
    , branches()                        // +0xC8 — empty
    , loop(nullptr)                     // +0xE0
    , parent()                          // +0xF0 — empty weak_ptr
    , globalRate(-1)                    // +0x100
    , defaultPrecompFlags(0)            // +0x104
    , loopBodyRunsAtLeastOnce(false)    // +0x108
    , branchMaySkipAllBodies(false)     // +0x109
    , trig(0)                           // +0x10C
{
    // loadRef default-initializes to empty weak_ptr (+0x18..+0x20 zero)
}

// ============================================================================
// Full constructor (20 params) — 0x26c4a0
//
// Unlike the simple ctor, nodeId is passed directly (NOT from TLS counter).
// play (+0xA0) is always initialized empty — no constructor parameter.
// ============================================================================
Node::Node(int nodeId, int asmId,
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
           int trig)
    : nodeId(nodeId)
    , asmId(asmId)
    , wavesPerDev(wavesPerDev)                      // deep copy via __init_with_size
    , deviceIndex(deviceIndex)
    , type(type)
    , config(config)                                // POD copy (0x20 bytes)
    , currentCwvf(currentCwvf)
    , lengthReg(lengthReg)
    , length(length)
    , indexOffsetReg(indexOffsetReg)
    , tableIndex(tableIndex)
    , play()                                        // always empty — no parameter
    , next(std::move(next))                         // refcount incremented
    , branches(branches)                            // deep copy, each refcount++
    , loop(std::move(loop))
    , parent(std::move(parent))                     // weak refcount incremented
    , globalRate(globalRate)
    , defaultPrecompFlags(defaultPrecompFlags)
    , loopBodyRunsAtLeastOnce(loopBodyRunsAtLeastOnce)
    , branchMaySkipAllBodies(branchMaySkipAllBodies)
    , trig(trig)
{
    // loadRef default-initializes to empty weak_ptr
}

// ============================================================================
// Destructor — 0x12afe0
//
// Destroys fields in reverse offset order:
// 1. parent (+0xF0): __release_weak on control block
// 2. loop (+0xE0): shared_ptr decref → dispose → release_weak
// 3. branches (+0xC8): reverse-iterate, decref each shared_ptr, free buffer
// 4. next (+0xB8): shared_ptr decref
// 5. play (+0xA0): reverse-iterate, __release_weak each, free buffer
// 6. wavesPerDev (+0x28): reverse-iterate, destroy long strings, free buffer
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
// Returns "Node (asm id <asmId>, type <type2str(type)>)"
// Note: uses asmId (+0x14, not nodeId) as the printed "asm id".
// Non-const method (confirmed from mangled name).
// ============================================================================
std::string Node::toString() {  // 0x264440
    std::ostringstream ss;
    ss << "Node (asm id " << asmId << ", type " << type2str(type) << ")";
    return ss.str();
}

// ============================================================================
// waveAtCurrentDeviceIndex() const — 0x1c7de0
//
// Returns wavesPerDev[deviceIndex] if deviceIndex >= 0 and the optional is
// engaged, otherwise returns nullopt.
// Each optional<string> is 0x20 bytes; engaged flag at +0x18 within element.
// ============================================================================
std::optional<std::string> Node::waveAtCurrentDeviceIndex() const {  // 0x1c7de0
    if (deviceIndex < 0)
        return std::nullopt;
    return wavesPerDev[deviceIndex];
}

// ============================================================================
// clone() const — 0x1d5d40
//
// Creates a new Node via simple ctor (type, wavesPerDev.size(), asmId),
// then copies a subset of fields. Does NOT copy configs, tree links
// (next, loop, branches, parent), globalRate, defaultPrecompFlags, or bools.
// ============================================================================
std::shared_ptr<Node> Node::clone() const {  // 0x1d5d40
    auto n = std::make_shared<Node>(type,
                                    asmId,
                                    static_cast<int>(wavesPerDev.size()));
    n->deviceIndex = deviceIndex;
    n->wavesPerDev = wavesPerDev;
    n->play = play;
    n->lengthReg = lengthReg;
    n->indexOffsetReg = indexOffsetReg;
    n->tableIndex = tableIndex;
    n->trig = trig;
    return n;
}

// ============================================================================
// last(node) — static, 0x1d5cb0
//
// Follows node->next chain until null, returns the tail node.
// ============================================================================
std::shared_ptr<Node> Node::last(std::shared_ptr<Node> node) {  // 0x1d5cb0
    while (node->next) {
        node = node->next;
    }
    return node;
}

// ============================================================================
// updateParent(parent, oldChild, newChild) — static, 0x1d2f50
//
// Replaces oldChild with newChild in parent's links. Checks in order:
// 1. parent->next == oldChild → replace with newChild
// 2. If parent is Branch type (0x4), scan parent->branches for oldChild
//    → replace or erase (if newChild == nullptr)
// 3. parent->loop == oldChild → replace with newChild
// Then sets newChild->parent = parent (as weak_ptr).
// ============================================================================
void Node::updateParent(std::shared_ptr<Node> parent,
                        std::shared_ptr<Node> oldChild,
                        std::shared_ptr<Node> newChild) {  // 0x1d2f50
    if (!parent)
        return;

    // Check next
    if (parent->next == oldChild) {
        parent->next = newChild;
    }
    // Check branches (only for Branch type)
    else if (static_cast<int>(parent->type) == 0x4) {
        auto& ch = parent->branches;
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
    // Check loop
    else if (parent->loop == oldChild) {
        parent->loop = newChild;
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
    newNode->next = shared_from_this();
    auto par = parent.lock();
    newNode->parent = parent;
    updateParent(par, shared_from_this(), newNode);
    parent = newNode;  // this node's parent becomes newNode
}

// ============================================================================
// remove(node) — static, 0x1d4440
//
// Removes node from the tree. If node has a next sibling, splices it into
// parent's slot (so the chain stays connected). Then recursively removes
// loop and each child in the branches vector.
// ============================================================================
void Node::remove(std::shared_ptr<Node> node) {  // 0x1d4440
    auto par = node->parent.lock();

    if (node->next) {
        // Splice next into parent's slot
        updateParent(par, node, node->next);
        node->next = nullptr;
    } else {
        // No sibling — just remove from parent
        updateParent(par, node, nullptr);
    }

    // Recursively remove loop
    if (node->loop) {
        remove(node->loop);
        node->loop = nullptr;
    }

    // Recursively remove each branch
    for (auto& child : node->branches) {
        if (child) {
            remove(child);
        }
    }
    node->branches.clear();
}

// ============================================================================
// swap(a, b) — static, 0x1d2720
//
// Precondition: b->parent must equal a (i.e., a is b's parent node).
// If this is not the case, throws ZIAWGCompilerException with error 0xa4.
//
// Walks up from `a` through Loop/Branch ancestors to find the first
// non-Loop/non-Branch ancestor node. Copies that ancestor's asmId
// to `b` if it's > 0.
//
// Then performs three updateParent calls:
//   1. updateParent(parentOfA, a, b)        — put b in a's former slot
//   2. updateParent(b, b_next, a)           — inside b, replace b's old
//                                             next with a
//   3. updateParent(a, b, b_next)           — inside a, replace b with
//                                             b's old next
//
// Net effect: a and b exchange their structural positions — b takes a's
// place in the parent, and a becomes a child/sibling inside b.
// ============================================================================
namespace {
// Shared throw used by the two precondition failures in Node::swap.
// Originally a `goto throw_error;` in the binary CFG (jne 0x...) — extracted
// into a helper so the function body has no gotos. Both call sites are
// post-`return` paths and share no scope, so this transformation is purely
// structural (no codegen impact on the success path).
[[noreturn]] void throwSwapNotConnected() {
    std::string errStr = errMsg[SwapNotConnected];
    std::string formatted = ErrorMessages::format(
        PrefetchError, errStr);
    throw ZIAWGCompilerException(std::move(formatted));
}
}  // namespace

void Node::swap(const std::shared_ptr<Node>& a,        // 0x1d2720
                const std::shared_ptr<Node>& b) {
    // --- Verify b->parent.get() == a.get() --- (0x1d273a..0x1d2788)
    Node* bNode = b.get();                             // 0x1d273a

    {
        auto bParentCtrl = bNode->parent.lock();       // 0x1d2749

        if (bParentCtrl) {
            Node* bParentRaw = bParentCtrl.get();
            Node* aRaw = a.get();
            if (bParentRaw != aRaw) {
                throwSwapNotConnected();
            }
        } else {
            if (a.get() != nullptr) {
                throwSwapNotConnected();
            }
        }
    }

    // --- Walk up from a through Loop/Branch ancestors --- (0x1d27c9..0x1d2868)
    std::shared_ptr<Node> current = a;

    while (true) {
        Node* cur = current.get();
        int t = static_cast<int>(cur->type);

        if (t == 8 || t == 4) {  // Loop or Branch
            auto parentLocked = cur->parent.lock();
            if (!parentLocked) {
                current.reset();
                break;
            }
            current = parentLocked;
            continue;
        }
        break;
    }

    // --- Copy asmId to b if > 0 --- (0x1d286a..0x1d2878)
    Node* ancestor = current.get();
    int asmIdToCopy = ancestor->asmId;                  // 0x1d286a: eax = r15->0x14
    if (asmIdToCopy > 0) {
        bNode->asmId = asmIdToCopy;
    }

    // --- Lock a's parent ---
    Node* aNode = a.get();
    std::shared_ptr<Node> parentOfA;
    {
        auto locked = aNode->parent.lock();
        if (locked) {
            parentOfA = locked;
        }
    }

    // --- Save b's next ---
    std::shared_ptr<Node> bNext = bNode->next;

    // === updateParent call 1 === (0x1d2933)
    updateParent(parentOfA, a, b);

    // === updateParent call 2 === (0x1d2a2d)
    updateParent(b, bNext, a);

    // === updateParent call 3 === (0x1d2b27)
    updateParent(a, b, bNext);
}

// ============================================================================
// toJson(unordered_map<int,int> const&) const — 0x264b90
//
// Serializes this Node to a boost::json::value (object).
// The map parameter remaps internal nodeId values to serializable indices.
// Pointer fields (next, loop, branches, parent) are serialized as integer
// IDs via the remapping map. -1 means null/absent.
//
// JSON keys confirmed from .rodata string addresses:
//   0x906161 "nodeId"                → nodeId (+0x10) via idMap lookup
//   0x906168 "asmId"                 → asmId (+0x14)
//   0x90616e "wavesPerDev"           → wavesPerDev (+0x28)
//   0x90617a "deviceIndex"           → deviceIndex (+0x40) — NOT asmId!
//   0x906186 "type"                  → type (+0x44) via type2str()
//   0x905e73 "length"                → length (+0x90)
//   0x90618b "lengthReg"             → lengthReg (+0x88)
//   0x906195 "indexOffsetReg"        → indexOffsetReg (+0x94)
//   0x9061a4 "tableIndex"            → tableIndex (+0x9C)
//   0x9060f9 "play"                  → play (+0xA0) — weak_ptr IDs as array
//   0x9061af "next"                  → next (+0xB8) — single int ID
//   0x9061b4 "branches"              → branches (+0xC8) — array of int IDs
//   0x906105 "loop"                  → loop (+0xE0) — single int ID
//   0x9061bd "parent"                → parent (+0xF0) — single int ID
//   0x9061c4 "globalRate"            → globalRate (+0x100)
//   0x9061cf "defaultPrecompFlags"   → defaultPrecompFlags (+0x104)
//   0x9061e3 "loopBodyRunsAtLeastOnce" → loopBodyRunsAtLeastOnce (+0x108)
//   0x9061fb "branchMaySkipAllBodies"  → branchMaySkipAllBodies (+0x109)
//   0x906212 "trig"                  → trig (+0x10C)
//   0x906217 "config"                → config (+0x48) via PlayConfig::toJson()
//   0x90621e "currentCwvf"           → currentCwvf (+0x68) via PlayConfig::toJson()
// ============================================================================
boost::json::value Node::toJson(const std::unordered_map<int,int>& idMap) const {  // 0x264b90
    // Serialize wavesPerDev vector (+0x28)
    std::vector<std::string> wavesFlat;
    wavesFlat.reserve(wavesPerDev.size());
    for (const auto& opt : wavesPerDev) {
        if (opt.has_value()) {
            wavesFlat.push_back(*opt);
        } else {
            wavesFlat.push_back("");  // empty string for nullopt
        }
    }

    // Serialize play (+0xA0) — transform weak_ptrs to IDs
    std::vector<int> playIds;
    for (const auto& wp : play) {
        if (auto sp = wp.lock()) {
            playIds.push_back(sp->nodeId);
        }
    }

    // Serialize branches (+0xC8) — extract IDs, -1 if null
    std::vector<int> branchIds;
    for (const auto& child : branches) {
        if (child) {
            branchIds.push_back(child->nodeId);
        } else {
            branchIds.push_back(-1);
        }
    }

    // Look up this node's remapped ID from the map
    int remappedId = idMap.at(asmId);  // 0x264fef

    // Serialize next (+0xB8) as ID or -1
    int nextId = -1;
    if (next) {
        nextId = next->nodeId;
    }

    // Serialize loop (+0xE0) as ID or -1
    int loopId = -1;
    if (loop) {
        loopId = loop->nodeId;
    }

    // Serialize parent (+0xF0) — lock weak_ptr, get id or -1
    int parentId = -1;
    if (auto par = parent.lock()) {
        parentId = par->nodeId;
    }

    // lengthReg (+0x88) → if valid, emit value; else -1
    int lengthRegVal = -1;
    if (lengthReg.isValid()) {
        lengthRegVal = static_cast<int>(lengthReg);
    }

    // indexOffsetReg (+0x94) → if valid, emit value; else -1
    int indexOffsetRegVal = -1;
    if (indexOffsetReg.isValid()) {
        indexOffsetRegVal = static_cast<int>(indexOffsetReg);
    }

    // Build JSON object (21 key-value pairs = 0x15)
    boost::json::array wavesArr(wavesFlat.begin(), wavesFlat.end());
    boost::json::array branchArr;
    for (int bid : branchIds) {
        branchArr.push_back(bid);
    }
    boost::json::array playArr;
    for (int pid : playIds) {
        playArr.push_back(pid);
    }

    boost::json::value result = {
        {"nodeId",                   remappedId},               // 0x264f39: "nodeId"
        {"asmId",                    asmId},                    // 0x264fb4: "asmId"
        {"wavesPerDev",              std::move(wavesArr)},      // 0x265133: "wavesPerDev"
        {"deviceIndex",              deviceIndex},              // 0x2651ed: "deviceIndex"
        {"type",                     type2str(type)},           // 0x265256: "type"
        {"length",                   length},                   // 0x2652e9: "length"
        {"lengthReg",                lengthRegVal},             // 0x26535c: "lengthReg"
        {"indexOffsetReg",           indexOffsetRegVal},        // 0x2653ee: "indexOffsetReg"
        {"tableIndex",               tableIndex},               // 0x26547c: "tableIndex"
        {"play",                     std::move(playArr)},       // 0x2654ef: "play" — weak_ptr IDs
        {"next",                     nextId},                   // 0x265672: "next"
        {"branches",                 std::move(branchArr)},     // 0x2656f4: "branches"
        {"loop",                     loopId},                   // 0x265804: "loop"
        {"parent",                   parentId},                 // 0x26588c: "parent"
        {"globalRate",               globalRate},               // 0x265969: "globalRate"
        {"defaultPrecompFlags",      defaultPrecompFlags},      // 0x2659dc: "defaultPrecompFlags"
        {"loopBodyRunsAtLeastOnce",  loopBodyRunsAtLeastOnce},  // 0x265a56
        {"branchMaySkipAllBodies",   branchMaySkipAllBodies},   // 0x265ad1
        {"trig",                     trig},                     // 0x265b45: "trig"
        {"config",                   config.toJson()},          // 0x265bb8: "config"
        {"currentCwvf",              currentCwvf.toJson()},     // 0x265c31: "currentCwvf"
    };

    return result;
}

// ============================================================================
// fromJson(boost::json::value const&) — 0x268280 (static)
//
// Deserializes a Node from JSON. Returns shared_ptr<Node> (via allocate_shared).
// Reads all scalar fields from JSON, leaves pointer fields (next,
// loop, branches, parent, play) empty — these are reconnected
// later by installPointers().
// ============================================================================
std::shared_ptr<Node> Node::fromJson(const boost::json::value& json) {  // 0x268280
    const auto& obj = json.as_object();

    // --- wavesPerDev ---
    const auto& wavesJsonArr = obj.at("wavesPerDev").as_array();
    std::vector<std::optional<std::string>> waves;
    for (const auto& elem : wavesJsonArr) {
        if (elem.is_string()) {
            waves.emplace_back(std::string(elem.as_string()));
        } else {
            waves.emplace_back(std::nullopt);
        }
    }

    // --- scalar fields ---
    int nId          = static_cast<int>(obj.at("nodeId").as_int64());
    int aId          = static_cast<int>(obj.at("asmId").as_int64());
    int devIdx       = static_cast<int>(obj.at("deviceIndex").as_int64());
    const auto& typeStr = obj.at("type").as_string();
    std::string typeStdStr(typeStr.data(), typeStr.size());
    NodeType nodeType = str2type(typeStdStr);                         // 0x268687

    // --- PlayConfigs ---
    PlayConfig cfg1 = PlayConfig::fromJson(obj.at("config"));        // 0x2686f0
    PlayConfig cfg2 = PlayConfig::fromJson(obj.at("currentCwvf"));   // 0x268748

    // --- AsmRegisters and other ints ---
    int lengthRegVal    = static_cast<int>(obj.at("lengthReg").as_int64());
    AsmRegister lReg(lengthRegVal);
    int lengthField     = static_cast<int>(obj.at("length").as_int64());
    int idxOffsetRegVal = static_cast<int>(obj.at("indexOffsetReg").as_int64());
    AsmRegister ioReg(idxOffsetRegVal);
    int tableIdx        = static_cast<int>(obj.at("tableIndex").as_int64());
    int gRate           = static_cast<int>(obj.at("globalRate").as_int64());
    unsigned int precompFlags = static_cast<unsigned int>(
        obj.at("defaultPrecompFlags").as_int64());
    bool loopRuns       = obj.at("loopBodyRunsAtLeastOnce").as_bool();
    bool branchSkips    = obj.at("branchMaySkipAllBodies").as_bool();
    int trigField       = static_cast<int>(obj.at("trig").as_int64());

    // --- Construct node via full constructor ---
    // Pointer fields left null/empty — reconnected by installPointers()
    auto node = std::make_shared<Node>(
        nId, aId, waves, devIdx, nodeType,
        cfg1, cfg2,
        lReg, lengthField, ioReg,
        tableIdx,
        std::shared_ptr<Node>(nullptr),          // next — reconnected later
        std::vector<std::shared_ptr<Node>>{},    // branches — reconnected later
        std::shared_ptr<Node>(nullptr),          // loop — reconnected later
        std::weak_ptr<Node>(),                   // parent — reconnected later
        gRate, precompFlags, loopRuns, branchSkips, trigField
    );

    return node;
}

// ============================================================================
// installPointers(nodeMap, json) — 0x269020
//
// Reconnects pointer fields after deserialization.
// The nodeMap maps serialized integer IDs back to the actual Node
// shared_ptrs that were created by fromJson.
//
// JSON keys read (confirmed from .rodata addresses):
//   0x9060f9 "play"     → play (+0xA0)     — array of int IDs → weak_ptr
//   0x9061af "next"     → next (+0xB8)     — single int ID → shared_ptr
//   0x9061b4 "branches" → branches (+0xC8) — array of int IDs → shared_ptr
//   0x906105 "loop"     → loop (+0xE0)     — single int ID → shared_ptr
//   0x9061bd "parent"   → parent (+0xF0)   — single int ID → weak_ptr
// ============================================================================
void Node::installPointers(
    const std::unordered_map<int, std::shared_ptr<Node>>& nodeMap,
    const boost::json::value& json)  // 0x269020
{
    // 0x269041: copy-construct local map (binary does this)
    std::unordered_map<int, std::shared_ptr<Node>> localMap(nodeMap);

    auto lookupNode = [&localMap](const boost::json::value& v) -> std::shared_ptr<Node> {
        int id = static_cast<int>(v.as_int64());
        if (id == -1) return nullptr;
        auto it = localMap.find(id);
        if (it != localMap.end()) return it->second;
        return nullptr;
    };

    const auto& obj = json.as_object();

    // --- play (+0xA0) from "play" array ---
    // 0x269076: obj.at("play").as_array()
    // 0x26916d: transform → back_inserter(this->play)
    const auto& playArr = obj.at("play").as_array();
    play.clear();
    for (const auto& elem : playArr) {
        int id = static_cast<int>(elem.as_int64());
        if (id != -1) {
            auto it = localMap.find(id);
            if (it != localMap.end()) {
                play.push_back(it->second);  // shared_ptr → weak_ptr
            }
        }
    }

    // --- next (+0xB8) from "next" ---
    // 0x269227: obj.at("next") → lookupNode → store at +0xB8
    {
        const auto& nextVal = obj.at("next");
        auto oldNext = std::move(next);
        next = lookupNode(nextVal);
    }

    // --- branches (+0xC8) from "branches" array ---
    // 0x269300: obj.at("branches").as_array()
    // 0x269409: loop → lookupNode → push_back to branches
    const auto& branchesArr = obj.at("branches").as_array();
    branches.clear();
    for (const auto& elem : branchesArr) {
        branches.push_back(lookupNode(elem));
    }

    // --- loop (+0xE0) from "loop" ---
    // 0x2694c9: obj.at("loop") → lookupNode → store at +0xE0
    {
        const auto& loopVal = obj.at("loop");
        auto oldLoop = std::move(loop);
        loop = lookupNode(loopVal);
    }

    // --- parent (+0xF0) from "parent" ---
    // 0x2695a2: obj.at("parent") → lookupNode → assign as weak_ptr
    {
        const auto& parentVal = obj.at("parent");
        auto sp = lookupNode(parentVal);
        parent = sp;  // shared_ptr → weak_ptr conversion
    }
}

} // namespace zhinst
