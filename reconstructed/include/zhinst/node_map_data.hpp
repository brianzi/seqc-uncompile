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
// Extracted from custom_functions.{hpp,cpp} during Phase 16b file-organization
// split (audit Section C1). See notes/audit_phase16a.md.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <boost/json.hpp>

namespace zhinst {

// ============================================================================
// NodeMapData — abstract base, vtable @0xb02c98
// ============================================================================
class NodeMapData {
public:
    virtual ~NodeMapData();                                  // @0x1c5720
    virtual bool compareEq(NodeMapData const& other) const = 0;
    virtual size_t hash() const = 0;
    virtual NodeMapData* clone() const = 0;
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
class VirtAddrNodeMapData : public NodeMapData {
public:
    VirtAddrNodeMapData(VirtAddrNodeMapData const&);         // @0x1c4dc0
    ~VirtAddrNodeMapData() override;                         // @0x1c4ec0, D0 @0x1c56c0
    bool compareEq(NodeMapData const& other) const override; // @0x1c4cc0
    size_t hash() const override;                            // @0x1c4f10
    NodeMapData* clone() const override;                     // @0x1c51e0
    void getJson(boost::json::object& obj) const override;   // @0x1c5240

    std::string             name_;       // +0x08
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
class DirectAddrNodeMapData : public NodeMapData {
public:
    ~DirectAddrNodeMapData() override;                       // @0x1c5730
    bool compareEq(NodeMapData const& other) const override; // @0x1c5460
    size_t hash() const override;                            // @0x1c5370
    NodeMapData* clone() const override;                     // @0x1c53c0
    void getJson(boost::json::object& obj) const override;   // @0x1c5400

    uint32_t addr_;  // +0x08 (after 8-byte vptr from NodeMapData)
};

// ============================================================================
// NodeMapItem — 0x18 (24) bytes
//
// Hashable, comparable. Used as key in unordered_map and element in vectors.
//
// Layout (from operator== @0x1c5490 and fastAddress @0x1c5470):
//   +0x00  8  NodeMapData*   data     (polymorphic, dynamic_cast in getNodeAddress)
//   +0x08  4  int32_t        typeIdx  (node type/index)
//   +0x0C  4  uint32_t       fastAddr (fast address)
//   +0x10  1  bool           hasFast  (has fast address)
//   +0x11  7  (padding)
// ============================================================================
struct NodeMapItem {
    NodeMapData*  data;       // +0x00
    int32_t       typeIdx;    // +0x08
    uint32_t      fastAddr;   // +0x0C
    bool          hasFast;    // +0x10
    char          pad_11[7];  // +0x11

    bool operator==(NodeMapItem const& other) const;  // @0x1c5490
    uint32_t fastAddress() const;                      // @0x1c5470

    boost::json::value toJson() const;                 // @0x1c54f0
    size_t size() const;                               // @0x1c55a0
};
static_assert(sizeof(NodeMapItem) == 0x18,
              "NodeMapItem must be 0x18 bytes");

} // namespace zhinst

// std::hash specialization required so NodeMapItem can be used as a key in
// std::unordered_map / unordered_set (needed by CustomFunctions members).
namespace std {
template<> struct hash<zhinst::NodeMapItem> {
    size_t operator()(zhinst::NodeMapItem const& item) const {
        // Delegates to NodeMapData::hash() — simplified here
        return item.data ? item.data->hash() : 0;
    }
};
} // namespace std
