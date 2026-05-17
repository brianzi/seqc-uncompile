// =============================================================================
// seqcc — owned compilation driver (Phase T5a.1 skeleton, T5a.2 owned flow,
//         T5b.5 stepwise back end).
//
// See driver.hpp for the multi-sub-phase roadmap.
//
// Pre-T5a.2: `compile()` was a thin pass-through to
// `compileSeqcWithIR()`, kept byte-identical to the legacy
// `compile.cpp::runCompile()` path.
//
// T5a.2: `compile()` took ownership of the outer compile flow
// — config marshalling, `AWGCompiler` construction, waveform
// registration, source compile, ELF capture, info-JSON assembly, and
// IR-sink population — line-for-line mirroring `compileSeqcImpl()`
// in `reconstructed/src/codegen/compile_seqc.cpp`.
//
// T5b.5: `compile()` drives the back end stage by stage via the three
// `AWGCompiler::stepXxx()` forwarders that wrap the new
// `AWGCompilerImpl::stepInnerCompile` / `stepAssembleOpcodes` /
// `stepCheckLimits` partition.  The driver short-circuits between
// the steps based on `opts.toStage`:
//   - `--to=lower` / `--to=asm` stop after `stepInnerCompile`.
//     `assemblerText_` is populated by then (suffices for `-S`); the
//     lowered AST / wavetable handles are populated by then too
//     (suffices for `-E`).  `stepAssembleOpcodes` / `stepCheckLimits`
//     / `writeToStream` are skipped.
//   - `--to=link` (default) runs all three back-end steps then
//     `writeToStream` as before.
//
// T6.2 (one-way diagnostic): `--to=asm-pre` drives the inner
// `Compiler` directly to capture its `AsmList` at the natural cut
// point (post-`stepOptPre`, pre-`stepPrefetch`) into
// `IRSinks::preprefetchAsmText`.  Back end is skipped — there is no
// ELF and no assembler text.  This is a one-way dump only: the
// round-trip resume path (`--from=asm`) was demoted to Q3 because
// reconstructing the post-`stepOptPre` state cleanly would require
// transporting additional sidecars (WavetableIR, Compiler::ast_,
// possibly more) which the user judged not worth pursuing.
//
// The two anonymous-namespace helpers from `compile_seqc.cpp`
// (`parseSequencerType`, `dispatchGetAwgDeviceProps`) are not
// reachable from outside that TU, so the driver re-implements them
// here.  Both are pure dispatch on enum values; behaviour is
// preserved by construction.
//
// Routing: gated by `SEQCC_USE_OWNED_DRIVER` (CMake option, ON since
// T5a.4).  When the gate is OFF this TU is still compiled and linked
// but no call site references `SeqcDriver`.
// =============================================================================

#include "driver.hpp"

#include "zhinst/asm/asm_list.hpp"
#include "zhinst/codegen/awg_compiler.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/codegen/compiler.hpp"
#include "zhinst/core/exception.hpp"
#include "zhinst/device/awg_device_props.hpp"
#include "zhinst/device/device_type.hpp"
#include "zhinst/io/zi_folder.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace seqcc {

