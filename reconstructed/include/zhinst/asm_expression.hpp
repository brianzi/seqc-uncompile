// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmExpression — parse tree node for the AWGAssembler's flex/bison parser
//
// sizeof(AsmExpression) = 0xa8 (168 bytes)
//
// Layout (from dtor @0x28b1f0, createValue @0x28bb90, createRegister @0x28bbf0,
// createName @0x28bc50, createArgList @0x28bdc0, appendArgList @0x28bec0,
// addNode @0x28bfd0, addCommand @0x28c600, and consumer methods):
//
//   Offset  Size  Type                                      Field
//   ------  ----  ----                                      -----
//   0x00    0x04  int32_t                                   type
//                   1 = register, 2 = label/name, 3 = integer/value
//   0x04    0x04  (padding)
//   0x08    0x18  std::string                               name
//                   Command/register/label name string.
//                   Set by createName (type=2). Read by addCommand
//                   to resolve command via commandFromString().
//   0x20    0x18  std::string                               str2
//                   Second string field. Used for NOP marker
//                   comment text (parseStringToAsmList case exprCmd==4).
//   0x38    0x04  uint32_t (Assembler::Command)             command
//                   Instruction command enum. Set to 0xFFFFFFFF (INVALID)
//                   by addNode, 0x02 (LABEL) by addCommand for pure labels.
//   0x3C    0x04  int32_t                                   value
//                   Integer value for type=3 (immediate) or type=1 (register
//                   number). Also stores program counter from addCommand.
//   0x40    0x18  std::vector<std::shared_ptr<AsmExpression>> children
//                   Child expression nodes (operands). Populated by
//                   createArgList/appendArgList. Iterated by
//                   parseStringToAsmList to categorize operands by type.
//   0x58    0x04  int32_t                                   labelIndex
//                   Label index / program counter / label-counter value.
//                   Set by addCommand (cmd->labelPc = lbl.pc) at +0x58, and
//                   by AWGAssemblerImpl::assembleAsmList (mov [r14+0x70], ecx
//                   where r14 = ctrl block start, so r14+0x70 = expr+0x58).
//                   Read by AWGAssemblerImpl::assembleExpressions
//                   (mov r15d, [rax+0x58]) during the first label-collection
//                   pass. ALSO referred to as "labelPc" or "lineNumber" in
//                   different reconstructed call sites — they are aliases.
//   0x5C    0x04  (padding to 8-byte align +0x60 string)
//   0x60    0x18  std::string                               labelName
//                   Label name string. Set by addCommand. Destroyed by
//                   dtor when hasLabel (+0x78) is true.
//   0x78    0x01  bool                                      hasLabel
//                   True if this expression carries label data. Called
//                   "labelType" in parseStringToAsmList. Guards dtor
//                   destruction of labelName string.
//   0x79    0x07  (padding)
//   0x80    0x18  std::string                               comment
//                   Comment string. Set by addNode (text after '#').
//                   When hasComment is true, parsed as JSON by
//                   parseStringToAsmList for "noOpt" placeholder data.
//   0x98    0x01  bool                                      hasComment
//                   True if the comment field is populated. Called "noOpt"
//                   in parseStringToAsmList. Guards dtor destruction of
//                   comment string.
//   0x99    0x07  (padding)
//   0xA0    0x01  bool                                      field_A0
//                   isWaveformCmd override flag. When true,
//                   parseStringToAsmList forces isWaveformCmd = true
//                   on the resulting AsmList::Asm entry.
//   0xA1    0x07  (padding to 0xA8)
//
// Dtor destruction order (reverse of field layout):
//   1. If hasComment (+0x98): destroy string at +0x80 (free heap buffer)
//   2. If hasLabel (+0x78): destroy string at +0x60 (free heap buffer)
//   3. Destroy vector<shared_ptr> at +0x40 (release all children, free buffer)
//   4. Destroy string at +0x20 (free heap buffer if long)
//   5. Destroy string at +0x08 (free heap buffer if long)
//
// Initialization pattern (common to createValue, createRegister, createName,
// createArgList, appendArgList):
//   - type (+0x00) set per factory function
//   - xorps zero +0x08..+0x57 (strings, cmd, value, vector, +0x58 byte)
//   - Explicit byte zeros: +0x78, +0x80, +0x98, +0xA0
//   - value (+0x3C) set per factory function
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zhinst {

class AsmParserContext;

namespace Assembler {
    enum Command : uint32_t;
}

// sizeof = 0xa8 (168 bytes)
struct AsmExpression {
    int32_t type;           // +0x00  1=reg, 2=label/name, 3=int/value
    // 4 bytes padding       // +0x04
    std::string name;       // +0x08  command/register/label name
    std::string str2;       // +0x20  secondary string (NOP comment text)
    uint32_t command;       // +0x38  Assembler::Command enum value
    int32_t value;          // +0x3C  integer value / register number / PC
    std::vector<std::shared_ptr<AsmExpression>> children;  // +0x40
    int32_t labelIndex;     // +0x58  label index/pc/counter (see header comment)
    // 4 bytes padding       // +0x5C
    std::string labelName;  // +0x60  label name string
    bool hasLabel;          // +0x78  true if label data present
    // 7 bytes padding       // +0x79
    std::string comment;    // +0x80  comment / JSON blob string
    bool hasComment;        // +0x98  true if comment present ("noOpt" flag)
    // 7 bytes padding       // +0x99
    bool field_A0;          // +0xA0  isWaveformCmd override flag
    // 7 bytes padding       // +0xA1 to 0xA8

    // Accessor aliases (forwarding methods, NOT separate storage —
    // these don't change struct layout). Different reconstructed call
    // sites use different names for the same fields:
    //   labelPc / lineNumber → labelIndex (+0x58)
    //   noOpt                → hasComment (+0x98)
    //   labelType            → hasLabel   (+0x78)
    int32_t&       labelPc()           { return labelIndex; }
    int32_t        labelPc()    const  { return labelIndex; }
    int32_t&       lineNumber()        { return labelIndex; }
    int32_t        lineNumber() const  { return labelIndex; }
    bool&          noOpt()             { return hasComment; }
    bool           noOpt()      const  { return hasComment; }
    bool&          labelType()         { return hasLabel; }
    bool           labelType()  const  { return hasLabel; }

    ~AsmExpression();       // 0x28b1f0
};

// ---- Factory free functions (called by flex/bison parser actions) ----

// Allocate an AsmExpression with type=3 and the given integer value.
AsmExpression* createValue(int value);                          // 0x28bb90

// Allocate an AsmExpression with type=1 and the given register number.
AsmExpression* createRegister(int regNum);                      // 0x28bbf0

// Allocate an AsmExpression with type=2 and name from the C string.
// Returns nullptr if the input string is empty.
AsmExpression* createName(const char* name);                    // 0x28bc50

// Allocate a new container AsmExpression (type=0) and push `first`
// as the first child (wrapped in shared_ptr).
AsmExpression* createArgList(AsmExpression* first);             // 0x28bdc0

// Append `child` to `list`'s children vector (wrapped in shared_ptr).
// If `list` is null, allocates a fresh container first.
AsmExpression* appendArgList(AsmExpression* list,
                             AsmExpression* child);             // 0x28bec0

// Stringify an AsmExpression tree (for debug/serialization).
std::string str(const std::shared_ptr<AsmExpression>& expr);    // 0x28cd20

}  // namespace zhinst
