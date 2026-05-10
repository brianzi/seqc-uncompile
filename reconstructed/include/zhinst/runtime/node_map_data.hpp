// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// NodeMapData — polymorphic base for node map entries used by CustomFunctions
// to track parameter-tree node access.
//
// Two subclasses:
//   VirtAddrNodeMapData  — vtable @0xb04cd8 (variable-address virtual node)
//   DirectAddrNodeMapData — vtable @0xb04d30 (direct hardware address)
//
// Plus NodeMapItem (24B) — hashable wrapper used as key in the map and as
// element in the access list.
//
// Extracted from custom_functions.{hpp,cpp} file-organization
// split (audit Section C1). See notes/audit_phase16a.md.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <boost/json.hpp>

namespace zhinst {

// NodeTypeIdx — node value encoding type for NodeMapItem::typeIdx (A9)
// Determines how the value is encoded when writing to a parameter-tree node.
//! \brief Selector tag stored in `NodeMapItem::typeIdx` that
//!        determines how a node's runtime value is encoded before
//!        being written to the parameter tree.
//!
//! \details The numeric value indexes a jump table inside the node
//! writer; each enumerator triggers a different conversion (raw
//! pass-through, IQ pair, IEEE-754 bit reinterpretation, fixed-point
//! frequency / phase, etc.). See `toFrequency()` / `toPhase()` for
//! the fixed-point helpers used by the last two cases.
enum class NodeTypeIdx : int32_t {
    //! Direct register / integer value — written verbatim to the node.
    IntegerPassthrough = 0,  // direct register/int value
    //! Dual 32-bit I+Q pair (`suser 0x17` + `suser 0x19`).
    SinePair           = 1,  // dual 32-bit I+Q (suser 0x17 + 0x19)
    //! IEEE-754 single-precision bit pattern reinterpreted as an integer.
    FloatBits          = 2,  // IEEE-754 single-precision bit pattern
    //! Low 32 bits of an IEEE-754 double; the high 32 bits are written by a paired second access.
    RawDoubleLow32     = 3,  // double low 32 bits + high32 via second write
    //! Hz value converted to 48-bit fixed-point via `toFrequency()`.
    Frequency          = 4,  // Hz → 48-bit fixed-point via toFrequency()
    //! Degrees converted to 23-bit fixed-point via `toPhase()`.
    Phase              = 5,  // degrees → 23-bit fixed-point via toPhase()
};

// ============================================================================
// NodeMapData — abstract base, vtable @0xb02c98
// ============================================================================
//! \brief Abstract polymorphic payload for parameter-tree node entries
//! tracked by `CustomFunctions` during SeqC compilation.
//!
//! Each device parameter referenced by a SeqC built-in (e.g. via
//! `setInt`, `getDIO`, `setOscFreq`) is represented by a `NodeMapItem`
//! that owns a `NodeMapData*` of one of two concrete subclasses
//! (`VirtAddrNodeMapData` or `DirectAddrNodeMapData`) selected when
//! the node is resolved from the device's parameter tree.  The four
//! pure-virtual methods (`compareEq`, `hash`, `clone`, `getJson`)
//! cover the operations needed to use `NodeMapItem` as a key in
//! `std::unordered_map` / `unordered_set` and to serialise the
//! collected node-access information into the compile-result JSON.
class NodeMapData {
public:
    //! \brief Virtual destructor; releases subclass storage through
    //! the base pointer.
    virtual ~NodeMapData();                                  // @0x1c5720
    //! \brief Returns `true` when `*this` and `other` describe the
    //! same parameter-tree node; the caller guarantees `typeid`
    //! match before dispatching here so subclasses may
    //! `static_cast<>` `other`.
    //! \param other Other payload to compare against.
    //! \return `true` if the two payloads are equivalent.
    virtual bool compareEq(NodeMapData const& other) const = 0;
    //! \brief Returns a hash combining every field that participates
    //! in `compareEq`; used by `std::hash<NodeMapItem>` to put
    //! entries into `CustomFunctions`'s `unordered_map`s.
    //! \return Combined hash value.
    virtual size_t hash() const = 0;
    //! \brief Returns a heap-allocated deep copy of `*this`; the
    //! caller owns the result.
    //! \return Newly `new`-allocated clone.
    virtual NodeMapData* clone() const = 0;
    //! \brief Writes the payload's serialisable fields into `obj`
    //! (used to render the node-access summary in the compile-result
    //! JSON).
    //! \param obj Target JSON object the payload appends keys to.
    virtual void getJson(boost::json::object& obj) const = 0;
};

// ============================================================================
// VirtAddrNodeMapData — vtable @0xb04cd8, layout 0x38 bytes
//
// Confirmed from D0 dtor @0x1c56c0 (delete[size]=0x38), copy ctor @0x1c4dc0
// (memcpy + __throw_length_error from vector<int>), and getJson @0x1c5240
// (writes "name" key from string field, then iterates vector reading int32_t
// per element):
//   +0x00   8  vptr (from base)
//   +0x08  24  std::string             name_       — node name (JSON "name")
//   +0x20  24  std::vector<int32_t>    addresses_  — virtual address list
// ============================================================================
//! \brief `NodeMapData` payload for parameter-tree nodes addressed
//! through a string path that resolves to one or more virtual
//! addresses.
//!
//! Holds the user-facing node `name_` and a list of `addresses_`
//! collected from the parameter tree; `getJson` emits both as the
//! `"name"` key plus an array of integer addresses, and `clone` /
//! `compareEq` / `hash` participate in the `NodeMapItem` map
//! protocol described on `NodeMapData`.
class VirtAddrNodeMapData : public NodeMapData {
public:
    //! \brief Default-constructs an empty payload with no name and
    //! no addresses.
    VirtAddrNodeMapData() = default;
    //! \brief Copy-constructs from `other`, duplicating `name_` and
    //! `addresses_`.
    //! \param other Source payload to copy.
    VirtAddrNodeMapData(VirtAddrNodeMapData const&);         // @0x1c4dc0
    //! \brief Destructor; releases `name_` and `addresses_`.
    ~VirtAddrNodeMapData() override;                         // @0x1c4ec0, D0 @0x1c56c0
    //! \brief Compares `name_` and `addresses_` against `other`
    //! (assumed to be a `VirtAddrNodeMapData`; the caller verifies
    //! type compatibility through `NodeMapItem::operator==`).
    //! \param other Other payload to compare against.
    //! \return `true` if both name and address list match.
    bool compareEq(NodeMapData const& other) const override; // @0x1c4cc0
    //! \brief Hashes `name_` (via `std::hash<string>`) combined with
    //! every element of `addresses_` using a golden-ratio
    //! splitmix-style mixer.
    //! \return Combined hash value.
    size_t hash() const override;                            // @0x1c4f10
    //! \brief Returns a `new`-allocated deep copy of this payload.
    //! \return Newly allocated clone owned by the caller.
    NodeMapData* clone() const override;                     // @0x1c51e0
    //! \brief Writes `obj["name"] = name_` and `obj["index"] =
    //! addresses_` as an array of integers.
    //! \param obj Target JSON object the payload appends keys to.
    void getJson(boost::json::object& obj) const override;   // @0x1c5240

