// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// SeqcParserContext: small accessors and error reporting helpers
//
// Bodies recovered from:
//   - currentLineNumber()  @ 0x247c80
//   - raiseError(string&)  @ 0x247ae0
//   - setSyntaxError()     @ 0x247cb0
//
// The class is NEVER instantiated inside reconstructed code — only used
// via pointers obtained from upstream parser-context handles. We therefore
// can't (and don't want to) commit to a struct layout in the public
// header. Instead, the bodies below access fields via raw byte offsets
// matching the disassembly verbatim. If a future phase adds a real
// instantiation, replace these with member accesses to a properly-typed
// layout.
// ============================================================================

#include "zhinst/seqc_parser_context.hpp"

#include <cstdint>
#include <iostream>
#include <string>

namespace zhinst {

namespace {
// Helpers to access raw byte offsets within the opaque SeqcParserContext.
// Equivalent to the binary's `mov ... DWORD PTR [rdi+N]` patterns.
template<typename T>
inline T& at_offset(void* base, std::size_t off) {
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(base) + off);
}
template<typename T>
inline const T& at_offset(const void* base, std::size_t off) {
    return *reinterpret_cast<const T*>(
        static_cast<const std::uint8_t*>(base) + off);
}
} // anonymous namespace

// ----------------------------------------------------------------------------
// 0x247c80 — currentLineNumber() const
//
// Body (4 instructions):  mov eax, DWORD PTR [rdi+0x4] ; ret
// ----------------------------------------------------------------------------
int32_t SeqcParserContext::currentLineNumber() const  // 0x247c80
{
    return at_offset<int32_t>(this, 0x04);
}

// ----------------------------------------------------------------------------
// 0x247c90 — incrementLineNumber()
//
// Body:  inc DWORD PTR [rdi+0x4] ; ret
// ----------------------------------------------------------------------------
void SeqcParserContext::incrementLineNumber()  // 0x247c90
{
    at_offset<int32_t>(this, 0x04)++;
}

// ----------------------------------------------------------------------------
// 0x247bf0 — isComment() const
//
// Body:  movzx eax, BYTE PTR [rdi] ; ret
// Returns true if the comment byte (offset +0x00) is non-zero.
// ----------------------------------------------------------------------------
bool SeqcParserContext::isComment() const  // 0x247bf0
{
    return at_offset<std::uint8_t>(this, 0x00) != 0;
}

// ----------------------------------------------------------------------------
// 0x247c40 — startBlockComment()
//
// Body:  mov BYTE PTR [rdi+0x1], 0x1 ; mov BYTE PTR [rdi], 0x1 ; ret
// ----------------------------------------------------------------------------
void SeqcParserContext::startBlockComment()  // 0x247c40
{
    at_offset<std::uint8_t>(this, 0x01) = 1;
    at_offset<std::uint8_t>(this, 0x00) = 1;
}

// ----------------------------------------------------------------------------
// 0x247c60 — endBlockComment()
//
// Body:  mov BYTE PTR [rdi+0x1], 0x0 ; mov BYTE PTR [rdi], 0x0 ; ret
// ----------------------------------------------------------------------------
void SeqcParserContext::endBlockComment()  // 0x247c60
{
    at_offset<std::uint8_t>(this, 0x01) = 0;
    at_offset<std::uint8_t>(this, 0x00) = 0;
}

// ----------------------------------------------------------------------------
// 0x247c00 — startLineComment()
//
// Body:  mov BYTE PTR [rdi+0x2], 0x1 ; mov BYTE PTR [rdi], 0x1 ; ret
// ----------------------------------------------------------------------------
void SeqcParserContext::startLineComment()  // 0x247c00
{
    at_offset<std::uint8_t>(this, 0x02) = 1;
    at_offset<std::uint8_t>(this, 0x00) = 1;
}

// ----------------------------------------------------------------------------
// 0x247c20 — endLineComment()
//
// Body:  mov BYTE PTR [rdi+0x2], 0x0 ; mov BYTE PTR [rdi], 0x0 ; ret
// ----------------------------------------------------------------------------
void SeqcParserContext::endLineComment()  // 0x247c20
{
    at_offset<std::uint8_t>(this, 0x02) = 0;
    at_offset<std::uint8_t>(this, 0x00) = 0;
}

// ----------------------------------------------------------------------------
// 0x247cb0 — setSyntaxError()
//
// Body (3 instructions):  mov BYTE PTR [rdi+0x3], 0x1 ; ret
// ----------------------------------------------------------------------------
void SeqcParserContext::setSyntaxError()  // 0x247cb0
{
    at_offset<std::uint8_t>(this, 0x03) = 1;
}

// ----------------------------------------------------------------------------
// 0x247ae0 — raiseError(string const& msg)
//
// Behaviour:
//   - Read errorCallback handle at [this+0x30]. If non-null, treat it as
//     a callable object whose vtable[6] (offset +0x30) is operator()(int,
//     string const&). Invoke it with (currentLineNumber(), msg) and return.
//     This is the libc++ `std::function<void(int,string const&)>` indirect
//     call pattern; the callee sees `(int line, string const& msg)`.
//
//   - Else, fall back to a boost.log severity-Error LogRecord that emits:
//       "[Line <currentLine>]: <msg>"
//     constructed via formatted_write calls at 0x247b51, 0x247b89, and
//     0x247bb2 (the three string fragments and the integer).
//
// Reproducing this exactly in source requires reaching into libc++
// std::function internals which we do not have. We provide a behaviourally-
// equivalent implementation that:
//   1. Checks for a non-null callback pointer at [this+0x30]
//   2. If present, dereferences its vtable[6] entry the same way the
//      binary does (`mov rax,[rdi]; call [rax+0x30]`)
//   3. Otherwise emits the formatted error to std::clog
//
// NOTE: this is a *partial* reconstruction. Without a typed layout for
// std::function we cannot implement the indirect call portably across
// libstdc++ and libc++. The fallback path is exercised when the test
// harness has not installed an error callback. Replace with a typed
// member call once SeqcParserContext gains a real layout.
// ----------------------------------------------------------------------------
void SeqcParserContext::raiseError(const std::string& msg)  // 0x247ae0
{
    // Layout-derived callback access — mirror disasm at 0x247af6/0x247b08.
    void* cb = at_offset<void*>(this, 0x30);
    if (cb) {
        // Indirect call through callback's vtable[6] (offset +0x30).
        // The callable's signature is `void (int, std::string const&)`.
        auto** vtbl = reinterpret_cast<void**>(cb);
        using CallbackInvoker = void(*)(void*, int, const std::string&);
        auto invoker = reinterpret_cast<CallbackInvoker>(vtbl[6]);
        int line = at_offset<int32_t>(this, 0x04);
        invoker(cb, line, msg);
        return;
    }

    // Fallback: emit to std::clog when no callback is registered.
    // Mirrors the boost.log Error LogRecord branch at 0x247b1d-0x247bb7
    // which writes "[Line <currentLine>]: <msg>".
    int line = at_offset<int32_t>(this, 0x04);
    std::clog << "[Line " << line << "]: " << msg << '\n';
}

} // namespace zhinst
