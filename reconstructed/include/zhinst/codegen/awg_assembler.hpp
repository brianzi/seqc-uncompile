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
    //! \brief Construct an assembler bound to a per-device
    //!        profile.
    //! \details Allocates the 0x170-byte `AWGAssemblerImpl` heap
    //! object via plain `operator new` and stores its address in
    //! `pImpl_`.  No source is parsed and no opcodes are emitted
    //! until one of the `assemble*` entry points is called.
    //! \param dc  Per-device constants table (register depth,
    //!            immediate ranges); captured by reference,
    //!            forwarded to the impl by pointer.  The caller
    //!            owns the lifetime and must keep `dc` alive for
    //!            the full lifetime of this `AWGAssembler`.
    AWGAssembler(DeviceConstants const& dc);
    //! \brief Destroy the assembler and release every owned
    //!        resource.
    //! \details Runs `AWGAssemblerImpl`'s destructor (which frees
    //! the parser context, label bimap, message vector, opcode
    //! buffer, and embedded strings in reverse construction
    //! order) and then frees the impl block via
    //! `operator delete(pImpl_, sizeof(AWGAssemblerImpl))`.
    ~AWGAssembler();

    //! \brief Slurp an `.asm` source file from disk and assemble
    //!        it.
    //! \details Thin forwarder to
    //! `AWGAssemblerImpl::assembleFile`; see that method for the
    //! file-existence check, source caching, and error
    //! behaviour.
    //! \param path  Filesystem path to the assembler source.
    //! \throws zhinst::Exception  Propagated from the impl when
    //!         the file does not exist or the assembler reports
    //!         a syntax error.
    void assembleFile(std::string const& path);
    //! \brief Assemble an in-memory `.asm` source string into the
    //!        impl-owned opcode buffer.
    //! \details Thin forwarder to
    //! `AWGAssemblerImpl::assembleString`; see that method for
    //! the line-by-line parse / label-resolution / opcode-emit
    //! pipeline.
    //! \param src  Assembler source text.
    void assembleString(std::string const& src);
    //! \brief Assemble a pre-built sequence of `Assembler`
    //!        instructions emitted by the SeqC compile pipeline.
    //! \details Thin forwarder to
    //! `AWGAssemblerImpl::assembleAsmList`.
    //! \param asmList  Pre-built instruction list.
    void assembleAsmList(std::vector<Assembler> const& asmList);
    // Returns by value (sret); vector of parsed expression objects.
    //! \brief Parse an `.asm` source string to an expressions
    //!        vector without emitting opcodes.
    //! \details Thin forwarder to
    //! `AWGAssemblerImpl::assembleStringToExpressionsVec`; see
    //! that method for the per-line parse behaviour, comment
    //! handling, and exception contract.
    //! \param src  Assembler source text.
    //! \return  Owning vector of parsed expressions.
    //! \throws zhinst::Exception  On a parser-level syntax error.
    std::vector<std::shared_ptr<struct AsmExpression>> assembleStringToExpressionsVec(std::string const& src);
    //! \brief Set the base address used when emitting the
    //!        assembled program to ELF.
    //! \details Thin forwarder to
    //! `AWGAssemblerImpl::setMemoryOffset`; the impl adds a
    //! fixed `+0x80` device-header padding before passing to the
    //! ELF writer.
    //! \param offset  Linear byte offset of the AWG instruction
    //!                memory.
    void setMemoryOffset(unsigned int offset);
    //! \brief Emit the most recently assembled program as a
    //!        standalone ELF on disk.
    //! \details Thin forwarder to
    //! `AWGAssemblerImpl::writeToFile`; no output is produced
    //! when the parser flagged a syntax error or when no opcodes
    //! were generated.
    //! \param path  Destination ELF path (truncated if it
    //!              exists).
    //! \throws zhinst::Exception  When the underlying writer
    //!         fails.
    //! \binarynote The impl clears the opcode buffer on the
    //!             success path; `getOpcode()` returns an empty
    //!             vector after a successful `writeToFile`.
    void writeToFile(std::string const& path);
    // Returns pointer to internal vector<uint64_t> at impl+0x50
    //! \brief Access the assembled opcode stream.
    //! \details Thin forwarder to
    //! `AWGAssemblerImpl::getOpcode`; the returned reference is
    //! valid until the next `assemble*` or `writeToFile` call
    //! mutates or clears the impl buffer.
    //! \return  Const reference to the 32-bit opcode words in
    //!          program order.
    std::vector<uint32_t> const& getOpcode() const;
    // Returns by value (sret); likely std::string
    //! \brief Format every accumulated diagnostic into a single
    //!        human-readable report string.
    //! \details Thin forwarder to
    //! `AWGAssemblerImpl::getReport`.
    //! \return  Report text; empty when no diagnostics were
    //!          emitted.
    std::string getReport() const;
    //! \brief Pretty-print the assembled program to `std::cout`.
    //! \details Thin forwarder to
    //! `AWGAssemblerImpl::printOpcode`.
    //! \param startIndex  Offset added to each printed
    //!                    instruction index so the listing can
    //!                    be relocated to a non-zero base
    //!                    address.
    void printOpcode(int startIndex) const;

private:
    // unique_ptr semantics (ctor news 0x170, dtor deletes with size 0x170)
    // Implemented as raw owning pointer (no unique_ptr control block observed)
    //! \brief Heap-allocated implementation owned by this pimpl
    //!        facade.
    //! \details Allocated with `operator new(0x170)` in the
    //! constructor and released by the destructor; every public
    //! method tail-calls the matching `AWGAssemblerImpl` member
    //! via `pImpl_`.  Never `nullptr` for the lifetime of the
    //! enclosing `AWGAssembler`.
    AWGAssemblerImpl* pImpl_;  // offset 0x00
};

} // namespace zhinst