    //! \brief Node name as written in the SeqC source (path through
    //! the device parameter tree).
    std::string             name_;       // +0x08
    //! \brief Virtual addresses resolved from `name_` against the
    //! device tree; multiple entries occur when the path matches a
    //! list-valued parameter.
    std::vector<int32_t>    addresses_;  // +0x20
};

// ============================================================================
// DirectAddrNodeMapData — vtable @0xb04d30, layout 0x10 bytes
//
// Used by getNodeAddress for fast path
// (dynamic_cast<DirectAddrNodeMapData*> in getNodeAddress @0x16ba10).
// Layout confirmed from getNodeAddress (16ba3f: `add rax, 8; mov eax, [rax]`):
//   +0x00  8  vptr (from base)
//   +0x08  4  uint32_t   addr_     direct hardware address
// ============================================================================
//! \brief `NodeMapData` payload for parameter-tree nodes that map
//! directly to a single hardware register address.
//!
//! Used as the fast path in `CustomFunctions::getNodeAddress`, which
//! down-casts via `dynamic_cast<DirectAddrNodeMapData*>` and reads
//! `addr_` straight out without consulting the virtual-address list
//! table.
class DirectAddrNodeMapData : public NodeMapData {
public:
    //! \brief Default-constructs with `addr_` left uninitialised
    //! (callers set the field explicitly before use).
    DirectAddrNodeMapData() = default;
    //! \brief Destructor; trivial — no owned resources.
    ~DirectAddrNodeMapData() override;                       // @0x1c5730
    //! \brief Compares `addr_` for equality against `other`'s
    //! `addr_`.
    //! \param other Other payload to compare against.
    //! \return `true` if both payloads carry the same direct address.
    bool compareEq(NodeMapData const& other) const override; // @0x1c5460
    //! \brief Returns a splitmix-style hash of `addr_` seeded with
    //! the golden-ratio constant.
    //! \return Hash of `addr_`.
    size_t hash() const override;                            // @0x1c5370
    //! \brief Returns a `new`-allocated copy carrying the same
    //! `addr_`.
    //! \return Newly allocated clone owned by the caller.
    NodeMapData* clone() const override;                     // @0x1c53c0
    //! \brief Writes `obj["direct_address"] = addr_` as a 64-bit
    //! integer.
    //! \param obj Target JSON object the payload appends keys to.
    void getJson(boost::json::object& obj) const override;   // @0x1c5400

