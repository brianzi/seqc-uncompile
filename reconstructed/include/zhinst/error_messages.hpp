// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// ErrorMessages and ResourcesException
// ============================================================================
#pragma once

#include <stdexcept>
#include <string>

namespace zhinst {

// Error message template IDs observed in disassembly:
//   0  — register validation failures ("Invalid register for <cmd>")
//   5  — overflow/underflow in toInt32 ("Value ... exceeds ... bit range")
//   0xB (11) — unsupported operation ("Operation <cmd> not supported")
enum class ErrorMessageT : int {
    InvalidRegister    = 0,
    ValueOverflow      = 5,
    UnsupportedOp      = 11,
};

// ErrorMessages::format — constructs error message strings.
// Templated on argument types (seen with const char*, double, int).
// Full implementation TBD (Phase 4b).
namespace ErrorMessages {
    template <typename... Args>
    std::string format(ErrorMessageT id, Args&&... args);

    // Overload taking raw int for the error id
    template <typename... Args>
    std::string format(int id, Args&&... args);
}

// ResourcesException — thrown on validation failures in AsmCommands.
// Likely inherits from std::runtime_error.
class ResourcesException : public std::runtime_error {
public:
    explicit ResourcesException(const std::string& msg)
        : std::runtime_error(msg) {}
};

} // namespace zhinst
