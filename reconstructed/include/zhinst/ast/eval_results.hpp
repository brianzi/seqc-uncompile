// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// EvalResults — central result type of the frontend lowering pipeline.
//
// Every SeqC built-in function and every SeqCAstNode::evaluate virtual
// returns shared_ptr<EvalResults>. The CustomFunctions dispatch map is
// typed as unordered_map<string, function<shared_ptr<EvalResults>(
//   vector<EvalResultValue> const&, shared_ptr<Resources>)>>.
//
// Ctor:      0x176bc0 (takes VarType)
// Copy ctor: 0x231c60
// Dtor:      0x16f3d0
//
// Offset  Size  Type                         Name
// +0x00   24    vector<EvalResultValue>      values_
// +0x18   24    vector<AsmList::Asm>         assemblers_
// +0x30   1     bool                         returnEncountered_
// +0x31   7     (padding)
// +0x38   16    shared_ptr<Node>             node_
// +0x48   16    shared_ptr<WaveformFront>    waveformFront_
// +0x58   24    std::string                  name_         (libc++ SSO = 24B)
// +0x70   16    shared_ptr<EvalResults>      arrayBacking_
// sizeof(EvalResults) = 0x80
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "zhinst/asm/asm_list.hpp"    // for AsmList::Asm (nested type, needs full def)
#include "zhinst/ast/value.hpp"       // for Value

namespace zhinst {

// Forward declarations
class Node;
class WaveformFront;
struct EvalResultValue;
enum VarType : int32_t;
enum VarSubType : int32_t;

//! \brief Result of evaluating a SeqC AST node or built-in function.
//!
//! `EvalResults` is the universal return type of the frontend lowering
//! dispatch: every override of `SeqCAstNode::evaluate()` and every entry in
//! the `CustomFunctions` built-in registry produces a
//! `std::shared_ptr<EvalResults>`.  The instance carries a vector of typed
//! `EvalResultValue`s (`values_`, used for multi-return built-ins such as
//! tuple-producing functions), an attached assembler instruction list
//! (`assemblers_`) emitted as a side effect of the evaluation, an optional
//! lowered `Node` subtree (`node_`), an optional waveform descriptor
//! (`waveformFront_`), an optional name binding (`name_`), and a link to a
//! backing array result (`arrayBacking_`) when the value participates in a
//! larger array expression.
//!
//! `returnEncountered_` is set by `SeqCReturnStatement` evaluation so that
//! enclosing scopes can short-circuit further statement processing.
class EvalResults {
public:
    // --- Fields (0x80 bytes total) ---

    std::vector<EvalResultValue> values_;               // +0x00 (24B)
    std::vector<AsmList::Asm>    assemblers_;            // +0x18 (24B)
    bool                         returnEncountered_ = false;      // +0x30 (1B + 7B pad)
    std::shared_ptr<Node>        node_;                  // +0x38 (16B)
    std::shared_ptr<WaveformFront> waveformFront_;       // +0x48 (16B)
    std::string                  name_;                  // +0x58 (24B libc++ / 32B libstdc++)
    std::shared_ptr<EvalResults> arrayBacking_;          // +0x70 (16B)

    // --- Constructors / Destructor ---

    //! \brief Default-constructs an empty results object: no values,
    //! no assemblers, no node, no waveform, no name, no array
    //! backing, `returnEncountered_ == false`.
    EvalResults() = default;                              // zero-initialized in make_shared emplace
    //! \brief Constructs a results object whose `values_` holds a
    //! single default-constructed `EvalResultValue` tagged with the
    //! given `VarType` (sub-type defaulted, `Value` left
    //! `Unspecified`, register invalid).  Used as the seed result
    //! for a typed evaluation that will later overwrite the value.
    EvalResults(VarType type);                            // @0x176bc0
    //! \brief Deep-copies all members: both vectors are
    //! element-copy-constructed; the three `shared_ptr`s have their
    //! refcounts bumped; `name_` and `returnEncountered_` are
    //! value-copied.
    EvalResults(EvalResults const& other);                // @0x231c60
    ~EvalResults();                                       // @0x16f3d0

    // --- getValue ---
    //! \brief Returns a copy of the `Value` payload of the **last**
    //! element of `values_`, or a default-constructed (`Unspecified`)
    //! `Value` if `values_` is empty.
    //! \binarynote The accessor reads only the trailing element; the
    //! varType/varSubType/register fields are not returned.
    Value getValue() const;                               // @0x211ab0

    // --- setValue overloads ---
    // All replace values_ with a single-element vector containing the
    // new value. The overloads differ in what fields they set on the
    // EvalResultValue (VarType, VarSubType, Value contents, AsmRegister).

    //! \brief Replaces `values_` with a single entry tagged
    //! `VarType_Const`, sub-type defaulted, holding `val`, with an
    //! invalid register binding.
    void setValue(Value const& val);                      // @0x15a750
    //! \brief Replaces `values_` with a single default-constructed
    //! entry tagged with `type` (sub-type defaulted, `Value`
    //! `Unspecified`, register invalid).
    void setValue(VarType type);                          // @0x20ad20
    //! \brief Replaces `values_` with a single entry tagged
    //! `(type, default)` whose `Value` is `Unspecified` but whose
    //! register binding is `AsmRegister(val, valid=true)`.  Used to
    //! attach a freshly-allocated register to a typed slot before the
    //! value itself has been computed.
    void setValue(VarType type, int val);                 // @0x15c850
    //! \brief Replaces `values_` with a single entry tagged
    //! `(VarType_Const, VarSubType_Vect)` holding `Value(val)`.
    //! \binarynote The sub-type is `VarSubType_Vect` (3), not
    //! `VarSubType_Default` — preserved from the binary; consumers
    //! that filter on sub-type must allow this case.
    void setValue(double val);                            // @0x2136a0
    //! \brief Replaces `values_` with a single entry tagged
    //! `(type, default)` holding `Value(s)`.
    void setValue(VarType type, std::string const& s);    // @0x20af20
    //! \brief Replaces `values_` with a single entry tagged
    //! `(type, default)` holding `val`, with an invalid register
    //! binding.
    void setValue(VarType type, Value const& val);        // @0x211b70
    //! \brief Replaces `values_` with a single entry tagged
    //! `(type, default)` holding `val` and bound to register
    //! `AsmRegister(i, valid=true)`.
    void setValue(VarType type, Value const& val, int i); // @0x2107b0
    //! \brief Replaces `values_` with a single entry tagged
    //! `(type, sub)` holding `val`, with an invalid register
    //! binding.
    void setValue(VarType type, VarSubType sub,
                  Value const& val);                      // @0x16bfb0
    //! \brief Full-featured form: replaces `values_` with a single
    //! entry whose every field — type, sub-type, value, and register
    //! binding — is caller-specified.
    void setValue(VarType type, VarSubType sub,
                  Value const& val, int i);               // @0x247600

    // --- addAssembler ---
    //! \brief Appends `entry` to `assemblers_`, recording an
    //! assembler instruction emitted as a side effect of the current
    //! evaluation.
    void addAssembler(AsmList::Asm const& entry);         // @0x15c1b0

    // No copy assignment operator observed in the binary.
    EvalResults& operator=(EvalResults const&) = delete;
};

// Binary: sizeof(EvalResults) will differ between libc++ (0x80) and
// libstdc++ (0x88+) due to std::string size difference.
// Cannot static_assert sizeof on the build host.

} // namespace zhinst
