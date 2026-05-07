// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// SeqcParserContext: small accessors and error reporting helpers
//
// Full layout now known — all methods use typed member access.
// Previous raw-offset at_offset<> pattern replaced with direct members.
// ============================================================================

#include "zhinst/ast/seqc_parser_context.hpp"

#include <iostream>

namespace zhinst {

// ----------------------------------------------------------------------------
// 0x247c80 — currentLineNumber() const
// ----------------------------------------------------------------------------
int32_t SeqcParserContext::currentLineNumber() const  // 0x247c80
{
    return currentLineNumber_;
}

// ----------------------------------------------------------------------------
// 0x247c90 — incrementLineNumber()
// ----------------------------------------------------------------------------
void SeqcParserContext::incrementLineNumber()  // 0x247c90
{
    ++currentLineNumber_;
}

// ----------------------------------------------------------------------------
// 0x247bf0 — isComment() const
// ----------------------------------------------------------------------------
bool SeqcParserContext::isComment() const  // 0x247bf0
{
    return isComment_ != 0;
}

bool SeqcParserContext::isLineComment() const {
    return lineComment_ != 0;
}

// ----------------------------------------------------------------------------
// 0x247c40 — startBlockComment()
//   Guard: if (lineComment_ != 0) return;
//   Then: isComment_ = 1; blockComment_ = 1;
// ----------------------------------------------------------------------------
void SeqcParserContext::startBlockComment()  // 0x247c40
{
    if (lineComment_ != 0) return;
    isComment_ = 1;
    blockComment_ = 1;
}

// ----------------------------------------------------------------------------
// 0x247c60 — endBlockComment()
//   Guard: if (lineComment_ != 0) return;   (binary @0x247c64 cmpb [rdi+1],0)
//   Then: isComment_ = 0; blockComment_ = 0;
//   This guard is what allows `*/` inside `// ...` line comment to be a
//   harmless no-op rather than terminating comment state mid-line. (IF-169)
// ----------------------------------------------------------------------------
void SeqcParserContext::endBlockComment()  // 0x247c60
{
    if (lineComment_ != 0) return;
    isComment_ = 0;
    blockComment_ = 0;
}

// ----------------------------------------------------------------------------
// 0x247c00 — startLineComment()
//   Guard: if (blockComment_ != 0) return;
//   Then: movw $0x101, [rdi]  →  isComment_ = 1; lineComment_ = 1;
// ----------------------------------------------------------------------------
void SeqcParserContext::startLineComment()  // 0x247c00
{
    if (blockComment_ != 0) return;
    isComment_ = 1;
    lineComment_ = 1;
}

// ----------------------------------------------------------------------------
// 0x247c20 — endLineComment()
//   Guard: if (blockComment_ != 0) return;
//   Then: movw $0, [rdi]  →  isComment_ = 0; lineComment_ = 0;
// ----------------------------------------------------------------------------
void SeqcParserContext::endLineComment()  // 0x247c20
{
    if (blockComment_ != 0) return;
    isComment_ = 0;
    lineComment_ = 0;
}

// ----------------------------------------------------------------------------
// 0x247ca0 — hadSyntaxError() const
// ----------------------------------------------------------------------------
bool SeqcParserContext::hadSyntaxError() const  // 0x247ca0
{
    return hadSyntaxError_ != 0;
}

// ----------------------------------------------------------------------------
// 0x247cb0 — setSyntaxError()
// ----------------------------------------------------------------------------
void SeqcParserContext::setSyntaxError()  // 0x247cb0
{
    hadSyntaxError_ = 1;
}

// ----------------------------------------------------------------------------
// 0x247cc0 — reset()
// Clears flags (bytes 0-3) to 0, sets currentLineNumber (+0x04) to 1.
// Does NOT reset errorCallback_.
// ----------------------------------------------------------------------------
void SeqcParserContext::reset()  // 0x247cc0
{
    isComment_ = 0;
    blockComment_ = 0;
    lineComment_ = 0;
    hadSyntaxError_ = 0;
    currentLineNumber_ = 1;
}

// ----------------------------------------------------------------------------
// 0x247a60 — setErrorCallback(std::function<void(int, const string&)>)
//   add $0x10, %rbx ; [swap __value_func]
// Swaps the new callback into errorCallback_.
// ----------------------------------------------------------------------------
void SeqcParserContext::setErrorCallback(  // 0x247a60
    std::function<void(int, const std::string&)> cb)
{
    errorCallback_ = std::move(cb);
}

// ----------------------------------------------------------------------------
// 0x247ae0 — raiseError(string const& msg)
//
// Checks if errorCallback_ is set. If so, invokes it with
// (currentLineNumber, msg). Otherwise falls back to logging
// "[Line N]: msg" to std::clog (mirrors boost.log Error branch
// at 0x247b1d-0x247bb7).
// ----------------------------------------------------------------------------
void SeqcParserContext::raiseError(const std::string& msg)  // 0x247ae0
{
    if (errorCallback_) {
        errorCallback_(currentLineNumber_, msg);
        return;
    }

    // Fallback: emit to std::clog when no callback is registered.
    // Mirrors the boost.log Error LogRecord branch at 0x247b1d-0x247bb7
    // which writes "[Line <currentLine>]: <msg>".
    std::clog << "[Line " << currentLineNumber_ << "]: " << msg << '\n';
}

} // namespace zhinst
