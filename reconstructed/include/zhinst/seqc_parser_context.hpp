// ============================================================================
// SeqcParserContext — minimal stub for Expression parser actions
//
// Full reconstruction deferred (Phase 11+ or later). Only the methods called
// by expression.cpp / compiler.cpp / awg_assembler are declared here.
//
// Partial layout recovered from method disasm (see seqc_parser_context.cpp):
//   +0x00  byte    isComment            (0x247bf0 reads it)
//   +0x01  byte    blockComment flag    (0x247c40/0x247c60)
//   +0x02  byte    lineComment flag     (0x247c00/0x247c20)
//   +0x03  byte    hadSyntaxError flag  (0x247ca0 reads, 0x247cb0 sets)
//   +0x04  int32   currentLineNumber    (0x247c80 reads, 0x247c90 inc)
//   +0x30  ptr     errorCallback opaque (read by 0x247ae0 raiseError)
//
// We only DECLARE methods that other reconstructed TUs call. The class is
// never instantiated inside reconstructed code (used by-pointer only),
// so we deliberately omit data members from the header to avoid
// committing to an incorrect size.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>

namespace zhinst {

class SeqcParserContext {
public:
    int32_t currentLineNumber() const;        // 0x247c80
    void incrementLineNumber();               // 0x247c90
    void raiseError(const std::string& msg);  // 0x247ae0
    void setSyntaxError();                    // 0x247cb0
    bool hadSyntaxError() const;              // 0x247ca0

    bool isComment() const;                   // 0x247bf0
    void startBlockComment();                 // 0x247c40
    void endBlockComment();                   // 0x247c60
    void startLineComment();                  // 0x247c00
    void endLineComment();                    // 0x247c20
};

} // namespace zhinst
