// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// MathCompiler — math expression compiler used as inline sub-object within
// CustomFunctions at +0xC8.
//
// Size: 0x30 bytes (two libc++ std::map instances of 24B each).
// Ctor: 0x1c2250
// Dtor: 0x1592f0 (implicitly defaulted; reverse declaration order matches
//       binary's destruction sequence: multiArgFns_ first @127e7e, then
//       singleArgFns_ @127e64).
//
// Extracted from custom_functions.hpp file-organization
// split (audit Section C1). See notes/audit_phase16a.md.
// ============================================================================
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <exception>

namespace zhinst {

// ============================================================================
// MathCompilerException
// ============================================================================
//! \brief Exception thrown by `MathCompiler` for invalid math-function calls.
//!
//! Raised when `MathCompiler::call` is invoked with an unknown
//! function name (no entry in either `singleArgFns_` or
//! `multiArgFns_`), or when a known single-argument function is
//! called with a different argument count, or when `pow` is called
//! with anything other than two arguments.  The message is
//! constructed via `ErrorMessages::format` parametrised by the
//! function name (e.g. `FuncSingleArg`, `UnknownFunction`,
//! `FuncExactly2Args`); line-number context is attached by the
//! caller in `CustomFunctions::call` before surfacing the error to
//! the user via `messages_`.
class MathCompilerException : public std::exception {
    //! \brief Pre-formatted, user-facing diagnostic returned by
    //!        `what()`.
    std::string msg_;
public:
    //! \brief Construct from a pre-formatted error message.
    //!
    //! \details The message is typically produced by
    //! `ErrorMessages::format` parametrised with the
    //! offending function name (`UnknownFunction`,
    //! `FuncSingleArg`, `FuncExactly2Args`).  Stored by copy
    //! into `msg_` and returned verbatim from `what()`.
    //!
    //! \param msg  Pre-formatted, user-facing error string.
    explicit MathCompilerException(std::string const& msg);
    //! \brief Destroy the exception and release `msg_`.
    ~MathCompilerException() override;
    //! \brief Return the pre-formatted message stored at
    //!        construction.
    //!
    //! \return  C-string view of `msg_`; valid for the
    //!          lifetime of the exception object.
    const char* what() const noexcept override;
};

// ============================================================================
// MathCompiler
//
// Layout (0x30 bytes total) — confirmed from CustomFunctions::~CustomFunctions
// @0x127c90:
//   +0x00  24B  std::map<std::string, std::function<double(double)>>
//                                                              singleArgFns_
//                  (dtor at 127e7e calls __tree<__value_type<string,
//                   function<double(double)>>>::destroy)
//   +0x18  24B  std::map<std::string,
//                std::function<double(std::vector<double> const&)>>
//                                                              multiArgFns_
//                  (dtor at 127e64 calls __tree<__value_type<string,
//                   function<double(vector<double> const&)>>>::destroy)
//
// Binary: Dtor visits +0x18 first then +0x00 — that's reverse-construction
//       order, so single-arg is at +0x00 in the canonical layout.
// ============================================================================
//! \brief Compile-time evaluator for built-in scalar math functions.
//!
//! Owns two name → callable maps populated in the constructor — one
//! for unary `double(double)` functions (trig / hyperbolic / log /
//! rounding / `abs` / `sign`) and one for variadic
//! `double(vector<double>)` functions (`avg`, `max`, `min`, `pow`,
//! `sum`).  Held as an inline sub-object inside `CustomFunctions`
//! and consulted from `CustomFunctions::call` after the user-defined
//! function map (`funcMap_`) misses: when the call's arguments are
//! all compile-time constants (`Const` / `Cvar`), the doubles are
//! extracted via `Value::toDouble` and the result is constant-folded
//! into a `Double` `EvalResults` so the surrounding SeqC code can
//! treat the expression as a literal.  Calls with the wrong arity or
//! an unknown name throw `MathCompilerException`.
class MathCompiler {
public:
    //! \brief Build the two name → callable maps that drive
    //!        compile-time math-function evaluation.
    //!
    //! \details Populates `singleArgFns_` with 23 unary
    //! `double(double)` entries (`abs`, `acos`, `acosh`, `asin`,
    //! `asinh`, `atan`, `atanh`, `ceil`, `cos`, `cosh`, `exp`,
    //! `floor`, `ln`, `log`, `log2`, `log10`, `round`, `sign`,
    //! `sin`, `sinh`, `sqrt`, `tan`, `tanh`) and `multiArgFns_`
    //! with 5 variadic `double(vector<double>)` entries (`avg`,
    //! `max`, `min`, `pow`, `sum`).  Each entry binds the
    //! corresponding `MathCompiler::*` member function with
    //! `std::bind`/`std::function`; the binary inlines the
    //! emplacements as a long sequence of `__tree::__emplace`
    //! calls.
    MathCompiler();

