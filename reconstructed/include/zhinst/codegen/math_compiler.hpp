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
class MathCompilerException : public std::exception {
    std::string msg_;
public:
    explicit MathCompilerException(std::string const& msg);  // @0x1c40c0
    ~MathCompilerException() override;                        // @0x1c4120, D0 @0x1c4160
    const char* what() const noexcept override;               // @0x1c41b0
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
// NOTE: Dtor visits +0x18 first then +0x00 — that's reverse-construction
//       order, so single-arg is at +0x00 in the canonical layout.
// ============================================================================
class MathCompiler {
public:
    MathCompiler();  // @0x1c2250

    // Single-argument math functions
    double abs(double);        // @0x1c3520
    double acos(double);       // @0x1c3530
    double acosh(double);      // @0x1c35f0
    double asin(double);       // @0x1c36b0
    double asinh(double);      // @0x1c3770
    double atan(double);       // @0x1c3780
    double atanh(double);      // @0x1c3790
    double cos(double);        // @0x1c3850
    double cosh(double);       // @0x1c3860
    double exp(double);        // @0x1c3870
    double ln(double);         // @0x1c3880
    double log(double);        // @0x1c3940
    double log2(double);       // @0x1c3a00
    double log10(double);      // @0x1c3ac0
    double sign(double);       // @0x1c3b80
    double sin(double);        // @0x1c3ba0
    double sinh(double);       // @0x1c3bb0
    double sqrt(double);       // @0x1c3bc0
    double tan(double);        // @0x1c3be0
    double tanh(double);       // @0x1c3bf0
    double ceil(double);       // @0x1c3c00
    double round(double);      // @0x1c3c10
    double floor(double);      // @0x1c3c20

    // Multi-argument math functions
    double avg(std::vector<double> const&);  // @0x1c3c30
    double max(std::vector<double> const&);  // @0x1c3c90
    double min(std::vector<double> const&);  // @0x1c3cf0
    double pow(std::vector<double> const&);  // @0x1c3d50
    double sum(std::vector<double> const&);  // @0x1c3e10

    bool functionExists(std::string const& name, size_t argCount, bool strict) const;  // @0x1c3e50
    double call(std::string const& name, std::vector<double> const& args);             // @0x1c3eb0

    std::map<std::string, std::function<double(double)>>                       singleArgFns_;  // +0x00
    std::map<std::string, std::function<double(std::vector<double> const&)>>   multiArgFns_;   // +0x18
};

} // namespace zhinst
