// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// MathCompiler — implementation extracted from custom_functions.cpp during
//  file-organization split (audit Section C1).
//
// See notes/audit_phase16a.md for the audit context.
// ============================================================================

#include "zhinst/codegen/math_compiler.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/core/types.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace zhinst {

// ============================================================================
// MathCompiler — ctor
// ============================================================================

MathCompiler::MathCompiler() {  // @0x1c2250
    // The binary inlines ~28 single-arg emplace_unique sequences and
    // ~5 multi-arg ones, each of the form:
    //   construct stack string "name"
    //   __tree::__emplace_unique_key_args<string, piecewise_construct_t,
    //       tuple<string&&>, tuple<>>(...)
    //   write &MathCompiler::fn into the new node's std::function slot (+0x40)
    // (See @0x1c2295..0x1c3517 for the full sequence.)
    //
    // Semantically equivalent reconstruction below — the compiler will lower
    // these emplaces into the same __tree calls.
    using namespace std::placeholders;
    auto bind1 = [this](double (MathCompiler::*p)(double)) {
        return std::bind(p, this, _1);
    };
    auto bindN = [this](double (MathCompiler::*p)(std::vector<double> const&)) {
        return std::bind(p, this, _1);
    };
    singleArgFns_.emplace("abs",   bind1(&MathCompiler::abs));
    singleArgFns_.emplace("acos",  bind1(&MathCompiler::acos));
    singleArgFns_.emplace("acosh", bind1(&MathCompiler::acosh));
    singleArgFns_.emplace("asin",  bind1(&MathCompiler::asin));
    singleArgFns_.emplace("asinh", bind1(&MathCompiler::asinh));
    singleArgFns_.emplace("atan",  bind1(&MathCompiler::atan));
    singleArgFns_.emplace("atanh", bind1(&MathCompiler::atanh));
    singleArgFns_.emplace("ceil",  bind1(&MathCompiler::ceil));
    singleArgFns_.emplace("cos",   bind1(&MathCompiler::cos));
    singleArgFns_.emplace("cosh",  bind1(&MathCompiler::cosh));
    singleArgFns_.emplace("exp",   bind1(&MathCompiler::exp));
    singleArgFns_.emplace("floor", bind1(&MathCompiler::floor));
    singleArgFns_.emplace("ln",    bind1(&MathCompiler::ln));
    singleArgFns_.emplace("log",   bind1(&MathCompiler::log));
    singleArgFns_.emplace("log2",  bind1(&MathCompiler::log2));
    singleArgFns_.emplace("log10", bind1(&MathCompiler::log10));
    singleArgFns_.emplace("round", bind1(&MathCompiler::round));
    singleArgFns_.emplace("sign",  bind1(&MathCompiler::sign));
    singleArgFns_.emplace("sin",   bind1(&MathCompiler::sin));
    singleArgFns_.emplace("sinh",  bind1(&MathCompiler::sinh));
    singleArgFns_.emplace("sqrt",  bind1(&MathCompiler::sqrt));
    singleArgFns_.emplace("tan",   bind1(&MathCompiler::tan));
    singleArgFns_.emplace("tanh",  bind1(&MathCompiler::tanh));

    multiArgFns_.emplace("avg", bindN(&MathCompiler::avg));
    multiArgFns_.emplace("max", bindN(&MathCompiler::max));
    multiArgFns_.emplace("min", bindN(&MathCompiler::min));
    multiArgFns_.emplace("pow", bindN(&MathCompiler::pow));
    multiArgFns_.emplace("sum", bindN(&MathCompiler::sum));
}

// MathCompiler::~MathCompiler @0x1592f0 — destroys multiArgFns_ then tail-calls
// singleArgFns_'s tree-destroy. Implicitly defaulted; reverse declaration order
// of the two maps gives the correct dtor sequence.