    // Single-argument math functions
    //! \brief `std::fabs(x)`. Absolute value.
    //! \param x  Operand.
    //! \return  Absolute value of `x`.
    double abs(double);
    //! \brief `std::acos(x)`. Arc cosine in radians.
    //!        NaN for `|x| > 1` (delegated to libc).
    //! \param x  Operand.
    //! \return  Arc cosine of `x` in radians, or NaN when `|x| > 1`.
    double acos(double);
    //! \brief `std::acosh(x)`. Inverse hyperbolic cosine.
    //!        NaN for `x < 1` (delegated to libc).
    //! \param x  Operand.
    //! \return  Inverse hyperbolic cosine of `x`, or NaN when `x < 1`.
    double acosh(double);
    //! \brief `std::asin(x)`. Arc sine in radians.
    //!        NaN for `|x| > 1` (delegated to libc).
    //! \param x  Operand.
    //! \return  Arc sine of `x` in radians, or NaN when `|x| > 1`.
    double asin(double);
    //! \brief `std::asinh(x)`. Inverse hyperbolic sine.
    //! \param x  Operand.
    //! \return  Inverse hyperbolic sine of `x`.
    double asinh(double);
    //! \brief `std::atan(x)`. Arc tangent in radians.
    //! \param x  Operand.
    //! \return  Arc tangent of `x` in radians.
    double atan(double);
    //! \brief `std::atanh(x)`. Inverse hyperbolic tangent.
    //!        NaN for `|x| > 1` (delegated to libc).
    //! \param x  Operand.
    //! \return  Inverse hyperbolic tangent of `x`, or NaN when `|x| > 1`.
    double atanh(double);
    //! \brief `std::cos(x)`. Cosine; `x` in radians.
    //! \param x  Angle in radians.
    //! \return  Cosine of `x`.
    double cos(double);
    //! \brief `std::cosh(x)`. Hyperbolic cosine.
    //! \param x  Operand.
    //! \return  Hyperbolic cosine of `x`.
    double cosh(double);
    //! \brief `std::exp(x)`. Natural exponential `e^x`.
    //! \param x  Operand.
    //! \return  `e` raised to the power `x`.
    double exp(double);
    //! \brief `std::log(x)`. Natural logarithm.
    //! \param x  Operand.
    //! \return  Natural logarithm of `x`.
    double ln(double);
    //! \brief `std::log(x)`. Natural logarithm.
    //!
    //! \binarynote In SeqC `log` is the **natural** logarithm,
    //!             not base-10 — same implementation as `ln`.
    //!             Use `log10` for base-10.
    //! \param x  Operand.
    //! \return  Natural logarithm of `x`.
    double log(double);
    //! \brief `std::log2(x)`. Base-2 logarithm.
    //! \param x  Operand.
    //! \return  Base-2 logarithm of `x`.
    double log2(double);
    //! \brief `std::log10(x)`. Base-10 logarithm.
    //! \param x  Operand.
    //! \return  Base-10 logarithm of `x`.
    double log10(double);
    //! \brief Sign of `x`: returns `+1.0` for `x > 0`,
    //!        `-1.0` for `x < 0`, `0.0` for `x == 0`.
    //!
    //! \details Computed as `(x > 0) - (x < 0)` so NaN inputs
    //! yield `0.0` (both comparisons are false).
    //! \param x  Operand.
    //! \return  `+1.0`, `-1.0`, or `0.0` depending on the sign of `x`;
    //!          `0.0` for NaN.
    double sign(double);
    //! \brief `std::sin(x)`. Sine; `x` in radians.
    //! \param x  Angle in radians.
    //! \return  Sine of `x`.
    double sin(double);
    //! \brief `std::sinh(x)`. Hyperbolic sine.
    //! \param x  Operand.
    //! \return  Hyperbolic sine of `x`.
    double sinh(double);
    //! \brief `std::sqrt(x)`. Non-negative square root.
    //!        NaN for `x < 0` (delegated to libc).
    //! \param x  Operand.
    //! \return  Non-negative square root of `x`, or NaN when `x < 0`.
    double sqrt(double);
    //! \brief `std::tan(x)`. Tangent; `x` in radians.
    //! \param x  Angle in radians.
    //! \return  Tangent of `x`.
    double tan(double);
    //! \brief `std::tanh(x)`. Hyperbolic tangent.
    //! \param x  Operand.
    //! \return  Hyperbolic tangent of `x`.
    double tanh(double);
    //! \brief `std::ceil(x)`. Smallest integer ≥ `x`.
    //! \param x  Operand.
    //! \return  Smallest integer value not less than `x`.
    double ceil(double);
    //! \brief `std::round(x)`. Round half away from zero.
    //! \param x  Operand.
    //! \return  `x` rounded to the nearest integer, halves away from zero.
    double round(double);
    //! \brief `std::floor(x)`. Largest integer ≤ `x`.
    //! \param x  Operand.
    //! \return  Largest integer value not greater than `x`.
    double floor(double);

