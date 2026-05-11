#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

#include "zhinst/asm/asm_parser_context.hpp"

namespace zhinst {

struct DeviceConstants;
class Assembler;
class AsmExpression;

// LabelBimap type alias — bimap<string, multiset_of<int>>
//! \brief Boost.Bimap mapping label *names* to their referencing
//!        instruction *indices* and back, used by `AWGAssemblerImpl`
//!        to resolve forward / backward branch targets during
//!        assembly.
//!
//! \details The `left` view is `std::string -> int` (unique label
//! name -> position); the `right` view is `int -> std::string` with
//! `multiset_of<int>` semantics so multiple branches can share a
//! target position without collapsing duplicate keys. Label
//! cleanup passes (`removeUnusedLabels`, `mergeLabels`) traverse
//! the right view to find / coalesce co-located labels.
using LabelBimap = boost::bimaps::bimap<
    std::string,
    boost::bimaps::multiset_of<int>>;

// ============================================================================
// AWGAssemblerImpl
//
// sizeof(AWGAssemblerImpl) = 0x170 (verified at 0x285050: mov edi,0x170)
//
// Layout (verified against ctor at 0x285180 and dtor at 0x2853c0):
//
//   Offset  Size  Type                                    Field
//   ------  ----  ----                                    -----
//   0x000   0x08  DeviceConstants const*                  deviceConstants_
//   0x008   0x18  std::string                             filename_
//   0x020   0x18  std::string                             unusedStr020_ (no observed reader/writer in reconstructed methods)
//   0x038   0x18  std::string                             asmSource_   (binary writes contents here in assembleFile@0x286047/0x2860c9, reads here in writeToFile@0x288836)
//   0x050   0x18  std::vector<uint32_t>                   opcodes_
//                                                         (verified by getOpcode()
//                                                          at 0x289060: lea rax,[rdi+0x50];
//                                                          and by writeToFile() at
//                                                          0x2885da passing &opcodes_
//                                                          directly to ElfWriter::addCode
//                                                          which takes vector<uint32_t>)
//   0x068   0x04  uint32_t                                memoryOffset_
//                                                         (verified by setMemoryOffset()
//                                                          at 0x288560: mov [rdi+0x68],esi)
//   0x06c   0x04  (padding)
//   0x070   0x04  int32_t                                 currentLine_
//                                                         (verified by errorMessage at
//                                                          0x289080 reading [rdi+0x70] as
//                                                          the int prefix of an emitted
//                                                          Message; reset to 0 at
//                                                          assembleString+0x16c)
//   0x074   0x04  (padding)
//   0x078   0x18  std::vector<std::string>                sourceLines_
//   0x090   0x18  std::vector<Message>                    messages_
//                                                         (Message stride = 0x20)
//   0x0a8   0x48  LabelBimap                              labelBimap_
//                                                         (sentinel at 0xc8, header
//                                                          ptr at 0xc0 → 0x50-byte heap
//                                                          allocation; tree pointers
//                                                          at 0xb0/0xd8/0xe0)
//   0x0f0   0x80  AsmParserContext                        parserCtx_
//                                                         (verified: errorMessage adds
//                                                          0xf0 to this then calls
//                                                          AsmParserContext::setSyntaxError
//                                                          which writes byte at +0x03 of
//                                                          parser ctx → AWG offset 0xf3)
//
// Total: 0x170.
// ============================================================================

//! \brief Implementation of the standalone AWG assembler facade `AWGAssembler`.
//!
//! Owns the parser context, label table, accumulated source lines,
//! diagnostic message vector, and the resulting opcode buffer for one
//! assemble session.  `assembleString` / `assembleFile` /
//! `assembleAsmList` drive the same internal pipeline (parse via
//! `getAST` → label collection + opcode emission via
//! `assembleExpressions`); `assembleStringToExpressionsVec` exits
//! after parsing without emitting opcodes, exposing the AST to
//! callers that want to inspect or rewrite it before assembly.
//!
//! Diagnostics are routed through `errorMessage` /
//! `parserMessage` into the embedded `messages_` vector;
//! `errorMessage` additionally flips the parser context's
//! syntax-error flag so the caller's `getReport` reflects the
//! failure.  The opcode emitters `opcode0`..`opcode5` factor out
//! shared bit-packing for the six register / immediate operand
//! shapes; `getReg` / `getVal` resolve operand expressions to
//! register numbers / immediate bits with range checking.  An
//! instance is held inside `AWGAssembler` (the public pimpl facade)
//! and is also embedded by `AWGCompilerImpl` for post-pipeline
//! `.asm` writes.
class AWGAssemblerImpl {
public:
    // ---- Message struct — stored in messages_ vector ----
    //
    // Layout (verified from errorMessage at 0x289070 and parserMessage at
    // 0x289190; emplace stride = 0x20):
    //
    //   +0x00  int        code           — line number (when written by
    //                                       errorMessage from currentLine_)
    //                                       OR severity level (when written
    //                                       by parserMessage from `level`
    //                                       arg). The same field; the binary
    //                                       does not distinguish them. There
    //                                       is NO separate `lineNumber` slot.
    //   +0x04  (4 bytes padding)
    //   +0x08  std::string text
    //
    // Total: 0x20.
    //! \brief Diagnostic entry produced by the standalone AWG assembler.
    //!
    //! Held in `AWGAssemblerImpl::messages_` and consumed by
    //! `getReport` to render the final report string.  The single
    //! `code` slot is overloaded: `errorMessage` writes the current
    //! source line number into it, while `parserMessage` writes the
    //! caller-supplied severity level — `getReport` formats them
    //! uniformly as a leading integer followed by `text`.
    struct Message {
        int code = 0;          //!< Overloaded numeric tag: holds the source line number for compiler errors (written by `errorMessage`) or the caller-supplied severity level for parser-issued messages (written by `parserMessage`). `getReport` formats it as the leading integer of each line. +0x00 — see note above
        std::string text;      //!< Human-readable message body rendered after `code` in the assembled report. +0x08
    };

