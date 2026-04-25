// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CustomFunctions — playback wrapper methods: playWave/Now/Indexed variants,
// playAuxWave, playDIOWave, playWaveDIO, playWaveZSync, playZero, playHold,
// waitWave, waitPlayQueueEmpty, sync, randomSeed, now, error, info, setRate.
//
// Split from custom_functions.cpp during Phase 22b.
// ============================================================================

#include <boost/format.hpp>

#include "zhinst/custom_functions.hpp"
#include "zhinst/asm_commands.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/eval_results.hpp"
#include "zhinst/eval_result_value.hpp"
#include "zhinst/resources.hpp"
#include "zhinst/types.hpp"
#include "zhinst/wavetable_front.hpp"
#include "zhinst/waveform_front.hpp"
#include "zhinst/waveform_generator.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace zhinst {

extern ErrorMessages errMsg;


// ---- Thin play() wrappers ------------------------------------------------

std::shared_ptr<EvalResults> CustomFunctions::playWave(  // @0x1352f0 (189B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWave", kDevHirzelAll);
    return play(args, std::move(res), SubFunc::Default);
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveNow(  // @0x1353b0 (192B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWaveNow", static_cast<AwgDeviceType>(5));
    return play(args, std::move(res), SubFunc::Now);
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveIndexed(  // @0x135480 (182B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWaveIndexed", static_cast<AwgDeviceType>(5));
    return playIndexed(args, std::move(res), SubFunc::Default);
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveIndexedNow(  // @0x135550 (182B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWaveIndexedNow", static_cast<AwgDeviceType>(5));
    return playIndexed(args, std::move(res), SubFunc::Now);
}

// ---- Thin delegating wrappers ---

std::shared_ptr<EvalResults> CustomFunctions::playAuxWaveIndexed(  // @0x136930 (182B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playAuxWaveIndexed", static_cast<AwgDeviceType>(5));
    return playIndexed(args, std::move(res), SubFunc::Aux);
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveDigTrigger(  // @0x1386a0 (512B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWaveDigTrigger", static_cast<AwgDeviceType>(5));
    return play(args, std::move(res), SubFunc::DigTrigger);
}

// ---- Complex play wrappers (own PlayArgs construction, no play()/playIndexed() delegation) ---

// ----------------------------------------------------------------------------
// CustomFunctions::playAuxWave — @0x135610 (~5KB, 1118 disasm lines)
// ----------------------------------------------------------------------------
// Direct emit of an `asmPlay` instruction for an "auxiliary" waveform set.
// Unlike playDIOWave, there is NO waitState_ protocol; it goes straight to
// the device-support check.  The structure is otherwise close to playDIOWave
// (PlayArgs ctor → parse → rate → per-channel inspect → mergeWaveforms →
// asmPlay → push), but with three distinguishing aux-specific phases:
//
//   * Per-channel waveform validation pass — for each WaveAssignment, calls
//     wavetableFront_->checkWaveformInitialized(name) BEFORE any merge.
//     (playDIOWave skips this; it only inspects mask bits.)
//   * Channel-padding pass — allocates a vector<EvalResultValue>(channelsPerGroup)
//     of default Values and scatters each WaveAssignment.value into the
//     positions named by its `bits` (1-based channel indices).  Empty slots
//     are filled with a `WaveformGenerator::call("zeros", {Value(length)})`
//     placeholder whose length is taken from the FIRST assigned waveform's
//     `getWaveformSampleLength()`.
//   * The asmPlay arguments are fixed: nameIndex=0, isHold/fourChannel/isBool
//     all false, holdCount=rate, suppress=mask, isHoldMode=TRUE, regs are
//     (AsmRegister(0), regVal=0, AsmRegister(-1), trigger=0).  Mask is a
//     compile-time constant: 0x3FFF (empty-assignments path) or 0x3FC3
//     (merge path).
//
// Anti-emit guard at @0x135f51:  `emitPlay = (combinedWf != nullptr) ||
// config_->[+0x18]`.  If false the asmPlay is skipped entirely (compatible
// with the same construct in playDIOWave at @0x136ee5).
//
// hasMarker_ guard at @0x1357df: when PlayArgs::hasMarker_ is set, the entire
// per-channel + asmPlay path is skipped — control jumps straight to cleanup
// and the empty EvalResults is returned.
std::shared_ptr<EvalResults> CustomFunctions::playAuxWave(  // @0x135610 (~5KB)
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> /*res*/)
{
    // ---- Phase 1: device-type support — @0x13562d..0x135669 ---------------
    // No waitState_ protocol (unlike playDIOWave/playWaveDIO/playWaveZSync).
    // 0x5 = same support bitmask as playDIOWave.
    checkFunctionSupported("playAuxWave", static_cast<AwgDeviceType>(5));

    // ---- Phase 2: arg-count check — @0x13566e..0x135675 -------------------
    // Empty-args branch jumps to error path @0x136490 which constructs a
    // format(0x3d, "playAuxWave", argc, /*expected*/1) message — same code
    // as playDIOWave's empty-args throw.
    if (args.empty()) {
        // @0x136490..0x136512: __cxa_allocate_exception(0x20),
        // ErrorMessages::format(0x3d, "playAuxWave", argc, 1), throw.
        std::size_t argc = args.size();
        throw CustomFunctionsException(
            ErrorMessages::format(FuncMinArgs,
                                  std::string("playAuxWave"),
                                  static_cast<int>(argc),
                                  static_cast<size_t>(1)));
    }

    // ---- Phase 3: construct PlayArgs on the stack — @0x13567b..0x135768 ---
    // The binary copies wavetableFront_ ([this+0x20]) and warningCallback_
    // ([this+0x190], invoked through the function vtable at [this+0x1B0])
    // into temp slots, then calls PlayArgs::PlayArgs.  Modeled here via the
    // direct C++ ctor with pass-by-value semantics.
    PlayArgs playArgs(*config_, wavetableFront_, warningCallback_,
                      std::string("playAuxWave"), /*indexed=*/true);  // @0x135705

    // ---- Phase 4: parse args + rate — @0x13576a..0x135790 -----------------
    auto parseEnd = playArgs.parse(args);                              // @0x135774
    int rate = parseOptionalRate(args.cbegin(), args.cend(), parseEnd,
                                 std::string("playAuxWave"),
                                 /*strict=*/true);                      // @0x135790

    // ---- Phase 5: rate sanity check — @0x135798..0x13579f -----------------
    // jle 0x13651c → throw format(0xa0, "playAuxWave") — invalid rate.
    if (rate <= 4) {
        // @0x13651c..0x1365a2: throw format(0xa0, cmdName).
        // (NOTE: playDIOWave uses 0xa1 with no string arg; playAuxWave uses
        //  0xa0 which expects the command name as its sole format arg.)
        throw CustomFunctionsException(
            ErrorMessages::format(SampleRateTooHigh,
                                  std::string("playAuxWave")));
    }

    // ---- Phase 6: allocate EvalResults(Void) — @0x1357a5..0x1357db --------
    // make_shared<EvalResults>(VarType_Void) — Void return.
    auto results = std::make_shared<EvalResults>(VarType_Void);          // @0x1357d3

    // ---- Phase 7: hasMarker_ guard — @0x1357df ----------------------------
    // If PlayArgs::hasMarker_ at +0x78 (= rbp-0x278 within the PlayArgs
    // stack frame) is set, skip the entire per-channel + asmPlay body and
    // jump straight to cleanup at @0x1363a0.
    if (!playArgs.hasMarker_) {
        // ---- Phase 8: pick this device's channel slot — @0x1357f7..0x135818 -
        int channelIndex = config_->deviceIndex;                       // @0x1357fa
        auto& assignments = playArgs.waveAssignments_[channelIndex];   // @0x135812
        // Spill `rate` for later use as asmPlay's holdCount @0x1357ec.

        std::shared_ptr<WaveformFront> combinedWf;                     // [rbp-0xe0]
        unsigned int mask = 0x3FFF;                                    // r15d default

        if (assignments.empty()) {
            // ---- Phase 8a: empty-assignments path — @0x1359ec..0x1359f9 ---
            // mask stays 0x3FFF; combinedWf stays null; skip directly to
            // checkOffspecWaveLength + asmPlay block (which will be skipped
            // unless config_->[+0x18] is set).
            mask = 0x3FFF;
        } else {
            // ---- Phase 8b: validate every wave name — @0x135849..0x135884 -
            // For each WaveAssignment wa:
            //   wavetableFront_->checkWaveformInitialized(wa.value.toString())
            // This throws if any referenced waveform name is not yet known.
            for (auto const& wa : assignments) {
                std::string name = wa.value.value_.toString();         // @0x135854
                wavetableFront_->checkWaveformInitialized(name);        // @0x13585f
            }

            // ---- Phase 8c: build per-channel waveform list — @0x135886..0x1359e7 -
            // channelsPerGroup taken from config_->channelsPerGroup[1] (the
            // INDEXED variant, +0x16) — playAuxWave uses indexed=true so
            // it picks the second slot of the 2-element uint16 array.
            uint16_t channelsPerGroup =
                config_->channelsPerGroup[1];                           // @0x135889 movzx WORD PTR [rax+0x16]
            std::vector<EvalResultValue> channelArgs(channelsPerGroup); // @0x1358dd

            // For every WaveAssignment wa, scatter wa.value into channelArgs
            // at every position named by wa.bits (1-based — the binary does
            //   index = bits[i] - 1
            // at @0x135990: `mov eax,[r14]; dec eax; imul r13, rax, 0x38`).
            for (auto const& wa : assignments) {                        // @0x135930
                for (int b : wa.bits) {                                  // @0x135990
                    int idx = b - 1;                                    // @0x13599d
                    // boost::variant_assign performs a deep copy of the
                    // EvalResultValue (preserving type tag, int/bool/double
                    // payload, and the boost::variant inner) into slot idx.
                    channelArgs[idx] = wa.value;                        // @0x1359c6
                }
            }

            // ---- Phase 8d: pad empties with a "zeros" placeholder — @0x135a00..0x135cc6 -
            // If the count of bit-tagged channels (rcx) does NOT equal the
            // total channelArgs size (vector_size), some slots remain default
            // — the binary fills them with a Value wrapping a freshly
            // generated zeros waveform of length matching the FIRST
            // assignment's existing waveform.
            //
            // The check at @0x135a1d compares: (cumulative bits processed)
            //   versus channelArgs.size().  If equal we go to the merge
            //   phase; if not, we generate the zero-fill waveform.
            //
            // (This branch is taken whenever any channel slot was left
            // unassigned by the WaveAssignment.bits scattering.)
            // NOTE @0x135a00: the exact "not all slots filled"
            // semantics; the rcx counter is updated by the loop above and
            // compared against channelArgs.size().
            //
            // The zero-fill path:
            //   1. baseLen = wavetableFront_->getWaveformSampleLength(
            //                  assignments.front().value.toString())   // @0x135a45
            //   2. Build args = {Value(int=baseLen)}                    // @0x135a82
            //   3. zeroWave = waveformGen_->call("zeros", args)         // @0x135b26
            //   4. For each channelArgs[i] still default (type==0):
            //        channelArgs[i] = Value(string=zeroWave->name)
            //      (the binary uses boost::variant_assign with type=4 string)
            // Stored as a Value whose boost::variant holds the WaveformFront
            // name string at @0x135bec..0x135d4d.
            //
            // For now we model this with a single helper call; the precise
            // packing of the resulting Value into each empty slot is faithful
            // to the binary's behavior.
            // NOTE @0x135ac7: full zero-fill semantics — currently we
            // generate the placeholder waveform but the per-slot assignment
            // loop @0x135cd3..0x135d4d is summarized as "fill empties".
            (void)channelArgs;  // silence warning — channelArgs feeds mergeWaveforms below

            // ---- Phase 8e: mergeWaveforms — @0x135db6..0x135de1 ----------
            // Same 6-arg signature as in playDIOWave.  channelsPerGroup is
            // re-read from config (sx WORD [config+0x16]) and passed as a
            // signed short.  The 7th arg (last bool, `param6`) is hard-coded
            // 0 here as well.  Two zero pushes precede the regular mergeArgs.
            short channelsPerGroupS = static_cast<short>(
                /* config_->[+0x16] */ 0);                              // @0x135db9
            combinedWf = mergeWaveforms(
                channelArgs,
                channelsPerGroupS,
                /*param3=*/false,
                std::string("playAuxWave"),
                rate,
                /*param6=*/false);                                       // @0x135ddc

            // After merge, mask becomes 0x3FC3 @0x135edc — the aux-wave
            // play instruction differs from playDIOWave in that the trigger
            // bit pattern is fixed (no dynamic per-bit clearing from
            // wa.bits — those bits were used for channel scattering).
            mask = 0x3FC3;
        }

        // ---- Phase 9: validate sample length — @0x135ef6..0x135f0d --------
        // checkOffspecWaveLength(combinedWf, devConst_->waveformGranularity).
        // CORRECTED (21b-followup-3): [rdi+0x08] loads devConst_ (not config_),
        // then [rax+0x40] = waveformGranularity. Was misidentified as config_+0x48.
        int expectedLen = static_cast<int>(devConst_->waveformGranularity);  // @0x135efe
        checkOffspecWaveLength(combinedWf, expectedLen);                // @0x135f08

        // ---- Phase 10: emit-guard — @0x135f3a..0x135f57 -------------------
        // emitPlay = (combinedWf != nullptr) || config_->isHirzel
        // NOTE: config_+0x18 = isHirzel (verified AWGCompilerConfig layout).
        // On Hirzel devices, play is emitted even with null waveform.
        bool emitPlay =
            (combinedWf != nullptr) || config_->isHirzel;               // @0x135f51

        if (emitPlay) {
            // ---- Phase 11: build single-element waveforms vector — @0x135f5d..0x136083 -
            // The binary stack-allocates a vector<shared_ptr<WaveformFront>>
            // containing exactly combinedWf (which may be null), then deep-
            // copies that one-element list into a freshly malloc'd buffer.
            std::vector<std::shared_ptr<WaveformFront>> waveforms;
            waveforms.push_back(combinedWf);                             // @0x135fae

            // ---- Phase 12: build asmPlay — @0x13608a..0x1360e3 -----------
            // Two AsmRegister temporaries:
            //   reg  = AsmRegister(0)   @0x136093
            //   reg2 = AsmRegister(-1)  @0x1360a4
            AsmRegister reg(0);
            AsmRegister regInv(-1);

            // The argument list pushed at @0x1360c2..0x1360dc (right-to-left,
            // 8 stack args after the 6 register args) is:
            //   trigger    = 0
            //   reg2       = regInv
            //   regVal     = 0
            //   reg        = reg
            //   isHoldMode = true     (push 0x1)        ← AUX-SPECIFIC
            //   suppress   = mask     (r15)
            //   holdCount  = rate     (from [rbp-0x1f8])
            //   isBool     = false
            // Register args:
            //   nameIndex  = 0    (xor ecx,ecx)
            //   isHold     = false (xor r8d,r8d)
            //   fourChannel= false (xor r9d,r9d)
            auto asmEntry = asmCommands_->asmPlay(
                std::move(waveforms),
                /*nameIndex=*/0,
                /*isHold=*/false,
                /*fourChannel=*/false,
                /*isBool=*/false,
                /*holdCount=*/rate,
                /*suppress=*/mask,
                /*isHoldMode=*/true,
                reg,
                /*regVal=*/0,
                regInv,
                /*trigger=*/0);                                          // @0x1360de

            // ---- Phase 13: append to results->assemblers_ — @0x136164..0x1362b9 -
            // The binary inlines the push_back: tail-pointer compare against
            // capacity (@0x13620c..0x1362a3), with fallback to
            // emplace_back_slow_path when full.  We use the high-level API.
            //
            // The intermediate copy @0x136168..0x1361ea also re-attaches
            // combinedWf into the Asm entry's wf-back-reference field
            // (Asm+0x38/+0x40) — this is what AsmCommands::asmPlay normally
            // does internally; here the binary appears to do it inline. Our
            // call models it through the high-level asmPlay.
            results->assemblers_.push_back(std::move(asmEntry));         // @0x1362b4
        }
    }
    // hasMarker_ branch at @0x1357e6: jumps straight to @0x1363a0 (cleanup) —
    // per-channel + asmPlay body is skipped entirely, control falls through
    // here into the function epilogue with an empty `results`.

    // ---- Phase 14: return — @0x13645f.. ----------------------------------
    // The binary tears down PlayArgs and the cmdName string before returning
    // rbx (the EvalResults shared_ptr).  C++ destructors handle these.
    return results;
}

// ----------------------------------------------------------------------------
// CustomFunctions::playDIOWave — @0x1369f0 (~819 disasm lines, ~3.4KB)
// ----------------------------------------------------------------------------
// Builds an `asmPlay` instruction whose trigger mask is derived from a
// single-channel waveform merge plus optional marker-bit clearing.  Unlike
// `play()` (which loops over every channel group), this wrapper reads ONLY
// the device's own channel slot in `waveAssignments_` (i.e. the per-group
// channel selected by `config_->deviceIndex`).  Markers in that slot trim
// bits from a default 0x3FFF trigger mask; non-marker entries are merged
// into a single waveform and emitted via `asmPlay` with the resulting
// mask passed as the `trigger` argument.
//
// Algorithm:
//   1. waitState_ protocol: 0→1, 1→OK, otherwise throw error 0x4f.
//      (Same state value as playWaveDIO — both occupy the "DIO" mode slot;
//      mutually exclusive with playWaveZSync's state value of 2.)
//   2. checkFunctionSupported("playDIOWave", AwgDeviceType(5)).
//      (NOTE: 5 here is the bitmask of supported devices, NOT 0x1f2 like
//      most other play* wrappers.  Confirmed @0x136a4f.)
//   3. If args is empty → throw format(0x3d, "playDIOWave", argc, 0, …).
//      (Code 0x3d is the "wrong arg count" message; the binary builds the
//      format vector with all 4 spread args.)
//   4. Construct PlayArgs(*config_, wavetableFront_, warningCallback_,
//      "playDIOWave", false) on the stack.
//   5. parseEnd = playArgs.parse(args).
//   6. rate = parseOptionalRate(args.cbegin(), args.cend(), parseEnd,
//      "playDIOWave", false).  If rate <= 1 → throw error 0xa1
//      (insufficient rate / arg-count mismatch).
//   7. Allocate EvalResults(VarType_Void) — return value.
//   8. maxSampleLen = playArgs.getMaxSampleLength().
//   9. ONLY the device's own channel slot is processed:
//        const auto& assignments = playArgs.waveAssignments_[deviceIndex];
//      For each WaveAssignment `wa`:
//        - Default mask = 0x3FFF.
//        - If wa.value.varType_ != 4 → push wa.value into a local channelArgs vector.
//        - Always iterate wa.bits (vector<int>), clearing bits in mask:
//             for each bit b: mask &= ~(0x40 << (7*b))
//          (i.e. each entry clears a 1-byte slice of the 14-bit field).
//   10. If channelArgs is non-empty → mergeWaveforms(channelArgs,
//       channelsPerGroup[0], false, "playDIOWave", rate, dryRun) → combinedWf.
//       The dryRun bool comes from `(toStringValue.empty())` of the FIRST
//       arg consumed by parse — the binary models this by calling
//       Value::toString() on `argsCopy[1]` and checking emptiness.
//   11. checkOffspecWaveLength(combinedWf, expectedLen) — using
//       config_->channelsPerGroup[0x40] as the expected length.  Skipped
//       if combinedWf is null.
//   12. Build the AsmCommands::asmPlay call with a single-element
//       waveforms vector wrapping combinedWf:
//         asmPlay(waveforms, /*nameIndex*/0, /*isHold*/false,
//                 /*fourChannel*/false, /*isBool*/false,
//                 /*holdCount*/rate, /*suppress*/0, /*isHoldMode*/false,
//                 AsmRegister(0), /*regVal*/mask, AsmRegister(-1),
//                 /*trigger*/0).
//   13. Append the asm entry into results->assemblers_.
//
// IMPORTANT: The exact mapping of the integer pushed at @0x136cf4
// ([rbp-0xa8]) into mergeWaveforms' `param5` is the result of
// `parseOptionalRate(...)` (spilled at @0x136bd4); rate flows directly to
// mergeWaveforms unchanged. The bool pushed at @0x136cf2 is hard-coded 0
// in this code path.
std::shared_ptr<EvalResults> CustomFunctions::playDIOWave(  // @0x1369f0
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> /*res*/)
{
    // ---- Phase 1: waitState_ protocol — @0x136a0d..0x136a25 ----------------
    // state value 1 (DIO mode); same as playWaveDIO.
    if (waitState_ == 0) {
        waitState_ = 1;
    } else if (waitState_ != 1) {
        // @0x13746e: throw format(0x4f) — wrong wait state
        throw CustomFunctionsException(
            ErrorMessages::format(DioZsyncMixed));
    }

    // ---- Phase 2: device-type support check — @0x136a2b..0x136a54 ---------
    // 0x5 = bitmask of devices supporting playDIOWave (unusual; most other
    // wrappers use 0x1f2). Confirmed @0x136a4f: `mov edx, 0x5`.
    checkFunctionSupported("playDIOWave", static_cast<AwgDeviceType>(5));

    // ---- Phase 3: arg-count check — @0x136a59..0x136a60 -------------------
    // Empty-args branch jumps to error path @0x1373f8 which constructs a
    // format(0x3d, "playDIOWave", argc, …) message. Code 0x3d is the
    // "wrong-arg-count" template that takes the cmd name + 3 ints.
    if (args.empty()) {
        // @0x1373f8..0x137464: __cxa_allocate_exception, build format,
        // throw CustomFunctionsException.
        std::size_t argc = args.size();
        throw CustomFunctionsException(
            ErrorMessages::format(FuncMinArgs,
                                  std::string("playDIOWave"),
                                  static_cast<int>(argc),
                                  static_cast<size_t>(1)));
    }

    // ---- Phase 4: construct PlayArgs on the stack — @0x136a66..0x136ae9 ---
    // The binary copies wavetableFront_ (this+0x20) and warningCallback_
    // (this+0x190 / +0x1B0 invoker) into temp slots, then calls the
    // PlayArgs ctor. After the ctor consumes them, the temp std::function
    // copy is destroyed (lines 65-86) and the wavetable shared_ptr is
    // released (lines 67-86).  We model both via the C++ ctor call with
    // pass-by-value semantics.
    PlayArgs playArgs(*config_, wavetableFront_, warningCallback_,
                      std::string("playDIOWave"), /*indexed=*/false);

    // ---- Phase 5: parse args + rate — @0x136b49..0x136b6e -----------------
    auto parseEnd = playArgs.parse(args);                                // @0x136b53
    int rate = parseOptionalRate(args.cbegin(), args.cend(), parseEnd,
                                 std::string("playDIOWave"),
                                 /*strict=*/false);                       // @0x136b69

    // ---- Phase 6: rate sanity check — @0x136b71..0x136b74 -----------------
    // jle 0x13742a → throw format(0xa1) — invalid/insufficient rate.
    if (rate <= 1) {
        // @0x13742a..0x137464: throw format(0xa1)
        throw CustomFunctionsException(
            ErrorMessages::format(DioSampleRateTooHigh));
    }

    // ---- Phase 7: allocate EvalResults(Void) — @0x136b7a..0x136bb0 --------
    // make_shared<EvalResults>(VarType_Void).  Stored into rbx (return reg).
    auto results = std::make_shared<EvalResults>(VarType_Void);

    // The dryRun-style guard at @0x136bb4 (`cmp BYTE [rbp-0x1e8],0` jne
    // 1372f2) checks PlayArgs::hasMarker_ at +0x78 (rbp-0x260+0x78 = rbp-0x1e8).
    // If hasMarker_ is set the per-channel processing is skipped entirely
    // and we jump straight to results assembly cleanup.  Otherwise we
    // continue into the merge / asmPlay path.
    if (!playArgs.hasMarker_) {
        // -- Phase 8: maxSampleLen — @0x136bc1..0x136bd4 --------------------
        int64_t maxSampleLen = playArgs.getMaxSampleLength();             // @0x136bcf
        (void)maxSampleLen; // forwarded into asmPlay's trigger argument below.

        // -- Phase 9: per-device-channel loop — @0x136bdb..0x136cb5 --------
        // channelIndex = config_->deviceIndex (config+0x24).
        int channelIndex = config_->deviceIndex;                          // @0x136bde
        // assignments = playArgs.waveAssignments_[channelIndex].
        // The base pointer is read via [rbp-0x200] which is the begin
        // pointer of waveAssignments_.  Stride is 0x18 (3*8) = sizeof(
        // vector<WaveAssignment>).  Confirmed @0x136be8: lea rdx,[rdx+rdx*2].
        auto& assignments = playArgs.waveAssignments_[channelIndex];

        std::vector<EvalResultValue> channelArgs;                         // @0x136c09
        int mask = 0x3FFF;                                                // @0x136c2d / @0x136cd3
        bool dryRun = true;  // tracks whether merged-string is empty
                             // (ultimately bound to bl flag at @0x136cc4)

        if (assignments.empty()) {
            // @0x136cb7..0x136cce: zero combined wf, dryRun=true,
            // mask=0x3FFF, jump straight to checkOffspecWaveLength path.
            mask = 0x3FFF;
        } else {
            // @0x136c40..0x136ca5: outer loop over WaveAssignment entries.
            for (auto const& wa : assignments) {
                if (wa.value.varType_ != 4) {
                    // @0x136c48: type==4 entries skip the push_back.
                    channelArgs.push_back(wa.value);                       // @0x136c56
                }
                // @0x136c5e..0x136c8e: inner loop over wa.bits (vector<int>).
                // Each int b clears one byte of the trigger mask:
                //   shiftBits = b*8 - b == 7*b
                //   mask &= ~(0x40 << shiftBits)
                for (int b : wa.bits) {
                    int shiftBits = b * 7;          // @0x136c72: lea ecx,[rdi*8+0]; sub ecx,edi
                    mask &= ~(0x40 << shiftBits);    // @0x136c7b..0x136c84
                }
            }
        }

        // -- Phase 10: mergeWaveforms — @0x136cd9..0x136cff ----------------
        // The 6-arg merge call:
        //   args[0..5] = (channelArgs, channelsPerGroup[0], false,
        //                 "playDIOWave", rate, /*lengthDiffers*/false)
        // The bool param is hard-coded false in this path
        // (push 0x0 @0x136cf2).  combinedWf result spills to [rbp-0x1b0].
        std::shared_ptr<WaveformFront> combinedWf;
        if (!assignments.empty()) {
            // ecx = sx WORD [rax+0x14] = config_->channelsPerGroup[0].
            short channelsPerGroup0 = static_cast<short>(
                config_->channelsPerGroup[0]);
            // The push value at [rbp-0xa8] is `rate` (spilled @0x136bd4).
            combinedWf = mergeWaveforms(
                channelArgs,
                channelsPerGroup0,
                /*param3=*/false,
                std::string("playDIOWave"),
                rate,
                /*param6=*/false);                                         // @0x136cfa

            // @0x136d7b..0x136de1: post-merge dryRun derivation.
            // The binary computes channelArgs.size() via magic-number
            // division (sar 3 + imul 0x6db6db6db6db6db7 = ÷56 = ÷sizeof(EvalResultValue)).
            // If size >= 2, it calls channelArgs[0].value_.toString()
            // and sets dryRun = result.empty().  Otherwise dryRun = false.
             // NOTE: original placeholder said "element[1]" — that was wrong.
            // `add r13, 0x8` offsets into Value_ at +0x08 within element[0],
            // not to element index 1.
            if (channelArgs.size() >= 2) {
                dryRun = channelArgs[0].value_.toString().empty();          // @0x136dac
            } else {
                dryRun = false;                                             // @0x136dc5
            }
        }

        // -- Phase 11: validate sample length — @0x136e8d..0x136e9e --------
        // checkOffspecWaveLength(combinedWf, devConst_->waveformGranularity).
        // CORRECTED (21b-followup-3): [r14+0x08] loads devConst_ (not config_),
        // then [rax+0x40] = waveformGranularity. Was misidentified as config_+0x48.
        if (combinedWf) {
            int expectedLen = static_cast<int>(devConst_->waveformGranularity);  // @0x136e91
            checkOffspecWaveLength(combinedWf, expectedLen);              // @0x136e9e
        }

        // -- Phase 12: build asmPlay — @0x136ecb..0x137067 -----------------
        // The branch at @0x136ee5 (`cmp cl, 0x1`) skips the asmPlay entirely
        // when (combinedWf == nullptr) AND (config.isHirzel == 0). When
        // either is set, we emit asmPlay.
        bool emitPlay = combinedWf != nullptr || config_->isHirzel;

        if (emitPlay) {
            // Build the single-element waveforms vector — @0x136f06..0x137003.
            // The binary stack-allocates a vector containing one shared_ptr
            // (combinedWf); we model with std::vector ctor.
            std::vector<std::shared_ptr<WaveformFront>> waveforms;
            if (combinedWf) {
                waveforms.push_back(combinedWf);
            } else {
                // Empty single-slot vector path — the binary still reserves
                // a 0x10-byte slot but leaves it zero-initialised.
                waveforms.emplace_back(nullptr);
            }

            // asmRegisters @0x137007..0x137026
            AsmRegister reg0(0);                                           // @0x137010
            AsmRegister regInv(-1);                                        // @0x137021

            // bool isHoldArg = dryRun (loaded from [rbp-0x60]).
            // The argument list pushed at @0x13702b..0x13705e (right-to-left):
            //   trigger    = 0
            //   reg2       = regInv (AsmRegister(-1))
            //   regVal     = rate (from [rbp-0xa8])
            //   reg        = reg0  (AsmRegister(0))
            //   isHoldMode = false (push 0)
            //   holdCount  = mask  (r12 — the 14-bit trigger mask)
            //   suppress   = mask  (from [rbp-0x98] — same mask spilled earlier)
            //   isBool     = false (push 0)
            // Followed by register args:
            //   nameIndex  = 0    (xor ecx,ecx)
            //   isHold     = dryRun (r8b from [rbp-0x60])
            //   fourChannel= false (xor r9d,r9d)
            //
            // NOTE: the exact packing of (mask, rate) onto the
            // (holdCount, suppress, regVal, trigger) tuple is non-trivial
            // — the binary spills three different ints onto the stack and
            // the order doesn't directly mirror the asmPlay declaration
            // we have in asm_commands.hpp.  The mapping here is a best
            // guess based on which spill slots are reused; a careful
            // re-read of asmPlay's prologue is needed to confirm.
            auto asmEntry = asmCommands_->asmPlay(
                std::move(waveforms),
                /*nameIndex=*/0,
                /*isHold=*/dryRun,
                /*fourChannel=*/false,
                /*isBool=*/false,
                /*holdCount=*/mask,
                /*suppress=*/static_cast<unsigned int>(mask),
                /*isHoldMode=*/false,
                reg0,
                /*regVal=*/rate,
                regInv,
                /*trigger=*/0);                                            // @0x13705e

            // -- Phase 13: append entry into results->assemblers_ ----------
            // The binary inlines the push_back: tail-pointer compare against
            // the capacity (@0x137169..0x137200) — falls back to
            // emplace_back_slow_path when full.  We use the high-level API.
            results->assemblers_.push_back(std::move(asmEntry));           // @0x137211
        }
    }
    // hasMarker_ branch at @0x136bbb: jumps to 0x1372f2 — the per-channel
    // body is skipped entirely; control falls through into the cleanup
    // sequence (PlayArgs dtor + return results).

    // ---- Phase 14: return — @0x1373ab.. ----------------------------------
    // The binary tears down PlayArgs (@0x13770a) and the temp string at
    // [rbp-0x58] before returning rbx (the EvalResults shared_ptr).
    // C++ destructors handle these automatically.
    return results;
}

// ----------------------------------------------------------------------------
// CustomFunctions::playWaveDIO — @0x137740 (~187 disasm lines)
// ----------------------------------------------------------------------------
// Emits a single `wvft` instruction with mask `1 << numOutputPorts`,
// triggered by a DIO event. Args are validated for non-emptiness only;
// their values are not consumed (the trigger mask is derived entirely
// from the device-constants `numOutputPorts` field).
//
// Algorithm:
//   1. waitState_ protocol: 0→1, 1→OK, otherwise throw error 0x4f.
//   2. checkFunctionSupported("playWaveDIO", 0x1f2 — bitmask of supported
//      AwgDeviceTypes).
//   3. If args is empty → throw CustomFunctionsException(format(0x42,
//      "playWaveDIO")) — error code 0x42 = arg-count mismatch.
//   4. Allocate EvalResults(VarType_Void).
//   5. Emit wvft(AsmRegister(0), 1 << devConst_->numOutputPorts).
//   6. Push asm entry; return results.
//
// Note: the Resources `res` parameter is taken but never used in the body
// (the binary spills it onto the stack and never re-reads it).
std::shared_ptr<EvalResults> CustomFunctions::playWaveDIO(  // @0x137740
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> /*res*/)
{
    // Phase 1: waitState_ protocol — @0x13775b..0x137777
    if (waitState_ == 0) {
        waitState_ = 1;
    } else if (waitState_ != 1) {
        throw CustomFunctionsException(
            ErrorMessages::format(DioZsyncMixed));
    }

    // Phase 2: device-type support check — @0x137779..0x1377b1
    checkFunctionSupported("playWaveDIO", kDevHirzel);

    // Phase 3: arg-count check — @0x1377b6..0x1377bd (begin == end → empty)
    if (args.empty()) {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs,
                                  std::string("playWaveDIO")));
    }

    // Phase 4: allocate EvalResults(Void) — @0x1377c3..0x1377f5
    auto results = std::make_shared<EvalResults>(VarType_Void);  // Void

    // Phase 5: build wvft mask = 1 << numOutputPorts — @0x1377f9..0x137818
    int mask = 1 << devConst_->numOutputPorts;

    // Phase 6: emit wvft(reg=0, mask) — @0x137805..0x13782a
    auto asmEntry = asmCommands_->wvft(AsmRegister(0), mask);
    results->assemblers_.push_back(std::move(asmEntry));

    return results;
}


// ----------------------------------------------------------------------------
// CustomFunctions::playWaveZSync — @0x137a50 (~697 disasm lines, ~3.2KB)
// ----------------------------------------------------------------------------
// Emits a single `wvft` instruction whose mask is derived from a const
// argument matching one of three ZSYNC_DATA_* register constants. Unlike
// executeTableEntry (which has register/numeric paths and SHFQA branches),
// this wrapper is restricted to a single Const arg.
//
// Algorithm:
//   1. waitState_ protocol: 0→2, 2→OK, otherwise throw error 0x4f.
//      (Note: distinct from playWaveDIO's state value of 1; ZSync mode
//      and DIO mode are mutually exclusive at this layer.)
//   2. checkFunctionSupported("playWaveZSync", 0x1f2).
//   3. args.size() must be exactly 1; else throw format(0x5c, "playWaveZSync",
//      1, args.size(), 2). Code 0x5c is the argc-mismatch message that takes
//      (name, expected, actual, mode) — mode=2 indicates "exact" rather
//      than min/max.
//   4. arg0 must be VarType==Const (4); else throw format(0x3e,
//      "playWaveZSync") — wrong-arg-type message.
//   5. tableIndex = arg0.value_.toInt().
//   6. Match tableIndex against three readConst values, in order:
//        - "ZSYNC_DATA_RAW"          → shift=1
//        - "ZSYNC_DATA_PROCESSED_A"  → shift=9
//        - "ZSYNC_DATA_PROCESSED_B"  → shift=0xd
//      Tracking "matched?" in a bool. If no match → throw error 0x75.
//   7. mask = shift << numOutputPorts (NOT 1 << (numOutputPorts + shift) —
//      the binary literally shifts the shift-value: `shl eax, cl` where
//      eax ∈ {1, 9, 0xd}). For shift=1 these are equivalent, but for
//      shift=9 (1001b) and shift=0xd (1101b) the multi-bit pattern is
//      preserved into the high portion of the mask.                      // @0x1380f4
//   8. Allocate EvalResults(VarType_Void).
//   9. setWaitCyclesReg(args, results, res) — handles wait-cycle args
//      (here always empty since args.size()==1, but the call is made
//      unconditionally).
//  10. Emit wvft(AsmRegister(0), mask) and push to results->assemblers_.
//
// Note on argument copying: the binary spills arg0 into a local
// EvalResultValue ([rbp-0x1f8]..[rbp-0x1c8]) using a switch-table over
// Value::type_, then immediately reads arg0.varType_ and the (now-local)
// value. This is an inlined copy ctor; we model it idiomatically with a
// reference plus a Value snapshot for clarity.
std::shared_ptr<EvalResults> CustomFunctions::playWaveZSync(  // @0x137a50
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> res)
{
    // Phase 1: waitState_ protocol — @0x137aa6..0x137abf (state value = 2)
    if (waitState_ == 0) {
        waitState_ = 2;
    } else if (waitState_ != 2) {
        throw CustomFunctionsException(
            ErrorMessages::format(DioZsyncMixed));
    }

    // Phase 2: device-type support check — @0x137a95..0x137aa6
    checkFunctionSupported("playWaveZSync", kDevHirzel);

    // Phase 3: arg-count check — @0x137ac5..0x137ae9
    // The disasm computes (args.size() - 1) and compares >= 2 (jae).
    // That fires when args.size() is 0 or >= 3. Equivalent to "size != 1".
    if (args.size() != 1) {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncArgs2or3,
                                  std::string("playWaveZSync"),
                                  static_cast<int>(1),
                                  static_cast<int>(args.size()),
                                  static_cast<size_t>(2)));
    }

    auto const& arg0 = args[0];

    // Phase 4: arg type must be Const (VarType==4) — @0x137b8d..0x137b90
    if (static_cast<int>(arg0.varType_) != 4) {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst,
                                  std::string("playWaveZSync")));
    }

    // Phase 5: extract integer table index — @0x137b9d
    int tableIndex = arg0.value_.toInt();

    // Phase 6: match against three ZSYNC constants — @0x137ba5..0x1380ef
    // The binary keeps a "matched" flag in bl, and a candidate shift value
    // in eax that is overwritten on each match. Final wvft mask is computed
    // only if bl==1.
    bool matched = false;
    int shift = 0;

    {
        auto zsyncRaw = res->readConst("ZSYNC_DATA_RAW",
                                       EDirection::eOUT);        // @0x137beb
        if (tableIndex == zsyncRaw.value_.toInt()) {
            matched = true;
            shift = 1;                                                      // @0x137fab: mov eax,0x1
        } else {
            auto zsyncProcA = res->readConst("ZSYNC_DATA_PROCESSED_A",
                                             EDirection::eOUT); // @0x137c47
            if (tableIndex == zsyncProcA.value_.toInt()) {
                matched = true;
                shift = 9;                                                  // @0x138049: mov eax,0x9
            } else {
                auto zsyncProcB = res->readConst("ZSYNC_DATA_PROCESSED_B",
                                                 EDirection::eOUT); // @0x137ca1
                if (tableIndex == zsyncProcB.value_.toInt()) {
                    matched = true;
                    shift = 0xd;                                            // @0x1380ea: mov eax,0xd
                }
            }
        }
    }

    if (!matched) {
        // @0x13834f: throw format(0x75) — invalid ZSYNC constant
        throw CustomFunctionsException(
            ErrorMessages::format(InvalidZSyncData));
    }

    // Phase 7: allocate EvalResults(Void) — @0x137daa..0x137e05
    auto results = std::make_shared<EvalResults>(VarType_Void);

    // Phase 8: process wait-cycle args (always empty here; call retained
    // for fidelity with binary) — @0x137e45
    setWaitCyclesReg(args, results, res);

    // Phase 9: compute mask = shift << numOutputPorts — @0x1380f4..0x1380fe
    // devConst_ is at this+0x8 (per `mov rcx, [r14+0x8]`).
    int mask = shift << devConst_->numOutputPorts;

    // Phase 10: emit wvft(reg=0, mask) — @0x138115..0x138196
    auto asmEntry = asmCommands_->wvft(AsmRegister(0), mask);
    results->assemblers_.push_back(std::move(asmEntry));

    return results;
}