// Single-argument functions — mostly trivial wrappers around <cmath>
double MathCompiler::abs(double x)   { return std::fabs(x); }      // @0x1c3520
double MathCompiler::acos(double x)  { return std::acos(x); }      // @0x1c3530
double MathCompiler::acosh(double x) { return std::acosh(x); }     // @0x1c35f0
double MathCompiler::asin(double x)  { return std::asin(x); }      // @0x1c36b0
double MathCompiler::asinh(double x) { return std::asinh(x); }     // @0x1c3770
double MathCompiler::atan(double x)  { return std::atan(x); }      // @0x1c3780
double MathCompiler::atanh(double x) { return std::atanh(x); }     // @0x1c3790
double MathCompiler::cos(double x)   { return std::cos(x); }       // @0x1c3850
double MathCompiler::cosh(double x)  { return std::cosh(x); }      // @0x1c3860
double MathCompiler::exp(double x)   { return std::exp(x); }       // @0x1c3870
double MathCompiler::ln(double x)    { return std::log(x); }       // @0x1c3880
double MathCompiler::log(double x)   { return std::log(x); }       // @0x1c3940
double MathCompiler::log2(double x)  { return std::log2(x); }      // @0x1c3a00
double MathCompiler::log10(double x) { return std::log10(x); }     // @0x1c3ac0
double MathCompiler::sign(double x)  { return (x > 0) - (x < 0); } // @0x1c3b80
double MathCompiler::sin(double x)   { return std::sin(x); }       // @0x1c3ba0
double MathCompiler::sinh(double x)  { return std::sinh(x); }      // @0x1c3bb0
double MathCompiler::sqrt(double x)  { return std::sqrt(x); }      // @0x1c3bc0
double MathCompiler::tan(double x)   { return std::tan(x); }       // @0x1c3be0
double MathCompiler::tanh(double x)  { return std::tanh(x); }      // @0x1c3bf0
double MathCompiler::ceil(double x)  { return std::ceil(x); }      // @0x1c3c00
double MathCompiler::round(double x) { return std::round(x); }     // @0x1c3c10
double MathCompiler::floor(double x) { return std::floor(x); }     // @0x1c3c20

// Multi-argument functions
double MathCompiler::avg(std::vector<double> const& v) {  // @0x1c3c30
    // Binary computes sum / size unconditionally (no empty guard); on empty,
    // result is 0/0 = NaN. The integer-to-double conversion at 1c3c64-1c3c81
    // is the standard libc++ uint64→double sequence.
    double s = 0.0;
    for (auto x : v) s += x;
    return s / static_cast<double>(v.size());
}

double MathCompiler::max(std::vector<double> const& v) {  // @0x1c3c90
    // Binary is a manual 2-element-bounded loop using maxsd / cmova.
    // std::max_element is the semantic equivalent.
    return *std::max_element(v.begin(), v.end());
}

double MathCompiler::min(std::vector<double> const& v) {  // @0x1c3cf0
    return *std::min_element(v.begin(), v.end());
}

double MathCompiler::pow(std::vector<double> const& v) {  // @0x1c3d50
    // Binary: if (size != 2) throw MathCompilerException(format(FuncExactly2Args, "pow"));
    // else jmp pow@plt with v[0], v[1].
    if (v.size() != 2) {
        throw MathCompilerException(
            ErrorMessages::format<char const*>(ErrorMessageT::FuncExactly2Args, "pow"));
    }
    return std::pow(v[0], v[1]);
}

double MathCompiler::sum(std::vector<double> const& v) {  // @0x1c3e10
    double s = 0.0;
    for (auto x : v) s += x;
    return s;
}

bool MathCompiler::functionExists(std::string const& name, size_t argCount, bool strict) const {  // @0x1c3e50
    // Disassembly logic:
    //   bl = strict (input)
    //   it = singleArgFns_.find(name)
    //   if (it != end)  al = (argCount == 1)
    //   else            it2 = multiArgFns_.find(name)
    //                   if (it2 != end)  al = (argCount != 0)
    //                   else             return false   (ebx <- 0)
    //   return bl | al
    bool found = false;
    auto it = singleArgFns_.find(name);
    if (it != singleArgFns_.end()) {
        found = (argCount == 1);
    } else {
        auto it2 = multiArgFns_.find(name);
        if (it2 == multiArgFns_.end()) {
            return false;
        }
        found = (argCount != 0);
    }
    return strict | found;
}

double MathCompiler::call(std::string const& name, std::vector<double> const& args) {  // @0x1c3eb0
    // Disassembly logic:
    //   it = singleArgFns_.find(name)
    //   if (found && args.size() == 1)  return it->second(args[0])
    //   if (found && args.size() != 1)  throw FuncSingleArg, name
    //   it2 = multiArgFns_.find(name)
    //   if (found2)                     return it2->second(args)   (tail call)
    //   throw UnknownFunction, name
    auto it = singleArgFns_.find(name);
    if (it != singleArgFns_.end()) {
        if (args.size() == 1) {
            return it->second(args[0]);
        }
        throw MathCompilerException(
            ErrorMessages::format<std::string const&>(ErrorMessageT::FuncSingleArg, name));
    }
    auto it2 = multiArgFns_.find(name);
    if (it2 != multiArgFns_.end()) {
        return it2->second(args);
    }
    throw MathCompilerException(
        ErrorMessages::format<std::string const&>(ErrorMessageT::UnknownFunction, name));
}

// ============================================================================
// MathCompilerException
// ============================================================================

MathCompilerException::MathCompilerException(std::string const& msg)  // @0x1c40c0
    : msg_(msg) {}

MathCompilerException::~MathCompilerException() {}  // @0x1c4120

const char* MathCompilerException::what() const noexcept {  // @0x1c41b0
    return msg_.c_str();
}

} // namespace zhinst
