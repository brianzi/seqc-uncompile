// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CustomFunctions — play core methods: setWaitCyclesReg, mergeWaveforms,
// play, playIndexed, writeToNode, addSyncCommand, printF, addWaitCycles,
// writeLS64bit, generateWaveform.
//
// Split from custom_functions.cpp
// ============================================================================

#include <boost/format.hpp>
#include <boost/regex.hpp>
#include "zhinst/runtime/custom_functions.hpp"
#include "zhinst/asm/asm_commands.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/ast/eval_results.hpp"
#include "zhinst/ast/eval_result_value.hpp"
#include "zhinst/runtime/node_map_data.hpp"
#include "zhinst/runtime/resources.hpp"
#include "zhinst/core/types.hpp"
#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/waveform/waveform_front.hpp"
#include "zhinst/waveform/waveform_generator.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace zhinst {

extern ErrorMessages errMsg;

// Helpers: emit a suser instruction and append to a list.
// Reduces the repeated 2-line pattern (suser + append) to a single call.
namespace {
inline void appendSuser(AsmList& list, std::shared_ptr<AsmCommands> const& cmds,
                        AsmRegister reg, detail::AddressImpl<unsigned int> addr) {
    list.append(cmds->suser(reg, addr));
}
inline void appendSuser(std::vector<AsmList::Asm>& vec, std::shared_ptr<AsmCommands> const& cmds,
                        AsmRegister reg, detail::AddressImpl<unsigned int> addr) {
    vec.push_back(cmds->suser(reg, addr));
}
} // anonymous namespace

// floatEqual @0x2ec050 — exact bitwise double equality (defined in waveform_generator.cpp).
bool floatEqual(double a, double b);


// setWaitCyclesReg @0x15ca90 — 0x2e8 bytes (ends at 0x15cd78)
// Checks device type bitmask for supported types (0x2,0x3,0x10,0x11,0x20,0x40,0x80,0x100).
// Requires exactly 2 args (vector size == 0x70 = 2 * sizeof(EvalResultValue) where stride=0x38).
// If first arg is an AsmRegister (type==2 at arg+0x38), uses it directly.
// Otherwise, converts arg to int and allocates a new register via Resources::getRegisterNumber(),
// emits ADDI instruction to load the immediate value.
// Finally emits SUSER instruction with address 0x6F.
// Moves the EvalResults shared_ptr from res to the output (return via first arg rdi).
void CustomFunctions::setWaitCyclesReg(std::vector<EvalResultValue> const& args,
                                        std::shared_ptr<EvalResults> results,
                                        std::shared_ptr<Resources> res) {  // @0x15ca90
    // @0x15caad: rax = [this+0x00] (config_), eax = [rax+0x00] (deviceType)
    uint32_t devType = static_cast<uint32_t>(config_->deviceType);

    // @0x15cab4: ecx = devType - 2, cmp ecx, 0x3E
    // @0x15caba: bt 0x4000000040004041, ecx → checks bits 0,6,14,16,30,62
    // which correspond to devType values: 2,3,8,16,18,32,64
    // @0x15cb06: also checks devType == 0x100 and 0x80
    // If none match, skip to move-results-and-return.
    bool supported = false;
    uint32_t shifted = devType - 2;
    if (shifted <= 0x3E) {
        // Bitmask encoding supported device types: HDAWG(2), UHFQA(4),
        // SHFQA(8), SHFSG(16), SHFQC_SG(32), SHFLI(64) after subtracting 2
        constexpr uint64_t kCheckPlaySupportedMask = 0x4000000040004041ULL;
        supported = (kCheckPlaySupportedMask >> shifted) & 1;
    }
    if (!supported) {
        if (devType == AwgDeviceType::VHFLI || devType == AwgDeviceType::GHFLI)
            supported = true;
    }

    if (supported) {
        // @0x15cad3: check args.size() == 2 (vector byte size == 0x70)
        // sizeof(EvalResultValue) == 0x38, so 0x70 / 0x38 == 2
        if (args.size() != 2) {
            // size mismatch → skip to move-and-return @0x15cd58
        } else {
            // @0x15cae5: construct AsmRegister(-1) as default waitReg
            AsmRegister waitReg(-1);  // @0x15cae5

            // Binary reads from args.data()+0x38 = args[1] (second element).
            // +0x38 = args[1].varType_, +0x68 = args[1].reg_, +0x40 = args[1].value_
            auto const& arg1 = args[1];

            if (arg1.varType_ == VarType_Var) {                     // @0x15caf3: cmp [base+0x38], 2
                // @0x15caf9: use existing register directly
                waitReg = arg1.reg_;                                 // @0x15cafd: [base+0x68]
            } else {
                // @0x15cb19: convert value to int
                int immVal = arg1.value_.toInt();                    // @0x15c250

                // @0x15cb25: allocate a fresh register
                int regNum = Resources::getRegisterNumber();         // @0x1e4bb0
                AsmRegister newReg(regNum);
                waitReg = newReg;

                // @0x15cb76: emit addi(waitReg, AsmRegister(0), Immediate(immVal))
                auto addiAsms = asmCommands_->addi(waitReg, AsmRegister(0), Immediate(immVal));
                results->assemblers_.insert(
                    results->assemblers_.end(),
                    addiAsms.begin(), addiAsms.end());               // @0x15cbad
            }

            // @0x15cc89: emit suser(waitReg, Address(kSuserWaitLegacy))
            appendSuser(results->assemblers_, asmCommands_, waitReg,
                        detail::AddressImpl<unsigned int>(kSuserWaitLegacy));  // 0x6F @0x15cc8e
        }
    }

    // @0x15cd58: move results shared_ptr to output (return value via hidden first arg)
}

// mergeWaveforms @0x15e060 — 2956 bytes (0x15e060..0x15eb1c)
//
// Returns a shared_ptr<WaveformFront> (via sret in rdi) representing the
// merged waveform from the provided argument values.
//
// Signature (from mangled symbol):
//   shared_ptr<WaveformFront> mergeWaveforms(
//       vector<EvalResultValue> const& args, short param2, bool param3,
//       string const& name, int param5, bool param6)
//
// Calls:
//   Value::toString() const                      @0x15de50
//   WavetableFront::getWaveformSampleLength()    @0x29c860
//   WavetableFront::getWaveformByName()          @0x29c180  (optional<string> const&)
//   WavetableFront::getWaveformByFunDescr()      @0x29c1f0
//   WavetableFront::newWaveform()                @0x29bce0
//   WaveformGenerator::grow()                    @0x260640
//   WaveformGenerator::merge()                   @0x25f5c0
//   WaveformGenerator::interleave()              @0x258140
//   WaveformGenerator::getOrCreateWaveform()     @0x25bca0
//   vector<Value>::reserve()                     @0x163e30
//   ErrorMessages::format(0xEF, string)          — empty waveform values error
//   ErrorMessages::format(0x9E, string, short, ushort) — channel count mismatch
//
// Error codes: 0xEF (empty values vector), 0x9E (channel count mismatch)
//
// ~3KB function with 7 phases. Structural outline only.
// Full implementation requires PlayArgs and WaveformGenerator complete integration.
std::shared_ptr<WaveformFront> CustomFunctions::mergeWaveforms(
    std::vector<EvalResultValue> const& args,
    short channelCount, bool useYSuffix,
    std::string const& callerName,
    int requestedLength, bool useFunDescrPath)
{  // @0x15e060
    // ----------------------------------------------------------------
    // Structural reconstruction. The original function is ~3KB (893
    // disasm lines including extensive cleanup/EH paths). Refcount and
    // SSO-string disposal sequences are elided; the C++ compiler will
    // reproduce them.
    //
    // Parameter mapping from binary signature
    //   (...sbRK<string>ib):
    //     args              = vector<EvalResultValue> const&  (rdx)
    //     channelCount      = short                           (rcx, ecx → r15d initial)
    //     useYSuffix        = bool                            (r8b → [rbp-0xec])
    //     callerName        = string const&                   (r9 → [rbp-0xe0])
    //     requestedLength   = int                             ([rbp+0x10])
    //     useFunDescrPath   = bool                            ([rbp+0x18])
    //
    // The result shared_ptr<WaveformFront> is returned via sret in rdi
    // (saved to [rbp-0xd8] at @0x15e0c4).
    // ----------------------------------------------------------------

    std::shared_ptr<WaveformFront> result;  // sret slot (rdi → [rbp-0xd8])

    // ----------------------------------------------------------------
    // --- 1. Collect Value objects from args, track max waveform ---
    // sample length seen.                                  @0x15e092..0x15e234
    //
    // The local vector<Value> is at [rbp-0x70]. Reservation is
    // ((args_byte_size / 0x38) * (0x6db... constant)) + 1, i.e.
    // args.size() + 1 (extra slot for the length value appended below).
    // ----------------------------------------------------------------
    std::vector<Value> values;
    values.reserve(args.size() + 1);                    // @0x15e0c4 reserve
    int maxSampleLen = 0;                               // r15d

    for (auto const& erv : args) {                      // stride 0x38, @0x15e0f4 loop head
        // @0x15e10c: Value::toString() — returns the waveform name
        //            referenced by this argument (or empty string).
        std::string wfName = erv.value_.toString();

        if (!wfName.empty()) {                          // @0x15e111..0x15e210
            // @0x15e13b: Look up sample length in the wavetable.
            // [this+0x20] = wavetableFront_
            uint32_t len = wavetableFront_->getWaveformSampleLength(wfName);  // @0x15e14d
            if (static_cast<int>(len) > maxSampleLen) {                       // @0x15e152 cmovg
                maxSampleLen = static_cast<int>(len);
            }

            // @0x15e159..0x15e1ff: emplace_back the EvalResultValue's
            // embedded Value into the local vector. Compiler unrolls the
            // tag/which/storage copy via a jump table over which_, but
            // the net effect is std::vector<Value>::push_back(erv.value_).
            //
            // The duplicated "fast path" inlines reallocation-free copy;
            // the slow path calls __emplace_back_slow_path<Value const&>
            // at @0x1731f0. Both end with size += 1.
            values.push_back(erv.value_);
        }
        // wfName destroyed here (@0x15e218 SSO/heap free dispatch)
    }

    // ----------------------------------------------------------------
    // --- 2. Empty check (@0x15e234..0x15e23f) ---
    //
    // If no waveform names were collected, throw
    // CustomFunctionsValueException(format(0xEF, callerName), 0).
    // Branch target at @0x15e9ec.
    // ----------------------------------------------------------------
    if (values.empty()) {                               // @0x15e23c
        // @0x15e9ec..0x15eabc: format(0xEF, callerName)
        throw CustomFunctionsValueException(
            ErrorMessages::format(NoWaveformInFunc,
                                  callerName),
            0);
    }

    // Capture waveform count BEFORE step 3 appends the trailing length
    // Value.  The binary's step 4 uses a stale r12 (values.begin() from
    // step 1) that was never refreshed after step 3's push_back, so
    // the size computation effectively returns the pre-append count.
    // Binary: @0x15e311-0x15e326 uses stale r12 (values.begin() from Phase 1,
    // never refreshed after Phase 3's push_back), so size = pre-append count.
    const size_t waveformCount = values.size();          // pre-Phase-3 count

    // ----------------------------------------------------------------
    // --- 3. Append length Value (@0x15e245..0x15e2df) ---
    //
    // Append either Value(requestedLength) (if it exceeds the max
    // observed sample length) or Value(0). The compiler builds the
    // Value in [rbp-0xd0..-0xc0] then emplace_back's it.
    // ----------------------------------------------------------------
    if (requestedLength > maxSampleLen) {               // @0x15e249
        values.push_back(Value(requestedLength));       // @0x15e26f / @0x15e2c4
    } else {
        values.push_back(Value(0));                     // @0x15e29f / @0x15e2d6
    }

    // ----------------------------------------------------------------
    // --- 4. Build the funDescr name (single vs multi-value) ---
    //                                                     @0x15e311..0x15e39b
    //
    // The disasm computes count = values.size() via the (byte-diff>>3)
    // * 0xCCCCCCCCCCCCCCCD trick (divide by 5, since stride is 0x28 =
    // 8*5 → byte/8/5 = byte/40). Compare count >= 2.
    //
    // If count >= 2: build "playWave" or "playWaveI" string, depending
    //                on useYSuffix. (@0x15e32c..0x15e377)
    // Else (single-value path): the funDescr name is taken from
    //                           values[0].toString() — i.e. the only
    //                           waveform's own name. (@0x15e39b..0x15e3b6)
    // ----------------------------------------------------------------
    std::string funDescr;
    bool multiValue = (waveformCount >= 2);              // @0x15e326 cmp ..,0x2 (uses pre-append count)

    if (multiValue) {
        // @0x15e32c..0x15e372: in-place build "playWave\0" then
        // append("Y", useYSuffix ? 1 : 0).
        // The "playWaveI" literal is a separate rodata entry at
        //   .rodata+0x900634 (byte 'I'); the "playWave" base sits at +0x8fddf3.
        // Binary appends 0 or 1 bytes from the rodata pointer depending on useYSuffix.
        funDescr = useYSuffix ? "playWaveI" : "playWave";
    } else {
        // @0x15e3a2..0x15e3b6: Value::toString() on values[0].
        funDescr = values.front().toString();
    }

    // ----------------------------------------------------------------
    // --- 5. Quick lookup by name (only on the multi-value path) ---
    //                                                     @0x15e3e0..0x15e4aa
    //
    // The disasm constructs a std::optional<std::string> in
    // [rbp-0xd0..-0xb8] (string at -0xd0/-0xc8/-0xc0; bool tag at -0xb8
    // pre-set to 1 at @0x15e3e0) and calls
    //   wavetableFront_->getWaveformByName(optName)
    //
    // Binary: Phase 5 is UNCONDITIONAL — runs for both single and
    // multi-value paths. Does NOT gate on multiValue.
    // When the waveform already exists (e.g. from `wave w = zeros(64)`),
    // the name lookup succeeds and Phase 6 is skipped entirely.
    //
    // If the name lookup returns a non-null shared_ptr, the function
    // skips Phase 6 entirely and proceeds to Phase 7 (channel check)
    // with that result. (@0x15e4aa: cmp [r15],0; je 15e956)
    // ----------------------------------------------------------------
    {
        std::optional<std::string> optName(funDescr);   // tag set to engaged @0x15e3e0
        result = wavetableFront_->getWaveformByName(optName);  // @0x15e3f8
        // result == null → fall through to 
    }

    // ----------------------------------------------------------------
    // --- 6. Build the actual waveform if not already cached ---
    //                                                     @0x15e4aa..0x15e93f
    //
    // Two sub-paths depending on useFunDescrPath ([rbp+0x18]):
    //
    //   A) useFunDescrPath == true  (@0x15e4d3..0x15e7c5):
    //      Re-build funDescr as "playWave"/"playWaveY" (binary builds
    //      it again locally at -0x40, even on the single-value path),
    //      then:
    //
    //        wf2 = wavetableFront_->getWaveformByFunDescr(             // @0x15e4f1
    //                  funDescr, values);
    //
    //      If wf2 is non-null, return it. Otherwise:
    //
    //      Multi-value:                                                // @0x15e570
    //        sig = waveformGen_->grow(values);                         // @0x15e580
    //        result = wavetableFront_->newWaveform(sig, funDescr,      // @0x15e59d
    //                                              values);
    //
    //      Single-value:                                               // @0x15e66e
    //        sig = waveformGen_->merge(values);                        // @0x15e67d
    //        result = wavetableFront_->newWaveform(sig, funDescr,      // @0x15e699
    //                                              values);
    //
    //   B) useFunDescrPath == false (@0x15e84b..0x15e7c5):
    //      Use waveformGen_->getOrCreateWaveform with a bound member
    //      function as factory. Three factory targets observed in the
    //      binary, all confirmed by direct `lea rax,[rip+...]` loads
    //      of the member-function address:
    //        @0x15e78a → WaveformGenerator::interleave  @0x258140
    //        @0x15e7d9 → WaveformGenerator::merge       @0x25f5c0
    //        @0x15e85e → WaveformGenerator::grow        @0x260640
    //
    //      Dispatch (decoded — partial):
    //        @0x15e774  test bl, bl
    //                   je 15e7c7    → MERGE
    //                                   else (fall through) → INTERLEAVE
    //        bl is read from [rbp-0x48] @0x15e6d9, which is a function-
    //        local value computed earlier in the multi-value path (NOT
    //        directly an incoming parameter). Likely derived from
    //        useYSuffix combined with a property of the args; the exact
    //        formula is still under investigation. Single-value path
    //        (@0x15e84b) is unconditionally GROW.
    //
    //      Approximation in source: we map (multiValue, useYSuffix) →
    //      {interleave, merge, grow} which gives correct behaviour for
    //      the common cases but may diverge on edge cases where the
    //      binary's local-state derivation produces a different bool.
    //      Re-investigation of [rbp-0x48] origin is tracked as 21a-followup.
    // ----------------------------------------------------------------
    if (!result) {                                       // @0x15e4aa cmp [r15],0
        // @0x15e4d3: if !multiValue → jump directly to grow path @0x15e84b,
        // REGARDLESS of useFunDescrPath.  The binary's control flow is:
        //   0x15e4aa  test result
        //   0x15e4d3  cmp multiValue → je 0x15e84b (grow, single-value)
        //   0x15e4xx  test useFunDescrPath → dispatch Sub-path A vs B
        // So the multiValue check is BEFORE the useFunDescrPath check.
        if (!multiValue) {
            // Single-value: unconditionally GROW via getOrCreateWaveform.     // @0x15e84b
            std::function<Signal(std::vector<Value> const&)> factory =
                [this](std::vector<Value> const& a) {
                    return waveformGen_->grow(a);
                };
            result = waveformGen_->getOrCreateWaveform(funDescr, values,
                                                       std::move(factory));
        } else if (useFunDescrPath) {
            // Sub-path A (multi-value only): explicit fun-descr lookup,
            // then explicit newWaveform if not present.
            std::string funDescr2 = "playWave";                                  // @0x15e4bd (SSO inline)

            result = wavetableFront_->getWaveformByFunDescr(funDescr2, values);  // @0x15e4f1

            if (!result) {
                // @0x15e570..0x15e5a2
                Signal sig = waveformGen_->grow(values);
                result = wavetableFront_->newWaveform(sig, funDescr2, values);
            }
        } else {
            // Sub-path B (multi-value only): dispatch on useYSuffix.  @0x15e774
            //
            // @0x15e774: test bl,bl (bl = useYSuffix from -0xec(%rbp))
            //   bl != 0 → interleave factory @0x15e778
            //   bl == 0 → merge factory      @0x15e7c7
            std::function<Signal(std::vector<Value> const&)> factory;
            if (useYSuffix) {
                // @0x15e778..0x15e7c5: interleave
                factory = [this](std::vector<Value> const& a) {
                    return waveformGen_->interleave(a);
                };
            } else {
                // @0x15e7c7..0x15e80f: merge
                factory = [this](std::vector<Value> const& a) {
                    return waveformGen_->merge(a);
                };
            }
            result = waveformGen_->getOrCreateWaveform(funDescr, values,
                                                       std::move(factory));
        }
    }

    // ----------------------------------------------------------------
    // --- 7. Channel count validation (@0x15e956..0x15ea21) ---
    //
    // Read [WaveformFront+0xC8] (16-bit channels field — see comment
    // in waveform_front.hpp; this is Waveform::signal.channels_ at
    // signal+0x48). Compare against the requested channelCount (sign-
    // extended from short).
    //
    // If actualChannels > requestedChannels, throw
    //   CustomFunctionsValueException(
    //       format(0x9E, callerName, channelCount, actualChannels), 0)
    // (jump @0x15ea23 → exception path @0x15eadc).
    // ----------------------------------------------------------------
    {
        // @0x15e956: r14 = result.get()
        WaveformFront* wf = result.get();
        // @0x15e959: movzx eax, WORD PTR [r14+0xc8]
        // (= signal+0x48 = Signal::channels_ ; accessed via Signal::channels())
        uint16_t actualChannels = wf->signal.channels();
        // @0x15e961: movsx ebx, WORD PTR [rbp-0xf0]
        int requested = static_cast<int>(channelCount);
        if (static_cast<int>(actualChannels) > requested) {  // @0x15e968 jg
            // @0x15ea23..0x15eb29: format(0x9E, callerName, short, ushort)
            throw CustomFunctionsValueException(
                ErrorMessages::format(TooManyChannels,
                                      callerName,
                                      channelCount,
                                      actualChannels),
                0);
        }
    }

    // ----------------------------------------------------------------
    // Cleanup epilogue (@0x15e970..0x15e9eb): destroy local vector,
    // pop the saved registers, and return the sret. The C++ compiler
    // will regenerate this from the local-scope destructor of `values`.
    // ----------------------------------------------------------------
    return result;
}

