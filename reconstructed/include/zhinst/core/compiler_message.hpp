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

    //! \brief Render the message for display, prefixed with the
    //!        type name and (optionally) the source line.
    //!
    //! \details Selects one of the fixed prefixes
    //! `"Compiler Error"`, `"Warning"`, or `"Info"` based on
    //! `type` (the prefix table is keyed directly by the enum
    //! value; an out-of-range / `Invalid` value would index past
    //! the end of the table — callers must therefore guarantee
    //! `type < Invalid`).  When `hideLine` is `false` and
    //! `lineNr > 0`, the suffix `" (line: N)"` is appended.  The
    //! body always ends with `": " + message`.
    //!
    //! \param hideLine  When `true`, omit the `" (line: N)"`
    //!                  suffix even if a line number is set.
    //! \return  Newly allocated formatted diagnostic string.
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
    //! \brief Append a diagnostic of the given `type` and line,
    //!        deduplicating against earlier identical entries.
    //!
    //! \details The implementation strips a single trailing
    //! `'\n'` from `msg` (so callers may pass formatted strings
    //! that include a terminating newline without producing
    //! double-blank-line output downstream) and then linearly
    //! scans `messages_` for an entry with the same `lineNr` and
    //! byte-identical message body.  When a match is found the
    //! call is silently dropped — this is the only call site that
    //! enforces the de-dup invariant; the typed wrappers
    //! (`errorMessage`, `warningMessage`, `infoMessage`,
    //! `parserMessage`) all funnel through here.  No side effect
    //! on `hadError_` even when `type == Error`; that flag is
    //! managed by the typed wrappers individually.
    //!
    //! \param type  Diagnostic class (`Error` / `Warning` /
    //!              `Info`); `Invalid` is rejected by the
    //!              `CompilerMessage::str()` formatter.
    //! \param line  SeqC source line, or `-1` when not source-
    //!              attached.  No fallback to the cursor here;
    //!              the typed wrappers do that.
    //! \param msg   Message body.  Trailing `'\n'` is trimmed
    //!              by-copy.
    void compilerMessage(CompilerMessage::CompilerMessageType type,
                         int line, const std::string& msg);     // 0x12b750
    //! \brief Record an error-class diagnostic and set the sticky
    //!        `hadError_` flag.
    //!
    //! \details Falls back to the `lineNr_` cursor when `line`
    //! is negative (the default), then forwards to
    //! `compilerMessage(Error, line, msg)` and unconditionally
    //! sets `hadError_ = true` on return — even when the
    //! underlying message was deduplicated.  The sticky flag is
    //! how the rest of the pipeline (e.g. `Compiler::compile`)
    //! decides to short-circuit later stages.
    //!
    //! \param msg   Pre-formatted error text (typically produced
    //!              by `ErrorMessages::format`).
    //! \param line  Override line.  Defaults to `-1`, which
    //!              substitutes the cursor `lineNr_`.
    void errorMessage(const std::string& msg, int line = -1);   // 0x12b720
    //! \brief Record a warning-class diagnostic.
    //!
    //! \details Same `lineNr_`-fallback and de-dup behaviour as
    //! `errorMessage`, but does **not** touch `hadError_` — a
    //! warning is informational and does not abort the compile.
    //!
    //! \param msg   Warning text.
    //! \param line  Override line.  Defaults to `-1`, which
    //!              substitutes the cursor `lineNr_`.
    void warningMessage(const std::string& msg, int line = -1); // 0x12ba10
    //! \brief Record an informational diagnostic.
    //!
    //! \details Same `lineNr_`-fallback and de-dup behaviour as
    //! `errorMessage`; does not affect `hadError_`.  Used for
    //! status messages that should appear in the compile report
    //! without altering its error / success state.
    //!
    //! \param msg   Informational text.
    //! \param line  Override line.  Defaults to `-1`, which
    //!              substitutes the cursor `lineNr_`.
    void infoMessage(const std::string& msg, int line = -1);    // 0x12b9f0
    //! \brief Record a parser-emitted error diagnostic with an
    //!        explicit line.
    //!
    //! \details Used by the flex / bison front-end, whose
    //! callbacks always know the offending line precisely.
    //! Forwards to `compilerMessage(Error, line, msg)` (so the
    //! de-dup pass still fires) but, unlike `errorMessage`,
    //! intentionally does **not** set `hadError_`.  Parser
    //! errors are tracked through a separate flag,
    //! `SeqcParserContext::hadSyntaxError_`, which
    //! `Compiler::parse()` checks immediately after `seqc_parse`
    //! returns and converts into a thrown
    //! `CompilerException("Syntax error while parsing seqC")`.
    //! Routing parser errors through this method instead of
    //! `errorMessage` keeps `messages_.hadCompilerError()`
    //! reserved for post-parse errors (AsmCommands, lowering,
    //! evaluation), so each tracking flag has exactly one
    //! source.
    //!
    //! \binarynote The asymmetry with `errorMessage` is verified
    //!             binary-faithful (see IF-233): `parserMessage`
    //!             is a 5-instruction tail-call to
    //!             `compilerMessage` with no write to the
    //!             `hadError_` byte.  Adding such a write would
    //!             be a regression — post-parse phases must not
    //!             be short-circuited by parser-only errors,
    //!             which `parse()` already converts into a
    //!             thrown exception.
    //!
    //! \param line  Source line attributed to the diagnostic.
    //! \param msg   Parser error text.
    void parserMessage(int line, const std::string& msg);       // 0x12ba30

    //! \brief Read-only access to the accumulated diagnostics.
    //!
    //! \details Returns a reference to the internal
    //! `messages_` vector in insertion order.  Stable across
    //! the lifetime of the collection until `reset()` is called
    //! or a new diagnostic is appended.
    //!
    //! \return  Const reference to the diagnostics vector.
    const std::vector<CompilerMessage>& messages() const;
    //! \brief Whether any error-class diagnostic has been
    //!        recorded via `errorMessage`.
    //!
    //! \details Returns the sticky `hadError_` flag.  Note that
    //! `parserMessage` does **not** set this flag, so a parser
    //! syntax error does not flip the result of this query —
    //! `Compiler::hadSyntaxError()` exists for that purpose.
    //!
    //! \return  `true` iff `errorMessage` has been called at
    //!          least once since construction or `reset()`.
    bool hadCompilerError() const { return hadError_; }
    //! \brief Update the "current line" cursor consulted by the
    //!        `*-1`-defaulted typed wrappers.
    //!
    //! \details The cursor is the source line the lowering /
    //! evaluation passes are currently visiting.  Setting it
    //! lets those passes call `errorMessage(msg)` /
    //! `warningMessage(msg)` without threading the line through
    //! every helper.
    //!
    //! \param nr  New line value (negative is permitted but
    //!            then no `(line: N)` suffix will appear in
    //!            rendered output).
    void setLineNr(int nr) { lineNr_ = nr; }
    //! \brief Read the current "current line" cursor.
    //!
    //! \return  The line value most recently passed to
    //!          `setLineNr`, or its default-initialised value
    //!          when never set.
    int lineNr() const { return lineNr_; }
    //! \brief Drop every recorded diagnostic and clear the
    //!        sticky error flag.
    //!
    //! \details Empties `messages_` and sets `hadError_` to
    //! `false`.  The line cursor `lineNr_` is **not** reset —
    //! callers that need a clean cursor must call
    //! `setLineNr(0)` (or the desired sentinel) explicitly.
    //! Invoked at the top of each `Compiler::compile` so a
    //! `Compiler` instance can be reused across multiple
    //! source compiles.
    void reset();                                                // 0x12ba90

