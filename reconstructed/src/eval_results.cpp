// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// EvalResults — out-of-line method definitions.
//
// All field offsets and addresses verified against /tmp/disasm_full.txt.
// See zhinst/eval_results.hpp for layout documentation.
//
// Implementation strategy: each setValue overload constructs a single
// EvalResultValue on the stack, then assigns values_ to a one-element
// vector containing it. The compiler-emitted code uses an explicit
// "destroy old elements, operator new(0x38), placement-construct new
// element, free old buffer" sequence; expressing this as
// `values_ = std::vector<EvalResultValue>{ev}` (or assign(1, ev))
// produces an equivalent call graph.
// ============================================================================

#include "zhinst/eval_results.hpp"

#include "zhinst/asm_list.hpp"
#include "zhinst/eval_result_value.hpp"
#include "zhinst/node.hpp"
#include "zhinst/resources.hpp"   // for VarType / VarSubType definitions
#include "zhinst/value.hpp"
#include "zhinst/waveform_front.hpp"

namespace zhinst {

namespace {

// Helper: build a default-initialized EvalResultValue with the given VarType
// and VarSubType (matching the binary pattern used by all setValue overloads
// and the VarType-taking ctor). reg_ defaults to AsmRegister(-1, false).
inline EvalResultValue makeERV(VarType type, VarSubType sub = VarSubType_Default)
{
    EvalResultValue ev{};
    ev.varType_ = type;
    ev.varSubType_ = sub;
    // ev.value_ default-constructed (ValueType::Unspecified, which_=0)
    // ev.reg_ default-constructed (-1, false)
    return ev;
}

inline EvalResultValue makeERV(VarType type, VarSubType sub, Value const& val)
{
    EvalResultValue ev{};
    ev.varType_ = type;
    ev.varSubType_ = sub;
    ev.value_ = val;
    return ev;
}

} // namespace

// ============================================================================
// EvalResults(VarType) — @0x176bc0
//
// Disasm pattern:
//   1. Build EvalResultValue on stack: {type, sub=0, value=Unspecified,
//      reg=AsmRegister(-1)} via call to AsmRegister::AsmRegister(int).
//   2. Call vector<EvalResultValue>::vector(1, val) at &this->values_.
//   3. Zero out fields from +0x18 (assemblers_) through +0x80
//      (arrayBacking_) — i.e. all remaining members default-construct.
//   4. The temp EvalResultValue's destructor runs (15c820), which may
//      free the embedded Value's heap string if any (none here).
// ============================================================================
EvalResults::EvalResults(VarType type)  // @0x176bc0
    : values_(1, makeERV(type))
    , assemblers_()
    , hasError_(false)
    , node_()
    , waveformFront_()
    , name_()
    , arrayBacking_()
{}

// ============================================================================
// ~EvalResults() — @0x16f3d0
//
// Destroys members in reverse declaration order:
//   1. arrayBacking_ shared_ptr release (refcount dec, possibly delete)
//   2. name_ string free if heap-allocated (SSO check at +0x58 byte 0)
//   3. waveformFront_ shared_ptr release
//   4. node_ shared_ptr release
//   5. assemblers_ vector: walk backward, call ~Asm() on each
//      (release node shared_ptr + ~Assembler), then operator delete buffer.
//   6. values_ vector: walk backward, call ~EvalResultValue() on each
//      (which calls ~Value() — frees string if variant holds heap one),
//      then operator delete buffer.
// All standard destruction; compiler-generated dtor matches the disasm.
// ============================================================================
EvalResults::~EvalResults() = default;  // @0x16f3d0

// ============================================================================
// addAssembler(AsmList::Asm const&) — @0x15c1b0
//
// Equivalent to assemblers_.push_back(entry). Disasm shows the inlined
// vector emplace_back fast path (capacity check, placement new of int +
// Assembler copy ctor + node shared_ptr copy + bool), with slow-path
// fallback to vector::__emplace_back_slow_path on reallocation.
// ============================================================================
void EvalResults::addAssembler(AsmList::Asm const& entry)  // @0x15c1b0
{
    assemblers_.push_back(entry);
}

// ============================================================================
// setValue(VarType, Value const&) — @0x211b70
//
// Replaces values_ with a single-element vector holding an EvalResultValue
// constructed as {type, sub=0, value=copy-of-val, reg=AsmRegister(-1)}.
// Disasm allocates a 0x38 EvalResultValue via operator new, copies the
// VarType/VarSubType/Value fields, then assigns the values_ vector storage
// pointers (free old buffer first, walk old elements destructing strings).
// ============================================================================
void EvalResults::setValue(VarType type, Value const& val)  // @0x211b70
{
    values_ = std::vector<EvalResultValue>{ makeERV(type, VarSubType_Default, val) };
}

// ============================================================================
// setValue(VarType, VarSubType, Value const&) — @0x16bfb0
//
// Same as the (VarType, Value const&) overload, but with the caller-supplied
// VarSubType stored at EvalResultValue+0x04.
// ============================================================================
void EvalResults::setValue(VarType type, VarSubType sub, Value const& val)  // @0x16bfb0
{
    values_ = std::vector<EvalResultValue>{ makeERV(type, sub, val) };
}

// ============================================================================
// setValue(VarType, int) — @0x15c850
//
// Replaces values_ with single EvalResultValue {type, sub=0, value=default
// (Unspecified), reg=AsmRegister(int_arg, true)}.
//
// Disasm shows AsmRegister::AsmRegister(int) called with esi=int param —
// confirmed by header note: "AsmRegister(int n) : value(n), valid(true)".
// The Value field stays default (which_=0, type_=Unspecified): the
// disasm zero-initializes the +0x50/+0x58 stack slots that become the
// embedded Value, and never writes into them before the copy.
// ============================================================================
void EvalResults::setValue(VarType type, int reg)  // @0x15c850
{
    EvalResultValue ev{};
    ev.varType_ = type;
    ev.varSubType_ = VarSubType_Default;
    // ev.value_ left default (Unspecified)
    ev.reg_ = AsmRegister(reg);  // {value=reg, valid=true}
    values_ = std::vector<EvalResultValue>{ std::move(ev) };
}

// ============================================================================
// EvalResults(EvalResults const&) — @0x231c60  [nice-to-have]
//
// Deep-copies both vectors (each Asm/EvalResultValue copy-constructed,
// shared_ptrs incref'd, strings copied), bumps refcount on the three
// shared_ptrs, copies the std::string (SSO or heap alloc + memcpy),
// and copies the bool at +0x30 as a single byte.
// ============================================================================
EvalResults::EvalResults(EvalResults const& other)  // @0x231c60
    : values_(other.values_)
    , assemblers_(other.assemblers_)
    , hasError_(other.hasError_)
    , node_(other.node_)
    , waveformFront_(other.waveformFront_)
    , name_(other.name_)
    , arrayBacking_(other.arrayBacking_)
{}

// ============================================================================
// getValue() const — @0x211ab0  [nice-to-have]
//
// Returns the LAST element of values_ by sret. Disasm reads
// vector::end at [this+0x08] then loads element from [end - 0x38].
// Caller is responsible for ensuring values_ is non-empty (the binary
// does not bounds-check).
// ============================================================================
EvalResultValue EvalResults::getValue() const  // @0x211ab0
{
    return values_.back();
}

// ============================================================================
// setValue(Value const&) — @0x15a750  [nice-to-have]
//
// Replaces values_ with one EvalResultValue containing only the Value;
// VarType and VarSubType derived from the Value's type tag are NOT
// applied here (varType_ stays 0). Reg defaults to (-1, false).
// ============================================================================
void EvalResults::setValue(Value const& val)  // @0x15a750
{
    EvalResultValue ev{};
    ev.value_ = val;
    values_ = std::vector<EvalResultValue>{ std::move(ev) };
}

// ============================================================================
// setValue(VarType) — @0x20ad20  [nice-to-have]
//
// Replaces values_ with a single default EvalResultValue tagged with
// the given VarType. Equivalent to the VarType-ctor's value, but applied
// post-construction.
// ============================================================================
void EvalResults::setValue(VarType type)  // @0x20ad20
{
    values_ = std::vector<EvalResultValue>{ makeERV(type) };
}

// ============================================================================
// setValue(double) — @0x2136a0  [nice-to-have]
//
// Replaces values_ with one EvalResultValue {VarType_Const(=4), sub=3,
// value=Value(double), reg=(-1,false)}. Disassembly @0x2136cf writes the
// literal `4` into the new EvalResultValue's varType_ field, and `3` into
// subType. Under the corrected VarType mapping (Phase 19c-followup,
// Finding 1), 4 IS VarType_Const.
// ============================================================================
void EvalResults::setValue(double val)  // @0x2136a0
{
    values_ = std::vector<EvalResultValue>{
        makeERV(VarType_Const, VarSubType_Numeric, Value(val))
    };
}

// ============================================================================
// setValue(VarType, std::string const&) — @0x20af20  [nice-to-have]
//
// Replaces values_ with one EvalResultValue {type, sub=0,
// value=Value(string s), reg=(-1,false)}.
// ============================================================================
void EvalResults::setValue(VarType type, std::string const& s)  // @0x20af20
{
    values_ = std::vector<EvalResultValue>{
        makeERV(type, VarSubType_Default, Value(s))
    };
}

// ============================================================================
// setValue(VarType, Value const&, int) — @0x2107b0  [nice-to-have]
//
// Same as setValue(VarType, Value const&) but with the trailing int
// stored as the AsmRegister field {value=int, valid=true}.
// ============================================================================
void EvalResults::setValue(VarType type, Value const& val, int reg)  // @0x2107b0
{
    EvalResultValue ev{};
    ev.varType_ = type;
    ev.varSubType_ = VarSubType_Default;
    ev.value_ = val;
    ev.reg_ = AsmRegister(reg);
    values_ = std::vector<EvalResultValue>{ std::move(ev) };
}

// ============================================================================
// setValue(VarType, VarSubType, Value const&, int) — @0x247600  [nice-to-have]
//
// Full-featured form: caller specifies all four EvalResultValue fields.
// ============================================================================
void EvalResults::setValue(VarType type, VarSubType sub,
                           Value const& val, int reg)  // @0x247600
{
    EvalResultValue ev{};
    ev.varType_ = type;
    ev.varSubType_ = sub;
    ev.value_ = val;
    ev.reg_ = AsmRegister(reg);
    values_ = std::vector<EvalResultValue>{ std::move(ev) };
}

} // namespace zhinst