std::shared_ptr<EvalResults> CustomFunctions::play(
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> res,
    SubFunc subFunc) {  // @0x15f090  (7536 bytes, ends at 0x160e00)

    // --- Step 1: Build command name from SubFunc --- @0x15f0c3
    std::string cmdName;
    // Binary: Jump table at 0x958ef8 with 5 entries (0-4). Callers pass
    // SubFunc enum values (Default=1, Aux=2, Now=3, DigTrigger=4).
    // The case-to-string mapping below is inferred from the enum values
    // and string targets. Case 0 ("prefetch") is unreachable from
    // normal callers but exists in the binary's jump table.
    switch (static_cast<int>(subFunc)) {
    case 0: cmdName = "prefetch";            break;                  // @0x15f0e0 (unreachable)
    case 1: cmdName = "playWave";            break;                  // SubFunc::Default @0x15f115
    // case 2 (SubFunc::Aux) is not used in play(), only in playIndexed()
    case 3: cmdName = "playWaveNow";         break;                  // SubFunc::Now @0x15f0f7
    case 4: cmdName = "playWaveDigTrigger";  break;                  // SubFunc::DigTrigger @0x15f12c
    default:
        // @0x15f145: binary logs warning about unknown SubFunc
        cmdName = "playWave";
        break;
    }

    // --- Step 2: Empty-args guard --- @0x15f1be
    if (args.empty()) {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncMinArgs,
                                  cmdName, 1, static_cast<int>(args.size())));  // @0x16080d
    }

    // --- Step 3: Copy args; DigTrigger extracts play-length --- @0x15f1ce
    std::vector<EvalResultValue> argsCopy(args);                     // @0x15f225
    int firstArgVal = 0;
    if (subFunc == SubFunc::DigTrigger) {                            // @0x15f264
        auto const& firstVal = argsCopy.front();
        // Type check: must be int-like
        if (firstVal.varType_ != VarType_Const) {
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, cmdName));  // @0x1608e4
        }
        firstArgVal = firstVal.value_.toInt();                        // @0x15f32b
        if (firstArgVal < 3) {
            throw CustomFunctionsException(
                ErrorMessages::format(IndexMustBe, cmdName,
                                      std::string("3 or larger")));  // @0x160940
        }
        // Strip first arg
        argsCopy.erase(argsCopy.begin());                            // @0x15f340
    }

    // --- Step 4: Construct PlayArgs --- @0x15f4b6
    PlayArgs playArgs(*config_, wavetableFront_, warningCallback_,
                      cmdName, false);                               // @0x15f53c
    auto parseEnd = playArgs.parse(argsCopy);                        // @0x15f5ad
    int rate = parseOptionalRate(argsCopy.cbegin(), argsCopy.cend(),
                                 parseEnd, cmdName, false);          // @0x15f5ca

    // --- Step 5: Create EvalResults --- @0x15f5d2
    auto results = std::make_shared<EvalResults>(VarType_Void);        // @0x15f5ee
    // @0x15f614: cmpb $0x0, [rbp-0x1a8] = PlayArgs+0x78 = hasMarker_.
    // When hasMarker_ is set (any wave arg has varSubType_==2, i.e. FunctionArg),
    // the function is being evaluated in a "first pass" / function body context
    // where wave parameters are unresolved. Skip all rendering and return early.
    // The binary jumps to 0x160664 which destroys waveAssignments and returns
    // the empty EvalResults(VarType_Void) created above.
    if (playArgs.hasMarker_) {
        return results;
    }

    // --- Step 6: Per-channel loop --- @0x15f628
    int64_t maxSampleLen = playArgs.getMaxSampleLength();            // @0x15f62f
    int numChannels = config_->numChannelGroups;                     // +0x1c  @0x15f642
    int channelIndex = config_->deviceIndex;                         // +0x24

    std::vector<std::shared_ptr<WaveformFront>> channelWaveforms(
        static_cast<size_t>(numChannels));                           // @0x15f679

    int mask = kPlayTriggerMaskFull;                                               // @0x15f6bb

    for (int ch = 0; ch < numChannels; ++ch) {                      // @0x15f6e6
        auto& assignments = playArgs.waveAssignments_[ch];
        if (assignments.empty())
            continue;

        std::vector<EvalResultValue> channelArgs;
        int waIndex = 0;                                               // rbx reset per group @0x15f773
        for (auto& wa : assignments) {                               // stride 0x50  @0x15f7ac
            if (wa.value.varType_ != VarType_Const) {                              // @0x15f7b4
                channelArgs.push_back(wa.value);
            }
            if (ch == channelIndex) {                                  // @0x15f889
                // Mask-clearing: extract name, clear bits             // @0x15f8ac
                std::string name = wa.value.value_.toString();
                if (!name.empty()) {
                    // SIMD-accelerated in binary @0x15f949; semantically
                    // identical to scalar bit-clearing per wa.bits entry.
                    // Binary: lea 0(,%rbx,8),%eax; sub %ebx,%eax → eax = rbx*7
                    // where rbx is the WA index within the channel group.
                    int shift = waIndex * 7;                           // @0x15f928
                    for (int b : wa.bits) {
                        mask &= ~(1 << ((b - 1) + shift));            // @0x15fa2c
                    }
                }
            }
            ++waIndex;                                                 // @0x15f780: inc rbx
        }

        // @0x15fa5f: merge waveforms for this channel
        // Disasm trace (call site verified against mangled signature):
        //   rdx (args)  = channelArgs (vector const&)
        //   rcx (short) = movsx [r13+0x14] = config_->channelsPerGroup[0]
        //   r8b  (bool) = xor r8d,r8d → false
        //   r9   (str)  = name
        //   stack[2nd push] (int) = QWORD [rbp-0xa8] = (int)PlayArgs::getMaxSampleLength()
        //       (set @0x15f634 by `mov [rbp-0xa8], rax` after call to
        //        PlayArgs::getMaxSampleLength@0x15f62f; pushed as 8B but
        //        called signature reads only low 32 bits — int truncation)
        //   stack[1st push] (bool) = setne al on cmp r14,[rbp-0xd8]:
        //       r14  = [rbp-0xb0] = ch (current channel-group index)
        //       0xd8 = r13[+0x24] = a per-iteration "reference channel"
        //              int (loaded @0x15f747 with `mov eax,[r13+0x24]`).
        //       The bool true ⇒ "current channel ≠ reference channel".
        //       Same comparison appears @0x15f890 inside the WA loop, so
        //       this bool gates merge-vs-passthrough behaviour for
        //       secondary channel iterations.
        if (!channelArgs.empty()) {
            int maxSampleLength = static_cast<int>(playArgs.getMaxSampleLength());
            // Note: in the binary `referenceChannel` is a per-iteration int
            // at PlayArgs/state +0x24; not yet exposed as a named accessor.
            // For now reconstruct the test against `channelIndex` which is
            // the natural variable carrying the same value in our source.
            bool isSecondaryChannel = (ch != channelIndex);
            channelWaveforms[ch] = mergeWaveforms(
                channelArgs,
                static_cast<short>(config_->channelsPerGroup[0]),  // FIXED: was deviceType
                false,                                              // FIXED: was (ch != channelIndex)
                cmdName,
                maxSampleLength,
                isSecondaryChannel);
        }
    }

    // --- Step 7: Validation --- @0x15fc27
    auto combinedWf = channelWaveforms[channelIndex];
    if (subFunc == SubFunc::DigTrigger) {
        checkWaveformMinLengthTrig(combinedWf);                      // @0x15fc8f
    }
    if (combinedWf) {
        checkOffspecWaveLength(combinedWf, static_cast<int>(devConst_->maxWaveformLength)); // @0x15fcef
    }

    // --- Step 8: Assembly generation --- @0x15fd0f
    // Binary control flow (verified via disassembly):
    //   SubFunc!=0: check isHirzel||combinedWf → asmPrefetch (if !isHirzel && combinedWf) → asmPlay → push
    //   SubFunc==0, isHirzel, combinedWf: asmPrefetch → fall through to asmPlay → push
    //   SubFunc==0, !isHirzel: throw error 0xa5
    if (static_cast<int>(subFunc) == 0) {
        // Prefetch path (SubFunc==0)                                @0x15fdf6
        if (!combinedWf) goto step9_return;                          // @0x15fdfd: test+je to 0x1605be
        if (!config_->isHirzel) {                                    // @0x15fe0d: cmpb $0x0,0x18(%rax)
            // Non-Hirzel + SubFunc==0 → throw error 0xa5            @0x16098c
            throw CustomFunctionsException(
                ErrorMessages::format(ErrorMessageT(0xa5)));
        }
        // Hirzel + SubFunc==0: emit asmPrefetch only (no asmPlay)
        {
            auto asmEntry = asmCommands_->asmPrefetch(combinedWf, channelIndex, 0,  // @0x15fe58
                                      static_cast<int>(maxSampleLen));
            results->assemblers_.push_back(asmEntry);
            if (results->node_) {
                results->node_->next = asmEntry.node;
            } else {
                results->node_ = asmEntry.node;
            }
        }
        goto step9_return;
    } else {
        // SubFunc!=0 path                                           @0x15fd14
        // Binary at 0x15fd21: cmpb $0x0, [config_+0x18] — isHirzel check.
        // Hirzel: always proceeds (even if combinedWf is null).
        // Non-Hirzel (Cervino): proceeds only if combinedWf is non-null.
        if (!config_->isHirzel && !combinedWf) goto step9_return;    // @0x15fd2f: je to 0x1605be

        // Non-Hirzel only: emit asmPrefetch before asmPlay          @0x15fe58
        // (Hirzel skips this — the Prefetch pass generates prf from Play node)
        if (!config_->isHirzel && combinedWf) {
            asmCommands_->asmPrefetch(combinedWf, channelIndex, 0,
                                      static_cast<int>(maxSampleLen));
        }
    }

    {
        // Common asmPlay path (reached from both SubFunc==0 Hirzel and SubFunc!=0)
        // @0x15fe8c..0x16019f: copy channelWaveforms, build optionals
        auto wfCopy = channelWaveforms;                              // @0x15fd57

        // For Hirzel, channelIndex comes from config_->deviceIndex when
        // the channelWaveforms vector is empty.                     @0x160194
        int playDeviceIndex = channelIndex;
        if (config_->isHirzel && channelWaveforms.empty()) {
            playDeviceIndex = config_->deviceIndex;
        }

        // Build waveform name optionals for asmPlay
        // @0x15fe8c..0x16019f
        AsmRegister regZero(0);                                         // @0x16019f
        AsmRegister regInvalid(-1);                                      // @0x1601ad

        auto asmEntry = asmCommands_->asmPlay(
            std::move(wfCopy), playDeviceIndex,
            false,                                                   // isHold: false for normal play
            subFunc == SubFunc::Now,
            false, rate, static_cast<unsigned int>(mask),
            false,
            regZero, firstArgVal, regInvalid,
            0u);                                                     // @0x160209

        // @0x160335: push into results->assemblers_
        results->assemblers_.push_back(asmEntry);

        // @0x160438: store play node into results->node_, chaining
        // to any existing node via ->next                           @0x160468
        if (results->node_) {
            results->node_->next = asmEntry.node;
        } else {
            results->node_ = asmEntry.node;
        }
    }
    step9_return:

    // --- Step 9: Return --- @0x1607a4
    return results;
}