namespace {

// Local re-implementation of compile_seqc.cpp's anonymous-namespace
// `parseSequencerType`.  Verified byte-equivalent: upper-case the
// input, match against "QA" / "SG", everything else (including
// "AUTO" and "") maps to Auto.
zhinst::AwgSequencerType parseSequencerType(std::string const& s) {
    std::string upper = boost::to_upper_copy(s);
    if (upper == "QA") return zhinst::AwgSequencerType::QA;
    if (upper == "SG") return zhinst::AwgSequencerType::SG;
    return zhinst::AwgSequencerType::Auto;
}

// Local re-implementation of compile_seqc.cpp's anonymous-namespace
// `dispatchGetAwgDeviceProps`.  The recon TU uses a hand-written
// switch over the 9 one-hot AwgDeviceType values to dispatch to the
// correct `getAwgDeviceProps<>` template instantiation — we mirror
// that exactly here.
zhinst::AwgDeviceProps
dispatchGetAwgDeviceProps(zhinst::AwgDeviceType dt,
                          zhinst::DeviceType const& devType) {
    using DT = zhinst::AwgDeviceType;
    switch (dt) {
    case DT::UHFLI:    return zhinst::getAwgDeviceProps<DT::UHFLI>(devType);
    case DT::HDAWG:    return zhinst::getAwgDeviceProps<DT::HDAWG>(devType);
    case DT::UHFQA:    return zhinst::getAwgDeviceProps<DT::UHFQA>(devType);
    case DT::SHFQA:    return zhinst::getAwgDeviceProps<DT::SHFQA>(devType);
    case DT::SHFSG:    return zhinst::getAwgDeviceProps<DT::SHFSG>(devType);
    case DT::SHFQC_SG: return zhinst::getAwgDeviceProps<DT::SHFQC_SG>(devType);
    case DT::SHFLI:    return zhinst::getAwgDeviceProps<DT::SHFLI>(devType);
    case DT::GHFLI:    return zhinst::getAwgDeviceProps<DT::GHFLI>(devType);
    case DT::VHFLI:    return zhinst::getAwgDeviceProps<DT::VHFLI>(devType);
    default:
        throw zhinst::Exception("Unsupported AWG device type");
    }
}

// T5b.5: decide whether the driver should run the back-end steps
// (`stepAssembleOpcodes` + `stepCheckLimits` + `writeToStream`) for a
// given `--to=<stage>` value.  `lower`, `asm`, and (T6.2) `asm-pre`
// short-circuit; `link` (default) does not.  Unknown stages reach
// the driver only when CLI validation has been bypassed, so we treat
// them as `link` to preserve full-pipeline behaviour.
bool needsBackEnd(std::string const& stage) {
    return !(stage == "lower" || stage == "asm" || stage == "asm-pre");
}

}  // namespace

SeqcDriver::SeqcDriver(Options const& opts)
    : opts_(opts) {}

