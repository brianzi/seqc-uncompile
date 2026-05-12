// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmParserContext — parser context for the AWGAssembler's flex/bison parser
//
// sizeof(AsmParserContext) >= 0x98 (exact size depends on unordered_set tail)
//
// Layout (from accessor methods at 0x28e7a0–0x28ead0 and field accesses
// in addNode, addCommand, raiseError, setErrorCallback, trackedStringCopy,
// cleanStringCopies, addLabel, hasLabel):
//
//   Offset  Size  Type                                          Field
//   ------  ----  ----                                          -----
//   0x00    0x01  bool                                          isComment_
//   0x01    0x01  bool                                          isLineComment_
//   0x02    0x01  bool                                          isBlockComment_
//   0x03    0x01  bool                                          hadSyntaxError_
//   0x04    0x01  bool                                          doOpt_
//   0x05    0x03  (padding)
//   0x08    0x04  int32_t                                       lineNumber_
//   0x0C    0x04  int32_t                                       programCounter_
//   0x10    0x30  std::function<void(int, string const&)>       errorCallback_
//   0x40    0x18  std::vector<char*>                            stringCopies_
//   0x58    ????  std::unordered_set<std::string>               labels_
//
// Notes:
//   - clearSyntaxError() zeroes the first 4 bytes (DWORD at +0x00) and sets
//     lineNumber_ to 1.
//   - startLineComment() sets isComment_=1, isLineComment_=1 unless
//     isBlockComment_ is already set.
//   - startBlockComment() sets isComment_=1, isBlockComment_=1 unless
//     isLineComment_ is already set.
//   - trackedStringCopy() calls strdup() and pushes result into stringCopies_.
//   - cleanStringCopies() calls free() on each element, then resets size to 0.
//   - addLabel() delegates to unordered_set::emplace at this+0x58.
//   - hasLabel() delegates to unordered_set::find at this+0x58.
//   - raiseError() invokes errorCallback_ if set, else logs via boost::log.
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace zhinst {

// Forward declarations
class AsmExpression;

//! \brief Per-parse mutable state passed as `yyextra` to the asm flex/bison parser.
//!
//! One instance is created per assembler input and threaded through the
//! generated scanner / parser via `asmset_extra` and `asmlex_init_extra`.
//! It tracks the lexer's comment state (line vs block), accumulates the
//! current line and program-counter position as the grammar progresses,
//! caches the set of label names declared so far so forward references
//! can be validated, owns the heap copies of identifier strings handed
//! to the parser by `trackedStringCopy` (freed in bulk by
//! `cleanStringCopies`), and forwards diagnostics through an optional
//! `errorCallback_` (falling back to boost::log when no callback is
//! installed).  The `doOpt_` flag is set to true at the start of every
//! line (rule `inst`) and cleared when the trailing `DISABLE_OPT`
//! token is seen (rule `inst DISABLE_OPT`); downstream the
//! `AWGAssemblerImpl` pipeline copies `!doOpt()` into the
//! per-instruction `noOpt` field so the optimiser leaves that
//! instruction alone.
class AsmParserContext {
public:
    // ---- Inner types ----

    // Label: (programCounter, name) pair used by addCommand to record labels.
    // sizeof(Label) = 0x20 (int + padding? + std::string)
    //
    // Layout (from Label ctor at 0x28eaa0):
    //   +0x00  int32_t   pc          — program counter value
    //   +0x04  (4 bytes padding)
    //   +0x08  std::string name      — label name (0x18 bytes, libc++ SSO)
    //! \brief Pairs a program-counter value with a label name.
    //!
    //! Constructed by `addCommand` whenever the parser sees a
    //! `name: instr ...` line, attached to the resulting
    //! `AsmExpression`'s `labelIndex` / `labelName` fields, and
    //! recorded in the enclosing context's `labels_` set so later
    //! references can resolve forward jumps.
    struct Label {
        int32_t pc;               //!< Program-counter value at which the label was defined.
        // 4 bytes padding
        std::string name;         //!< Label identifier as written in the source.

        //! \brief Constructs a label/PC pair.
        //! \param pc Program-counter value to associate with the label.
        //! \param name Label identifier; moved into the new instance.
        Label(int pc, std::string name);                   // 0x28eaa0
        //! \brief Compares two labels for equality by name only (the
        //! `pc` field is not consulted).
        //! \param other Label to compare against.
        //! \return `true` if both labels share the same name string.
        bool operator==(const Label& other) const;         // 0x28ead0
    };

    // ---- Comment tracking ----

    //! \brief Returns true while the lexer is inside any comment
    //! (line- or block-form).
    //! \return `true` while either a line- or block-comment scope is
    //! currently latched.
    bool isComment() const;                                // 0x28e7a0
    //! \brief Returns true while the lexer is specifically inside a
    //! line comment.
    //! \return `true` only while a line-comment scope is active.
    bool isLineComment() const;                            // 0x28e7b0
    //! \brief Begins line-comment state.
    //! \note No-op when already inside a block comment, so a `//`
    //! sequence appearing inside a `/* ... */` span does not switch
    //! the lexer into line-comment mode.
    void startLineComment();                               // 0x28e7c0
    //! \brief Ends line-comment state at the next newline.
    //! \note No-op when inside a block comment.
    void endLineComment();                                 // 0x28e7e0
    //! \brief Begins block-comment state.
    //! \note No-op when already inside a line comment, so a `/*`
    //! sequence inside a `//` line does not start a block scope.
    void startBlockComment();                              // 0x28e830
    //! \brief Ends block-comment state.
    //! \note No-op when inside a line comment.
    void endBlockComment();                                // 0x28e850

    // ---- Optimization flag ----

    //! \brief Clears the per-line `doOpt_` flag, marking the current
    //! instruction as opt-out for the downstream optimiser.
    //! \details The grammar invokes this on the trailing
    //! `DISABLE_OPT` token; `AWGAssemblerImpl` later copies
    //! `!doOpt()` into the per-instruction `noOpt` field so the
    //! optimiser leaves that instruction alone.
    void disableOpt();                                     // 0x28e800
    //! \brief Sets the `doOpt_` flag back to true; called at the
    //! start of every line so each instruction begins opted-in.
    void enableOpt();                                      // 0x28e810
    //! \brief Returns the current value of the per-line optimisation
    //! flag.
    //! \return `true` while the current instruction is eligible for
    //! optimisation; `false` once `disableOpt()` has fired for this
    //! line.
    bool doOpt() const;                                    // 0x28e820

    // ---- Syntax error tracking ----

    //! \brief Returns the latched syntax-error flag.
    //! \return `true` if `setSyntaxError()` has been called since the
    //! last `clearSyntaxError()`.
    bool hadSyntaxError() const;                           // 0x28e870
    //! \brief Latches the sticky syntax-error flag, consulted by the
    //! assembler driver to reject the whole input after parsing.
    void setSyntaxError();                                 // 0x28e880
    //! \brief Resets per-parse state: clears all comment flags and
    //! the syntax-error flag, and resets `lineNumber_` to 1.
    //! \binarynote Despite the name, this method clears more than
    //! the syntax-error flag — it also discards any latched comment
    //! state.  It does **not** clear the error callback, the program
    //! counter, the tracked string list, or the label set.
    void clearSyntaxError();                               // 0x28e890

    // ---- Line / program counter ----

    //! \brief Returns the current 1-based source line number.
    //! \return The line counter as last advanced by
    //! `incrementLineNumber()`.
    int currentLineNumber() const;                         // 0x28e8b0
    //! \brief Increments the source line counter; called by the
    //! lexer on every newline.
    void incrementLineNumber();                            // 0x28e8c0
    //! \brief Returns the current program-counter value used to
    //! address generated instructions.
    //! \return The instruction-address counter as last advanced by
    //! `incrementProgramCounter()`.
    int programCounter() const;                            // 0x28e8d0
    //! \brief Advances the program counter by one; called by the
    //! grammar after each successfully recognised instruction.
    void incrementProgramCounter();                        // 0x28e8e0

    // ---- String memory management ----

    //! \brief Duplicates a C string with `strdup` and records the
    //! allocation for bulk cleanup.
    //! \details The returned pointer is owned by the context and
    //! freed by `cleanStringCopies()`; the caller must not `free`
    //! it itself.  Used by the lexer to give the parser stable
    //! pointers to identifier and string-literal tokens.
    //! \param s Source C string to duplicate (must not be null).
    //! \return Heap-allocated copy of `s`, owned by this context.
    char* trackedStringCopy(const char* s);                // 0x28e8f0
    //! \brief Frees every string previously returned by
    //! `trackedStringCopy()` and empties the tracking vector.
    //! \warning Invalidates every pointer previously returned by
    //! `trackedStringCopy()`; the caller must ensure no parser-owned
    //! `AsmExpression` still references them when this is invoked.
    void cleanStringCopies();                              // 0x28ea10

    // ---- Label set ----

    //! \brief Records `label` as a defined label in the current
    //! parse, so subsequent forward-reference checks can validate it.
    //! \param label Label identifier to insert.
    void addLabel(const std::string& label);               // 0x28ea60
    //! \brief Checks whether `label` has previously been registered
    //! via `addLabel()`.
    //! \param label Label identifier to look up.
    //! \return `true` if the label is already in the set.
    bool hasLabel(const std::string& label) const;         // 0x28ea80

    // ---- Error reporting ----

    //! \brief Installs the diagnostic sink invoked by `raiseError()`.
    //! \param cb Function object invoked with `(line_number,
    //! message)` for every `raiseError()` call; pass an empty
    //! `std::function` to clear and fall back to the boost::log sink.
    void setErrorCallback(
        const std::function<void(int, const std::string&)>& cb);  // 0x28e610
    //! \brief Reports an error on the current source line.
    //! \details Invokes the registered `errorCallback_` with
    //! `(currentLineNumber(), msg)` if one is installed; otherwise
    //! logs the diagnostic at error severity through boost::log
    //! using the format `"Line N: asm syntax error: <msg>"`.
    //! \param msg Diagnostic text forwarded verbatim to the
    //! callback (or the fallback log line).
    void raiseError(const std::string& msg);               // 0x28e690

private:
    bool isComment_;           //!< True while inside any comment scope.
    bool isLineComment_;       //!< True while inside a `//` line comment.
    bool isBlockComment_;      //!< True while inside a `/* ... */` block comment.
    bool hadSyntaxError_;      //!< Sticky flag latched by `setSyntaxError()`.
    bool doOpt_;               //!< Per-line optimisation flag (cleared by `DISABLE_OPT`).
    // 3 bytes padding         // +0x05
    int32_t lineNumber_;       //!< 1-based current source line.
    int32_t programCounter_;   //!< Instruction-address counter for the current line.
    std::function<void(int, const std::string&)> errorCallback_;   //!< User-supplied diagnostic sink.
    std::vector<char*> stringCopies_;                              //!< `strdup`'d token strings owned by this context.
    std::unordered_set<std::string> labels_;                       //!< Set of label names declared in this parse.
};

// ============================================================================
// Free functions used by the flex/bison asm parser
// ============================================================================

// Allocate an AsmExpression node from a string token.
// Splits on '#', extracts name and label, trims whitespace.
// Returns a heap-allocated AsmExpression* (0xa8 bytes).
//! \brief Parser action that builds an `AsmExpression` carrying the
//! trailing `#`-prefixed comment from a single token.
//! \details Splits `text` on the literal `'#'` separator, expects
//! exactly two parts (`addNode` raises an error through `ctx`
//! otherwise), trims the comment, and stores it in the new
//! expression's `comment` slot with the `hasComment` flag set.
//! The freshly allocated node has `cmd = INVALID` and is the
//! starting point for `addCommand` to fill in command and label
//! data later in the rule.
//! \param ctx Parser context that receives any diagnostics.
//! \param text Token text from the lexer; must not be null.
//! \return Heap-allocated `AsmExpression` owned by the caller (or
//! `nullptr` if `text` was null).
AsmExpression* addNode(AsmParserContext* ctx, const char* text);      // 0x28bfd0

// Process a command expression: looks up command name via
// Assembler::commandFromString(), assigns label data, and populates
// the AsmExpression's Label field.
// Parameters:
//   ctx      — parser context
//   argList  — the output expression node (may be null; allocated if so)
//   cmdToken — command token expression carrying the instruction name
//   pc       — program counter value for this command
//   label    — optional label string (may be null)
// Returns the (possibly newly allocated) argList expression.
//! \brief Parser action that finalises one assembled instruction
//! line, attaching command and label data to its operand container.
//! \details Looks up the command name carried by `cmdToken` (the
//! substring up to the first space) via
//! `Assembler::commandFromString`, raising `"Unknown command:
//! <name>"` through `ctx` on no match.  The resolved opcode and
//! the program counter are stored in `argList`.  When `label` is
//! non-null it is checked for duplicates against the context's
//! label set (raising `"label <name> already defined"` on
//! collision), then registered, and a `Label(pc, label)` is
//! attached to `argList`.  When only `label` is supplied (no
//! `cmdToken`), the resulting expression's command is set to
//! `LABEL` so it represents a pure label line.
//! \param ctx Parser context that receives diagnostics and owns
//! the label set.
//! \param argList Existing operand-list expression to populate; if
//! null, a fresh container expression is allocated.
//! \param cmdToken Command-name expression to consume; may be null
//! when only a label is being defined.  Always destroyed if
//! non-null.
//! \param pc Program-counter value to store in `argList` (and in
//! the attached `Label`, if any).
//! \param label Optional label C string; may be null.
//! \return The (possibly newly allocated) `argList` expression
//! with command, label, and pc fields populated.
//! \warning Both `cmdToken` and `label` must not be null
//! simultaneously; the function raises an error and returns
//! `nullptr` in that case.  The `cmdToken` argument is consumed
//! (destroyed) by this call regardless of whether the lookup
//! succeeded.
AsmExpression* addCommand(AsmParserContext* ctx,
                          AsmExpression* argList,
                          AsmExpression* cmdToken,
                          int pc,
                          const char* label);                         // 0x28c600

}  // namespace zhinst

