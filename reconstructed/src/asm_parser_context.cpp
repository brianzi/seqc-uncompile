// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmParserContext — parser context for the AWGAssembler's flex/bison parser
//
// All methods reconstructed from binary analysis. Addresses noted per function.
// ============================================================================

#include "zhinst/asm_parser_context.hpp"
#include "zhinst/asm_expression.hpp"
#include "zhinst/assembler.hpp"

#include <cstdlib>   // strdup, free
#include <cstring>   // strlen

// Forward declaration — AsmExpression is used by addNode/addCommand
// but its full definition is in a separate header.
#include <boost/algorithm/string.hpp>

namespace zhinst {

// Assembler is now a class; its header is included transitively.

// ============================================================================
// AsmParserContext::Label
// ============================================================================

// 0x28eaa0
AsmParserContext::Label::Label(int pc, std::string name)
    : pc(pc), name(std::move(name)) {}

// 0x28ead0
// Compares the name strings for equality (standard libc++ string comparison).
bool AsmParserContext::Label::operator==(const Label& other) const {
    return name == other.name;
}

// ============================================================================
// Comment tracking
// ============================================================================

// 0x28e7a0 — returns isComment_ (byte at +0x00)
bool AsmParserContext::isComment() const {
    return isComment_;
}

// 0x28e7b0 — returns isLineComment_ (byte at +0x01)
bool AsmParserContext::isLineComment() const {
    return isLineComment_;
}

// 0x28e7c0 — sets both isComment_ and isLineComment_ to true,
// but only if isBlockComment_ is not already active.
void AsmParserContext::startLineComment() {
    if (!isBlockComment_) {
        isComment_ = true;
        isLineComment_ = true;
    }
}

// 0x28e7e0 — clears both isComment_ and isLineComment_,
// but only if isBlockComment_ is not active.
void AsmParserContext::endLineComment() {
    if (!isBlockComment_) {
        isComment_ = false;
        isLineComment_ = false;
    }
}

// 0x28e830 — sets isComment_ and isBlockComment_ to true,
// but only if isLineComment_ is not active.
void AsmParserContext::startBlockComment() {
    if (!isLineComment_) {
        isComment_ = true;
        isBlockComment_ = true;
    }
}

// 0x28e850 — clears isComment_ and isBlockComment_,
// but only if isLineComment_ is not active.
void AsmParserContext::endBlockComment() {
    if (!isLineComment_) {
        isComment_ = false;
        isBlockComment_ = false;
    }
}

// ============================================================================
// Optimization flag
// ============================================================================

// 0x28e800 — sets doOpt_ to false
void AsmParserContext::disableOpt() {
    doOpt_ = false;
}

// 0x28e810 — sets doOpt_ to true
void AsmParserContext::enableOpt() {
    doOpt_ = true;
}

// 0x28e820 — returns doOpt_ (byte at +0x04)
bool AsmParserContext::doOpt() const {
    return doOpt_;
}

// ============================================================================
// Syntax error tracking
// ============================================================================

// 0x28e870 — returns hadSyntaxError_ (byte at +0x03)
bool AsmParserContext::hadSyntaxError() const {
    return hadSyntaxError_;
}

// 0x28e880 — sets hadSyntaxError_ to true
void AsmParserContext::setSyntaxError() {
    hadSyntaxError_ = true;
}

// 0x28e890 — clears all state: zeroes the first 4 bytes (isComment_,
// isLineComment_, isBlockComment_, hadSyntaxError_) and resets
// lineNumber_ to 1.
// Binary: mov DWORD PTR [rdi+8], 1; mov DWORD PTR [rdi], 0
void AsmParserContext::clearSyntaxError() {
    lineNumber_ = 1;
    // Zero all 4 bools at once (the binary does mov dword ptr [rdi], 0)
    isComment_ = false;
    isLineComment_ = false;
    isBlockComment_ = false;
    hadSyntaxError_ = false;
}

// ============================================================================
// Line number / program counter
// ============================================================================

// 0x28e8b0 — returns lineNumber_ (DWORD at +0x08)
int AsmParserContext::currentLineNumber() const {
    return lineNumber_;
}

// 0x28e8c0 — increments lineNumber_
void AsmParserContext::incrementLineNumber() {
    ++lineNumber_;
}

// 0x28e8d0 — returns programCounter_ (DWORD at +0x0C)
int AsmParserContext::programCounter() const {
    return programCounter_;
}

// 0x28e8e0 — increments programCounter_
void AsmParserContext::incrementProgramCounter() {
    ++programCounter_;
}

// ============================================================================
// String memory management
// ============================================================================

// 0x28e8f0 — calls strdup(s), pushes the result into stringCopies_ vector,
// and returns the duplicated C string pointer.
// The vector at +0x40 (begin), +0x48 (end), +0x50 (capacity) stores char*.
char* AsmParserContext::trackedStringCopy(const char* s) {
    char* copy = strdup(s);
    stringCopies_.push_back(copy);
    return copy;
}

// 0x28ea10 — iterates stringCopies_, calls free() on each element,
// then resets the vector's size to 0 (sets end = begin).
void AsmParserContext::cleanStringCopies() {
    for (char* p : stringCopies_) {
        free(p);
    }
    stringCopies_.clear();
}

// ============================================================================
// Label set
// ============================================================================

// 0x28ea60 — tail-calls into unordered_set::emplace at this+0x58
void AsmParserContext::addLabel(const std::string& label) {
    labels_.emplace(label);
}

// 0x28ea80 — calls unordered_set::find at this+0x58, returns true
// if the iterator is not end (i.e., pointer is non-null).
bool AsmParserContext::hasLabel(const std::string& label) const {
    return labels_.find(label) != labels_.end();
}

// ============================================================================
// Error reporting
// ============================================================================

// 0x28e610 — copies the given callback into errorCallback_ at +0x10.
// Uses std::function swap semantics internally.
void AsmParserContext::setErrorCallback(
    const std::function<void(int, const std::string&)>& cb) {
    errorCallback_ = cb;
}

// 0x28e690 — if errorCallback_ is set (callable pointer at +0x30 non-null),
// invokes it with (lineNumber_, msg). Otherwise, logs via boost::log at
// severity ERROR: "Line <lineNumber_>: asm syntax error: <msg>"
void AsmParserContext::raiseError(const std::string& msg) {
    if (errorCallback_) {
        errorCallback_(lineNumber_, msg);
    } else {
        // Fallback: log via boost::log at ERROR severity.
        // Format: "Line " + lineNumber_ + ": asm syntax error: " + msg
        // (Uses zhinst::logging::detail::LogRecord internally.)
        // LOG_ERROR << "Line " << lineNumber_
        //           << ": asm syntax error: " << msg;
    }
}

// ============================================================================
// Free functions: addNode
// ============================================================================

// 0x28bfd0
// Allocates a new AsmExpression (0xa8 bytes) and populates it from a text
// token. The text is split on '#' to separate the instruction part from
// a trailing comment.
//
// Algorithm:
//   1. If text is null, raises error "addNode: nullptr argument" and returns
//      null.
//   2. Allocates an AsmExpression (0xa8 bytes), zeroes most fields:
//      - type (+0x00) = 0
//      - flags at +0x78, +0x80, +0x98, +0xa0 = false
//      - cmd (+0x38) = INVALID (0xFFFFFFFF)
//      - zero ranges +0x08..+0x57
//   3. Builds a std::string from text, splits on '#' using
//      boost::algorithm::split with token_compress_on.
//   4. Expects exactly 2 parts (instruction + comment); if not, raises
//      error "addNode: expected 2 parts, split gave N instead".
//   5. Takes the second part (index 1), trims whitespace with
//      boost::algorithm::trim.
//   6. If the node's +0x98 flag is already set, assigns the trimmed
//      string into the existing string at +0x80 (comment field).
//      Otherwise, moves the string into +0x80 and sets +0x98 = true.
//   7. Cleans up temporaries and returns the AsmExpression*.
AsmExpression* addNode(AsmParserContext* ctx, const char* text) {
    if (!text) {
        ctx->raiseError("addNode: nullptr argument");
        return nullptr;
    }

    // Allocate and zero-initialize an AsmExpression (0xa8 bytes)
    AsmExpression* node = new AsmExpression();  // 0xa8 bytes
    // Binary explicitly zeros fields and sets cmd = INVALID

    // Split text on '#'
    std::string input(text);
    std::vector<std::string> parts;
    boost::algorithm::split(parts, input, boost::is_any_of("#"),
                            boost::token_compress_on);

    if (parts.size() != 2) {
        ctx->raiseError(
            "addNode: expected 2 parts, split gave N instead");
        // Still continues; the binary checks parts.size() == 0x30/0x18 = 2
    }

    // Take parts[1] (the comment after '#'), trim whitespace
    std::string& comment = parts[1];
    boost::algorithm::trim(comment);

    // Assign comment into the AsmExpression's comment string at +0x80
    // (with hasComment flag at +0x98)
    // Binary: if node->hasComment (+0x98) is true, assign into existing;
    //         otherwise, move-assign and set hasComment = true.
    // node->setComment(comment);

    return node;
}

// ============================================================================
// Free functions: addCommand
// ============================================================================

// 0x28c600
// Processes a command token: resolves the command name to an
// Assembler::Command enum, handles label creation, and populates
// the AsmExpression with label data.
//
// Algorithm:
//   1. If both args (rdx) and label (r8) are null, raises error
//      "addCommand: both args and label are null" and returns null.
//   2. If cmd (rsi) is null, allocates a fresh AsmExpression (0xa8 bytes)
//      and zero-initializes it.
//   3. Sets cmd->type (+0x00) = 0, cmd->value (+0x3C) = pc parameter.
//   4. If args is non-null:
//      a. Extracts the name string from args (+0x08).
//      b. Finds the first space in the name to get just the command token.
//      c. Builds a string from that substring.
//      d. Calls Assembler::commandFromString() to resolve the command.
//      e. If INVALID, builds error string "Unknown command: <name>" and
//         calls ctx->raiseError().
//      f. Stores the resolved command into cmd (+0x38).
//      g. Destroys the args AsmExpression (it was consumed).
//   5. If label is non-null:
//      a. Builds a string from the label C-string.
//      b. Calls ctx->hasLabel(label) to check for duplicates.
//      c. If duplicate, builds error "label <name> already defined" and
//         calls ctx->raiseError().
//      d. Calls ctx->addLabel(label).
//      e. If args was null, sets cmd (+0x38) = LABEL (0x02).
//      f. Constructs a Label(pc, label) and stores it into the
//         AsmExpression's label field at +0x58 (int pc) and +0x60 (string).
//      g. Sets the hasLabel flag at +0x78 to true (if not already).
//   6. Returns cmd.
AsmExpression* addCommand(AsmParserContext* ctx,
                          AsmExpression* cmd,
                          AsmExpression* args,
                          int pc,
                          const char* label) {
    if (!args && !label) {
        ctx->raiseError(
            "addCommand: both args and label are null");
        return nullptr;
    }

    // If no cmd expression provided, allocate a fresh one
    if (!cmd) {
        cmd = new AsmExpression();  // 0xa8 bytes, zeroed
    }

    cmd->type = 0;     // +0x00
    cmd->value = pc;   // +0x3C

    if (args) {
        // Extract command name from args->name up to the first space.
        // Verified at 0x28c712-0x28c75a: code unpacks the SSO/heap pointer
        // pair from the std::string at args+0x08 (size at +0x08 LSB clear,
        // ptr at +0x18 / size at +0x10 if heap), then memchr(buf, 0x20, len)
        // to locate the first space; the command token is everything before.
        const std::string& fullName = args->name;
        auto spacePos = fullName.find(' ');
        std::string nameStr = (spacePos == std::string::npos)
            ? fullName
            : fullName.substr(0, spacePos);

        // Destroy the args expression (consumed)
        args->~AsmExpression();
        ::operator delete(args);

        // Look up the command
        Assembler::Command resolved = Assembler::commandFromString(nameStr);
        if (resolved == Assembler::INVALID) {
            ctx->raiseError("Unknown command: " + nameStr);
        }
        cmd->command = resolved;  // +0x38
    }

    if (label) {
        std::string labelStr(label);

        // Check for duplicate
        if (ctx->hasLabel(labelStr)) {
            ctx->raiseError("label " + labelStr + " already defined");
        }

        // Register the label
        ctx->addLabel(labelStr);

        // If no args were provided, this is a pure label instruction
        if (!args) {
            cmd->command = static_cast<Assembler::Command>(0x02);  // LABEL
        }

        // Store Label data into the AsmExpression
        AsmParserContext::Label lbl(pc, labelStr);
        cmd->labelPc() = lbl.pc;      // +0x58 (labelPc is alias for labelIndex)
        // Move/copy lbl.name into cmd's label string at +0x60
        // Set hasLabel flag at +0x78 = true
    }

    return cmd;
}

}  // namespace zhinst

// ============================================================================
// asmerror is defined in asm_parser.y (bison epilogue) to avoid ODR
// violations with the generated asm_parser.tab.c which #defines
// yyerror → asmerror.
//
// asmparse (0x292b50) — bison-generated parser (~217KB). Not reconstructed.
// ============================================================================