    //! \brief Hardware register address the parameter resolves to.
    uint32_t addr_;  // +0x08 (after 8-byte vptr from NodeMapData)
};

// ============================================================================
// NodeMapItem — 0x18 (24) bytes
//
// Hashable, comparable. Used as key in unordered_map and element in vectors.
//
// Layout (from operator== @0x1c5490 and fastAddress @0x1c5470):
//   +0x00  8  NodeMapData*   data     (polymorphic, dynamic_cast in getNodeAddress)
//   +0x08  4  NodeTypeIdx    typeIdx  (node type/index)
//   +0x0C  4  uint32_t       fastAddr (fast address)
//   +0x10  1  bool           hasFast  (has fast address; doubles as
//                                       AccessMode 0/1 selector in playback —
//                                       NOTE: IF-112)
//   +0x11  7  (padding)
// ============================================================================
//! \brief Hashable handle to a parameter-tree node, used as the
//! key/value type in `CustomFunctions`'s node-tracking maps.
//!
//! Pairs a non-owning `NodeMapData*` (one of the two concrete
//! subclasses) with a `NodeTypeIdx` value-encoding tag and a cached
//! `fastAddr` shortcut for direct-address nodes.  The trailing
//! `hasFast` flag records whether `fastAddr` is valid; in
//! `custom_functions_play.cpp` it is also reinterpreted as an
//! `AccessMode` selector (Soft=0 / Direct=1) when emitting node
//! reads, while `Custom`(2) is only ever inserted into
//! `CustomFunctions::accessModeMap_` via explicit literal call sites.
//! `operator==` and the `std::hash` specialisation defined below are
//! required for storage in `std::unordered_map<NodeMapItem, ...>`.
struct NodeMapItem {
    //! \brief Non-owning pointer to the polymorphic payload that
    //! describes the underlying parameter-tree node.
    NodeMapData*  data;       // +0x00
    //! \brief Value-encoding selector applied when the node is
    //! written from SeqC (`IntegerPassthrough`, `Frequency`, etc.).
    NodeTypeIdx   typeIdx;    // +0x08
    //! \brief Cached direct hardware address when the node is
    //! addressable without a virtual-address lookup; valid only
    //! when `hasFast == true`.
    uint32_t      fastAddr;   // +0x0C
    // hasFast: only 0/1 ever observed (51 lookupNode hits across full test
    // suite via GDB; see notes/incidental_findings.md IF-112). The byte is
    // also read by custom_functions_play.cpp:1511 as
    // `AccessMode(hasFast)` → Soft(0)/Direct(1); Custom(2) only enters
    // accessModeMap_ via explicit literal in other call sites.
    //! \brief Flag indicating that `fastAddr` is valid; reused as an
    //! `AccessMode` selector (Soft=0 / Direct=1) when emitting node
    //! reads in `custom_functions_play.cpp`.
    bool          hasFast;    // +0x10
    //! \brief Padding to align the struct size to 0x18 bytes.
    char          pad_11[7];  // +0x11

    //! \brief Two-stage equality: first compares `typeIdx`,
    //! `hasFast`, and (when both have fast addresses) `fastAddr`;
    //! then verifies `typeid(*data) == typeid(*other.data)` and
    //! dispatches to `data->compareEq`.
    //! \param other Other item to compare against.
    //! \return `true` if both items refer to the same parameter node.
    bool operator==(NodeMapItem const& other) const;  // @0x1c5490
    //! \brief Returns `fastAddr` when `hasFast` is set, otherwise
    //! `0`.
    //! \return Cached direct address or `0` when unavailable.
    uint32_t fastAddress() const;                      // @0x1c5470

    //! \brief Renders the item as a JSON object by delegating field
    //! emission to `data->getJson` and appending a `"size"` key
    //! whose value comes from a 4-entry lookup table indexed by
    //! `typeIdx - 1`.
    //! \return JSON object describing the node-access record.
    boost::json::value toJson() const;                 // @0x1c54f0
    //! \brief Returns the in-words size used when writing the node
    //! value to hardware; resolved through the same `typeIdx`-keyed
    //! 4-entry table used by `toJson`.
    //! \return Word count for the value-encoding (2 for IQ/double/
    //! integer pairs, 1 for `SinePair`, etc.).
    size_t size() const;                               // @0x1c55a0
};
static_assert(sizeof(NodeMapItem) == 0x18,
              "NodeMapItem must be 0x18 bytes");

} // namespace zhinst

// std::hash specialization required so NodeMapItem can be used as a key in
// std::unordered_map / unordered_set (needed by CustomFunctions members).
namespace std {
template<> struct hash<zhinst::NodeMapItem> {
    //! \brief Delegates to `NodeMapData::hash()` when `data` is
    //! non-null, returning `0` otherwise; enables use of
    //! `NodeMapItem` as a key in `std::unordered_map` /
    //! `unordered_set`.
    size_t operator()(zhinst::NodeMapItem const& item) const {
        // Delegates to NodeMapData::hash() — simplified here
        return item.data ? item.data->hash() : 0;
    }
};
} // namespace std