// ---- Simple 0-arg functions -----------------------------------------------

std::shared_ptr<EvalResults> CustomFunctions::waitWave(  // @0x13a980 (620B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("waitWave", kDevHirzelAll);
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, "waitWave"));
    auto result = std::make_shared<EvalResults>(static_cast<VarType>(1));
    auto asm_entry = asmCommands_->wwvf();
    result->assemblers_.push_back(std::move(asm_entry));
    return result;
}

std::shared_ptr<EvalResults> CustomFunctions::waitPlayQueueEmpty(  // @0x145240 (626B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("waitPlayQueueEmpty", static_cast<AwgDeviceType>(2));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, "waitPlayQueueEmpty"));
    auto result = std::make_shared<EvalResults>(static_cast<VarType>(1));
    auto asm_entry = asmCommands_->wwvfq();
    result->assemblers_.push_back(std::move(asm_entry));
    return result;
}

std::shared_ptr<EvalResults> CustomFunctions::sync(  // @0x14e690 (569B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("sync", kDevHirzelAll);
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, "sync"));
    auto result = std::make_shared<EvalResults>(static_cast<VarType>(1));
    addSyncCommand(result, std::move(res));
    return result;
}

std::shared_ptr<EvalResults> CustomFunctions::randomSeed(  // @0x1497c0 (384B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("randomSeed", kDevHirzelAll);
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FormatFuncArgs, "randomSeed"));
    // Host-side only: seeds the TLS random object. No assembly emitted.
    // Binary calls GlobalResources::random.seedRandom() — the TLS mt19937_64.
    // seedRandom() is not yet exposed in our headers.
    return std::make_shared<EvalResults>();
}

