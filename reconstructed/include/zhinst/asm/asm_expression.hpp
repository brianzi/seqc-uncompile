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
//   0x00    0x04  AsmExprType                               type
//                   Container=0, Register=1, Label=2, Integer=3
//   0x04    0x04  (padding)
//   0x08    0x18  std::string                               name
//                   Command/register/label name string.
//                   Set by createName (type=2). Read by addCommand
//                   to resolve command via commandFromString().
//   0x20    0x18  std::string                               nopComment
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
//   0xA0    0x01  bool                                      noOptOverride_
//                   noOpt override flag. When true,
//                   parseStringToAsmList forces noOpt = true
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

class Assembler;

// AsmExprType — expression node type discriminator (A2)
//! \brief Tag selecting which payload an `AsmExpression` carries.
enum class AsmExprType : int32_t {
    Container = 0,   //!< Operand-list / container node holding child expressions.
    Register  = 1,   //!< Register reference; register number is in `value`.
    Label     = 2,   //!< Label or command-name token; identifier is in `name`.
    Integer   = 3,   //!< Integer immediate; numeric value is in `value`.
};

// sizeof = 0xa8 (168 bytes)
//! \brief Parse-tree node produced by the AWG assembler's flex/bison parser.
//!
//! Each instruction line scanned by the assembler produces a tree of
//! `AsmExpression` nodes whose root is a container (operand list) and
//! whose children are typed by `AsmExprType` — register references,
//! integer immediates, and label/command name tokens.  The factory
//! free functions in this header are the actions invoked from the
//! parser grammar.
//!
//! Two downstream consumers walk these trees:
//!
//! - `AWGAssemblerImpl::assembleExpressions` runs a two-pass label
//!   collection / opcode emission over the parsed expressions to
//!   produce the binary instruction stream.
//! - `AsmList::parseStringToAsmList` classifies each node's operands
//!   by `type` to rebuild a structured `AsmList`, decoding the JSON
//!   payload carried in `comment` to recover prefetch / noOpt
//!   placeholder metadata.
//!
//! Label data (`labelIndex`, `labelName`, `hasLabel`) is attached by
//! `addCommand` when the parser sees `label: instr ...`.  Comment
//! data (`comment`, `hasComment`, `noOptOverride_`) is attached by
//! `addNode` from text following `#` on the line and is interpreted
//! as JSON only when the consumer is `parseStringToAsmList`.
struct AsmExpression {
    AsmExprType type;       //!< Discriminator for which payload fields are meaningful.
    // 4 bytes padding       // +0x04
    std::string name;       //!< Command, register, or label identifier text.
    std::string nopComment;       //!< Secondary string used for NOP marker comment text.
    uint32_t command;       //!< Resolved `Assembler::Command` opcode (`INVALID` until set by `addCommand`).
    int32_t value;          //!< Integer immediate (type=Integer), register number (type=Register), or PC for command nodes.
    std::vector<std::shared_ptr<AsmExpression>> children;  //!< Child expressions; populated for Container nodes by `createArgList` / `appendArgList`.
    int32_t labelIndex;     //!< Label PC value attached by `addCommand` (also called `labelPc` at some call sites).
    // 4 bytes padding       // +0x5C
    std::string labelName;  //!< Label identifier when `hasLabel` is true.
    bool hasLabel;          //!< True when `labelIndex` and `labelName` carry valid label data.
    // 7 bytes padding       // +0x79
    std::string comment;    //!< Trailing `#`-prefixed comment text; treated as JSON metadata by `parseStringToAsmList`.
    bool hasComment;        //!< True when `comment` is populated; aliased as `noOpt` at some call sites.
    // 7 bytes padding       // +0x99
    bool noOptOverride_;          //!< When true, forces `noOpt` on the resulting `AsmList::Asm` entry regardless of `hasComment`.
    // 7 bytes padding       // +0xA1 to 0xA8

