// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmList and AsmList::Asm method implementations
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/asm_list.hpp"

#include <iomanip>
#include <sstream>

#include <boost/json.hpp>

namespace zhinst {

// ============================================================================
// Thread-local unique ID counter for AsmList::Asm records.
// Located at TLS offset 0x40 in the binary (.tbss segment).
// Accessed via __tls_get_addr in inlined createUniqueID() calls.
// ============================================================================
static thread_local int nextID = 0;  // TLS offset 0x40

// ============================================================================
// AsmList::Asm::createUniqueID(bool reset) — always inlined, no standalone addr
//
// If reset=true: resets nextID to 0, returns 0.
// If reset=false: returns nextID++ (post-increment).
// ============================================================================
int AsmList::Asm::createUniqueID(bool reset) {
    if (reset) {
        nextID = 0;
        return 0;
    }
    return nextID++;
}

// ============================================================================
// AsmList::Asm::~Asm() — 0x122dd0
//
// Teardown order:
// 1. Release shared_ptr<Node> at +0x90 (control block at +0x98)
// 2. Destroy AssemblerInstr at +0x08 (tail-call to Assembler::~Assembler)
// Scalars (sequenceId, wavetableFront, isWaveformCmd) need no destruction.
// ============================================================================
AsmList::Asm::~Asm() = default;  // compiler-generated matches binary

// ============================================================================
// AsmList::Asm::serializeNodeToJsonString(idMap) const — 0x2698b0
//
// Calls node->toJson(idMap), then boost::json::serialize() with default options.
// Returns the JSON string. Uses sret calling convention (hidden first param).
// ============================================================================
std::string AsmList::Asm::serializeNodeToJsonString(  // 0x2698b0
    const std::unordered_map<int,int>& idMap) const
{
    // 0x2698be: rsi = this->node.get() (shared_ptr data ptr at +0x90)
    // 0x2698cc: call Node::toJson(idMap)
    boost::json::value jsonVal = node->toJson(idMap);

    // 0x2698d5: serialize_options = default (byte 0x00)
    // 0x2698df: call boost::json::serialize(jsonVal, opts)
    return boost::json::serialize(jsonVal);
}

// ============================================================================
// AsmList::~AsmList() — 0x11d5b0
//
// Iterates entries from end to begin. For each element:
//   1. Release shared_ptr<Node> (atomic decrement at +0x98)
//   2. Destroy Assembler subobject at +0x08
// Then frees the vector buffer via operator delete.
// ============================================================================
AsmList::~AsmList() = default;  // compiler-generated vector dtor matches binary

// ============================================================================
// AsmList::append(entry) — 0x15d180
//
// Copy-appends an Asm record to the vector.
// Fast path (capacity available): placement-new with field-by-field copy.
//   sequenceId (int copy), Assembler (copy ctor), wavetableFront (int copy),
//   node (shared_ptr copy + atomic inc), isWaveformCmd (bool copy).
// Slow path: calls vector::__emplace_back_slow_path for reallocation.
// ============================================================================
void AsmList::append(const Asm& entry) {  // 0x15d180
    entries.push_back(entry);
}

// ============================================================================
// AsmList::maxRegister() const — 0x269910
//
// Iterates all entries (stride 0xA8). Calls highestRegisterNumber() on each
// entry's assembler. Returns the maximum register number found.
// highestRegisterNumber() returns int64: bit 32 = valid, lower 32 = reg num.
// ============================================================================
int AsmList::maxRegister() const {  // 0x269910
    int maxReg = 0;
    for (const auto& entry : entries) {
        int64_t result = entry.assembler.highestRegisterNumber();
        bool valid = (result >> 32) & 1;       // bit 32 = valid flag
        int regNum = static_cast<int>(result);  // lower 32 bits
        if (valid) {
            if (regNum > maxReg) {
                maxReg = regNum;
            }
        }
    }
    return maxReg;
}

// ============================================================================
// AsmList::print(showNode, os, showHeader) const — 0x264250
//
// Iterates entries. For each:
//   If showHeader: prints "NNN (NNN): " with wavetableFront and sequenceId
//                  (each field-width 3).
//   If opcode != -1 (real instruction): prints assembler.str(true) + "\n".
//   If opcode == -1 (placeholder):
//     If showNode && node present: "// placeholder: " + node->toString() + "\n"
//     If showNode && no node:      "// <empty command>\n"
//     Else:                        "\n"
// ============================================================================
void AsmList::print(bool showNode, std::ostream& os, bool showHeader) const {  // 0x264250
    for (const auto& entry : entries) {
        if (showHeader) {
            os << std::setw(3) << entry.wavetableFront   // +0x88
               << " ("
               << std::setw(3) << entry.sequenceId       // +0x00
               << "): ";
        }

        if (static_cast<int>(entry.assembler.cmd) != -1) {
            // Real instruction
            os << entry.assembler.str(true) << "\n";
        } else {
            // Placeholder (opcode == -1)
            if (showNode) {
                if (entry.node) {
                    os << "// placeholder: " << entry.node->toString() << "\n";
                } else {
                    os << "// <empty command>\n";
                }
            } else {
                os << "\n";
            }
        }
    }
}

// ============================================================================
// AsmList::serialize() const — 0x2646d0
//
// Two-pass serialization:
//
// Pass 1: Build idMap (unordered_map<int,int>) assigning sequential indices
//   to entries. Skips entries where:
//   - opcode == 4 (NOP)
//   - opcode == -1 AND node is null
//
// Pass 2: For each entry:
//   If opcode != -1:
//     Emit assembler.str(true).
//     If isWaveformCmd (+0xA0) and opcode == 5: append " #disableOpt".
//     Append "\n".
//   If opcode == -1 AND node != null:
//     Emit "placeholder # " + boost::json::serialize(node->toJson(idMap)) + "\n".
//
// Returns the complete serialized string.
// ============================================================================
std::string AsmList::serialize() const {  // 0x2646d0
    std::ostringstream ss;

    // Pass 1: build idMap
    std::unordered_map<int,int> idMap;
    int nextIdx = 0;
    for (const auto& entry : entries) {
        int opcode = static_cast<int>(entry.assembler.cmd);
        if (opcode == 4)  // skip NOP
            continue;
        if (opcode == -1 && !entry.node)  // skip empty placeholders
            continue;
        idMap[entry.sequenceId] = nextIdx++;
    }

    // Pass 2: serialize
    for (const auto& entry : entries) {
        int opcode = static_cast<int>(entry.assembler.cmd);

        if (opcode != -1) {
            // Real instruction
            ss << entry.assembler.str(true);

            // Append " #disableOpt" for certain waveform commands
            if (entry.isWaveformCmd && opcode == 5) {
                ss << " #disableOpt";
            }

            ss << "\n";
        } else if (entry.node) {
            // Placeholder with node — serialize as JSON
            boost::json::value jsonVal = entry.node->toJson(idMap);
            ss << "placeholder # " << boost::json::serialize(jsonVal) << "\n";
        }
        // Entries with opcode==-1 and no node are silently skipped
    }

    return ss.str();
}

// ============================================================================
// AsmList::deserialize(str) — 0x266050
//
// Calls parseStringToAsmList(str) which returns tuple<AsmList, string>.
// Moves the AsmList from the tuple into *this. Returns *this.
// ============================================================================
AsmList& AsmList::deserialize(const std::string& str) {  // 0x266050
    auto [parsed, warnings] = parseStringToAsmList(str);
    entries = std::move(parsed.entries);
    return *this;
}

// ============================================================================
// AsmList::parseStringToAsmList(str) — 0x266160 (static)
//
// Complex ~7000-byte parser. High-level flow:
//   1. getDeviceConstants(AwgDeviceType(2)) — hardcoded device type
//   2. Construct AWGAssembler from those constants
//   3. AWGAssembler::assembleStringToExpressionsVec(str) — parse assembly text
//   4. Reset createUniqueID (nextID = 0)
//   5. Iterate expressions, build Asm entries with sequential IDs
//   6. For "placeholder # {...}" lines, parse JSON → Node::fromJson + installPointers
//
// Returns tuple<AsmList, string> where string is error/warning messages.
//
// Full reconstruction deferred — this is a large, complex parser with
// AWGAssembler and JSON parsing dependencies.
// ============================================================================
std::tuple<AsmList, std::string> AsmList::parseStringToAsmList(  // 0x266160
    const std::string& str)
{
    // TODO: Full reconstruction deferred (~7000 bytes of code).
    // Depends on: AWGAssembler, DeviceConstants, getDeviceConstants(),
    //             Node::fromJson, Node::installPointers.
    // Key steps documented in header comment above.
    (void)str;
    return {AsmList{}, ""};
}

} // namespace zhinst