    // Multi-argument math functions
    //! \brief Arithmetic mean of `v`.
    //!
    //! \details Computes `sum(v) / v.size()` with no
    //! empty-input guard; passing an empty vector yields
    //! `0.0 / 0.0 == NaN` rather than raising.  Caller is
    //! responsible for arity validation if that matters.
    //!
    //! \param v  Operand list; may be empty (returns NaN).
    //! \return  Mean of the elements, or NaN when `v` is empty.
    double avg(std::vector<double> const&);
    //! \brief Largest element of `v`.
    //!
    //! \details Forwards to `std::max_element`.  Behaviour
    //! is **undefined** for an empty vector (dereferences
    //! `end()`); callers must ensure `!v.empty()` — typically
    //! enforced upstream by the SeqC frontend's arity check.
    //!
    //! \param v  Operand list; must be non-empty.
    //! \return  Largest element of `v`.
    double max(std::vector<double> const&);
    //! \brief Smallest element of `v`.
    //!
    //! \details Forwards to `std::min_element`.  Behaviour
    //! is **undefined** for an empty vector — see `max`.
    //!
    //! \param v  Operand list; must be non-empty.
    //! \return  Smallest element of `v`.
    double min(std::vector<double> const&);
    //! \brief `pow(v[0], v[1])`. Strictly two-argument.
    //!
    //! \details Throws `MathCompilerException` formatted from
    //! `ErrorMessageT::FuncExactly2Args` with `"pow"` whenever
    //! `v.size() != 2`; otherwise tail-calls `std::pow`.
    //!
    //! \param v  Operand list; must contain exactly two elements
    //!           (base, exponent).
    //! \return  `v[0]` raised to the power `v[1]`.
    //! \throws MathCompilerException  When `v.size() != 2`.
    double pow(std::vector<double> const&);
    //! \brief Sum of all elements of `v`. Returns `0.0` for
    //!        an empty vector.
    //! \param v  Operand list; may be empty.
    //! \return  Sum of the elements of `v`, or `0.0` when `v` is empty.
    double sum(std::vector<double> const&);