std::shared_ptr<EvalResults> CustomFunctions::playIndexed(
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> res,
    SubFunc subFunc) {  // @0x160e00  (6428 bytes, ends at 0x16271c)

    // ============================================================
    //
    //  (cmdName build, arg-count guard, PlayArgs ctor,
    // parse + parseOptionalRate, EvalResults + waveIndex extract)
    // are verified against disasm 0x160e00..0x16122d.
    //
    //  (per-channel arg gathering, getWaveformSampleLength,
    // WaveformGenerator::call("zeros"), mergeWaveforms @0x161c2b,
    // loadWaveform, addi+asmSetVarPlaceholder, asmPlay, cleanup,
    // error tail) are partially reconstructed; remaining stubs noted
    // inline (from 21b-prereq-B).
    //
    // The binary calls asmPlay @0x162343 with an additional
    // `addi indexReg, 0, waveIndex` step beforehand. There is no
    // asmTable in the binary body of playIndexed.
    // ============================================================

    // --- 1. Build command name from SubFunc --- @0x160e31
    // Binary stores the cmdName as a libc++ short-string in the
    // 24-byte slot at [rbp-0x40]. Empty string for default path.
    std::string cmdName;
    switch (static_cast<int>(subFunc)) {
    case 1: cmdName = "playWaveIndexed";    break;                   // @0x160e43, "playWaveIndexed"   (15 chars, size byte 0x1e)
    case 2: cmdName = "playAuxWaveIndexed"; break;                   // @0x160e68, rodata @0x900134    (18 chars, size byte 0x24)
    case 3: cmdName = "playWaveIndexedNow"; break;                   // @0x160e84, rodata @0x900115    (18 chars, size byte 0x24)
    default:
        // @0x160e9d..0x160f15: emits a logging::Severity(1) (warning)
        // LogRecord — formatted "playIndexed: unknown SubFunc " + int
        // — but does NOT throw. Falls through with cmdName empty.
        // Binary: emits a boost::log warning here via LogRecord.
        // Logging facade not integrated; warning is silently dropped.
        break;
    }

    // --- 2. Arg-count guard (@0x160f16) ---
    // Binary: (size_in_bytes / 56) > 2  →  args.size() >= 3 to proceed.
    // Below 3 → error 0xC8 (200) "wrong number of arguments" at @0x162735.
    // (Existing stub used `< 2`, which was off by one.)
    if (args.size() < 3) {
        // Error @0x162735 → format(0xC8, cmdName, ?, args.size())
        // Binary: error code 0x3d is inferred (the "expected at least N args"
        // template). Binary throw at @0x162735.
        throw CustomFunctionsException(
            ErrorMessages::format(FuncMinArgs,
                                  cmdName,
                                  2,                                  // expected min
                                  static_cast<int>(args.size())));    // got
    }

    // --- 3. Construct PlayArgs (@0x160f99..0x160fd1) ---
    // r9b = (subFunc == Aux) ? 1 : 0  → indexed flag.
    // Cross-check: play()=false, playAuxWave=true, playDIOWave=false,
    // assignWaveIndex=false, playIndexed=(subFunc==Aux).
    // (Existing stub had `(subFunc != SubFunc::Aux)` — INVERTED.)
    bool indexed = (subFunc == SubFunc::Aux);                        // @0x160faa
    PlayArgs playArgs(*config_, wavetableFront_, warningCallback_,
                      cmdName, indexed);                             // @0x160fd1

    // --- 4. parse() + validate index/length + parseOptionalRate ---
    // @0x16104c..0x1611af
    // parse() returns iterator past last consumed wave arg.
    auto parseEnd = playArgs.parse(args);                            // @0x16104c

    // --- : Validate index/length arg types --- @0x1610fb..0x161190
    // Binary validates parseEnd[0] and parseEnd[1] (the index and length
    // args) BEFORE calling parseOptionalRate.  These two args are consumed
    // here; parseOptionalRate receives parseEnd+2 as its cursor.
    // Binary @0x1610fb: parseEnd[0].varType_ must pass bt $0x54 (bits 2,4,6 set).
    // Accepts VarType values {2=Var, 4=Const, 6=Cvar}. Error 0x98 on mismatch.
    {
        int vt0 = static_cast<int>(parseEnd[0].varType_);
        if (vt0 > 6 || !((0x54 >> vt0) & 1)) {
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsOffsetAndLength, cmdName));  // 0x98 @0x162976
        }
    }
    // Binary @0x161145: parseEnd[1].varType_ must pass the same bt $0x54 test.
    {
        int vt1 = static_cast<int>(parseEnd[1].varType_);
        if (vt1 > 6 || !((0x54 >> vt1) & 1)) {
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsOffsetAndLength, cmdName));  // 0x98 @0x16299d
        }
    }

    // parseOptionalRate receives parseEnd+2 (past the index/length args).
    // strict flag: r8b = (subFunc == Aux) ? 1 : 0
    auto rateBegin = parseEnd + 2;
    int rate = parseOptionalRate(args.cbegin(), args.cend(), rateBegin,
                                 cmdName,
                                 /*strict=*/(subFunc == SubFunc::Aux));  // @0x1611af

    // Post-rate guard @0x1611b4..0x1611d0:
    //     if (subFunc == Aux && rate < 5)  →  error 0xa0 @0x162797
    if (subFunc == SubFunc::Aux && rate < 5) {
        throw CustomFunctionsException(
            ErrorMessages::format(SampleRateTooHigh, cmdName));  // @0x162797
    }

    // --- 5. EvalResults + extract waveIndex (@0x1611d6..0x161228) ---
    // Binary type-check on parseEnd[1] (the length arg): varType ∈ {4 (Const), 6 (Cvar)}.
    // Else error 0x9a @0x1627c5.
    {
        VarType rateType = parseEnd[1].varType_;
        // (varType & ~2) == 4   matches {4,6}
        if ((static_cast<int>(rateType) & 0xfffffffd) != 0x4) {
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsSamplesConst, cmdName));  // 0x9a @0x1627c5
        }
    }

    // Allocate EvalResults(VarType::Void = 1) — same pattern as play().
    auto results = std::make_shared<EvalResults>(VarType_Void);        // @0x1611ed..0x16121d

    // Wave index = first parsed arg's Value::toInt().
    // The binary indexes via [rbp-0x318] (the rate-slot variant, after
    // parseOptionalRate has copied the wave-index variant into it via
    // variant_assign @0x161131). At source level that is simply the
    // first parsed positional argument's int value.
    // Binary: the index is read from the first parsed positional arg.
    //         Reads from `rbx = [rbp-0x318]` at @0x161228.
    int waveIndex = parseEnd[1].value_.toInt();                     // @0x161228 — length arg

    // === : waveIndex==0 early-exit warning ===              @0x16131b..0x1613c9
    // Binary @0x161236: `test eax, eax; je 16131b` — when waveIndex
    // is zero the function does NOT throw; it formats error 0x9c with
    // cmdName, calls `warningCallback_(...)` (vtable[+0x30] indirect
    // through `[res+0x1b0]`), then jumps to the success-tail cleanup
    // and returns the (currently empty) `results`.
    if (waveIndex == 0) {
        std::string warning = ErrorMessages::format(
            LengthIsZero, cmdName);              // @0x161362
        if (warningCallback_) {
            warningCallback_(warning);                               // @0x161382 vtable[+0x30] indirect
        }
        return results;  // jumps to common cleanup-and-return @0x1625ea
    }

    // --- 6. Locate per-channel WaveAssignment vector (@0x161250..0x16127b) ---
    // Binary @0x161250-0x16125f computes:
    //   rax = [r12]                  ; r12 = `this`; rax = config_ ptr
    //   eax = [rax + 0x24]           ; config_->deviceIndex
    //   rbx = (eax * 3) << 3         ; deviceIndex * 24
    //   rbx += [rbp-0x440]           ; add outer-vector __begin_
    //
    // The `*24` stride is sizeof(libc++ vector<>) = 24 bytes, indexing
    // a `vector<vector<PlayArgs::WaveAssignment>>` (confirmed by the
    // destructor symbol at @0x162661). The outer vector lives in a
    // stack local at `[rbp-0x440]` and is populated as a side effect
    // somewhere upstream. Resolved: `[rbp-0x440]` IS `playArgs.waveAssignments_`
    // at PlayArgs+0x60 (0x4a0 - 0x60 = 0x440, direct member offset).
    //
    // Then @0x161272-0x16127b: `AsmRegister regZero(0)` — constructs
    // the constant-0 register that will be the addend in the addi
    // emission later.
    //
    // Source-level model: `playArgs.waveAssignments_[config_->deviceIndex]`
    // selects the inner WaveAssignment vector. (deviceIndex named per
    // `awg_compiler_config.hpp:70`.)
    int deviceIndex = config_->deviceIndex;                          // = [config_+0x24]
    AsmRegister regZero(0);                                          // @0x16127b

    // The per-channel WaveAssignment vector is `playArgs.waveAssignments_
    // [deviceIndex]` — one inner vector<WaveAssignment> per device-channel.
    //
    // For Aux: iterates the inner vector and validates each name via
    //          WavetableFront::checkWaveformInitialized.
    // For others: jumps to a different gather path @0x1613ec.

    // Local channel-arg vector, populated below (Phase 7) on the
    // non-Aux path. On the Aux path it remains empty until merging.
    std::vector<EvalResultValue> channelArgs;                        // [rbp-0x90] in binary
    int triggerMask = kPlayTriggerMaskFull;                                        // r13d init @0x1613d6 / @0x161405

    if (subFunc == SubFunc::Aux) {
        // --- 6 (Aux only). Name validation loop (@0x1612b4..0x161319) ---
        // For each WaveAssignment `wa` in the per-channel vector:
        //     std::string name = wa.value.toString();
        //     wavetableFront_->checkWaveformInitialized(name);
        // Stride @0x1612d0 is 0x50 = sizeof(WaveAssignment), and
        // `wa.value` lives at +0x8 (matches WaveAssignment layout
        //  documented in struct_layouts.md).
        //
        // [rbp-0x440] = playArgs.waveAssignments_ (at PlayArgs+0x60);
        // offset 0x4a0 - 0x60 = 0x440 confirms it's the member field
        // populated by PlayArgs::parse() internally.
        for (auto const& wa : playArgs.waveAssignments_[deviceIndex]) {
            wavetableFront_->checkWaveformInitialized(wa.value.value_.toString());
        }

        triggerMask = kPlayTriggerMaskFull;                                        // @0x1613d6
        // r14b=1 @0x1613dc — flag indicating Aux path taken; used in
        // later phases to gate the asmPlay variant.
    } else {
        // --- 7 (non-Aux). Per-channel arg-gather loop (@0x161410..0x1615f0) ---
        //
        // Outer loop iterates waveAssignments_[deviceIndex] with
        // stride 0x50 (sizeof(WaveAssignment) = 80).
        // @0x161439: `lea rbx,[r14+r14*4]; shl rbx,0x4` = index*80
        //            `cmp [rsi+rbx],0x4` checks varType.
        //
        // For each entry whose varType != 4:
        //     channelArgs.push_back(wa.value);
        //
        // After push, computes Value::toString() name for each entry
        // (used downstream to dedupe/coalesce; result used at @0x16146b
        //  to populate something at [rbp-0x1f0..0x200]).
        //
        // Inner loop over wa.bits clears corresponding trigger mask bits.
        // SSE-vectorized path @0x161523..0x16159d processes 8 bits at a
        // time; scalar tail @0x1615d0..0x1615f0 handles remainder.
        // Formula: triggerMask &= ~((1 << (b-1)) << shift)
        // where shift = assignmentIndex * 7 (7-bit slot per assignment).

        auto const& assignments = playArgs.waveAssignments_[deviceIndex];
        for (size_t i = 0; i < assignments.size(); ++i) {           // @0x161410
            auto const& wa = assignments[i];

            if (wa.value.varType_ != VarType_Const) {                           // @0x161439: cmp [rsi+rbx],0x4
                channelArgs.push_back(wa.value);                    // @0x161450
            }

            // Value::toString() for downstream waveform name lookup // @0x16146b
            std::string waveName = wa.value.value_.toString();
            if (waveName.empty()) continue;

            // Inner loop: clear mask bits per wa.bits                // @0x1615d0..0x1615f0
            auto const& bits = wa.bits;
            if (bits.empty()) continue;

            int shift = static_cast<int>(i) * 7;  // i*7 = i*8 - i (lea + sub)
            for (auto it = bits.begin(); it != bits.end(); ++it) {  // @0x161523 (SIMD) / @0x1615d0 (scalar)
                int bit = *it;
                triggerMask &= ~(1 << ((bit - 1) + shift));
            }
        }
    }

    // ================================================================
    // --- 8. getWaveformSampleLength probe (@0x161853..0x161867) ---
    //
    // Disasm:
    //   mov  r15, [rbp-0xa0]            ; r15 = this
    //   mov  rbx, [r15+0x20]            ; rbx = this->wavetableFront_   (CustomFunctions+0x20)
    //   add  rsi, 0x8                   ; rsi = &firstWA->value (WA+0x8 = EvalResultValue's value)
    //   lea  rdi, [rbp-0x200]           ; sret slot for std::string
    //   call Value::toString
    //   ...
    //   mov  rdi, rbx                   ; this = wavetableFront_
    //   call WavetableFront::getWaveformSampleLength
    //   mov  r14d, eax                  ; saved length
    //
    // Source-level: name = playArgs.waveAssignments_[deviceIndex][0]
    //                          .value.value_.toString();
    // [rbp-0x440] IS playArgs.waveAssignments_ (at PlayArgs+0x60);
    // offset 0x4a0 - 0x60 = 0x440. No separate population mechanism.
    auto const& firstWA = playArgs.waveAssignments_[deviceIndex].front();
    uint32_t baseLen = wavetableFront_->getWaveformSampleLength(
        firstWA.value.value_.toString());                            // r14d = WaveformSampleLength

    // ================================================================
    // --- 9. Synthesize zero-fill wave (@0x16189a..0x1619d0) ---
    //
    // Builds the literal string "zeros" (size byte 0xa = 5<<1 SSO):
    //   mov  BYTE  [rbp-0x70], 0xa
    //   mov  DWORD [rbp-0x6f], 0x6f72657a  ; "zero"
    //   mov  WORD  [rbp-0x6b], 0x73        ; "s"
    // Then constructs `Value(Int=Type1, value=baseLen)`:
    //   mov  DWORD [rbp-0x200], 0x1        ; ValueType::Int
    //   mov  DWORD [rbp-0x1f0], r14d       ; storage = baseLen
    // Then std::vector<Value>{ Value(int=baseLen) } via operator new(0x28)
    // and an __uninitialized_allocator_copy_impl into the new buffer,
    // then calls WaveformGenerator::call("zeros", {Value(baseLen)}).
    //
    // Source-level: createDummyWaveform(baseLen) is the documented
    // helper for exactly this idiom (see waveform_generator.hpp:137).
    std::shared_ptr<WaveformFront> combined =
        waveformGen_->createDummyWaveform(static_cast<int>(baseLen));  // @0x161951

    // ================================================================
    // --- : mergeWaveforms --- @0x161bf5..0x161c2b
    //
    // Disasm:
    //   xor  r8d, r8d
    //   cmp  DWORD [rbp-0x94], 0x2          ; subFunc == Aux?
    //   sete r8b                             ; useYSuffix bool
    //   mov  rax, [r12]                      ; r12 = this; rax = config_
    //   mov  rcx, [rbp-0xc8]                 ; runtime-computed offset (0x14 or 0x16)
    //   movsx ecx, WORD [rax+rcx]           ; channelCount = config_->[off]
    //   lea  rdi, [rbp-0x200]                ; sret
    //   lea  rdx, [rbp-0x90]                 ; &channelArgs
    //   lea  r9,  [rbp-0x40]                 ; &cmdName
    //   mov  rsi, r12                        ; this
    //   push 0   ; useFunDescrPath = false
    //   push 0   ; requestedLength = 0
    //   call CustomFunctions::mergeWaveforms
    //
    // The runtime offset [rbp-0xc8] is established earlier (phase 6/7
    // setup): @0x16182a / @0x16186a write 0x16. The Aux-vs-non-Aux
    // branch above writes either 0x14 (channelsPerGroup[0]) or 0x16
    // channelsPerGroup[0] at config+0x14; channelsPerGroup[1] at config+0x16
    // (uint16_t[2]); Aux path uses [0], non-Aux uses [1].
    short channelCount = static_cast<short>(
        config_->channelsPerGroup[subFunc == SubFunc::Aux ? 0 : 1]);
    bool useYSuffix = (subFunc == SubFunc::Aux);
    combined = mergeWaveforms(channelArgs, channelCount, useYSuffix,
                              cmdName, /*requestedLength=*/0,
                              /*useFunDescrPath=*/false);            // @0x161c2b

    // ================================================================
    // --- : loadWaveform + post-merge length check --- @0x161d31..0x161d76
    //
    // Disasm @0x161d31..0x161d4b:
    //   mov  rax, [rbp-0xb0]                ; combined.get() (raw)
    //   mov  rcx, [rbp-0x228]               ; saved waveIndex
    //   cmp  ecx, [rax+0xd0]                ; combined->signal.length_ (uint64_t, read as 32-bit)
    //   jg   1629cb                         ; → error 0x98 (wave-index out of range)
    //   mov  rdi, [r12+0x20]                ; this->wavetableFront_
    //   ...                                 ; copy combined into a local shared_ptr slot
    //   call WavetableFront::loadWaveform
    //
    // [combined+0xD0] = Waveform::signal (+0x80) . Signal::length_ (+0x50)
    // = combined->signal.length(). Confirmed via field offset chain.
    if (combined) {
        if (waveIndex > static_cast<int>(combined->signal.length())) {          // @0x161d3f
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsOffsetAndLength,
                                      cmdName));                                // @0x162a20
        }
        wavetableFront_->loadWaveform(combined);                                // @0x161d76
    }

    // r14b @0x161cfc..0x161d12: bool indicating whether the cmdName
    // string was "small/empty" (no toString result) — used later to
    // gate cleanup branches; not semantically observable at source level.

    // ================================================================
    // --- : addi(indexReg, 0, Immediate(waveIndex)) --- @0x161dc2..0x161e56
    //
    // For varType ∈ {Const(4), Cvar(6)} (the rate-slot type-check at
    // @0x161db5 `cmp ecx, 0x4` after `and ecx, ~2`), the binary takes
    // the inline addi path:
    //
    //   call Value::toInt                  ; eax = rate (cap-checked twice)
    //   cmp  eax, [combined+0xd0]          ; rate >  cap → throw 0x?? (long-form, @0x162b58)
    //   add  eax, [rbp-0x228]              ; rate += waveIndex
    //   cmp  eax, [combined+0xd0]          ; sum  >  cap → throw 0x?? (long-form, @0x162b8c)
    //   call Resources::getRegisterNumber  ; → eax = regNum
    //   call AsmRegister::AsmRegister(eax) ; indexReg
    //   ...
    //   call Value::toInt                  ; rate again (no caching)
    //   call Immediate::Immediate(rate)
    //   call AsmCommands::addi(indexReg, AsmRegister(0), Immediate(rate))
    //
    // Then asmCommands_->asmSetVarPlaceholder(indexReg)
    // then push_back into the local Assembler instance at [rbp-0x1f8]
    //.
    //
    // Dispatch on parseEnd[0].varType_ (the OFFSET arg, not length):
    //   Const(4)/Cvar(6) → addi/SetVarPlaceholder path (below).
    //   Var(2)           → reuse parseEnd[0].reg_ as indexReg; skip
    //                       entirely (no addi, no
    //                      asmSetVarPlaceholder, no push of those
    //                      entries into results->assemblers_). The
    //                      pre-existing register holding `t` is fed
    //                      directly to asmPlay so the resulting Play
    //                      node carries the per-iteration offset and
    //                      length, allowing the optimization pipeline
    //                      to keep the prefetch INSIDE the for-loop.
    //                      Binary: @0x161f5c-0x161f6a copies
    //                      parseEnd[0].reg_ (saved at [rbp-0x328]) into
    //                      the indexReg slot ([rbp-0xe8]) and jumps to
    //                      the shared post-Phase-14 tail @0x162096.
    //   other            → throws @0x162b27 (long-form CustomFunctions
    //                      exception). Modeled by the parseEnd[0] Phase
    //                      4b validator above which restricts varType
    //                      to {Var, Const, Cvar}.
    AsmRegister indexReg(0);  // placeholder; assigned by branch below.
    VarType offsetVarType = parseEnd[0].varType_;
    if (offsetVarType == VarType_Var) {
        // ---  Var-branch --- @0x161f5c..0x161f6a
        // Reuse the AsmRegister already bound to the runtime variable
        // `t` (parseEnd[0].reg_). No addi or placeholder is emitted;
        // no entries are pushed for this phase.
        indexReg = parseEnd[0].reg_;
    } else {
        // ---  Const/Cvar-branch --- @0x161dc2..0x161e56
        int regNum = Resources::getRegisterNumber();                 // @0x161df1
        indexReg = AsmRegister(regNum);
        AsmRegister regZeroForAddi(0);
        Immediate rateImm(parseEnd[0].value_.toInt());  // offset value, NOT rate
        std::vector<AsmList::Asm> addiEntries = asmCommands_->addi(
            indexReg, regZeroForAddi, rateImm);                      // @0x161e56

        // ============================================================
        // --- : asmSetVarPlaceholder(indexReg) --- @0x161ee2
        //
        // Emits the placeholder marker that downstream optimization
        // passes resolve once the wave-index is known. Inserted into
        // the local Assembler instance at [rbp-0x1f8] (an in-progress
        // AsmList::Asm).
        AsmList::Asm placeholderEntry =
            asmCommands_->asmSetVarPlaceholder(indexReg);            // @0x161ee2

        // ============================================================
        // --- : push addi + placeholder into local Assembler --- @0x161e8d..0x161f81
        //
        // The binary inlines std::vector<Asm>::push_back twice (fast
        // path @0x161ed4 for the addi entry, then again for the
        // placeholder entry), with the slow-path emplace_back_slow_path
        // fallback @0x161f79.
        for (auto& entry : addiEntries) {
            results->assemblers_.push_back(std::move(entry));        // @0x161ed4
        }
        results->assemblers_.push_back(std::move(placeholderEntry)); // @0x161f79
    }

    // ================================================================
    // --- : checkOffspecWaveLength --- @0x16210d..0x16214a
    //
    // Disasm:
    //   mov  rax, [r12+0x8]                 ; this->devConst_ (+0x08)
    //   mov  edx, [rax+0x40]                ; devConst_->maxWaveformLength (+0x40)
    //   call CustomFunctions::checkOffspecWaveLength(combined, expected)
    if (combined) {
        int expectedLen = static_cast<int>(devConst_->maxWaveformLength);  // @0x16210d
        checkOffspecWaveLength(combined, expectedLen);               // @0x16214a
    }

    // ================================================================
    // --- : asmPlay --- @0x162246..0x162343
    //
    // Decoded SysV arg mapping (21b-followup-3 Group C):
    //   rdi = sret (AsmList::Asm return)
    //   rsi = asmCommands_ (this)
    //   rdx = &waveforms vector
    //   ecx = 0                               → deviceIndex = 0
    //   r8b = (subFunc == Aux)                → isHold
    //   r9b = (subFunc == DigTrigger)         → fourChannel
    //   stack[0] = false                      → hold
    //   stack[1] = rate                       → rate
    //   stack[2] = triggerMask (r13=0x3fff)   → suppress
    //   stack[3] = (subFunc == Aux)           → is4Channel
    //   stack[4] = indexReg                   → lengthReg
    //   stack[5] = waveIndex                  → length
    //   stack[6] = AsmRegister(-1)            → reg2
    //   stack[7] = 0                          → trigger
    //
    // unknowns.md #121 fully resolved: fourChannel=Now (GDB confirmed),
    // is4Channel=Aux (GDB confirmed), isHold=false (empirically verified).
    AsmRegister regInvalid(-1);                                          // @0x1622f2
    std::vector<std::shared_ptr<WaveformFront>> waveforms;
    if (combined) {
        waveforms.push_back(combined);                               // @0x1622bd loop
    }
    AsmList::Asm playEntry = asmCommands_->asmPlay(
        std::move(waveforms),
        /*deviceIndex=*/0,
        /*isHold=*/false,  // r8b @0x162314: movzbl -0x78(%rbp),%r8d.
                           // -0x78(%rbp) is overwritten at 0x161db8 by `mov [rbp-0x78],r14`,
                           // where r14's low byte holds an unrelated `sete` flag computed at
                           // 0x161cfc/0x161d12 (= "channelArgs[0] toString empty"), and is 0
                           // via xor at 0x161d06 when channelArgs.size()<2. Empirically (GDB
                           // confirmed across both Aux and non-Aux paths) the low byte read
                           // here is always 0 — channelArgs[0] is the wave name just merged,
                           // never empty; and on the Aux path channelArgs stays empty so
                           // size<2 forces r14=0. The semantic value is therefore false.
                           // The earlier writes at 0x1612aa (=rbx, vector ptr) and 0x1613df
                           // (=NULL) are dead — both paths unconditionally re-write [rbp-0x78]
                           // at 0x161db8 before the read.
        /*fourChannel=*/(subFunc == SubFunc::Now),   // r9b: GDB confirmed cmp $0x3 (Now), not DigTrigger
        /*hold=*/false,                                              // stack[0]
        /*rate=*/rate,                                               // stack[1]
        /*suppress=*/static_cast<unsigned int>(triggerMask),         // stack[2]: r13
        /*is4Channel=*/(subFunc == SubFunc::Aux),                    // stack[3]: GDB confirmed cmp $0x2 (Aux)
        indexReg,                                                    // stack[4]
        /*length=*/waveIndex,                                        // stack[5]
        regInvalid,                                                      // stack[6]
        /*trigger=*/0u);                                             // stack[7]: @0x162343

    // ================================================================
    // --- : push asmPlay entry into results->assemblers_ --- @0x162462..0x162511
    //
    // Disasm @0x162506: lea rdi, [r15+0x18] (vector<Asm> &) followed
    // by emplace_back_slow_path. r15 = [r13+0] where r13 is the saved
    // first-arg slot — i.e. results->assemblers_.
    //
    // Also: chain playEntry.node into results->node_. Without this,
    // the Play node is never linked into the tree walked by
    // Prefetch::prepareTree, causing wavesPerDev[0] (the wave name set
    // by asmPlay) to never reach collectUsedWaves → empty .waveforms /
    // missing .wf_* sections.  Mirrors the same chaining done by the
    // simple-play path at @0x160438..0x160468.
    if (results->node_) {
        results->node_->next = playEntry.node;
    } else {
        results->node_ = playEntry.node;
    }
    results->assemblers_.push_back(std::move(playEntry));            // @0x162511

    // ================================================================
    // --- : cleanup + return --- @0x16254c..0x16271b
    //
    // The remaining 0x1cf bytes are:
    //   * Assembler dtor for the local @[rbp-0x1f8]                   @0x162553
    //   * Cleanup of waveforms vector + temporary strings             @0x162558..0x162707
    //   * Cleanup of [rbp-0x440] outer WaveAssignment vector          @0x16265a
    //     (calls vector<vector<WaveAssignment>>::__base_destruct_at_end)
    //   * Cleanup of cmdName SSO string @[rbp-0x40]                   @0x1626f0
    //   * `mov rax, r13; ...; ret` returning the EvalResults shared_ptr
    //
    // The exception path tail @0x162735..0x162cf5 contains:
    //   * 0x3d: "wrong number of arguments" formatter @0x16283a       (cmdName, expected, got)
    //   * 0x98: invalid arg type (first/second) @0x162976,0x16299d    (cmdName)
    //   * 0xa0: rate-too-low formatter @0x1628e6                      (cmdName)
    //   * 0x9a: rate type must be const @0x16293c                     (cmdName)
    //   * std::__throw_bad_function_call @0x162971
    //   * std::vector::__throw_length_error path @0x162aa3
    //   * Two more CustomFunctionsException ctor sites (longest-form
    //     length-error formatter using 0x6db... reciprocal) @0x162a3a/0x162a8c
    //   These are wired into the throws above ().

    return results;                                                  // @0x162707
}