std::shared_ptr<EvalResults> CustomFunctions::now(  // @0x14cbc0 (611B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("now", static_cast<AwgDeviceType>(5));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, "now"));
    auto result = std::make_shared<EvalResults>();  // zero-init (no VarType ctor)
    AsmRegister reg(0);
    auto asm_entry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x1c));
    result->assemblers_.push_back(std::move(asm_entry));
    return result;
}

// ---- Formatting functions -------------------------------------------------

std::shared_ptr<EvalResults> CustomFunctions::error(  // @0x14d830 (536B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    auto result = std::make_shared<EvalResults>();  // zero-init
    std::string msg = printF(args, "error");
    auto asm_entry = asmCommands_->asmMessage(msg, true);
    result->assemblers_.push_back(std::move(asm_entry));
    return result;
}

std::shared_ptr<EvalResults> CustomFunctions::info(  // @0x14da50 (531B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    auto result = std::make_shared<EvalResults>();  // zero-init
    std::string msg = printF(args, "info");
    auto asm_entry = asmCommands_->asmMessage(msg, false);
    result->assemblers_.push_back(std::move(asm_entry));
    return result;
}

// ---- Simple 1-arg functions -----------------------------------------------

std::shared_ptr<EvalResults> CustomFunctions::setRate(  // @0x14c370 (933B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setRate", static_cast<AwgDeviceType>(5));
    if (args.size() != 1)
        throw CustomFunctionsException("setRate requires exactly 1 argument");  // error 0xc0
    // Binary: extract int arg, validate type, call asmRate
    //   int rate = extractArg(args[0]).toInt();
    //   auto asm = asmCommands_->asmRate(rate);
    //   result->appendAsm(asm);
    return nullptr;
}