    //! \brief Test whether `name` is a known math function and
    //!        — when `strict` is `true` — whether `argCount`
    //!        matches its expected arity.
    //!
    //! \details Looks up `name` in `singleArgFns_` first, then
    //! `multiArgFns_`.  When `strict` is `true` the function
    //! returns `true` only if the name is found **and** the
    //! arity is compatible: `argCount == 1` for unary
    //! functions, `argCount != 0` for variadic ones.  When
    //! `strict` is `false` the arity check is skipped and the
    //! result is `true` for any matching name regardless of
    //! `argCount`.
    //!
    //! \note The binary's lazy-evaluation pattern is
    //!       preserved: `strict | arity_match`.  In the
    //!       non-strict case the arity bit becomes a
    //!       don't-care; a known name with the wrong
    //!       arity still returns `true` so the caller
    //!       (typically `CustomFunctions::call` doing a
    //!       pre-check) can defer the arity diagnostic
    //!       to the eventual `call` site.
    //!
    //! \param name      Function name to look up.
    //! \param argCount  Number of arguments the call site
    //!                  intends to pass.
    //! \param strict    When `true`, also enforce arity
    //!                  matching; when `false`, return
    //!                  `true` for any match.
    //! \return  Whether `name` is known (and, if `strict`,
    //!          callable with `argCount` arguments).
    bool functionExists(std::string const& name, size_t argCount, bool strict) const;
    //! \brief Dispatch `name(args...)` to one of the bound
    //!        unary or variadic math callables.
    //!
    //! \details Search order: `singleArgFns_` first,
    //! `multiArgFns_` only on miss.  For a unary match
    //! `args.size()` must be exactly `1`; otherwise a
    //! `MathCompilerException` formatted with
    //! `ErrorMessageT::FuncSingleArg` and `name` is thrown.
    //! For a variadic match the bound callable is invoked
    //! with `args` as-is (arity validation, where applicable
    //! — e.g. `pow` requires exactly two — happens inside
    //! the bound function).  An entirely unknown name throws
    //! `MathCompilerException` formatted with
    //! `ErrorMessageT::UnknownFunction` and `name`.
    //!
    //! \warning Wrong-arity for a variadic function is
    //!             *not* diagnosed here — only `pow` self-checks
    //!             arity; `avg`/`max`/`min`/`sum` accept any
    //!             non-zero count and may produce undefined
    //!             behaviour or NaN on empty input (see the
    //!             individual function briefs).  Callers
    //!             needing strict arity validation should use
    //!             `functionExists(name, args.size(), true)`
    //!             before dispatching.
    //!
    //! \param name  Math-function name.
    //! \param args  Operand list.
    //! \return  Result of the bound math function.
    //! \throws MathCompilerException  On unknown name, or
    //!         when a unary function is invoked with
    //!         `args.size() != 1`, or when `pow` is invoked
    //!         with `args.size() != 2`.
    double call(std::string const& name, std::vector<double> const& args);

    //! \brief Registry of unary `double(double)` math functions
    //!        keyed by name.
    //! \details Populated by the constructor with 23 entries
    //! (`abs`, `acos`, …, `tanh`).  Consulted first by `call`
    //! and `functionExists`; missing names fall through to
    //! `multiArgFns_`.
    std::map<std::string, std::function<double(double)>>                       singleArgFns_;  // +0x00
    //! \brief Registry of variadic `double(vector<double>)`
    //!        math functions keyed by name.
    //! \details Populated by the constructor with 5 entries
    //! (`avg`, `max`, `min`, `pow`, `sum`).  Searched only on a
    //! `singleArgFns_` miss; per-entry arity checks (e.g. `pow`
    //! requires exactly 2) live inside the bound callables.
    std::map<std::string, std::function<double(std::vector<double> const&)>>   multiArgFns_;   // +0x18
};

} // namespace zhinst