// ============================================================================
// writeToNode @0x164550..0x16b740 — 0x71f0 bytes (29KB)
//
// Returns shared_ptr<EvalResults>. Walks the node-write path string through
// 4 successive boost::regex matches, validating the addressed device/channel/
// oscillator against the runtime configuration, and emits AsmCommands::suser
// / AsmCommands::addi instructions into the result's assemblers_ list.
//
// Reconstruction sub-phases:
//   21b.1 (this commit) — function skeleton, regex statics, setup, Block A
//                         (absDevRegex), trailing lookupNode() call.
//   21b.2 — Blocks B (awgNodeRegex) + C (sineNodeRegex).
//   21b.3 — Block D part 1 (oscselNodeRegex bulk dispatch, first ~10KB).
//   21b.4 — Block D part 2 (remaining oscsel dispatch).
//   21b.5 — Blocks E/F/G + setInt / setDouble forwarder wiring.
//
// Static regex objects (4):
//   absDevRegex     @ b84748   (guard byte at b84758) — absolute device path
//                                e.g. "/dev1234/awgs/0/..." → captures [device-id, rest]
//   awgNodeRegex    @ b84760   (guard byte at b84770) — AWG channel-relative node
//   sineNodeRegex   @ b84778   (guard byte at b84788) — sine/oscillator node
//   oscselNodeRegex @ b84790   (guard byte at b847a0) — osc-select node (largest)
//
// Pattern strings live in .rodata; not yet decoded. They are constructed lazily
// at the cold paths @0x169ea5 / @0x169efd / @0x169f54 / @0x169fab on the first
// call (Block F).
//
// SysV ABI: rdi=sret, rsi=this, rdx=&path, rcx=&val, r8=&type, r9=res-shared_ptr.
// EvalResultValue is non-trivially-copyable (contains string in Value), so it is
// passed by hidden reference per Itanium C++ ABI even though declared by-value.
// ============================================================================