// ---- Bison/flex interface functions (C linkage in binary) ----

// Called by the bison-generated parser on syntax errors.
// Wraps the error message in a std::string, calls ctx->raiseError(),
// then calls ctx->setSyntaxError(). Returns 1.
//! \brief Bison error reporter for the asm parser.
//! \details Forwards `msg` to `ctx->raiseError()` then latches
//! `ctx->setSyntaxError()` so the assembler driver rejects the
//! whole input after parsing finishes.
//! \param ctx Parser context that records the diagnostic and
//! syntax-error state.
//! \param result Unused output slot for the result tree (kept for
//! the bison signature).
//! \param scanner Opaque flex scanner state (unused).
//! \param msg Diagnostic text supplied by bison.
//! \return Always `1` (the bison error-return convention).
int asmerror(zhinst::AsmParserContext* ctx,
             zhinst::AsmExpression** result,
             void* scanner,
             const char* msg);                                        // 0x292a60

// Bison-generated parser entry point (~217KB, not reconstructed).
//! \brief Bison-generated entry point that parses one assembler
//! input.
//! \param ctx Parser context shared with the lexer; collects
//! diagnostics and label state.
//! \param result Receives the root `AsmExpression` of the parsed
//! line on success.
//! \param scanner Opaque flex scanner instance previously
//! initialised via `asmlex_init_extra()`.
//! \return `0` on success, non-zero on parse failure.
int asmparse(zhinst::AsmParserContext* ctx,
             zhinst::AsmExpression** result,
             void* scanner);                                          // 0x292b50

// Flex scanner helpers (operate on the opaque scanner state, not on ctx):
//! \brief Attaches `ctx` as the `yyextra` user-data of an existing
//! flex scanner instance.
//! \param ctx Parser context to expose to lexer actions.
//! \param scanner Flex scanner previously created with
//! `asmlex_init_extra()`.
void asmset_extra(zhinst::AsmParserContext* ctx, void* scanner);      // 0x292840

//! \brief Initialises a fresh flex scanner with `ctx` as its
//! `yyextra` user-data.
//! \param ctx Parser context to thread through lexer actions.
//! \param scanner Output pointer that receives the new opaque
//! flex scanner handle; the caller must release it with
//! `asmlex_destroy`.
//! \return `0` on success; non-zero on allocation failure.
int  asmlex_init_extra(zhinst::AsmParserContext* ctx, void** scanner);// 0x292960
