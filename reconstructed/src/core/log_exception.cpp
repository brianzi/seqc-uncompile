// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Function: zhinst::logging::detail::logExceptionToClog @0x314a30 (953 bytes)
// ============================================================================

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>

namespace zhinst {
namespace logging {
namespace detail {

// The function rethrows an exception_ptr inside a try/catch to inspect it,
// then logs diagnostic information to std::clog.
//
// Three catch clauses (determined from LSDA selector values):
//   selector 3: catches a type that is both boost::exception and std::exception
//               -> uses boost::exception_detail::diagnostic_information_impl
//   selector 2: catches std::exception
//               -> uses e.what()
//   default:    catch(...)
//               -> logs "Unknown exception."
//
// The outermost try/catch (...) at 0x314dcf silently swallows any exception
// that escapes the logging logic itself, so this function never throws.

void logExceptionToClog(std::exception_ptr eptr, const char* prefix, bool /*flag*/) // 0x314a30
{
    // 0x314a44-0x314a65: copy eptr, extract internal pointer, check for null
    if (!eptr)                      // 0x314a65: test r14,r14
        return;                     // 0x314a68-0x314a81: early return

    try {
        // 0x314a82: test rbx,rbx — check if prefix is non-null
        std::string label;
        if (prefix) {
            // 0x314b11-0x314bd5: construct string from prefix, then insert " in " at front
            // string::insert(0, " in ", 4) at 0x314b98
            // Result: label = " in <prefix>"
            label = prefix;                             // 0x314b11-0x314b81
            label.insert(0, " in ", 4);                 // 0x314b86-0x314b9d
        }

        // 0x314a9a-0x314ae7: output to clog
        // String at 0x90d24c, length 16 = "Exception caught"
        // Then the label string, then ": " (0x8fde17, length 2)
        std::clog << "Exception caught"                 // 0x314a9a-0x314aad
                  << label                              // 0x314ab2-0x314ace
                  << ": ";                              // 0x314ad3-0x314ae7

        // 0x314aee-0x314b0c: copy eptr and rethrow
        std::string msg;
        try {
            std::rethrow_exception(eptr);               // 0x314b0c
        }
        catch (boost::exception const& be) {            // selector 3, 0x314bf2
            // 0x314bf7-0x314c0b: call diagnostic_information_impl
            // args: (boost::exception const*, std::exception const*, false, true)
            // The caught object is dynamic_cast-able to both; the compiler
            // emits a multi-inheritance catch here.
            msg = boost::exception_detail::diagnostic_information_impl(
                &be,
                dynamic_cast<std::exception const*>(&be),
                false,
                true);                                  // 0x314c0b
        }
        catch (std::exception const& e) {              // selector 2, 0x314c15
            // 0x314c20-0x314cca: call e.what(), build string from result
            msg = e.what();                             // 0x314c26: call *0x10(%rcx)
        }
        catch (...) {                                   // default, 0x314c59
            // 0x314c59-0x314c86: current_exception(), set msg = "Unknown exception."
            // Literal at 0x90d25d: "Unknown exception." (18 chars + null)
            // The SSO inline string is built directly:
            //   movb $0x24 -> length byte 0x24 = 36 >> 1 = 18
            //   movups from 0x90d25d -> "Unknown exceptio"
            //   movw $0x2e6e -> "n."
            //   movb $0x00  -> null terminator
            msg = "Unknown exception.";                 // 0x314c65-0x314c7e
        }

        // 0x314ccf-0x314d04: __cxa_end_catch, then output msg + "\n" to clog
        std::clog << msg                                // 0x314cd4-0x314cef
                  << "\n";                              // 0x314cf4-0x314d04

        // 0x314d09-0x314d44: cleanup label and eptr, return
    }
    catch (...) {                                       // 0x314dcf-0x314ddc
        // Silently swallow any exception from the logging itself.
        // 0x314dd2: __cxa_begin_catch
        // 0x314dd7: __cxa_end_catch
        // 0x314ddc: jmp to epilogue
    }
}

} // namespace detail
} // namespace logging
} // namespace zhinst