DriverResult SeqcDriver::compile(std::string const& source,
                                 std::string const& jsonConfig,
                                 std::string const& devoptsStr) {
    DriverResult result;

    // ---- 1. Parse JSON config (matches compileSeqcImpl L159-167). ----
    boost::json::value jv;
    try {
        jv = boost::json::parse(jsonConfig);
    } catch (...) {
        jv = boost::json::object{};
    }
    auto& jobj = jv.as_object();

    // ---- 2. Sequencer hint (L169-175). ----
    zhinst::AwgSequencerType seqType = zhinst::AwgSequencerType::Auto;
    if (auto* v = jobj.if_contains("sequencer"); v && v->is_string()) {
        seqType = parseSequencerType(std::string(v->as_string()));
    }

    // ---- 3. Upper-case deviceId and options (L177-180). ----
    std::string deviceId = opts_.devtype;
    boost::to_upper(deviceId);
    std::string upperOptions = boost::to_upper_copy(devoptsStr);

    // ---- 4. Resolve DeviceType / AwgDeviceType / AwgDeviceProps
    //         (L182-193). ----
    zhinst::DeviceType devType(deviceId, upperOptions);
    zhinst::DeviceTypeCode dtCode = devType.code();
    zhinst::AwgDeviceType awgDT = zhinst::toAwgDeviceType(dtCode, seqType);
    if (static_cast<int>(awgDT) == 0) {
        throw zhinst::Exception(
            zhinst::makeUnsupportedAwgSequencerErrorMessage(dtCode, seqType));
    }
    zhinst::AwgDeviceProps props = dispatchGetAwgDeviceProps(awgDT, devType);

    // ---- 5. Default waveform search path (L195-201). ----
    zhinst::ZiFolder dataFolder =
        zhinst::ZiFolder::ziFolder(zhinst::ZiFolder::DirectoryType::Data);
    std::string dataFolderPath = dataFolder.folderPath("", "");
    boost::filesystem::path waveSearchPath(dataFolderPath);
    waveSearchPath /= "awg";
    waveSearchPath /= "waves";

    // ---- 6. Pull remaining config fields out of JSON (L203-240). ----
    double sampleRate = std::numeric_limits<double>::quiet_NaN();
    if (auto* v = jobj.if_contains("samplerate")) {
        if (v->is_double()) sampleRate = v->as_double();
        else if (v->is_int64()) sampleRate = static_cast<double>(v->as_int64());
    }

    std::string filename;
    if (auto* v = jobj.if_contains("filename"); v && v->is_string()) {
        filename = std::string(v->as_string());
    }

    std::string configWavePath;
    if (auto* v = jobj.if_contains("wavepath"); v && v->is_string()) {
        configWavePath = std::string(v->as_string());
    }

    std::vector<std::string> waveformPaths;
    if (auto* v = jobj.if_contains("waveforms"); v && v->is_string()) {
        std::string wfStr(v->as_string());
        boost::split(waveformPaths, wfStr, boost::is_any_of(";"),
                     boost::token_compress_on);
        waveformPaths.erase(
            std::remove_if(waveformPaths.begin(), waveformPaths.end(),
                           [](std::string const& s) { return s.empty(); }),
            waveformPaths.end());
    }

    // T8/IF-305: optional AsmOptimize bitmask override (L242-263).
    std::uint64_t optimizationFlags = 0xFF;
    if (auto* v = jobj.if_contains("optimizationFlags")) {
        if (v->is_int64()) {
            int64_t i = v->as_int64();
            if (i >= 0) optimizationFlags = static_cast<std::uint64_t>(i);
        } else if (v->is_uint64()) {
            optimizationFlags = v->as_uint64();
        } else if (v->is_double()) {
            double d = v->as_double();
            if (d >= 0.0) optimizationFlags = static_cast<std::uint64_t>(d);
        }
    }

    // ---- 7. Populate AWGCompilerConfig (L265-290). ----
    zhinst::AWGCompilerConfig config{};
    config.deviceType = props.deviceType;
    config.sampleFormat = zhinst::SampleFormat(props.sampleFormat);
    config.deviceSampleRate = sampleRate;
    config.addressImpl = props.addressImpl;
    config.channelsPerGroup[0] = 0x0002;
    config.channelsPerGroup[1] = 0x0004;
    config.isHirzel = props.isHirzel;
    config.cacheType =
        (props.deviceType == zhinst::AwgDeviceType::HDAWG) ? 1 : 0;
    config.numChannelGroups = 1;
    config.awgIndex = static_cast<int32_t>(opts_.awgIndex);
    config.deviceIndex = 0;
    config.optimizationFlags = optimizationFlags;
    config.numCores = 0;
    config.loopUnrollLimit = 0x20000;

    if (!filename.empty()) {
        config.debugDumpPath = filename;
    }
    if (!configWavePath.empty()) {
        config.searchPath = boost::filesystem::path(configWavePath);
    } else {
        config.searchPath = waveSearchPath;
    }

    // ---- 8. Construct compiler + register external waveforms. ----
    zhinst::AWGCompiler compiler(config);
    if (!waveformPaths.empty()) {
        compiler.addWaveforms(waveformPaths);
    }

    // ---- 9-12. Run the inner compile (one of two strategies) +
    //            back end (when required).  See file-level comment
    //            for the path matrix. ----
    bool const runBackEnd = needsBackEnd(opts_.toStage);
    bool const isAsmPre   = (opts_.toStage == "asm-pre");

    std::string elfData;
    std::string jsonResult;
    try {
        // ---- 9. Inner compile — dispatch on path. ----
        if (isAsmPre) {
            // T6.2 front-end-only path (one-way diagnostic dump).
            // Drive the inner Compiler stage by stage so we can
            // capture its `AsmList` at the natural cut point
            // (post-`stepOptPre`, pre-`stepPrefetch`) — see IF-308.
            // No back end runs; sinks are populated only with
            // `preprefetchAsmText` below.
            //
            // We deliberately don't go through `stepInnerCompile`
            // because that one drives all front-end steps to
            // completion and we want to stop after `stepOptPre`.
            // The device-type validation that `stepInnerCompile`
            // performs is also not needed here — `--to=asm-pre` is
            // a front-end dump and the device-type only constrains
            // back-end opcode assembly.
            auto& inner = compiler.compiler();
            inner.reset();
            // stepParse may return an engaged optional (empty input
            // early-out).  In that case there is no asm to capture;
            // leave `preprefetchAsmText` empty so the dump shows the
            // input was a no-op program.
            auto early = inner.stepParse(source);
            if (!early.has_value()) {
                inner.stepToSeqCAst();
                inner.stepLower();
                inner.stepBuildAsmPreamble();
                inner.stepOptPre();
                result.sinks.preprefetchAsmText =
                    inner.asmList().serialize();
            }
        } else {
            // T5b.5: step 9 — inner Compiler::compile + asm-text
            // build + message append.  Always required on the
            // from-source path (the IR sinks would otherwise be
            // empty and `-E` would have nothing to emit).
            compiler.stepInnerCompile(source);
        }

        if (runBackEnd) {
            // T5b.5: step 10 + 11+12 — AsmList → opcode words, then
            // device-limit enforcement.  Skipped for --to=lower and
            // --to=asm.
            compiler.stepAssembleOpcodes();
            compiler.stepCheckLimits();

            // Produce the ELF only when the back end ran — otherwise
            // the assembler has no opcodes and writeToStream would
            // throw `EmptyInput`.
            std::ostringstream oss;
            compiler.writeToStream(oss, "output");
            elfData = oss.str();
        }

        std::string compileReport = compiler.getCompileReport();
        std::string waveMemInfo = compiler.getJsonWaveformMemoryInfo();

        boost::json::object info;
        info["messages"] = compileReport;
        info["maxelfsize"] = props.maxElfSize;
        if (!waveMemInfo.empty()) {
            try {
                info["wavemem"] = boost::json::parse(waveMemInfo);
            } catch (...) {
                info["wavemem"] = boost::json::object{};
            }
        } else {
            info["wavemem"] = boost::json::object{};
        }
        jsonResult = boost::json::serialize(info);
    } catch (zhinst::Exception const& ex) {
        std::string report = compiler.getCompileReport();
        std::string msg = "Compilation failed: ";
        if (!report.empty()) {
            msg += report;
        } else {
            msg += ex.what();
            msg += "\n";
        }
        throw zhinst::Exception(msg);
    }

    // ---- 13. Populate IR sinks. ----
    //
    // The driver always captures sinks (cheap shared_ptr copies that
    // outlive `compiler` because we move the result out below).  The
    // pre-T10a `compile.cpp` path conditionally chose
    // `compileSeqc()` vs `compileSeqcWithIR()` based on whether any
    // consumer needed the IR; this driver elides that branching since
    // the cost of always populating sinks is negligible and it
    // simplifies the T5b stepwise API.
    //
    // T10a: captured directly via the public `compiler() →
    // Compiler::{ast(), wavetable(), asmList()}` accessors, replacing
    // the pre-T10a `fillIntrospection()` friend helper and the
    // `CompileSeqcIntrospection` carrier struct.  The `asmList()`
    // accessor returns a const ref to the on-`Compiler` member —
    // copy it into a fresh `shared_ptr` so the snapshot survives the
    // `AWGCompiler` destructor.  `ast()` / `wavetable()` already
    // return `shared_ptr` to the same recon-side objects.
    //
    // T6.2: skip the IR capture on the `--to=asm-pre` path.  That
    // path stops after the inner `stepOptPre`, before
    // `stepPrefetch` / `stepOptPost` / `stepProject`, so the
    // AWGCompiler-side state (`compileString_asmList_`,
    // `wavetableIR_`, `assemblerText_`) is empty or partial and the
    // `asm-pre` dump path consumes only `sinks.preprefetchAsmText`,
    // which was populated above.
    if (!isAsmPre) {
        zhinst::Compiler const& innerCompiler = compiler.compiler();
        result.sinks.loweredAst = innerCompiler.ast();
        result.sinks.wavetable  = innerCompiler.wavetable();
        result.sinks.asmList    =
            std::make_shared<zhinst::AsmList>(innerCompiler.asmList());
        result.sinks.assemblerText = compiler.assemblerText();
    }

    result.elf = std::move(elfData);
    result.infoJson = std::move(jsonResult);
    return result;
}

}  // namespace seqcc
