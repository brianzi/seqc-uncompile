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
    struct Label {
        int32_t pc;               // +0x00
        // 4 bytes padding
        std::string name;         // +0x08

        Label(int pc, std::string name);                   // 0x28eaa0
        bool operator==(const Label& other) const;         // 0x28ead0
    };

    // ---- Comment tracking ----

    bool isComment() const;                                // 0x28e7a0
    bool isLineComment() const;                            // 0x28e7b0
    void startLineComment();                               // 0x28e7c0
    void endLineComment();                                 // 0x28e7e0
    void startBlockComment();                              // 0x28e830
    void endBlockComment();                                // 0x28e850

    // ---- Optimization flag ----

    void disableOpt();                                     // 0x28e800
    void enableOpt();                                      // 0x28e810
    bool doOpt() const;                                    // 0x28e820

    // ---- Syntax error tracking ----

    bool hadSyntaxError() const;                           // 0x28e870
    void setSyntaxError();                                 // 0x28e880
    void clearSyntaxError();                               // 0x28e890

    // ---- Line / program counter ----

    int currentLineNumber() const;                         // 0x28e8b0
    void incrementLineNumber();                            // 0x28e8c0
    int programCounter() const;                            // 0x28e8d0
    void incrementProgramCounter();                        // 0x28e8e0

    // ---- String memory management ----

    // Calls strdup(s) and tracks the allocation for later cleanup.
    // Returns the duplicated string pointer.
    char* trackedStringCopy(const char* s);                // 0x28e8f0
    void cleanStringCopies();                              // 0x28ea10

    // ---- Label set ----

    void addLabel(const std::string& label);               // 0x28ea60
    bool hasLabel(const std::string& label) const;         // 0x28ea80

    // ---- Error reporting ----

    void setErrorCallback(
        const std::function<void(int, const std::string&)>& cb);  // 0x28e610
    void raiseError(const std::string& msg);               // 0x28e690

private:
    bool isComment_;           // +0x00
    bool isLineComment_;       // +0x01
    bool isBlockComment_;      // +0x02
    bool hadSyntaxError_;      // +0x03
    bool doOpt_;               // +0x04
    // 3 bytes padding         // +0x05
    int32_t lineNumber_;       // +0x08
    int32_t programCounter_;   // +0x0C
    std::function<void(int, const std::string&)> errorCallback_;  // +0x10
    std::vector<char*> stringCopies_;                              // +0x40
    std::unordered_set<std::string> labels_;                       // +0x58
};

// ============================================================================
// Free functions used by the flex/bison asm parser
// ============================================================================

// Allocate an AsmExpression node from a string token.
// Splits on '#', extracts name and label, trims whitespace.
// Returns a heap-allocated AsmExpression* (0xa8 bytes).
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
int asmerror(zhinst::AsmParserContext* ctx,
             zhinst::AsmExpression** result,
             void* scanner,
             const char* msg);                                        // 0x292a60

// Bison-generated parser entry point (~217KB, not reconstructed).
int asmparse(zhinst::AsmParserContext* ctx,
             zhinst::AsmExpression** result,
             void* scanner);                                          // 0x292b50

// Flex scanner helpers (operate on the opaque scanner state, not on ctx):
void asmset_extra(zhinst::AsmParserContext* ctx, void* scanner);      // 0x292840
int  asmlex_init_extra(zhinst::AsmParserContext* ctx, void** scanner);// 0x292960