namespace {
// Static regex objects mirror the four .bss slots in the binary. The ctors run
// once on first use (libc++ guard semantics ≡ the binary's `cmpxchg` on the
// guard byte); thereafter the cached objects are reused.
//
// Pattern strings extracted from .rodata:
//   absDevRegex    @0x9008eb: "/([0-9]+)/([\w/]+[\w])"    flags=0 (perl)
//   awgNodeRegex   @0x900902: "awgs/([0-9]+)/.*"          flags=0x100000 (save_subexpression_location)
//   sineNodeRegex  @0x900913: "sines/([0-9]+)/.*"         flags=0x100000
//   oscselNodeRegex@0x900925: "sines/[0-9]+/oscselect"    flags=0x100000
//
// Cold-path constructors: @0x169ea5, @0x169efd, @0x169f54, @0x169fab.
// BSS slots: b84748, b84760, b84778, b84790.
boost::regex const absDevRegex("/([0-9]+)/([\\w/]+[\\w])");
boost::regex const awgNodeRegex("awgs/([0-9]+)/.*");
boost::regex const sineNodeRegex("sines/([0-9]+)/.*");
boost::regex const oscselNodeRegex("sines/[0-9]+/oscselect");
}  // namespace

std::shared_ptr<EvalResults> CustomFunctions::writeToNode(
        EvalResultValue path, EvalResultValue val,
        EvalResultValue type, std::shared_ptr<Resources> res) {  // @0x164550
    // ----------------------------------------------------------------
    // Setup @0x164550..0x164608 (~180B)
    //
    // Allocate a shared EvalResults (size 0x98 incl. control-block header).
    // The binary uses a single `__shared_ptr_emplace<EvalResults>` allocation
    // (vtable @b03d00) — std::make_shared<EvalResults>() is the source-level
    // equivalent. EvalResults body is zero-initialised (8x movups xmm0).
    //
    // Then checks path.varSubType_ at +0x04; if varSubType_ == 2, returns
    // the empty results immediately (je 0x169df4 → epilogue).
    // ----------------------------------------------------------------
    auto results = std::make_shared<EvalResults>();   // @0x16457f..164584 + zero-init

    // @0x1645da: cmp DWORD [r15+0x04], 0x2; je 0x169df4 (epilogue).
    // Binary: varSubType_==2 means "nothing to write" — early exit.
    if (path.varSubType_ == VarSubType_FunctionArg) {
        return results;                                // @0x169df4
    }

    // @0x1645e9: build a working std::string from path.value_.toString().
    // The result lives in the local buffer that subsequent regex_match calls
    // consume (libc++ short-string deref pattern shr-by-1 / test-bit-0).
    std::string pathStr = path.value_.toString();      // @0x1645e9..1645f8

    // Local match_results buffer is reused across all four regex matches
    // (binary keeps it at [rbp-0x220]).
    boost::cmatch matches;

    // ----------------------------------------------------------------
    // Block A: absDevRegex @0x16460e..~0x164800 (~500B)
    //
    // Pattern: "/([0-9]+)/([\w/]+[\w])"
    // Match absolute device path "/<deviceId>/<rest>". On match:
    //   1. Extract capture 1 (device-id), parse via stoul, validate against
    //      config_->deviceIndex (AWGCompilerConfig+0x24).
    //   2. Extract capture 2 (rest of path) and replace pathStr.
    //
    // If absDevRegex does NOT match, control falls through with pathStr
    // unchanged; the next regex (awgNodeRegex / Block B) is tried.
    //
    // The binary fetches sub_match at offsets +0x48 (capture 1) and +0x60
    // (capture 2), guarded by `cmp eax,4` / `cmp eax,5` against array size.
    // Boost cmatch indices: [0]=full, [1]=device-id, [2]=rest.
    // ----------------------------------------------------------------
    if (boost::regex_match(pathStr.c_str(), pathStr.c_str() + pathStr.size(),
                           matches, absDevRegex))
    {
        // @0x1646f2..164714: extract numeric device-id capture, parse with stoul.
        std::string devIdStr = matches[1].str();      // sub_match[+0x48]
        unsigned long requestedDev = std::stoul(devIdStr, nullptr, 10);

        // @0x164734: validate against config_->deviceIndex (= [config_+0x24]).
        // r14 = this; [r14] = config_; [config_+0x24] = deviceIndex.
        if (config_->deviceIndex != static_cast<int32_t>(requestedDev)) {
            // @0x16473b: jne 0x169d83 → cleanup and silent return.
            // Binary: device-mismatch returns empty results, does NOT throw.
            return results;
        }

        // @0x1647a2..1647ec: extract rest-of-path capture, replace pathStr with it.
        // The binary moves sub_match[5] (offset +0x60) into the working buffer
        // via a 16-byte xmm copy (libc++ string SSO move).
        pathStr = matches[2].str();                   // sub_match[+0x60]
    }
    // @0x1647ed: lookupNode(pathStr) — performed unconditionally after Block A,
    // whether absDevRegex matched or not. Result currently unused at this scope
    // (consumed by later blocks).
    NodeMapItem node = lookupNode(pathStr);           // @0x1647ed..164802

    // ----------------------------------------------------------------
    // Block B: awgNodeRegex @0x164803..~0x16491e (~290B)
    //
    // Match `awgs/<channel>/...` style node-relative path. On match:
    //   1. Extract the numeric capture (channel/sine/osc index) and parse
    //      via stoul into a signed int (r13d).
    //   2. Normalise index against config_->numChannelGroups
    //      (= [config+0x1c]; values 1, 2, or 4):
    //        - numChannelGroups == 2 → r13 /= 2  (signed shift via add+sar)
    //        - numChannelGroups == 4 → r13 /= 4  (signed shift via lea+cmovns+sar)
    //        - otherwise (1)         → r13 unchanged
    //      The arithmetic encoding splits across @0x1648f9..164904 and
    //      @0x1649e1..1649ef.
    //   3. Validate `r13 == config_->awgIndex` (= [config+0x20]); on
    //      mismatch jump to error tail @0x1649ff (formats with the
    //      device-id from [config+0x24]).
    //
    // On miss, falls through to Block C without modifying any state.
    // ----------------------------------------------------------------
    if (boost::regex_match(pathStr.c_str(), pathStr.c_str() + pathStr.size(),
                           matches, awgNodeRegex))
    {
        std::string chIdxStr = matches[1].str();              // sub_match[+0x48]
        int channelIdx = static_cast<int>(std::stoul(chIdxStr, nullptr, 10));

        int numGroups = config_->numChannelGroups;            // [config+0x1c]
        if (numGroups == 2) {
            channelIdx = channelIdx / 2;                      // @0x1648f9..164904
        } else if (numGroups == 4) {
            // Signed-divide-by-4 with cmovns rounding (matches @0x1649e1).
            channelIdx = channelIdx / 4;
        }
        // else numGroups == 1: keep channelIdx as-is.

        if (channelIdx != config_->awgIndex) {              // [config+0x20]
            // @0x16a06b: ErrorMessages::format<int, string>(0x85, deviceIndex, pathStr)
            // → CustomFunctionsValueException (with lineNumber=0).
            throw CustomFunctionsValueException(
                ErrorMessages::format(SequencerCantDrive,
                                      config_->deviceIndex, pathStr),
                /*lineNumber=*/0);
        }
    }

    // ----------------------------------------------------------------
    // Block C: sineNodeRegex @0x16491e..0x164ae4 (~460B)
    //
    // Match `sines/<oscIdx>/...` (oscillator-relative node). On match:
    //   1. Extract numeric capture, stoul → signed int (r13d).
    //   2. Map oscillator index → channel index. The mapping depends on
    //      config_->numChannelGroups via @0x164aad..164ad8:
    //         - numChannelGroups == 2 → idx /= 4   (esi=4 from sete+lea)
    //         - otherwise             → idx /= 2   (esi=2)
    //         - if numChannelGroups == 4, divide AGAIN by 4 (i.e. /8 total)
    //      In effect: oscIdx → (oscIdx / oscPerChannel), where oscPerChannel
    //      depends on the grouping: 2 oscs/ch for ungrouped, 4 oscs/ch when
    //      numGroups==2, 8 oscs/ch when numGroups==4.
    //   3. Validate against `config_->awgIndex` (same channel cap as
    //      Block B); mismatch → error @0x16a006.
    //
    // On miss, falls through to Block D (oscselNodeRegex).
    // ----------------------------------------------------------------
    if (boost::regex_match(pathStr.c_str(), pathStr.c_str() + pathStr.size(),
                           matches, sineNodeRegex))
    {
        std::string oscIdxStr = matches[1].str();             // sub_match[+0x48]
        int oscIdx = static_cast<int>(std::stoul(oscIdxStr, nullptr, 10));

        int numGroups = config_->numChannelGroups;            // [config+0x1c]
        // Primary divide: 4 if numGroups==2, else 2.
        int divisor = (numGroups == 2) ? 4 : 2;
        oscIdx = oscIdx / divisor;
        // Secondary: if numGroups==4, divide by 4 again (ceiling-toward-zero).
        if (numGroups == 4) {
            oscIdx = oscIdx / 4;
        }

        if (oscIdx != config_->awgIndex) {                   // [config+0x20]
            // @0x16a0d9: ErrorMessages::format<int, string>(0x85, computed_idx, pathStr)
            // → CustomFunctionsValueException (with lineNumber=0).
            // Binary: r14d = r13d*r12d + r14d, a combined index.
            throw CustomFunctionsValueException(
                ErrorMessages::format(SequencerCantDrive,
                                      config_->deviceIndex, pathStr),
                /*lineNumber=*/0);
        }
    }

    // ----------------------------------------------------------------
    // Block D: oscselNodeRegex @0x164b19..~0x169d00 (~21KB — the bulk
    // of writeToNode's code size).
    //
    // Pattern: "sines/[0-9]+/oscselect" (ZERO captures — boolean match only).
    // Despite the name "oscselNodeRegex", this matches specifically the
    // oscillator-select sub-path under sines/ and dispatches into
    // per-NodeMapItem-typeIdx codegen.
    //
    // The dispatch is a 3-way structure:
    //
    //   1. Match path against oscselNodeRegex; if no match, jump to error
    //      tail @0x169e0d.
    //   2. Insert literal "MF" tag string into this->usedFeatures_
    //      (set<string> at +0x1C8) — unconditional. The compiler inlined
    //      a __tree __emplace_unique_key_args walk @0x164b51..164bee.
    //      "MF" likely stands for "Multi-Frequency" — meaningful only to
    //      whatever consumes usedFeatures_.
    //   3. addNodeAccess(node, accessMode), where accessMode is read as
    //      a byte from NodeMapItem+0x10 — the SAME slot we model as the
    //      `hasFast` bool.  GDB tracing across the full test suite (51
    //      lookupNode hits, see notes/incidental_findings.md IF-112)
    //      confirms the byte only ever holds 0 or 1, so `bool` typing
    //      is correct.  The cast `AccessMode(hasFast)` therefore yields
    //      Soft(0) when no fast address exists and Direct(1) when one
    //      does — i.e. hasFast doubles as the implicit access-mode
    //      selector for the playback dispatch path.  Custom(2) is
    //      passed only by explicit `static_cast<AccessMode>(2)` from
    //      other call sites (setDIO/setTrigger paths) and never
    //      derives from hasFast.
    //   4. reg = res->getRegisterNumber(); destReg = AsmRegister(reg);
    //      Allocate a local AsmList for instruction accumulation.
    //   5. THREE-WAY DISPATCH on the address resolution:
    //        (A) hasFast == true  @0x164c50..0x164c7d:
    //              addr = node.fastAddress();
    //              dispatch jump table @958f68 (cases 0..5)
    //        (B) hasFast == false AND
    //            dynamic_cast<DirectAddrNodeMapData*>(node.data) succeeds
    //            @0x164cb9..0x164cff:
    //              addr = ((DirectAddrNodeMapData*)node.data)->addr_;
    //              dispatch jump table @958f50 (cases 0..5)
    //        (C) hasFast == false AND dynamic_cast fails (or node.data == nullptr)
    //            @0x164d92..0x164de2:
    //              auto it = nodeIndexMap_.find(node);
    //              if (it == end()) throw  @0x16a045;
    //              addr = it->second;
    //              dispatch jump table @958f50 (cases 0..5) — SAME jt as (B)
    //   6. Path (B) and (C) bounds-check typeIdx ≤ 5; if not, jump to
    //      warning-log path @0x164d05 which writes a boost::log record
    //      and then early-returns based on this->config_->something == 2
    //      (returns the partly-built results without emitting any asm).
    //   7. After per-case emission, append local AsmList to
    //      results->assemblers_.
    //
    // Per-case asm-emission bodies remain incomplete in Block D part 2
    // (see markers).
    //
    // Cumulative asm-emit call counts in Block D: 53 suser, 44 addi,
    // 25 AsmList::append, 48 AsmRegister ctor, 44 Immediate ctor,
    // 48 Assembler dtor.
    // ----------------------------------------------------------------
    if (boost::regex_match(pathStr.c_str(), pathStr.c_str() + pathStr.size(),
                           matches, oscselNodeRegex))
    {
        // @0x164b3a..164b4a: construct inline SSO short-string "MF"
        // (header byte = 4 = 2<<1, then bytes 'M','F','\0').
        std::string const tagMF("MF");

        // @0x164b51..164bee: scan/insert into this->usedFeatures_.
        // The compiler inlined the __tree __emplace_unique_key_args walk;
        // source-level this is just `usedFeatures_.insert("MF")`.
        // The insert is UNCONDITIONAL — runs every time the oscselNodeRegex
        // matches.
        // Consumer: AWGCompilerImpl::getJsonVersion (awg_compiler.cpp:1182) reads
        // usedFeatures_ and emits it as "required_options" JSON array. See unknowns.md #122.
        usedFeatures_.insert(tagMF);
    }
    // Binary: addNodeAccess, register allocation, address resolution, and
    // per-typeIdx dispatch are UNCONDITIONAL — @0x164b34 jumps over only
    // the "MF" insert (je 0x164c0e), landing directly at addNodeAccess.
    {
        // accessMode is the byte at NodeMapItem+0x10 (NodeMapItem::hasFast).
        // NOTE: IF-112 — Soft(0) / Direct(1) only (confirmed).
        AccessMode accessMode = static_cast<AccessMode>(node.hasFast);
        addNodeAccess(node, accessMode);

        // @0x164c24: getRegisterNumber() → AsmRegister(reg)
        int regNum = res->getRegisterNumber();
        AsmRegister destReg(regNum);
        AsmList localList;  // @0x164c34..164c43: zero-init 24B

        // r12 = &val (saved at @0x164c4a from [rbp-0x50]).
        // val is read in fast/slow paths via [r12+0x..]; primary fields:
        //   [r12+0x00] = val.varType_  (VarType — checked against 2 = Var)
        //   [r12+0x30] = val.reg_      (AsmRegister)
        //   val+0x08   = val.value_   (Value, used via toDouble/toInt/etc.)
        EvalResultValue const& valRef = val;

        // ------------------------------------------------------------
        // Resolve `addr` via one of three paths, then dispatch on typeIdx.
        // Paths (B) and (C) share the same per-typeIdx code (jt @958f50);
        // path (A) uses jt @958f68. We flatten this in source by computing
        // `addr` first and selecting which switch to enter.
        // ------------------------------------------------------------
        bool useFastJt = false;   // true → jt @958f68 (path A)
        uint32_t addr = 0;
        bool emitWarnAndReturn = false;  // path (C) miss OR typeIdx>5

        if (node.hasFast) {
            // --- Path (A) @0x164c50..164c5c ---
            addr = node.fastAddress();
            useFastJt = true;
            // typeIdx bounds: jt @958f68 covers 0..5; default at @0x165407
            // is a warning-log path (same "Unknown NodeMapType with code N"
            // message as the slow-path default at @0x164d05).
            if (static_cast<uint32_t>(node.typeIdx) > 5u) {
                emitWarnAndReturn = true;
            }
        } else if (node.data != nullptr) {
            // --- Path (B): try DirectAddrNodeMapData ---
            // @0x164cc9..164cdf: __dynamic_cast(node.data, &NodeMapData_ti,
            //                                    &DirectAddrNodeMapData_ti, 0)
            auto* direct = dynamic_cast<DirectAddrNodeMapData*>(node.data);
            if (direct != nullptr) {
                // @0x164ce7: addr = direct->addr_  (read from base+0x8)
                addr = direct->addr_;
                useFastJt = false;
                if (static_cast<uint32_t>(node.typeIdx) > 5u) {
                    emitWarnAndReturn = true;  // → @0x164d05
                }
            } else {
                // --- Path (C): nodeIndexMap_.find ---
                auto mapIt = nodeIndexMap_.find(node);
                if (mapIt == nodeIndexMap_.end()) {
                    // @0x16a045: unordered_map::at throws std::out_of_range
                    // ("unordered_map::at: key not found") — raw stdlib
                    // exception, not ErrorMessages::format.
                    throw std::out_of_range("unordered_map::at: key not found");
                }
                addr = mapIt->second;
                useFastJt = false;
                if (static_cast<uint32_t>(node.typeIdx) > 5u) {
                    emitWarnAndReturn = true;
                }
            }
        } else {
            // node.data == nullptr → straight to nodeIndexMap_ (skip dyncast).
            auto mapIt = nodeIndexMap_.find(node);
            if (mapIt == nodeIndexMap_.end()) {
                throw CustomFunctionsException(
                    "writeToNode: node not registered in nodeIndexMap_");
            }
            addr = mapIt->second;
            useFastJt = false;
            if (static_cast<uint32_t>(node.typeIdx) > 5u) {
                emitWarnAndReturn = true;
            }
        }

        if (emitWarnAndReturn) {
            // @0x164d05..0x164d81: boost::log warning record with the
            // unsupported typeIdx, then early-return.
            // Log message: "Unknown NodeMapType with code " + typeIdx (int)
            // (string at .rodata 0x900949, length 0x1e=30 chars).
            // Severity = 1 (warning). Falls through to return.
            return results;
        }

        if (useFastJt) {
            // --- Per-case codegen: jump table @958f68 (path A) ---
            // All cases end with addi(destReg, R0, Immediate(addr)) appended.
            // Cases 0-1 require varType==2 (val is a register); cases 2-5
            // require varType!=2 (val is a scalar — extracted via toDouble).
            // Cases 0-1 use suser(val.reg_, 0x17) directly. Cases 2-5
            // compute an Immediate, emit addi(destReg, R0, Immediate(...)),
            // then suser(destReg, 0x17).
            switch (node.typeIdx) {
                case NodeTypeIdx::IntegerPassthrough: {
                    // @0x164c7f: register passthrough (single suser).
                    if (valRef.varType_ != VarType_Var) {
                        // slow-arm @0x165b17: scalar value → convert to int,
                        // warn if not exact integer, then emit as Immediate.
                        double d = valRef.value_.toDouble();          // @0x165b1e
                        int intVal = valRef.value_.toInt();           // @0x165b2b
                        if (!floatEqual(d, static_cast<double>(intVal))) {
                            // @0x165b46: warning 0x80 — value truncated.
                            std::string valStr = valRef.value_.toString();
                            std::string msg = ErrorMessages::format(
                                NodePrecisionLoss, valStr, "integer");
                            warningCallback_(msg);
                        }
                        {   // addi(destReg, R0, Immediate(intVal))   // @0x165c17
                            auto vec = asmCommands_->addi(
                                destReg, AsmRegister(0), Immediate(intVal));
                            localList.entries.insert(localList.entries.end(),
                                vec.begin(), vec.end());
                        }
                        // suser(destReg, 0x17)                   // @0x165c7b
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeDirect));
                    } else {
                        // fast-arm: varType==2 (register).
                        appendSuser(localList, asmCommands_, valRef.reg_, detail::AddressImpl<uint32_t>(kSuserNodeDirect));  // @0x164c8a
                    }
                    break;
                }
                case NodeTypeIdx::SinePair: {
                    // @0x165263: sine pair — two suser calls (0x17 + 0x19).
                    if (valRef.varType_ != VarType_Var) {
                        // slow-arm @0x165d97: scalar → split double into
                        // low32/high32 words, emit addi+suser for each.
                        double d = valRef.value_.toDouble();          // @0x165d9e
                        double d2 = valRef.value_.toDouble();         // @0x165dab (second call)
                        int64_t truncated = static_cast<int64_t>(d2); // cvttsd2si
                        if (!floatEqual(d, static_cast<double>(truncated))) {
                            // @0x165dbf: warning 0x80 — value truncated.
                            std::string valStr = valRef.value_.toString();
                            std::string msg = ErrorMessages::format(
                                NodePrecisionLoss, valStr, "integer");
                            warningCallback_(msg);
                        }
                        double valD = valRef.value_.toDouble();       // @0x165e57
                        int64_t rawBits;
                        std::memcpy(&rawBits, &valD, sizeof(rawBits));
                        int low32 = static_cast<int>(rawBits);
                        int high32 = static_cast<int>(rawBits >> 32);
                        // addi(destReg, R0, Immediate(low32))        // @0x165eb6
                        {
                            auto vec = asmCommands_->addi(
                                destReg, AsmRegister(0), Immediate(low32));
                            localList.entries.insert(localList.entries.end(),
                                vec.begin(), vec.end());
                        }
                        // suser(destReg, 0x17)                       // @0x165f1a
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeDirect));
                        // addi(destReg, R0, Immediate(high32))       // @0x165f7d
                        {
                            auto vec = asmCommands_->addi(
                                destReg, AsmRegister(0), Immediate(high32));
                            localList.entries.insert(localList.entries.end(),
                                vec.begin(), vec.end());
                        }
                        // suser(destReg, 0x19)                       // @0x165fe5
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeDirectB));
                    } else {
                        // fast-arm: varType==2 (register).
                        // suser(val.reg_, 0x17)                   // @0x16526e
                        appendSuser(localList, asmCommands_, valRef.reg_, detail::AddressImpl<uint32_t>(kSuserNodeDirect));
                        // suser(R0, 0x19)                         // @0x1652a4
                        appendSuser(localList, asmCommands_, AsmRegister(0), detail::AddressImpl<uint32_t>(kSuserNodeDirectB));
                    }
                    break;
                }
                case NodeTypeIdx::FloatBits: {
                    // @0x165013: float-bits — toDouble → cvtsd2ss → Imm(float bits).
                    if (valRef.varType_ == VarType_Var) {
                        // @0x16a191: varType==2 is invalid for this case — throw.
                        std::string valStr = valRef.value_.toString();
                        std::string msg = ErrorMessages::format(
                            NodeOnlySetDouble, valStr);
                        throw CustomFunctionsException(msg);
                    }
                    double d = valRef.value_.toDouble();           // @0x165025
                    float f = static_cast<float>(d);               // cvtsd2ss
                    int fbits;
                    std::memcpy(&fbits, &f, sizeof(fbits));
                    {
                        auto vec = asmCommands_->addi(             // @0x165082
                            destReg, AsmRegister(0), Immediate(fbits));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // suser(destReg, 0x17)                    // @0x1672f5
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeDirect));
                    break;
                }
                case NodeTypeIdx::RawDoubleLow32: {
                    // @0x165137: raw double low-32 bits.
                    if (valRef.varType_ == VarType_Var) {
                        // @0x16a1f5: varType==2 is invalid — throw.
                        std::string valStr = valRef.value_.toString();
                        std::string msg = ErrorMessages::format(
                            NodeOnlySetDouble, valStr);
                        throw CustomFunctionsException(msg);
                    }
                    double d = valRef.value_.toDouble();           // @0x165149
                    int64_t rawBits;
                    std::memcpy(&rawBits, &d, sizeof(rawBits));
                    int low32 = static_cast<int>(rawBits);         // truncate
                    {
                        auto vec = asmCommands_->addi(             // @0x165182
                            destReg, AsmRegister(0), Immediate(low32));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // suser(destReg, 0x17)                    // @0x167509
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeDirect));
                    break;
                }
                case NodeTypeIdx::Frequency: {
                    // @0x164ee7: frequency conversion.
                    if (valRef.varType_ == VarType_Var) {
                        // @0x16a12d: varType==2 is invalid for frequency — throw.
                        std::string valStr = valRef.value_.toString();
                        std::string msg = ErrorMessages::format(
                            FreqNodeConstOnly, valStr);
                        throw CustomFunctionsException(msg);
                    }
                    double d = valRef.value_.toDouble();           // @0x164ef9
                    double clk = getSampleClock();                 // @0x164f06
                    int64_t freq = NodeMap::toFrequency(d, clk);   // @0x164f13
                    int freqInt = static_cast<int>(freq);
                    {
                        auto vec = asmCommands_->addi(             // @0x164f5d
                            destReg, AsmRegister(0), Immediate(freqInt));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // suser(destReg, 0x17)                    // @0x16793e
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeDirect));
                    break;
                }
                case NodeTypeIdx::Phase: {
                    // @0x1652e6: phase conversion.
                    if (valRef.varType_ == VarType_Var) {
                        // @0x16a259: varType==2 is invalid for phase — throw.
                        std::string valStr = valRef.value_.toString();
                        std::string msg = ErrorMessages::format(
                            PhaseNodeConstOnly, valStr);
                        throw CustomFunctionsException(msg);
                    }
                    double d = valRef.value_.toDouble();           // @0x1652f8
                    float f = static_cast<float>(d);               // cvtsd2ss
                    int phaseInt = NodeMap::toPhase(f);            // @0x165301
                    {
                        auto vec = asmCommands_->addi(             // @0x165363
                            destReg, AsmRegister(0), Immediate(phaseInt));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // suser(destReg, 0x17)                    // @0x16772a
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeDirect));
                    break;
                }
            }
            // --- Per-case post-tails (path A) ---
            // Each case has its own tail sequence after the case body.
            // Only cases 0,5 use addi(addr)+suser(0x16) ("commit").
            // Cases 2 has no post-tail.
            // Cases 3,4 emit additional 64-bit split components.
            // Case 1 (sine pair) has an extensive multi-field tail.
            // See notes/writeToNode_block_d_protocol.md §Per-case post-tails.
            switch (node.typeIdx) {
                case NodeTypeIdx::IntegerPassthrough: {
                    // @0x165c90: addi(destReg, R0, Imm(addr)) — shared common tail
                    {
                        auto vec = asmCommands_->addi(
                            destReg, AsmRegister(0), Immediate(static_cast<int>(addr)));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // @0x1685d5: suser(destReg, 0x16) — commit
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeCommit));
                    // Hirzel-only: addi(destReg, R0, 5) + suser(destReg, 0x69) — wait 5 cycles after commit.
                    // Binary: not present on Cervino (UHF) devices.
                    if (devConst_->seqClockDivider != 0) {
                        {
                            auto vec = asmCommands_->addi(
                                destReg, AsmRegister(0), Immediate(5));
                            localList.entries.insert(localList.entries.end(),
                                vec.begin(), vec.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserWaitCycles));
                    }
                    break;
                }
                case NodeTypeIdx::SinePair: {
                    // Sine pair extended tail — writes I/Q + addr + tag + freq + phase + commit.
                    // @0x166045: addi(destReg, R0, Imm(addr))
                    {
                        auto vec = asmCommands_->addi(
                            destReg, AsmRegister(0), Immediate(static_cast<int>(addr)));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // @0x166148: addi(destReg, R0, Imm(3)) — sine node tag
                    {
                        auto vec = asmCommands_->addi(
                            destReg, AsmRegister(0), Immediate(3));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // @0x1661ac: suser(destReg, 0x10) — user-store low
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeTag));
                    // @0x16867e: suser(destReg, 0x11) — metadata component 1
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeAddr));
                    // @0x168740..1687a0: toDouble→getSampleClock→toFrequency→addi(freq)
                    {
                        double freqD = valRef.value_.toDouble();          // @0x168747
                        double clkF  = getSampleClock();                  // @0x168754
                        int64_t freqVal = NodeMap::toFrequency(freqD, clkF); // @0x16875a
                        auto vec = asmCommands_->addi(                    // @0x1687a0
                            destReg, AsmRegister(0),
                            Immediate(static_cast<int>(freqVal)));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // @0x1688b6: suser(destReg, 0x11) — metadata component 2
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeAddr));
                    // @0x168978..1689c8: toDouble→cvtsd2ss→toPhase→addi(phase)
                    {
                        double phaseD = valRef.value_.toDouble();         // @0x168978
                        float phaseF  = static_cast<float>(phaseD);       // cvtsd2ss
                        int phaseVal  = NodeMap::toPhase(phaseF);         // @0x168981
                        auto vec = asmCommands_->addi(                    // @0x1689c8
                            destReg, AsmRegister(0), Immediate(phaseVal));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // @0x168aea: suser(destReg, 0x16) — commit
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeCommit));
                    break;
                }
                case NodeTypeIdx::FloatBits:
                    // No post-tail. The float-bits write completes with suser(0x17).
                    break;
                case NodeTypeIdx::RawDoubleLow32: {
                    // @0x167610: addi(destReg, R0, Imm(high32)) — upper 32 bits of raw double.
                    // Recompute high32 from the same double (the body computed low32).
                    {
                        double dHi = valRef.value_.toDouble();
                        int64_t rawHi;
                        std::memcpy(&rawHi, &dHi, sizeof(rawHi));
                        int high32 = static_cast<int>(rawHi >> 32);
                        auto vec = asmCommands_->addi(
                            destReg, AsmRegister(0), Immediate(high32));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // @0x168b8e: suser(destReg, 0x19) — direct-write secondary
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeDirectB));
                    break;
                }
                case NodeTypeIdx::Frequency: {
                    // @0x167a3e: addi(destReg, R0, Imm(freqHigh32)) — upper 32 bits of frequency.
                    // Recompute from the same toFrequency result (body computed freqLow32).
                    {
                        double dF2  = valRef.value_.toDouble();           // re-read
                        double clk2 = getSampleClock();
                        int64_t freq2 = NodeMap::toFrequency(dF2, clk2);
                        int freqHigh32 = static_cast<int>(freq2 >> 32);
                        auto vec = asmCommands_->addi(
                            destReg, AsmRegister(0), Immediate(freqHigh32));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // @0x168e4e: suser(destReg, 0x19) — direct-write secondary
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeDirectB));
                    // @0x168f47: addi(destReg, R0, Imm(addr))
                    {
                        auto vec = asmCommands_->addi(
                            destReg, AsmRegister(0), Immediate(static_cast<int>(addr)));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // @0x169251: suser(destReg, 0x18) — frequency commit
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeFreqCommit));
                    break;
                }
                case NodeTypeIdx::Phase: {
                    // @0x167823: addi(destReg, R0, Imm(addr))
                    {
                        auto vec = asmCommands_->addi(
                            destReg, AsmRegister(0), Immediate(static_cast<int>(addr)));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    // @0x168daa: suser(destReg, 0x16) — commit
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeCommit));
                    break;
                }
            }
        } else {
            // --- Per-case codegen: jump table @958f50 (paths B & C) ---
            // Cases 0,1,5 are simple: addi with a small literal. Cases 2,3,4
            // embed multi-step triplet sequences (suser opcodes 0x10/0x11/0x12).
            // After cases 0,1,5: suser(destReg, 0x10) + addi(destReg, R0, Imm(addr)).
            switch (node.typeIdx) {
                case NodeTypeIdx::IntegerPassthrough: {
                    // @0x164de4: addi(destReg, R0, Immediate(1)).
                    auto vec = asmCommands_->addi(
                        destReg, AsmRegister(0), Immediate(1));
                    localList.entries.insert(localList.entries.end(),
                        vec.begin(), vec.end());
                    break;
                }
                case NodeTypeIdx::SinePair: {
                    // @0x16591b: addi(destReg, R0, Immediate(2)).
                    auto vec = asmCommands_->addi(
                        destReg, AsmRegister(0), Immediate(2));
                    localList.entries.insert(localList.entries.end(),
                        vec.begin(), vec.end());
                    break;
                }
                case NodeTypeIdx::FloatBits: {
                    // @0x165587: double-triplet (sine I+Q write).
                    if (valRef.varType_ != VarType_Var) {
                        // slow-arm @0x166107: scalar value path.
                        // Uses tag 3 (not 0xc) and converts value to float bits.
                        // First triplet:
                        {   // addi(destReg, R0, Immediate(3))        // @0x166148
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(3));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeTag));
                        {   // addi(destReg, R0, Immediate(addr))     // @0x16620f
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(static_cast<int>(addr)));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeAddr));
                        // Convert value to float bits                // @0x16629f
                        {
                            double d = valRef.value_.toDouble();
                            float f = static_cast<float>(d);
                            int fbits;
                            std::memcpy(&fbits, &f, sizeof(fbits));
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(fbits));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeValue));
                        // Commit + trap  @0x16636b: uses type.value_.toDouble() → float32 → addi
                        {
                            double scaleD = type.value_.toDouble();
                            float scaleF  = static_cast<float>(scaleD);
                            int scaleBits;
                            std::memcpy(&scaleBits, &scaleF, sizeof(scaleBits));
                            auto sv = asmCommands_->addi(destReg, AsmRegister(0), Immediate(scaleBits));
                            localList.entries.insert(localList.entries.end(), sv.begin(), sv.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeSlowCommit));
                        localList.entries.push_back(asmCommands_->trap());
                    } else {
                        // fast-arm: varType==2 (register).
                        // @0x165592..165747: binary generates ONE triplet (tag=0xc, I-channel),
                        // then jmp to slow-commit @0x16636b.
                        {
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(0xc));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeTag));
                        {
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(static_cast<int>(addr)));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeAddr));
                        appendSuser(localList, asmCommands_, valRef.reg_, detail::AddressImpl<uint32_t>(kSuserNodeValue));
                        // Fall to slow-commit @0x16636b: floatEqual warning on type value + float32(type) + suser(0x14) + trap
                        // @0x16636b: The slow-commit region reads type.value_, checks floatEqual,
                        // emits warning with "integer" hint if not representable, then addi(float32(type)) + suser(0x14) + trap.
                        {
                            double d2 = type.value_.toDouble();
                            int intVal = type.value_.toInt();
                            if (!floatEqual(d2, static_cast<double>(intVal))) {
                                std::string valStr = type.value_.toString();
                                std::string msg = ErrorMessages::format(
                                    NodePrecisionLoss, valStr, "integer");  // @0x1663bc: "integer" hint
                                warningCallback_(msg);
                            }
                        }
                        {
                            double scaleD = type.value_.toDouble();
                            float scaleF  = static_cast<float>(scaleD);
                            int scaleBits;
                            std::memcpy(&scaleBits, &scaleF, sizeof(scaleBits));
                            auto sv = asmCommands_->addi(destReg, AsmRegister(0), Immediate(scaleBits));
                            localList.entries.insert(localList.entries.end(), sv.begin(), sv.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeSlowCommit));
                        localList.entries.push_back(asmCommands_->trap());
                    }
                    break;
                }
                case NodeTypeIdx::RawDoubleLow32: {
                    // @0x165751: single triplet (Q channel only).
                    if (valRef.varType_ != VarType_Var) {
                        // slow-arm @0x166537: scalar path — 64-bit double write.
                        // Uses tag 4, splits double into low32 (suser 0x12)
                        // + high32 (suser 0x13).
                        {   // addi(destReg, R0, Immediate(4))        // @0x166578
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(4));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeTag));
                        {   // addi(destReg, R0, Immediate(addr))     // @0x16663f
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(static_cast<int>(addr)));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeAddr));
                        // Convert value to raw double bits            // @0x1666cf
                        double d = valRef.value_.toDouble();
                        int64_t rawBits;
                        std::memcpy(&rawBits, &d, sizeof(rawBits));
                        int low32 = static_cast<int>(rawBits);
                        int high32 = static_cast<int>(rawBits >> 32);
                        // low32 → addi + suser(0x12)                 // @0x166723
                        {
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(low32));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeValue));
                        // high32 → addi + suser(0x13)                // @0x1667d0
                        {
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(high32));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeValueHi));
                        // Second value: floatEqual check + float conv // @0x16687f
                        {
                            double d2 = valRef.value_.toDouble();
                            int intVal = valRef.value_.toInt();
                            if (!floatEqual(d2, static_cast<double>(intVal))) {
                                std::string valStr = valRef.value_.toString();
                                std::string msg = ErrorMessages::format(
                                    NodePrecisionLoss, valStr, "integer");
                                warningCallback_(msg);
                            }
                            double d3 = valRef.value_.toDouble();
                            float f = static_cast<float>(d3);
                            int fbits;
                            std::memcpy(&fbits, &f, sizeof(fbits));
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(fbits));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                    } else {
                        // fast-arm: varType==2 (register) — same as triplet B of case 2.
                        {
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(0xd));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeTag));
                        {
                            auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                        Immediate(static_cast<int>(addr)));
                            localList.entries.insert(localList.entries.end(),
                                v.begin(), v.end());
                        }
                        appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeAddr));
                        appendSuser(localList, asmCommands_, valRef.reg_, detail::AddressImpl<uint32_t>(kSuserNodeValue));
                    }
                    break;
                }
                case NodeTypeIdx::Frequency: {
                    // @0x165488: single triplet (general case).
                    // tag 2 → suser(0x10), addr → suser(0x11),
                    // val.reg_ → suser(0x12).
                    if (valRef.varType_ == VarType_Var) {
                        // @0x16a2bd: varType==2 is invalid — throw.
                        std::string valStr = valRef.value_.toString();
                        std::string msg = ErrorMessages::format(
                            NodeOnlySetDouble, valStr);
                        throw CustomFunctionsException(msg);
                    }
                    {
                        auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                    Immediate(2));
                        localList.entries.insert(localList.entries.end(),
                            v.begin(), v.end());
                    }
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeTag));
                    {
                        auto v = asmCommands_->addi(destReg, AsmRegister(0),
                                                    Immediate(static_cast<int>(addr)));
                        localList.entries.insert(localList.entries.end(),
                            v.begin(), v.end());
                    }
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeAddr));
                    appendSuser(localList, asmCommands_, valRef.reg_, detail::AddressImpl<uint32_t>(kSuserNodeValue));
                    break;
                }
                case NodeTypeIdx::Phase: {
                    // @0x165a17: addi(destReg, R0, Immediate(3)).
                    if (valRef.varType_ == VarType_Var) {
                        // @0x16a31e: varType==2 is invalid — throw.
                        std::string valStr = valRef.value_.toString();
                        std::string msg = ErrorMessages::format(
                            PhaseNodeConstOnly, valStr);
                        throw CustomFunctionsException(msg);
                    }
                    auto vec = asmCommands_->addi(
                        destReg, AsmRegister(0), Immediate(3));
                    localList.entries.insert(localList.entries.end(),
                        vec.begin(), vec.end());
                    break;
                }
            }
            // --- Common tail for BC cases 0,1,5 ---
            // Cases 2,3,4 embed their own suser sequences inline and
            // do NOT hit this common tail.
            if (node.typeIdx == NodeTypeIdx::IntegerPassthrough || node.typeIdx == NodeTypeIdx::SinePair || node.typeIdx == NodeTypeIdx::Phase) {
                // suser(destReg, 0x10) → tag
                appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeTag));
                // addi(destReg, R0, Immediate(addr))
                {
                    auto vec = asmCommands_->addi(
                        destReg, AsmRegister(0),
                        Immediate(static_cast<int>(addr)));
                    localList.entries.insert(localList.entries.end(),
                        vec.begin(), vec.end());
                }
                // suser(destReg, 0x11) → addr
                appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeAddr));
                // value → suser(0x12)
                if (valRef.varType_ == VarType_Var) {
                    // register path: suser(valRef.reg_, 0x12)
                    appendSuser(localList, asmCommands_, valRef.reg_, detail::AddressImpl<uint32_t>(kSuserNodeValue));
                } else {
                    // const path: addi(destReg, R0, value) + suser(destReg, 0x12)
                    {
                        auto vec = asmCommands_->addi(
                            destReg, AsmRegister(0),
                            Immediate(valRef.value_.toInt()));
                        localList.entries.insert(localList.entries.end(),
                            vec.begin(), vec.end());
                    }
                    appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeValue));
                }
                // commit + trap
                // @0x166438-16646e: binary converts type.value_.toDouble() → float32 bits,
                // then calls addi(destReg, R0, float32Bits). This encodes the scale argument
                // (default 1.0 → float32 0x3F800000; with 3rd arg e.g. 0.532 → 0x3F083127).
                // The addi generates hi/lo split automatically when lower 12 bits are nonzero.
                {
                    double scaleD = type.value_.toDouble();           // @0x166438
                    float scaleF  = static_cast<float>(scaleD);       // @0x166460: cvtsd2ss
                    int scaleBits;
                    std::memcpy(&scaleBits, &scaleF, sizeof(scaleBits));
                    auto scaleVec = asmCommands_->addi(               // @0x16648e
                        destReg, AsmRegister(0), Immediate(scaleBits));
                    localList.entries.insert(localList.entries.end(),
                        scaleVec.begin(), scaleVec.end());
                }
                appendSuser(localList, asmCommands_, destReg, detail::AddressImpl<uint32_t>(kSuserNodeSlowCommit));
                localList.entries.push_back(asmCommands_->trap());
            }
        }

        // --- Splice localList into results->assemblers_ ---
        // @0x169320..169357: vector::__insert_with_size on results->assemblers_
        // (vector<AsmList::Asm> at +0x18 in EvalResults).
        if (!localList.entries.empty()) {
            results->assemblers_.insert(
                results->assemblers_.end(),
                localList.entries.begin(),
                localList.entries.end());                     // @0x169352
        }
    }

    // ----------------------------------------------------------------
    // Epilogue: cleanup temporaries and return results.
    // @0x169d83..0x169df4: shared_ptr/vector cleanup, string dealloc.
    // @0x169df4: function return (mov rax,[rbp-0x1c8]; epilogue; ret).
    //
    // Binary: Block F (0x169ea5..0x16a3f0) contains static regex lazy-init.
    // Block G (0x16a3f0..0x16b740) is unwinding/landing-pad code.
    // ----------------------------------------------------------------

    return results;
}

