// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CompilerMessage, CompilerMessageCollection, CompilerException
//
// CompilerMessage: 0x20 bytes — type enum + lineNr + message string
// CompilerMessageCollection: 0x20 bytes — vector<CompilerMessage> + lineNr + hadError
// CompilerException: 0x20 bytes — inherits std::exception, vptr + string
// ============================================================================
#pragma once

#include <exception>
#include <string>
#include <vector>

namespace zhinst {

// ---- CompilerMessage (0x20 bytes) ----

struct CompilerMessage {
    enum CompilerMessageType : int {
        Error   = 0,  // "Compiler Error"
        Warning = 1,  // "Warning"
        Info    = 2,  // "Info"
        Invalid = 3,  // sentinel — out-of-range guard used in bounds checks
    };

    CompilerMessageType type;   // +0x00
    int lineNr;                 // +0x04
    std::string message;        // +0x08 (24 bytes SSO)
    // +0x20 END

    std::string str(bool hideLine) const;   // 0x104340
};

// ---- CompilerMessageCollection (0x20 bytes, embedded in Compiler at +0x38) ----

class CompilerMessageCollection {
public:
    void compilerMessage(CompilerMessage::CompilerMessageType type,
                         int line, const std::string& msg);     // 0x12b750
    void errorMessage(const std::string& msg, int line = -1);   // 0x12b720
    void warningMessage(const std::string& msg, int line = -1); // 0x12ba10
    void infoMessage(const std::string& msg, int line = -1);    // 0x12b9f0
    void parserMessage(int line, const std::string& msg);       // 0x12ba30

    const std::vector<CompilerMessage>& messages() const;
    bool hadCompilerError() const { return hadError_; }
    void setLineNr(int nr) { lineNr_ = nr; }
    int lineNr() const { return lineNr_; }
    void reset();                                                // 0x12ba90

private:
    std::vector<CompilerMessage> messages_;  // +0x00 (24 bytes)
    int lineNr_;                             // +0x18
    bool hadError_;                          // +0x1c
    // +0x1d: 3 bytes padding
    // +0x20 END
};

// ---- CompilerException (0x20 bytes) ----

class CompilerException : public std::exception {
public:
    CompilerException(const std::string& msg);   // 0x11dec0
    ~CompilerException() override;               // 0x11df20
    const char* what() const noexcept override;  // 0x123bd0

private:
    std::string message_;  // +0x08 (24 bytes)
    // +0x20 END
};

}  // namespace zhinst