    // Accessor aliases (forwarding methods, NOT separate storage —
    // these don't change struct layout). Different reconstructed call
    // sites use different names for the same fields:
    //   labelPc      → labelIndex (+0x58) — positive evidence, kept
    //   noOpt        → hasComment (+0x98)
    //   lineNumber, labelType → dropped (Cluster E); callers use labelIndex, hasLabel directly
    //! \brief Mutable alias for `labelIndex`, named to match call
    //! sites that treat the field as a program-counter slot.
    //! \return Reference to the underlying `labelIndex` storage.
    int32_t&       labelPc()           { return labelIndex; }
    //! \brief Read-only alias for `labelIndex`, named to match call
    //! sites that treat the field as a program-counter slot.
    //! \return Current `labelIndex` value.
    int32_t        labelPc()    const  { return labelIndex; }
    //! \brief Mutable alias for `hasComment`, named to match call
    //! sites that treat the field as the per-instruction `noOpt`
    //! flag.
    //! \return Reference to the underlying `hasComment` storage.
    bool&          noOpt()             { return hasComment; }
    //! \brief Read-only alias for `hasComment`, named to match call
    //! sites that treat the field as the per-instruction `noOpt`
    //! flag.
    //! \return Current `hasComment` value.
    bool           noOpt()      const  { return hasComment; }

    //! \brief Releases all owned strings and child expressions.
    //! \details Destruction order mirrors the binary: the optional
    //! `comment` and `labelName` strings are released only when
    //! their `hasComment` / `hasLabel` guards are set, then the
    //! `children` vector releases its `shared_ptr`s, followed by
    //! `nopComment` and `name`.
    ~AsmExpression();       // 0x28b1f0
};

// ---- Factory free functions (called by flex/bison parser actions) ----

// Allocate an AsmExpression with type=Integer and the given integer value.
//! \brief Allocates an integer-immediate `AsmExpression`.
//! \param value Integer value to store in the new expression's
//! `value` field.
//! \return Heap-allocated `AsmExpression` with `type = Integer`.
AsmExpression* createValue(int value);                          // 0x28bb90

// Allocate an AsmExpression with type=Register and the given register number.
//! \brief Allocates a register-reference `AsmExpression`.
//! \param regNum Register index to store in the new expression's
//! `value` field.
//! \return Heap-allocated `AsmExpression` with `type = Register`.
AsmExpression* createRegister(int regNum);                      // 0x28bbf0

// Allocate an AsmExpression with type=Label and name from the C string.
// Returns nullptr if the input string is empty.
//! \brief Allocates a label/name-token `AsmExpression`.
//! \param name C string copied into the new expression's `name`
//! field.
//! \return Heap-allocated `AsmExpression` with `type = Label`, or
//! `nullptr` when `name` is empty.
AsmExpression* createName(const char* name);                    // 0x28bc50

// Allocate a new container AsmExpression (type=Container) and push `first`
// as the first child (wrapped in shared_ptr).
//! \brief Allocates a container `AsmExpression` and seeds it with
//! one child.
//! \details `first` is wrapped in a `std::shared_ptr` and pushed
//! into the container's `children` vector; the container takes
//! ownership of the raw pointer.
//! \param first Child expression to adopt; may be null to create
//! an empty container.
//! \return Heap-allocated container expression.
AsmExpression* createArgList(AsmExpression* first);             // 0x28bdc0

// Append `child` to `list`'s children vector (wrapped in shared_ptr).
// If `list` is null, allocates a fresh container first.
//! \brief Appends one child to a container `AsmExpression`,
//! allocating the container on demand.
//! \details `child` is wrapped in a `std::shared_ptr` and pushed
//! into `list->children`; ownership transfers to the container.
//! When `list` is null a fresh container is allocated first; when
//! `child` is null only the container is returned.
//! \param list Container expression to extend; may be null.
//! \param child Child expression to adopt; may be null.
//! \return The extended (or newly allocated) container expression.
AsmExpression* appendArgList(AsmExpression* list,
                             AsmExpression* child);             // 0x28bec0

// Stringify an AsmExpression tree (for debug/serialization).
//! \brief Renders an `AsmExpression` tree as a debug/disassembly
//! string.
//! \details Each node prints its head (mnemonic, register, label,
//! or integer) followed by ` (<tag>)\n` identifying the
//! discriminator (`cmd`, `reg`, `name`, `value`, or `?` for an
//! unknown type), then each child indented by two spaces on its
//! own line.  Out-of-range type discriminators emit a Debug log
//! record `Unknown AsmOperationType with code <n>.` and contribute
//! no head text.
//! \param expr Root expression to stringify; must be non-null.
//! \return Human-readable representation of the tree.
//! \verifyme Unverifiable from SeqC: the function has no external
//! caller in the binary (only a recursive self-call inside its body),
//! so no `compile_seqc` input drives a difftest of its output.
//! Reconstructed from disassembly; whitespace and tag spelling
//! cannot be confirmed against any reference output.
std::string str(const std::shared_ptr<AsmExpression>& expr);    // 0x28cd20

}  // namespace zhinst