// addSyncCommand @0x16bb30 — 608 bytes (0x16bb30..0x16bd8f)
//
// Checks device type from EvalResults[0]:
//   deviceType == 2 (Hirzel): calls AsmCommands::asmSyncHirzel() @0x279d00
//   deviceType == 1 (Cervino): calls AsmCommands::asmSyncPlaceholderCervino() @0x279b80,
//                              stores shared_ptr in results (+0x38/+0x40)
// In both cases, appends the asm entry to results->assemblers_.
void CustomFunctions::addSyncCommand(std::shared_ptr<EvalResults> results,
                                      std::shared_ptr<Resources> res) {  // @0x16bb30
    // @0x16bb4d-16bb52: Binary reads *(int32_t*)(*(this+0x00)) = config_->deviceType.
    // (rsi=this, not rsi=results.__ptr_.) The comparison values 2 and 1
    // match AwgDeviceType::HDAWG and AwgDeviceType::UHFLI respectively.
    int deviceType = static_cast<int>(config_->deviceType);

    if (deviceType == HDAWG) {
        // Hirzel path @0x16bb74
        // @0x16bba8: call AsmCommands::asmSyncHirzel() @0x279d00
        auto asm_entry = asmCommands_->asmSyncHirzel();  // @0x279d00
        // @0x16bc3b: push_back into results->assemblers_ (+0x18)
        results->assemblers_.push_back(std::move(asm_entry));
    } else if (deviceType == UHFLI) {
        // Cervino path @0x16bc56
        // @0x16bc80: call AsmCommands::asmSyncPlaceholderCervino() @0x279b80
        auto asm_entry = asmCommands_->asmSyncPlaceholderCervino();  // @0x279b80
        // @0x16bcc8: store shared_ptr into results->node_ (+0x38/+0x40)
        // (The placeholder carries a shared_ptr<Node> that needs to be
        //  propagated to the results for later sync resolution.)
        results->node_ = asm_entry.node;  // @0x16bcc8
        // @0x16bd15: push_back into results->assemblers_
        results->assemblers_.push_back(std::move(asm_entry));
    }
    // @0x16bd60..0x16bd8f: move res shared_ptr into return (cleanup)
}

