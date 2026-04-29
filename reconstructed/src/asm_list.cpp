// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmList and AsmList::Asm method implementations
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/asm_list.hpp"
#include "zhinst/asm_expression.hpp"
#include "zhinst/awg_assembler.hpp"
#include "zhinst/device_constants.hpp"

#include <iomanip>
#include <sstream>

#include <boost/json.hpp>

#include "zhinst/log_macros.hpp"

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
// Scalars (sequenceId, wavetableFront, noOpt) need no destruction.
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
//   node (shared_ptr copy + atomic inc), noOpt (bool copy).
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
//     If noOpt (+0xA0) and opcode ∉ {3,4,5}: append " #disableOpt".
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

            // Append " #disableOpt" for waveform commands with opcode ∉ {3,4,5}
            // Binary 0x26480e: checks noOpt, excludes (opcode-3)<2 and opcode==5
            if (entry.noOpt && opcode != 3 && opcode != 4 && opcode != 5) {
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
// AWGAssembler, DeviceConstants, getDeviceConstants(),
// Node::fromJson, Node::installPointers, boost::json.
//
// Function: 0x266160 – 0x267e78 (7448 bytes of main body + exception handlers to 0x268130)
//
// Algorithm overview:
//   1. getDeviceConstants(AwgDeviceType(2)) — hardcoded HDAWG device type.
//   2. Construct AWGAssembler from those constants.
//   3. AWGAssembler::assembleStringToExpressionsVec(str) — parse assembly text
//      into vector<shared_ptr<AsmExpression>>.
//   4. Reset createUniqueID (nextID = 0).
//   5. Iterate expressions. For each AsmExpression:
//      a. If labelType == true (0x78): expression is a direct instruction reference.
//         Build AssemblerInstr with cmd from expr->command, copy label string
//         from expr (+0x60). Assign sequenceId = nextID++.
//      b. If expr->command == 3 (MESSAGE) or 5 (ERROR_MSG): Build instr with
//         that command. Parse first child's string as Immediate, push to
//         outputs vector. Use vtable dispatch for Immediate type assignment.
//      c. If expr->command == 4 (unused/NOP marker): Similar to (b) but
//         copies string from expr (+0x20) instead of (+0x60).
//      d. Otherwise (general instructions): Iterate sub-expressions by type:
//         - type==3 (integer): collect shared_ptr into input-expressions vector
//         - type==1 (register): collect shared_ptr into register-expressions vector
//         - type==3 again in second pass: collect into output-expressions vector
//         Then check for type==2 (label): copy label string if present.
//      e. After categorizing, construct AssemblerInstr:
//         - cmd = expr->command
//         - Build immediates vector from input-expressions (Immediate(value))
//         - Assign registers based on Assembler::getRegisterOrder(cmd):
//           * 0: no register operands
//           * 1: regSrc = regExprs[0].value
//           * 2: regDst = regExprs[0].value
//           * 3: regAux = regExprs[0].value, regSrc = regExprs[1].value
//           * 4: regDst = regExprs[0].value, regSrc = regExprs[1].value
//           * else: log warning "Unknown Assembler::RegOrder with code N."
//         - Build outputs vector from output-expressions
//         - Copy label string
//      f. Check if expr has a "noOpt" JSON blob (byte at +0x98):
//         If true, parse JSON from expr comment (+0x80), store into
//         idMap<int, json::value>, then Node::fromJson → nodeMap<int, shared_ptr<Node>>.
//         Assign node to the Asm entry, and override noOpt = true.
//      g. Append Asm entry to result list.
//   6. After all expressions: iterate result entries and call
//      Node::installPointers(nodeMap, jsonValue) for each entry that has a node.
//   7. Get report string from AWGAssembler.
//   8. Return tuple<AsmList, string>(entries, report).
//
// The wavetableFront counter increments sequentially for each Asm entry.
// ============================================================================
std::tuple<AsmList, std::string> AsmList::parseStringToAsmList(  // 0x266160
    const std::string& str)
{
    // Step 1: Get HDAWG device constants (hardcoded device type 2)
    // 0x26618d: call getDeviceConstants(AwgDeviceType(2))
    DeviceConstants constants = getDeviceConstants(AwgDeviceType(2));

    // Step 2: Construct AWGAssembler
    // 0x26619f: call AWGAssembler(constants)
    AWGAssembler assembler(constants);

    // Step 3: Parse assembly text into expression trees
    // 0x2661b1: call assembleStringToExpressionsVec(str)
    std::vector<std::shared_ptr<AsmExpression>> expressions =
        assembler.assembleStringToExpressionsVec(str);

    // Step 4: Initialize result list and counters
    AsmList result;
    Immediate imm1;  // initialized with float 1.0f (0x3f800000)
    Immediate imm2;  // initialized with float 1.0f (0x3f800000)

    // 0x266224: __tls_get_addr to get nextID pointer
    // 0x266241: wavetableFront counter = 0
    int wavetableFront = 0;

    // Maps used for JSON-based node reconstruction (used in noOpt path)
    // idMap at [rbp-0x230]: maps sequenceId → boost::json::value
    // nodeMap at [rbp-0x400]: maps sequenceId → shared_ptr<Node>
    std::unordered_map<int, boost::json::value> idMap;
    std::unordered_map<int, std::shared_ptr<Node>> nodeMap;

    // Step 5: Iterate over parsed expressions
    // 0x26620a: loop begin, r12 = expressions.begin, compare to expressions.end
    for (auto it = expressions.begin(); it != expressions.end(); ++it) {
        // Each expression is a shared_ptr<AsmExpression> (16 bytes: ptr + ctrl)
        // The expression vector elements are at stride 0x10

        // 0x2662d8: Collect sub-expressions into category vectors
        //   vec_input:  [rbp-0x110] — shared_ptr<AsmExpression> for type==3 (integers/immediates)
        //   vec_reg:    [rbp-0xf0]  — shared_ptr<AsmExpression> for type==1 (registers)
        //   vec_output: [rbp-0xd0]  — shared_ptr<AsmExpression> for type==3 (output immediates)
        std::vector<std::shared_ptr<AsmExpression>> vec_input;
        std::vector<std::shared_ptr<AsmExpression>> vec_reg;
        std::vector<std::shared_ptr<AsmExpression>> vec_output;

        AsmExpression* expr = it->get();

        // 0x2662dc: check expr->labelType (byte at +0x78, alias for hasLabel)
        if (expr->labelType()) {
            // --- Case A: Direct instruction reference (labelType == true) ---
            // 0x2662e6: Build AssemblerInstr with cmd = LABEL (2)
            AssemblerInstr instr;
            instr.cmd = Assembler::LABEL;  // cmd = 2

            // 0x26635a: Copy label string from expr (+0x60) into instr.label
            instr.label = expr->labelName;

            // 0x266393: sequenceId = createUniqueID(false)
            int seqId = Asm::createUniqueID(false);

            // Build the Asm entry
            Asm entry;
            entry.sequenceId = seqId;
            entry.assembler = instr;
            entry.wavetableFront = wavetableFront;
            entry.node = nullptr;
            entry.noOpt = noOpt(instr);  // (cmd-3) < 3u

            result.append(entry);

            // 0x266457: Check next expression's type for follow-up processing
            // If expr->command (+0x38) == 2, jump to no-opt handling at 0x266d87
            int exprCmd = expr->command;
            if (exprCmd == 2) {
                // Skip to cleanup of vec_output, vec_reg, vec_input
                // (RAII at scope exit) then next iteration.
                // 0x266d87 ff: release shared_ptrs in vec_output, vec_reg, vec_input
                wavetableFront++;
                continue;
            }
        }

        // 0x2664c0: Check expr->command
        {
            int exprCmd = expr->command;

            if (exprCmd == 3 || exprCmd == 5) {
                // --- Case B: MESSAGE or ERROR_MSG ---
                // 0x266590: Build instr with cmd = exprCmd (3 or 5)
                AssemblerInstr instr;
                instr.cmd = static_cast<Assembler::Command>(exprCmd);

                // 0x266603: Get first child expression (+0x40 vector, first element)
                // child = expr->children[0]
                // 0x266615: Immediate(child->name) — construct from string at child+0x08
                auto& children = expr->children;
                Immediate imm(children[0]->name);

                // Push immediate into outputs vector of instr
                // 0x26661a: Check capacity, push to outputs vector
                // Uses vtable dispatch for Immediate type assignment
                instr.outputs.push_back(imm);

                // 0x266b53: sequenceId = -1 (INVALID)
                // Actually sequenceId is not set to -1; looking again:
                // After the MESSAGE/ERROR path, it builds an Asm entry with cmd = -1 (INVALID)
                Asm entry;
                entry.sequenceId = -1;  // placeholder
                entry.assembler = instr;
                entry.wavetableFront = wavetableFront;
                entry.node = nullptr;
                entry.noOpt = noOpt(instr);

                result.append(entry);

            } else if (exprCmd == 4) {
                // --- Case C: NOP marker ---
                // 0x2664de: Build instr with cmd = 4
                AssemblerInstr instr;
                instr.cmd = static_cast<Assembler::Command>(4);

                // 0x266555: Copy string from expr (+0x20, comment field) into instr
                instr.comment = expr->comment;

                // Similar Asm entry construction
                Asm entry;
                entry.sequenceId = -1;
                entry.assembler = instr;
                entry.wavetableFront = wavetableFront;
                entry.node = nullptr;
                entry.noOpt = noOpt(instr);

                result.append(entry);

            } else {
                // --- Case D: General instruction ---
                // 0x266672: Iterate sub-expressions (children at +0x40)

                // First pass: collect type==3 into vec_input
                // 0x2666a8: loop over children, check child->type == 3
                auto& children = expr->children;
                auto childIt = children.begin();
                auto childEnd = children.end();

                for (; childIt != childEnd; ++childIt) {
                    AsmExpression* child = childIt->get();
                    if (child->type != 3) break;  // stop at first non-integer
                    vec_input.push_back(*childIt);
                }

                // Second pass: collect type==1 into vec_reg
                // 0x266818: loop continues, check child->type == 1
                for (; childIt != childEnd; ++childIt) {
                    AsmExpression* child = childIt->get();
                    if (child->type != 1) break;
                    vec_reg.push_back(*childIt);
                }

                // Third pass: collect type==3 into vec_output
                // 0x266988: loop continues, check child->type == 3
                for (; childIt != childEnd; ++childIt) {
                    AsmExpression* child = childIt->get();
                    if (child->type != 3) break;
                    vec_output.push_back(*childIt);
                }

                // Check for label (type==2) at current position
                // 0x266ac2: zero-init label string
                std::string labelStr;
                // 0x266ad5: check if more children remain and next is type==2
                if (childIt != childEnd) {
                    AsmExpression* child = childIt->get();
                    if (child->type == 2) {
                        // 0x266aec: copy child->name (at +0x08) into labelStr
                        labelStr = child->name;
                    }
                }

                // 0x266f0c: exprCmd = expr->command (+0x38)
                int cmdVal = expr->command;

                // Build a copy of vec_reg for register assignment
                // 0x266f29: Copy vec_reg into local regExprs vector
                std::vector<std::shared_ptr<AsmExpression>> regExprs(
                    vec_reg.begin(), vec_reg.end());

                // 0x266fbb: Build AssemblerInstr
                AssemblerInstr instr;
                instr.cmd = static_cast<Assembler::Command>(cmdVal);

                // Build immediates from vec_input
                // 0x267032: iterate vec_input, for each: construct Immediate(child->value)
                for (auto& inputExpr : vec_input) {
                    AsmExpression* e = inputExpr.get();
                    Immediate imm(e->value);
                    instr.immediates.push_back(imm);
                }

                // 0x267045: Assign registers based on getRegisterOrder(cmd)
                int regOrder = Assembler::getRegisterOrder(
                    static_cast<Assembler::Command>(cmdVal));

                switch (regOrder) {
                    case 0:
                        // No register operands
                        break;
                    case 1:
                        // 0x26706d: regSrc = regExprs[0]->value
                        instr.regSrc = AsmRegister(regExprs[0]->value);
                        break;
                    case 2:
                        // 0x2671eb: regDst = regExprs[0]->value
                        instr.regDst = AsmRegister(regExprs[0]->value);
                        break;
                    case 3:
                        // 0x267213: regAux = regExprs[0]->value,
                        //           regSrc = regExprs[1]->value
                        instr.regAux = AsmRegister(regExprs[0]->value);
                        instr.regSrc = AsmRegister(regExprs[1]->value);
                        break;
                    case 4:
                        // 0x2671b0: regDst = regExprs[0]->value,
                        //           regSrc = regExprs[1]->value
                        instr.regDst = AsmRegister(regExprs[0]->value);
                        instr.regSrc = AsmRegister(regExprs[1]->value);
                        break;
                    default:
                        // 0x26725c: Log warning
                        // "Unknown Assembler::RegOrder with code N."
                        LOG_WARNING("Unknown Assembler::RegOrder with code "
                                    << regOrder << ".");
                        break;
                }

                // Build outputs from vec_output
                // 0x2672e0: iterate vec_output
                for (auto& outExpr : vec_output) {
                    AsmExpression* e = outExpr.get();
                    Immediate imm(e->value);
                    instr.outputs.push_back(imm);
                }

                // 0x267400: Copy label string into instr.label
                instr.label = labelStr;

                // 0x267466: sequenceId = createUniqueID(false)
                int seqId = Asm::createUniqueID(false);

                // 0x267488: Copy instr into entry
                Asm entry;
                entry.sequenceId = seqId;
                entry.assembler = instr;
                entry.wavetableFront = wavetableFront;

                // 0x2675b9: Check expr->noOpt (byte at +0x98, alias for hasComment)
                if (expr->noOpt()) {
                    // 0x2675ca: Parse JSON from expr->comment (+0x80)
                    // boost::json::parse(comment_str)
                    std::string_view commentSv = expr->comment;
                    boost::json::value jsonVal = boost::json::parse(commentSv);

                    // 0x267642: Insert into idMap<int, json::value>
                    idMap[seqId] = std::move(jsonVal);

                    // 0x267688: Look up seqId in idMap to get the JSON
                    const auto& jv = idMap.at(seqId);

                    // 0x26778d: Node::fromJson(jv) → shared_ptr<Node>
                    std::shared_ptr<Node> node = Node::fromJson(jv);

                    // Store node into entry
                    entry.node = node;

                    // 0x267808: Get node->nodeId and store into nodeMap
                    int nodeSeqId = node->nodeId;  // +0x10
                    nodeMap[nodeSeqId] = node;

                    // 0x26788f: noOpt override
                    entry.noOpt = true;
                } else {
                    // 0x26789d: Check expr->noOptOverride_ (byte at +0xA0)
                    if (expr->noOptOverride_) {
                        entry.noOpt = true;
                    } else {
                        entry.noOpt = noOpt(instr);
                    }
                }

                // 0x2678a9: Append entry to result
                result.append(entry);

                // 0x2679ab: Free label string if heap-allocated
                // (handled by destructor)
            }
        }

        // 0x266d87 ff: cleanup of vec_output, vec_reg, vec_input
        // (RAII at end of for-loop scope), then next iteration.
        wavetableFront++;
    }

    // Step 6: Post-processing — install node pointers
    // 0x267a2f: Iterate result entries
    for (auto& entry : result.entries) {
        if (!entry.node) continue;

        // Look up entry.sequenceId in idMap to get JSON value
        const auto& jv = idMap.at(entry.sequenceId);

        // 0x267a5a: Node::installPointers(nodeMap, jv)
        entry.node->installPointers(nodeMap, jv);
    }

    // Step 7: Get report from AWGAssembler
    // 0x267c11: call AWGAssembler::getReport()
    std::string report = assembler.getReport();

    // Step 8: Move results into return tuple
    // 0x267c20: Initialize tuple<AsmList, string>
    // AsmList data is moved via __init_with_size
    return {std::move(result), std::move(report)};
}

} // namespace zhinst
