// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CompilerMessage, CompilerMessageCollection, CompilerException
// ============================================================================

#include "zhinst/core/compiler_message.hpp"

#include <cstring>
#include <sstream>

namespace zhinst {

// ============================================================================
// CompilerMessage
// ============================================================================

// 0x104340
std::string CompilerMessage::str(bool hideLine) const {
    std::ostringstream oss;

    static const char* typeNames[] = { "Compiler Error", "Warning", "Info" };
    static const size_t typeLens[] = { 14, 7, 4 };

    if (type >= CompilerMessage::Invalid) {
        // LOG_WARNING about unknown type
    }

    oss.write(typeNames[type], typeLens[type]);

    if (!hideLine && lineNr > 0) {
        oss << " (line: " << lineNr << ")";
    }

    oss << ": ";
    oss << message;

    return oss.str();
}

// ============================================================================
// CompilerMessageCollection
// ============================================================================

// 0x12b750
void CompilerMessageCollection::compilerMessage(
    CompilerMessage::CompilerMessageType type, int line,
    const std::string& msg)
{
    // Strip trailing newline if present
    std::string text(msg);
    if (!text.empty() && text.back() == '\n') {
        text.resize(text.size() - 1);
    }

    // Check for duplicate (same line + same message text)
    for (auto it = messages_.begin(); it != messages_.end(); ++it) {
        if (it->lineNr == line &&
            it->message.size() == text.size() &&
            std::memcmp(it->message.data(), text.data(), text.size()) == 0) {
            return;  // duplicate — skip
        }
    }

    messages_.push_back(CompilerMessage{type, line, std::move(text)});
}

// 0x12b720
void CompilerMessageCollection::errorMessage(const std::string& msg, int line) {
    if (line < 0) line = lineNr_;
    compilerMessage(CompilerMessage::Error, line, msg);
    hadError_ = true;
}

// 0x12ba10
void CompilerMessageCollection::warningMessage(const std::string& msg, int line) {
    if (line < 0) line = lineNr_;
    compilerMessage(CompilerMessage::Warning, line, msg);
}

// 0x12b9f0
void CompilerMessageCollection::infoMessage(const std::string& msg, int line) {
    if (line < 0) line = lineNr_;
    compilerMessage(CompilerMessage::Info, line, msg);
}

// 0x12ba30
void CompilerMessageCollection::parserMessage(int line, const std::string& msg) {
    compilerMessage(CompilerMessage::Error, line, msg);
    // Note: does NOT set hadError_
}

const std::vector<CompilerMessage>& CompilerMessageCollection::messages() const {
    return messages_;
}

// 0x12ba90
void CompilerMessageCollection::reset() {
    hadError_ = false;
    messages_.clear();
}

// ============================================================================
// CompilerException
// ============================================================================

CompilerException::CompilerException(const std::string& msg)  // 0x11dec0
    : message_(msg) {}

CompilerException::~CompilerException() {}  // 0x11df20

const char* CompilerException::what() const noexcept {  // 0x123bd0
    if (message_.empty())
        return "";
    return message_.c_str();
}

}  // namespace zhinst