// printF @0x16c470 — 2370 bytes (0x16c470..0x16d380-ish)
//
// Implements printf-style string formatting using boost::basic_format.
//
// First arg must be VarType==3 (string) — used as format string.
// Remaining args fed to formatter:
//   VarType 3 (string) → string feed
//   VarType 4 or 6 (double) → checks if integer via floatEqual(val, round(val));
//     if integer: feeds as int; if not: feeds as double
//   Other types → throw error 0x46 with str(VarType)
// Empty args → throw error 0x88
// Catches boost::io::too_few_args → error 0xA6
// Catches boost::io::too_many_args → error 0xA8 (CustomFunctionsValueException)
std::string CustomFunctions::printF(std::vector<EvalResultValue> const& args,
                                     std::string const& funcName) {  // @0x16c470
    // @0x16c4a5: check args.empty()
    if (args.empty()) {
        // @0x16d1d0: throw error 0x88
        throw CustomFunctionsException(
            ErrorMessages::format(FuncSingleArg, funcName));
    }

    // @0x16c4c0: first arg must be VarType==3 (string)
    auto const& fmtArg = args[0];
    // Extract format string from first argument
    std::string fmtStr = fmtArg.value_.toString();

    // @0x16c530: construct boost::basic_format from format string
    boost::format formatter(fmtStr);

    // @0x16c580..0x16cf80: iterate remaining args and feed to formatter
    try {
        for (size_t i = 1; i < args.size(); ++i) {
            auto const& arg = args[i];
            VarType varType = arg.varType_;

            if (varType == VarType_String) {
                // String arg — feed directly
                // @0x16c620: string feed path
                std::string s = arg.value_.toString();
                formatter % s;
            } else if (varType == VarType_Const || varType == VarType_Cvar) {
                // Numeric arg — check if integer
                // @0x16c6a0: double value path
                double val = arg.value_.toDouble();
                double rounded = std::round(val);
                // @0x16c6e0: floatEqual(val, round(val)) — checks if value is integral
                if (val == rounded && std::isfinite(val)) {
                    // Feed as integer @0x16c710
                    formatter % static_cast<long long>(val);
                } else {
                    // Feed as double @0x16c750
                    formatter % val;
                }
            } else {
                // Unknown type — throw error 0x46 @0x16cf90
                throw CustomFunctionsValueException(
                    ErrorMessages::format(FuncInvalidArgType,
                                                                                                    std::to_string(static_cast<int>(varType)),
                                          funcName,
                                          std::to_string(i),
                                          std::string("string, int or double")),
                    i);
            }
        }

        // @0x16cfa0: return str(formatter)
        return boost::str(formatter);

    } catch (boost::io::too_few_args const&) {
        // @0x16d060: error 0xA6 — not enough arguments for format string
        throw CustomFunctionsException(
            ErrorMessages::format(FormatMoreArgs, funcName));
    } catch (boost::io::too_many_args const&) {
        // @0x16d120: error 0xA8 — too many arguments for format string
        throw CustomFunctionsValueException(
            ErrorMessages::format(FormatCantInterpret, funcName), 0);
    }
}