    // ---- Lifecycle ----
    //! \brief Construct an empty assembler bound to a device profile.
    //! \details Captures `dc` by raw pointer (the caller owns the
    //! `DeviceConstants` lifetime and must outlive this instance), then
    //! default-constructs every owned container — opcode buffer,
    //! source-line / message vectors, label bimap, and inline
    //! `AsmParserContext`.  No source is parsed and no opcodes are
    //! emitted until one of the `assemble*` entry points is invoked.
    //! \param dc  Per-device constants table consulted by `getReg` /
    //!            `getVal` for register-depth and immediate-range
    //!            checks during opcode encoding.
    AWGAssemblerImpl(DeviceConstants const& dc);            // 0x285180
    //! \brief Release the parser context, label bimap, message
    //!        vector, opcode buffer, and the embedded strings in
    //!        reverse construction order.
    ~AWGAssemblerImpl();                                    // 0x2853c0

    // ---- Public API ----
    //! \brief Slurp an `.asm` source file from disk and assemble it.
    //! \details Calls `boost::filesystem::status` first; a missing or
    //! unreadable entry raises `ZIAWGCompilerException` formatted with
    //! `FileNotExist`.  On success records `path` in `filename_`,
    //! reads the whole file via `ifstream::rdbuf` into `asmSource_`,
    //! and forwards to `assembleString`.
    //! \param path  Filesystem path to the assembler source.
    //! \throws zhinst::Exception  When the file does not exist or
    //!         when `assembleString` propagates a syntax error.
    void assembleFile(std::string const& path);             // 0x285ec0
    //! \brief Assemble an in-memory `.asm` source string into the
    //!        opcode buffer.
    //! \details Resets the parser's syntax-error flag and installs
    //! `parserMessage` as the lexer/parser error callback, then
    //! iterates the source line-by-line: each line is parsed by
    //! `getAST`, the resulting `AsmExpression` (if any) is tagged
    //! with the current `noOpt()` flag, appended to a local
    //! expressions vector, and a copy of the raw source line is
    //! appended to `sourceLines_`.  After all lines are processed
    //! `assembleExpressions` runs the label-collection + opcode
    //! emission pass.
    //! \param src  Assembler source text (newline-separated).
    void assembleString(std::string const& src);            // 0x286490
    //! \brief Assemble a pre-built sequence of `Assembler`
    //!        instructions emitted by the SeqC compile pipeline.
    //! \details Used by `AWGCompilerImpl::compileString` to feed the
    //! optimised `vector<Assembler>` from `Compiler::compile` into
    //! the assembler without going through textual round-tripping.
    //! Skips entries marked `ERROR_MSG` / `MESSAGE` (printing a
    //! warning to `std::cout` instead), synthesises an
    //! `AsmExpression` per remaining instruction with children laid
    //! out in the canonical immediates → regDst → regAux → regSrc →
    //! outputs → label order, then runs `assembleExpressions`.
    //! \param asmList  Instruction list from the SeqC compile
    //!                 pipeline.
    //! \note `LABEL` entries do not advance the internal
    //!       label-counter so the per-label index tracks the
    //!       eventual opcode position rather than the asmList
    //!       position.
    void assembleAsmList(std::vector<Assembler> const& asmList);
    //! \brief Parse an `.asm` source string to an expressions vector
    //!        without emitting opcodes.
    //! \details Same per-line parse loop as `assembleString` but
    //! also captures `//`-comments (via `extractComment`) onto each
    //! `AsmExpression`, fabricates a NOP expression with command 4
    //! for comment-only lines, and returns the collected vector by
    //! value (sret) so callers can inspect or rewrite the AST
    //! before driving `assembleExpressions` themselves.  Raises
    //! `ZIAWGCompilerException("Syntax error in assembly source")`
    //! after dumping the report to `std::cout` if the parser flagged
    //! a syntax error.
    //! \param src  Assembler source text.
    //! \return  Owning vector of parsed expressions (one per
    //!          non-empty line, plus NOP placeholders for comment
    //!          lines).
    //! \throws zhinst::Exception  On a parser-level syntax error.
    std::vector<std::shared_ptr<AsmExpression>>
        assembleStringToExpressionsVec(std::string const& src);   // 0x286e40
    //! \brief Set the base address used when writing the assembled
    //!        output to ELF.
    //! \details Stores `offset` into `memoryOffset_`; consumed by
    //! `writeToFile` (which adds a fixed `+0x80` device-header
    //! padding before passing to `ElfWriter::setMemoryOffset`).
    //! \param offset  Linear byte offset of the AWG instruction
    //!                memory the opcodes will be loaded into.
    void setMemoryOffset(unsigned int offset);              // 0x288560
    //! \brief Emit the most recently assembled program as a
    //!        standalone ELF on disk.
    //! \details No-op if the parser flagged a syntax error or if no
    //! opcodes were generated.  Otherwise constructs an
    //! `ElfWriter(2)` at `memoryOffset_ + 0x80`, appends the opcode
    //! stream as `.text`, the version banner as `.comment`, the
    //! source-file basename as `.filename`, and `asmSource_` as
    //! `.asm`, then flushes via `ElfWriter::writeFile`.  After a
    //! successful write the opcode buffer is cleared, so back-to-back
    //! writes require a fresh `assemble*` call.
    //! \param path  Destination ELF path (truncated if it exists).
    //! \throws zhinst::Exception  Formatted with `CantWriteFile`
    //!         when the underlying writer fails.
    //! \binarynote The opcode buffer is cleared on the success
    //!             path; `getOpcode()` returns an empty vector
    //!             after `writeToFile`.
    void writeToFile(std::string const& path);              // 0x288570
    //! \brief Access the assembled opcode stream.
    //! \details Returns a reference into the impl-owned vector;
    //! valid until the next `assemble*` / `writeToFile` call
    //! mutates or clears the buffer.
    //! \return  Const reference to the 32-bit opcode words in
    //!          program order.
    std::vector<uint32_t> const& getOpcode() const;         // 0x289060
    //! \brief Format every accumulated diagnostic message into a
    //!        single human-readable report string.
    //! \details Iterates `messages_` and emits one
    //! `"Assembler message at <code> : <text>\n"` line per entry.
    //! `code` is the line number for messages emitted by
    //! `errorMessage` and the severity level for messages emitted
    //! by `parserMessage`; the report does not distinguish them.
    //! \return  Report text; empty if no messages were emitted.
    std::string getReport() const;                          // 0x285bc0
    //! \brief Pretty-print the assembled program to `std::cout`.
    //! \details Walks the opcode buffer and emits, for each index,
    //! the resolved label name (looked up in `labelBimap_.right`),
    //! the `(idx + startIndex)` 8-digit hex offset, the 8-digit hex
    //! opcode, and the matching `sourceLines_` entry — falling back
    //! to `"nop"` for trailing zero opcodes that have no source
    //! line.
    //! \param startIndex  Offset added to each printed instruction
    //!                    index so the listing can be relocated to
    //!                    a non-zero base address.
    void printOpcode(int startIndex) const;                     // 0x288b50

