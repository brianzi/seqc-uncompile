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
    int32_t currentLineNumber() const;        // 0x247c80
    void incrementLineNumber();               // 0x247c90
    void raiseError(const std::string& msg);  // 0x247ae0
    void setSyntaxError();                    // 0x247cb0
    bool hadSyntaxError() const;              // 0x247ca0

    bool isComment() const;                   // 0x247bf0
    bool isLineComment() const;
    void startBlockComment();                 // 0x247c40
    void endBlockComment();                   // 0x247c60
    void startLineComment();                  // 0x247c00
    void endLineComment();                    // 0x247c20

    // --- Lifecycle ---
    void reset();                             // 0x247cc0
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
