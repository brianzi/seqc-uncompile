// ============================================================================
// SeqcParserContext — parser context for SeqC flex/bison parser
//
// Full layout recovered  from binary ctor/dtor/method disassembly:
//   +0x00  uint8_t  isComment_           (composite: nonzero if in any comment)
//   +0x01  uint8_t  lineComment_         (in line comment)
//   +0x02  uint8_t  blockComment_        (in block comment)
//   +0x03  uint8_t  hadSyntaxError_      (set by setSyntaxError)
//   +0x04  int32_t  currentLineNumber_   (1-based source line)
//   +0x08  [8 bytes padding]
//   +0x10  std::function<void(int, const std::string&)> errorCallback_  (0x28 bytes, libc++)
//   Total size: 0x38 bytes
//
// reset() at 0x247cc0: clears flags to 0, sets lineNumber to 1, does NOT
// reset the error callback.
//
// setErrorCallback() at 0x247a60: swaps a new callback into errorCallback_.
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace zhinst {

//! \brief Per-parse state shared between the SeqC flex lexer and bison parser.
//!
//! `SeqcParserContext` is the user-data argument threaded through the
//! reentrant `seqc_lex` scanner and `seqc_parse` parser.  It tracks the
//! current source line number (incremented by the lexer on each newline),
//! the comment-nesting flags (line-comment and block-comment forms set as
//! the lexer enters and leaves each form), a sticky syntax-error flag
//! latched by `setSyntaxError()` for whole-file rejection, and a
//! user-supplied error callback that bison invokes via `seqc_error()` to
//! report diagnostics.
//!
//! `reset()` clears the per-parse state but preserves the error callback
//! so a single context instance can be reused across multiple compilations.
class SeqcParserContext {
public:
    // --- Accessors ---
    //! \brief Returns the current 1-based source line number.
    //! \return The current line counter as last advanced by
    //! incrementLineNumber().
    int32_t currentLineNumber() const;        // 0x247c80
    //! \brief Increments the current source line number; called by
    //! the lexer on every newline.
    void incrementLineNumber();               // 0x247c90
    //! \brief Reports an error on the current source line: invokes
    //! the registered `errorCallback_` with `(currentLineNumber_,
    //! msg)`, or falls back to logging `"[Line N]: msg"` to
    //! `std::clog` if no callback is set.
    //! \param msg Diagnostic text forwarded verbatim to the callback
    //! (or the fallback log line).
    void raiseError(const std::string& msg);  // 0x247ae0
    //! \brief Latches the sticky `hadSyntaxError_` flag; consulted
    //! by the compiler driver after parsing to decide whether to
    //! reject the whole input.
    void setSyntaxError();                    // 0x247cb0
    //! \brief Returns the latched syntax-error flag.
    //! \return `true` if setSyntaxError() has been called since the
    //! last reset(); `false` otherwise.
    bool hadSyntaxError() const;              // 0x247ca0

    //! \brief Returns true while the lexer is inside any comment
    //! (line- or block-form).
    //! \return `true` while either a line- or block-comment state is
    //! currently latched.
    bool isComment() const;                   // 0x247bf0
    //! \brief Returns true while the lexer is specifically inside a
    //! line comment.
    //! \return `true` only while a line-comment scope is active.
    bool isLineComment() const;
    //! \brief Begins block-comment state.
    //! \binarynote No-op when already inside a line comment, so an
    //! embedded `/` followed by `*` inside `//` line text does not
    //! start a block scope.  See IF-169 for the symmetric end-marker
    //! no-op.
    void startBlockComment();                 // 0x247c40
    //! \brief Ends block-comment state.
    //! \binarynote No-op when inside a line comment, so a stray
    //! block-comment end marker inside a `//` line is harmless
    //! rather than terminating comment state mid-line (IF-169).
    void endBlockComment();                   // 0x247c60
    //! \brief Begins line-comment state.
    //! \binarynote No-op when already inside a block comment.
    void startLineComment();                  // 0x247c00
    //! \brief Ends line-comment state at the next newline.
    //! \binarynote No-op when inside a block comment.
    void endLineComment();                    // 0x247c20

    // --- Lifecycle ---
    //! \brief Clears all comment flags, the syntax-error flag, and
    //! resets `currentLineNumber_` to 1.  Does **not** clear
    //! `errorCallback_`, allowing the same context instance to be
    //! reused across multiple compilations.
    void reset();                             // 0x247cc0
    //! \brief Installs the diagnostic sink invoked by `raiseError()`.
    //! The callback receives `(line_number, message)` for each
    //! diagnostic.
    //! \param cb Function object invoked with `(line_number,
    //! message)` for every raiseError() call; pass an empty
    //! `std::function` to clear and fall back to `std::clog`.
    void setErrorCallback(                    // 0x247a60
        std::function<void(int, const std::string&)> cb);

private:
    // +0x00
    uint8_t isComment_{0};
    uint8_t lineComment_{0};
    uint8_t blockComment_{0};
    uint8_t hadSyntaxError_{0};
    // +0x04
    int32_t currentLineNumber_{1};
    // +0x08 — padding to align std::function at +0x10
    char pad_[8]{};
    // +0x10
    std::function<void(int, const std::string&)> errorCallback_;
};

} // namespace zhinst