    // ---- Opcode encoding (awg_assembler_opcodes.cpp) ----
    //! \brief Resolve an `AsmExpression` operand to a register
    //!        number.
    //! \details Reports `ErrorMessage` 8 ("expected register") if
    //! `expr` is not of `Register` kind, `ErrorMessage` 3 if the
    //! register number is negative, and `ErrorMessage` 16 (with the
    //! register and the device's `registerDepth`) if the number
    //! exceeds the device limit.  All error paths return 0.
    //! \param expr  Operand expression from a parsed instruction.
    //! \return  Register number bounded by
    //!          `deviceConstants_->registerDepth`.
    unsigned int getReg(std::shared_ptr<AsmExpression> const& expr);     // 0x2892b0
    //! \brief Resolve an `AsmExpression` operand to an immediate
    //!        bit-pattern of the requested width.
    //! \details Evaluates an integer-literal operand, label
    //! reference (resolved against `labelBimap_`), or arithmetic
    //! sub-expression and range-checks the result against `bits`.
    //! Out-of-range values and unresolved labels emit diagnostics
    //! via `errorMessage` and return 0.
    //! \param expr  Operand expression.
    //! \param bits  Maximum width of the encoded immediate.
    //! \return  Immediate value masked to `bits`.
    unsigned int getVal(std::shared_ptr<AsmExpression> const& expr,
                        int bits);                                       // 0x289370
    //! \brief Encode a no-operand instruction (just the base
    //!        opcode word).
    //! \param base  Base opcode value.
    //! \param expr  Parsed expression (operand list expected
    //!              empty).
    //! \return  Encoded 32-bit instruction word in the low half.
    uint64_t opcode0(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x2895c0
    //! \brief Encode a `register + 20-bit immediate` instruction.
    //! \param base  Base opcode value.
    //! \param expr  Parsed expression with `[reg, imm20]`
    //!              children.
    //! \return  Encoded 32-bit instruction word.
    uint64_t opcode1(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x289860
    //! \brief Encode a `register + three 8-bit immediates`
    //!        instruction.
    //! \param base  Base opcode value.
    //! \param expr  Parsed expression with `[reg, imm8, imm8,
    //!              imm8]` children.
    //! \return  Encoded 32-bit instruction word.
    uint64_t opcode2(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x289a10
    //! \brief Encode a two-register / immediate instruction (and
    //!        the `WVFS_H` / `ADDI` literal special cases).
    //! \param base  Base opcode value (selects the literal sub-form
    //!              for known sentinels).
    //! \param expr  Parsed expression with the operand layout
    //!              required by the chosen base.
    //! \return  Encoded 32-bit instruction word.
    uint64_t opcode3(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x289c90
    //! \brief Encode the dispatch family covering 0-, 1-, and
    //!        2-operand control / I/O instructions (`TRAP`, `JMP`,
    //!        `WTRIGI`, `ST`, `LD` variants, …).
    //! \param base  Base opcode value used both as the encoded
    //!              prefix and as a discriminator selecting the
    //!              concrete operand layout.
    //! \param expr  Parsed expression matching the layout for
    //!              `base`.
    //! \return  Encoded 32-bit instruction word.
    uint64_t opcode4(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x28a010
    //! \brief Encode a two-immediate (14-bit + 14-bit) instruction.
    //! \param base  Base opcode top nibble.
    //! \param expr  Parsed expression with `[imm14, imm14]`
    //!              children.
    //! \return  Encoded 32-bit instruction word.
    uint64_t opcode5(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x28a610

    // ---- Pipeline (awg_assembler_impl_pipeline.cpp) ----
    // Phase S.2 M5: removed three header-only declarations
    // (`parseLine`, `parseString`, `encodeExpressions`) that were
    // never defined in the recon and never referenced by any caller.
    // `nm` on `_seqc_compiler.so` confirms the binary has no
    // matching `AWGAssemblerImpl::parseLine/parseString/encodeExpressions`
    // symbols either, so the prior decls were spurious.
    //! \brief Parse a single source line into an `AsmExpression`
    //!        tree.
    //! \details Initialises the flex/bison scanner via
    //! `asmlex_init_extra(&parserCtx_, &scanner)`, scans `line` with
    //! `asm_scan_string`, drives `asmparse` to completion, and
    //! wraps the resulting raw `AsmExpression*` in a `shared_ptr`
    //! before tearing the scanner down.  Returns an empty
    //! `shared_ptr` on lexer-init failure or on a parser error
    //! (with the latter logged via `LOG_ERROR`).
    //! \param src  Source text for one assembler line.
    //! \return  Parsed expression, or empty pointer on failure.
    std::shared_ptr<AsmExpression> getAST(std::string const& src);
    //! \brief Two-pass label resolution + opcode emission core.
    //! \details Pass 1 walks `expressions` and inserts every
    //! `hasLabel` entry into `labelBimap_` keyed by name with the
    //! label's index.  Pass 2 sets `currentLine_` from the matching
    //! `lineNumbers` slot, dispatches each non-LABEL expression to
    //! `evaluate`, and appends the produced opcode word to
    //! `opcodes_`.  After the second pass a trailing zero word is
    //! appended unless the program already ends with one, and a
    //! parser-recorded syntax error raises
    //! `ZIAWGCompilerException("Syntax error in assembly source")`
    //! after dumping the report to `std::cout`.
    //! \param exprs    Collected per-line `AsmExpression` trees.
    //! \param offsets  Line numbers (one entry per expression),
    //!                 written into `currentLine_` before
    //!                 evaluating each.
    //! \throws zhinst::Exception  On a syntax error caught during
    //!         the second pass.
    void assembleExpressions(std::vector<std::shared_ptr<AsmExpression>> const& exprs,
                             std::vector<uint64_t> const& offsets);          // 0x285620
    //! \brief Dispatch one expression to the appropriate
    //!        `opcodeN` encoder and return the emitted word.
    //! \details Returns 0 for non-`Container` expressions and for
    //! `INVALID` commands.  Otherwise looks up the encoding family
    //! via `Assembler::getOpcodeType(command)` and forwards to
    //! `opcode0`..`opcode5`.
    //! \param expr  Container expression representing one
    //!              instruction.
    //! \return  Encoded opcode word in the low 32 bits.
    int evaluate(std::shared_ptr<AsmExpression> const& expr);                // 0x285b20
    //! \brief Extract the substring after the first `//` comment
    //!        marker.
    //! \details Inlined into both call sites in the binary; this
    //! out-of-line definition is provided so the reconstructed
    //! pipeline TU links.  Returns an empty string when no `//`
    //! marker is present; whitespace is preserved.
    //! \param line  Source line to inspect.
    //! \return  Comment text following the marker, or empty.
    std::string extractComment(std::string const& line);

    // ---- Error reporting helpers ----
    // Both append to messages_ at offset 0x90; the leading int of the
    // emitted Message is currentLine_ (errorMessage) or `level`
    // (parserMessage). errorMessage additionally calls
    // parserCtx_.setSyntaxError() afterwards.
    //! \brief Record a fatal assembler diagnostic and flag the
    //!        parser context as having seen a syntax error.
    //! \details Pushes a `Message{currentLine_, msg}` onto
    //! `messages_` and then calls `parserCtx_.setSyntaxError()` so
    //! downstream `hadSyntaxError()` checks short-circuit further
    //! emission.
    //! \param msg  Human-readable diagnostic text.
    void errorMessage(std::string const& msg);                               // 0x289070
    //! \brief Record a parser-level message at the supplied
    //!        severity.
    //! \details Same emit pattern as `errorMessage` but the leading
    //! `code` slot of the new `Message` carries the caller-supplied
    //! `level` rather than the current source line.
    //! \param level  Severity level passed in by the lexer/parser
    //!               error callback.
    //! \param msg    Diagnostic text.
    //! \binarynote Like `errorMessage`, this also flips
    //!             `parserCtx_.setSyntaxError()`, so any parser
    //!             call site treating it as a non-fatal warning
    //!             will still cause subsequent emission stages to
    //!             short-circuit.
    void parserMessage(int level, std::string const& msg);                   // 0x289190

    // ---- Label access ----
    //! \brief Read-only view of the resolved label table.
    //! \return  Reference to the `name ↔ index` bimap populated
    //!          during `assembleExpressions`.
    LabelBimap const& getLabelBimap() const { return labelBimap_; }

    // ---- Fields (verified offsets in comment block above) ----
    //! \brief Per-device profile (register depth, immediate ranges)
    //!        consulted by `getReg` / `getVal`; not owned.
    DeviceConstants const* deviceConstants_;     // 0x000  //!< Per-device profile (register depth, immediate ranges) consulted by `getReg` / `getVal`; not owned.
    //! \brief Source filename recorded by `assembleFile`; emitted as
    //!        the `.filename` ELF section by `writeToFile`.
    std::string filename_;                       // 0x008  //!< Source filename recorded by `assembleFile`; surfaced in the `.filename` ELF section by `writeToFile`.
    //! \brief Reserved string slot present in the binary layout but
    //!        with no observed reader or writer in the reconstructed
    //!        pipeline.
    std::string unusedStr020_;                    // 0x020  //!< Reserved string slot present in the binary layout but with no observed reader or writer in the reconstructed pipeline.
    //! \brief Cached source text from the most recent
    //!        `assembleFile`; emitted as the `.asm` ELF section by
    //!        `writeToFile`.
    std::string asmSource_;                       // 0x038  //!< Cached source text from the most recent `assembleFile` call; emitted as the `.asm` section by `writeToFile`.
    //! \brief Emitted 32-bit instruction words; populated by
    //!        `assembleExpressions` and exposed via `getOpcode`.
    std::vector<uint32_t> opcodes_;              // 0x050  //!< Emitted 32-bit instruction words; populated by `assembleExpressions` and exposed via `getOpcode`.
    //! \brief Base address used when writing the standalone ELF;
    //!        `writeToFile` adds a fixed `+0x80` device-header
    //!        padding before forwarding to `ElfWriter`.
    uint32_t memoryOffset_ = 0;                  // 0x068  //!< Base address used when writing the standalone ELF; `writeToFile` adds a fixed `+0x80` device-header padding before forwarding to `ElfWriter`.
    //! \brief Alignment padding to keep `currentLine_` 8-byte
    //!        aligned.
    uint32_t pad_memOffset_ = 0;                 // 0x06c  //!< Alignment padding to keep `currentLine_` 8-byte aligned.
    //! \brief Source line currently being parsed / evaluated; written
    //!        into the `code` slot of each `errorMessage` diagnostic.
    int32_t currentLine_ = 0;                    // 0x070  //!< Source line currently being parsed / evaluated; written into the `code` slot of each `errorMessage` diagnostic.
    //! \brief Alignment padding ahead of the embedded vectors.
    uint32_t pad_currentLine_ = 0;               // 0x074  //!< Alignment padding ahead of the embedded vectors.
    //! \brief Per-line source-text cache appended to during the
    //!        parse pass; consumed by `printOpcode` to interleave
    //!        instructions with their text.
    std::vector<std::string> sourceLines_;       // 0x078  //!< Per-line source-text cache appended to during the parse pass; consumed by `printOpcode` to interleave instructions with their text.
    //! \brief Diagnostic vector populated by `errorMessage` /
    //!        `parserMessage` and rendered by `getReport`.
    std::vector<Message> messages_;              // 0x090  //!< Diagnostic vector populated by `errorMessage` / `parserMessage` and rendered by `getReport`.
    //! \brief Bidirectional `name ↔ opcode-index` map populated in
    //!        pass 1 of `assembleExpressions` and read by `getVal`
    //!        / `printOpcode`.
    LabelBimap labelBimap_;                      // 0x0a8  //!< Bidirectional `name ↔ opcode-index` map populated in pass 1 of `assembleExpressions` and read by `getVal` / `printOpcode`.
    //! \brief Inline flex/bison parser context shared with the
    //!        lexer (`yyextra`) and used to query the syntax-error
    //!        flag after assembly.
    AsmParserContext parserCtx_;                 // 0x0f0  //!< Inline flex/bison context shared with the lexer (yyextra) and used to query the syntax-error flag after assembly.
};

// Verifying sizeof(Message) == 0x20 requires the binary's libc++ ABI
// (std::__1::basic_string == 24 bytes). Build hosts using libstdc++ have
// std::string == 32 bytes, so a host-side static_assert(sizeof(Message) == 0x20)
// would falsely fail. Layout has been verified directly from the disassembly:
// Message stride is 0x20 (errorMessage emplace at 0x289070: add r14, 0x20).

// A static_assert(sizeof(AWGAssemblerImpl) == 0x170) is desirable
// but depends on libc++/Boost ABI sizes for vector, string, bimap, and
// AsmParserContext matching the reconstructed offsets exactly. Add it
// once those component sizes are verified end-to-end.

} // namespace zhinst
