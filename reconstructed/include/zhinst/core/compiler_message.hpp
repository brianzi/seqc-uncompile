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

//! \brief One diagnostic emitted during a compile run — error, warning, or
//!        info — together with its source line.
//!
//! Holds a `type` discriminant (`Error` / `Warning` / `Info` / `Invalid`),
//! the SeqC source line it refers to (or `-1` when not source-attached),
//! and the message text.  `str(hideLine)` renders the diagnostic for
//! display, prefixing "Compiler Error" / "Warning" / "Info" per `type`
//! and optionally suppressing the line number suffix.
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

//! \brief Diagnostic sink used by every front-end and back-end pass to
//!        emit errors, warnings, and informational messages.
//!
//! Keeps a vector of `CompilerMessage`s alongside a "current line"
//! cursor that the typed `errorMessage` / `warningMessage` /
//! `infoMessage` helpers fall back to when the caller does not supply
//! one explicitly.  A sticky `hadCompilerError()` flag is set whenever
//! an `Error`-typed message is recorded, so the driver can short-circuit
//! later phases.  `parserMessage()` is the parser-specific entry that
//! takes the line first; `reset()` empties the vector and clears the
//! flag between compile runs.
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

//! \brief Lightweight `std::exception` subclass thrown for fatal compiler
//!        errors that abort the front-end early.
//!
//! Independent of the boost-backed `zhinst::Exception` hierarchy: holds
//! only the message string and exposes it through `what()`.  Used at
//! points where the compiler cannot continue but where the richer
//! `ZIAPIException` machinery is unwanted.
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