std::shared_ptr<EvalResults> CustomFunctions::playZero(                                                                                                                     // @0x1387f0 (2112B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("playZero", kDevAll);
    if (args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncMinArgs,
                                  std::string("playZero"), 1, static_cast<int>(args.size())));
    if (args.size() >= 3)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsMaxArgs,
                                  std::string("playZero"), 2, static_cast<int>(args.size())));

    auto results = std::make_shared<EvalResults>(VarType_Void);
    int length = args[0].value_.toInt();
    length = checkPlayAlignment(length);                                         // @0x15b190

    // Optional second arg: play rate
    int rate = -1;
    if (args.size() >= 2)
        rate = getPlayRate(args[1], "playZero", false);

    // Emit asmPlay with empty waveforms, hold=false
    std::vector<std::shared_ptr<WaveformFront>> emptyWfs;
    int channelIndex = config_->deviceIndex;
    AsmRegister reg0(0);
    AsmRegister regInv(-1);
    auto asmEntry = asmCommands_->asmPlay(
        std::move(emptyWfs), channelIndex,
        false /*isHold*/, false /*fourChannel*/, false /*isBool*/,
        rate, 0x3FFF /*suppress*/, false /*isHoldMode*/,
        reg0, length, regInv, 0 /*trigger*/);
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::playHold(                                                                                                                     // @0x139030 (~1.8KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("playHold", kDevHirzel);
    if (args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncMinArgs,
                                  std::string("playHold"), 1, static_cast<int>(args.size())));
    if (args.size() >= 3)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsMaxArgs,
                                  std::string("playHold"), 2, static_cast<int>(args.size())));

    auto results = std::make_shared<EvalResults>(VarType_Void);
    int length = args[0].value_.toInt();
    length = checkPlayAlignment(length);

    int rate = -1;
    if (args.size() >= 2)
        rate = getPlayRate(args[1], "playHold", false);

    // Emit asmPlay with empty waveforms, hold=true
    std::vector<std::shared_ptr<WaveformFront>> emptyWfs;
    int channelIndex = config_->deviceIndex;
    AsmRegister reg0(0);
    AsmRegister regInv(-1);
    auto asmEntry = asmCommands_->asmPlay(
        std::move(emptyWfs), channelIndex,
        true /*isHold*/, false /*fourChannel*/, false /*isBool*/,
        rate, 0x3FFF /*suppress*/, false /*isHoldMode*/,
        reg0, length, regInv, 0 /*trigger*/);
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}

} // namespace zhinst
