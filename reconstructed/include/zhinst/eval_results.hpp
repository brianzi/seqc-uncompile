// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// EvalResults — central result type of the frontend lowering pipeline.
//
// Size: 0x80 (128 bytes). Allocated via make_shared (__shared_ptr_emplace
// alloc = 0x98 = 0x18 control block + 0x80 payload).
//
// Ctor:      0x176bc0 (takes VarType)
// Copy ctor: 0x231c60
// Dtor:      0x16f3d0
//
// Every SeqC built-in function and every SeqCAstNode::evaluate virtual
// returns shared_ptr<EvalResults>. The CustomFunctions dispatch map is
// typed as unordered_map<string, function<shared_ptr<EvalResults>(
//   vector<EvalResultValue> const&, shared_ptr<Resources>)>>.
//
// Layout (Phase 15a-i, 2026-04-23):
//   +0x00  24  vector<EvalResultValue>     values_
//   +0x18  24  vector<AsmList::Asm>        assemblers_
//   +0x30   1  bool                        hasError_
//   +0x31   7  (padding)
//   +0x38  16  shared_ptr<Node>            node_
//   +0x48  16  shared_ptr<WaveformFront>   waveformFront_
//   +0x58  24  std::string                 name_
//   +0x70  16  shared_ptr<EvalResults>     arrayBacking_
//   +0x80                                  END
//
// Evidence chain:
//   - Ctor @0x176bc0 constructs vector<EvalResultValue>(1, val) at +0x00
//     using element stride 0x38.
//   - Copy ctor @0x231c60 deep-copies both vectors, bumps refcounts on
//     3 shared_ptrs, copies string with SSO or heap alloc.
//   - Dtor @0x16f3d0 destroys in reverse order: shared_ptr +0x70, string
//     +0x58, shared_ptr +0x48, shared_ptr +0x38, vector +0x18 (stride
//     0xa8 = AsmList::Asm), vector +0x00 (stride 0x38 = EvalResultValue).
//   - SeqCArray::evaluate @0x211140 writes shared_ptr<EvalResults> into
//     ER+0x70 at 0x211798/0x2117a0 — identifies the arrayBacking_ field.
//   - SeqCAssign::evaluate @0x243e60 propagates +0x70 from rhs if non-null
//     (check at 0x243f13).
//   - addAssembler @0x15c1b0 pushes to vector at +0x18 with element
//     stride 0xa8.
//   - getValue @0x211ab0 returns the LAST EvalResultValue from values_.
//   - setValue(double) @0x2136a0 replaces values_ with a single-element
//     vector containing VarType=3.
//   - bool at +0x30 confirmed by ctor byte-write at emplace+0x48
//     (SeqCOperator::evaluate 0x210b12) and copy ctor single-byte copy.
//
// Source path (inferred):
//   /builds/labone/labone/ziAWG/ziAWGUtils/src/main/include/EvalResults.hpp
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <zhinst/asm_list.hpp>    // for AsmList::Asm (nested type, needs full def)
#include <zhinst/value.hpp>       // for Value

namespace zhinst {

// Forward declarations
class Node;
class WaveformFront;
struct EvalResultValue;
enum VarType : int32_t;
enum VarSubType : int32_t;

class EvalResults {
public:
    // --- Fields (0x80 bytes total) ---

    std::vector<EvalResultValue> values_;               // +0x00 (24B)
    std::vector<AsmList::Asm>    assemblers_;            // +0x18 (24B)
    bool                         hasError_ = false;      // +0x30 (1B + 7B pad)
    std::shared_ptr<Node>        node_;                  // +0x38 (16B)
    std::shared_ptr<WaveformFront> waveformFront_;       // +0x48 (16B)
    std::string                  name_;                  // +0x58 (24B libc++ / 32B libstdc++)
    std::shared_ptr<EvalResults> arrayBacking_;          // +0x70 (16B)

    // --- Constructors / Destructor ---

    EvalResults() = default;                              // zero-initialized in make_shared emplace
    EvalResults(VarType type);                            // @0x176bc0
    EvalResults(EvalResults const& other);                // @0x231c60
    ~EvalResults();                                       // @0x16f3d0

    // --- getValue ---
    // Returns the LAST EvalResultValue from values_ by sret.
    // Evidence: getValue @0x211ab0 reads [this+0x08] as vector.end,
    //   then returns *(end - 1).
    // Returns: struct with {VarType, which, value_union} per EvalResultValue.
    EvalResultValue getValue() const;                     // @0x211ab0

    // --- setValue overloads ---
    // All replace values_ with a single-element vector containing the
    // new value. The overloads differ in what fields they set on the
    // EvalResultValue (VarType, VarSubType, Value contents, AsmRegister).

    void setValue(Value const& val);                      // @0x15a750
    void setValue(VarType type);                          // @0x20ad20
    void setValue(VarType type, int val);                 // @0x15c850
    void setValue(double val);                            // @0x2136a0
    void setValue(VarType type, std::string const& s);    // @0x20af20
    void setValue(VarType type, Value const& val);        // @0x211b70
    void setValue(VarType type, Value const& val, int i); // @0x2107b0
    void setValue(VarType type, VarSubType sub,
                  Value const& val);                      // @0x16bfb0
    void setValue(VarType type, VarSubType sub,
                  Value const& val, int i);               // @0x247600

    // --- addAssembler ---
    // Pushes an AsmList::Asm entry into assemblers_ (element size 0xa8).
    // Evidence: addAssembler @0x15c1b0 uses vector emplace_back with
    //   stride 0xa8 and copies from the Asm parameter.
    void addAssembler(AsmList::Asm const& entry);         // @0x15c1b0

    // No copy assignment operator observed in the binary.
    EvalResults& operator=(EvalResults const&) = delete;
};

// NOTE: sizeof(EvalResults) will differ between libc++ (0x80) and
// libstdc++ (0x88+) due to std::string size difference.
// Cannot static_assert sizeof on the build host.

} // namespace zhinst
