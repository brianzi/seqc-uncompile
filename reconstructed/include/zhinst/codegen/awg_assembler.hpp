#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zhinst {

class AWGAssemblerImpl;
class Assembler;
struct DeviceConstants;

// sizeof(AWGAssembler) = 0x8
//! \brief Standalone assembler for hand-written AWG `.asm` text.
//!
//! Exposes the assembler half of `_seqc_compiler` independently of the
//! SeqC compiler front end: callers feed in textual instruction
//! sequences (via `assembleString`/`assembleFile`/`assembleAsmList`)
//! using the mnemonic syntax accepted by the flex/bison parser
//! (`AsmParserContext` + `asm_lexer.l` + `asm_parser.y`), and the
//! assembler resolves labels, computes the program counter, and
//! produces the binary opcode stream returned by `getOpcode()`.
//! Unlike the SeqC compiler pipeline, this path supports the
//! diagnostic-only `msg` / `rer` mnemonics (warning / error directives
//! routed through `AsmOptimize::reportUserMessages`).  A pimpl over
//! `AWGAssemblerImpl`; one instance is also embedded inside
//! `AWGCompilerImpl` to support post-pipeline `.asm` writes from the
//! main compiler.
class AWGAssembler {
public:
    AWGAssembler(DeviceConstants const& dc);
    ~AWGAssembler();

    void assembleFile(std::string const& path);
    void assembleString(std::string const& src);
    void assembleAsmList(std::vector<Assembler> const& asmList);
    // Returns by value (sret); vector of parsed expression objects.
    std::vector<std::shared_ptr<struct AsmExpression>> assembleStringToExpressionsVec(std::string const& src);
    void setMemoryOffset(unsigned int offset);
    void writeToFile(std::string const& path);
    // Returns pointer to internal vector<uint64_t> at impl+0x50
    std::vector<uint32_t> const& getOpcode() const;
    // Returns by value (sret); likely std::string
    std::string getReport() const;
    void printOpcode(int startIndex) const;

private:
    // unique_ptr semantics (ctor news 0x170, dtor deletes with size 0x170)
    // Implemented as raw owning pointer (no unique_ptr control block observed)
    AWGAssemblerImpl* pImpl_;  // offset 0x00
};

} // namespace zhinst