private:
    //! \brief Diagnostics recorded so far, in insertion order; the
    //! container the typed `*Message()` helpers funnel into and that
    //! `messages()` exposes read-only.
    std::vector<CompilerMessage> messages_;  // +0x00 (24 bytes)
    //! \brief "Current line" cursor used as the default `line` when
    //! the typed wrappers are called without an explicit override.
    int lineNr_;                             // +0x18
    //! \brief Sticky flag set by `errorMessage()`; queried by
    //! `hadCompilerError()` so later passes can short-circuit.  Note
    //! that `parserMessage()` does **not** set this flag (parser
    //! errors are tracked separately).
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
    //! \brief Construct the exception with a pre-formatted
    //!        diagnostic message.
    //!
    //! \details Copies `msg` into `message_`.  `what()` later
    //! returns it verbatim, falling back to the empty string
    //! when `msg` was empty.  Callers typically produce `msg`
    //! via `ErrorMessages::format(...)`.
    //!
    //! \param msg  Pre-formatted diagnostic, owned-by-copy.
    CompilerException(const std::string& msg);   // 0x11dec0
    //! \brief Release the embedded `message_` storage.
    //!
    //! \details Empty body; `message_`'s destructor frees any
    //! long-form heap buffer.  Out-of-line so the vtable slot
    //! is emitted in this translation unit.
    ~CompilerException() override;               // 0x11df20
    //! \brief Return the stored message, or the empty string
    //!        when none was supplied.
    //!
    //! \details Inspects `message_`: returns
    //! `message_.c_str()` when non-empty, otherwise the empty
    //! string literal.  Unlike the WaveformGenerator exceptions
    //! there is no static fallback diagnostic — callers that
    //! want a non-empty message must supply one at
    //! construction.
    //!
    //! \return  Null-terminated diagnostic, possibly empty.
    const char* what() const noexcept override;  // 0x123bd0

private:
    //! \brief Diagnostic message returned verbatim by `what()`; copied
    //! in from the constructor argument.
    std::string message_;  // +0x08 (24 bytes)
    // +0x20 END
};

}  // namespace zhinst