// addWaitCycles @0x16d8c0 — 630 bytes (0x16d8c0..0x16db36)
//
// Gets a register via Resources::getRegisterNumber(), then emits:
//   1. addi(reg, AsmRegister(0), Immediate(cycles))  — load cycle count
//   2. suser(reg, Address(0x69))                     — write to wait-cycles user register
// Both instructions are inserted into results->assemblers_.
void CustomFunctions::addWaitCycles(int cycles,
                                     std::shared_ptr<EvalResults> results,
                                     std::shared_ptr<Resources> res) {  // @0x16d8c0
    // @0x16d8f0: call Resources::getRegisterNumber() @0x1e4bb0
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    AsmRegister zero(0);

    // @0x16d930: call AsmCommands::addi(reg, AsmRegister(0), Immediate(cycles)) @0x275c70
    auto addiResult = asmCommands_->addi(reg, zero, Immediate(cycles));

    // @0x16d9a0: insert all addi results into results->assemblers_
    for (auto& asm_entry : addiResult) {
        results->assemblers_.push_back(std::move(asm_entry));  // @0x16d9e0
    }

    // @0x16da30: call AsmCommands::suser(reg, Address(0x69)) @0x277350
    // Address 0x69 is the wait-cycles user register address
    // @0x16daa0: push_back into results->assemblers_
    appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserWaitCycles));  // @0x16dae0
    // @0x16db00..0x16db36: cleanup, move res shared_ptr
}

// writeLS64bit @0x16dbb0 — 1184 bytes (0x16dbb0..0x16e04f)
//
// Writes a 64-bit value to two user registers (low 32 bits then high 32 bits).
// Gets a scratch register via Resources::getRegisterNumber(), then emits:
//   1. addi(reg, AsmRegister(0), Immediate(value & 0xFFFFFFFF))  — load low 32
//   2. suser(reg, Address(reg1))                                 — write to reg1
//   3. addi(reg, AsmRegister(0), Immediate(value >> 32))         — load high 32
//   4. suser(reg, Address(reg2))                                 — write to reg2
void CustomFunctions::writeLS64bit(unsigned long value, int reg1, int reg2,
                                    std::shared_ptr<EvalResults> results,
                                    std::shared_ptr<Resources> res) {  // @0x16dbb0
    // @0x16dbf0: call Resources::getRegisterNumber() @0x1e4bb0
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    AsmRegister zero(0);

    // --- Low 32 bits ---
    // @0x16dc30: addi(reg, AsmRegister(0), Immediate(value & 0xFFFFFFFF))
    uint32_t lowBits = static_cast<uint32_t>(value & 0xFFFFFFFF);
    auto addiLow = asmCommands_->addi(reg, zero, Immediate(static_cast<int>(lowBits)));

    // @0x16dca0: insert into results->assemblers_
    for (auto& asm_entry : addiLow) {
        results->assemblers_.push_back(std::move(asm_entry));  // @0x16dce0
    }

    // @0x16dd20: suser(reg, Address(reg1))
    appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(
        static_cast<unsigned int>(reg1)));  // @0x16dd90

    // --- High 32 bits ---
    // @0x16dde0: addi(reg, AsmRegister(0), Immediate(value >> 32))
    uint32_t highBits = static_cast<uint32_t>(value >> 32);
    auto addiHigh = asmCommands_->addi(reg, zero, Immediate(static_cast<int>(highBits)));

    // @0x16de50: insert into results->assemblers_
    for (auto& asm_entry : addiHigh) {
        results->assemblers_.push_back(std::move(asm_entry));  // @0x16de90
    }

    // @0x16ded0: suser(reg, Address(reg2))
    appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(
        static_cast<unsigned int>(reg2)));  // @0x16df40
    // @0x16df80..0x16e04f: cleanup, move res shared_ptr
}

// generateWaveform @0x15a9f0 — 0x4C0 bytes (ends at 0x15aeb0)
// Copies args into a new vector, prepends a string EvalResultValue containing
// the waveform name, then copies the Resources shared_ptr and delegates to
// CustomFunctions::generate(newArgs, resCopy).
// On catch of CustomFunctionsValueException (personality type 1): re-throws as
//   CustomFunctionsValueException(what(), argIndex) via __cxa_throw.
// On catch of CustomFunctionsException (personality type 2): re-throws as
//   CustomFunctionsException(what()) via __cxa_throw.
std::shared_ptr<EvalResults> CustomFunctions::generateWaveform(
    std::string const& name,
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> res) {  // @0x15a9f0

    // @0x15aa19: xorps → zero-init local vector<EvalResultValue> newArgs
    std::vector<EvalResultValue> newArgs;

    // @0x15aa28: r12 = args.data(), rbx = args.data() + args.size() (end ptr)
    // @0x15aa3e: r14 = end - begin (byte size)
    // @0x15aa41: if size == 0 → skip allocation
    if (!args.empty()) {
        // @0x15aa72: allocate r14 bytes, copy args into newArgs
        newArgs.assign(args.begin(), args.end());
    }

    // @0x15aaa3: Build an EvalResultValue containing `name` as a string value.
    // @0x15aaa7: DWORD [rbp-0xa8] = 4 (VarType_Const —
    //            note: even though the variant payload is a std::string, the
    //            varType_ tag is Const, not String.  This is intentional:
    //            function names are propagated as string-valued constants,
    //            matching the addConst pattern in Resources).
    // @0x15aab4: copy name string into the EvalResultValue at rbp-0x98
    // @0x15ab04: DWORD [rbp-0xa0] = 3 (subType=Numeric), QWORD [rbp-0x78] = 3,
    //            DWORD [rbp-0x70] = type
    // This constructs an EvalResultValue with type=string containing `name`,
    // then prepends it at the front of newArgs.

    // @0x15ab70: AsmRegister(-1) placeholder
    // @0x15ab7d: call vector::insert at position 0 (prepend the name value)

    // Prepend a string-typed EvalResultValue with `name`                @0x15aaa3-0x15ab7d
    // Binary sets: varType_=4 (Const), varSubType_=3 (Numeric, from DWORD at rbp-0xa0),
    // value_ = Value(name), reg_ = AsmRegister(-1)
    {
        EvalResultValue nameVal;
        nameVal.varType_ = VarType_String;                              // @0x15aaa7: first arg must be string type for generate()
        nameVal.varSubType_ = VarSubType_Vect;                       // @0x15ab04: movl $3
        nameVal.value_ = Value(name);                                   // @0x15aab4: copy string
        nameVal.reg_ = AsmRegister(-1);                                 // @0x15ab70
        newArgs.insert(newArgs.begin(), std::move(nameVal));             // @0x15ab7d
    }

    // @0x15abdd: copy res shared_ptr (increment refcount)
    // @0x15ac03: call CustomFunctions::generate(newArgs, resCopy) @0x149940
    std::shared_ptr<EvalResults> result;
    try {
        result = generate(newArgs, std::move(res));  // @0x15ac0a → @0x149940
    }
    catch (CustomFunctionsValueException& e) {
        // @0x15ad00: __cxa_begin_catch, personality type 1
        // @0x15ad41: allocate 0x40 byte exception
        // @0x15adba: construct string from e.what() (or empty string if null)
        // @0x15adca: re-throw as CustomFunctionsValueException(msg, e.argIndex)
        // The argIndex is read from e+0x20.
        throw CustomFunctionsValueException(
            e.what() ? e.what() : "",
            0 /* e.argIndex_ — offset +0x20 in caught exception */);  // @0x15adcd
    }
    catch (CustomFunctionsException& e) {
        // @0x15ad34: __cxa_begin_catch, personality type 2
        // @0x15ad73: construct string from e.what() (or empty string if null)
        // @0x15ad7f: re-throw as CustomFunctionsException(msg)
        throw CustomFunctionsException(
            e.what() ? e.what() : "");  // @0x15ad82
    }

    return result;  // @0x15ac97
}

// ============================================================================
// SeqC built-in function implementations
//
// Signature: shared_ptr<EvalResults>(vector<EvalResultValue> const&, shared_ptr<Resources>)
// Registered in funcMap_ during construction.
//
// Categories:
//   - Thin play wrappers: playWave/Now/Indexed/IndexedNow → play()/playIndexed()
//   - Thin aux wrappers:  playAuxWave/Indexed → play()/playIndexed() with different SubFunc
//   - Simple 0-arg:       waitWave, waitPlayQueueEmpty, sync, randomSeed
//   - Simple 1-arg:       setRate, setTrigger, getCnt, lock, unlock
//   - Formatting:         error, info (use printF + asmMessage)
//   - Complex multi-arg:  playZero, playHold, wait, setDIO, getDIO, etc.
// ============================================================================

} // namespace zhinst
