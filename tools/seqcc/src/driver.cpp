// =============================================================================
// seqcc — owned compilation driver (Phase T5a.1 skeleton, T5a.2 owned flow)
//
// See driver.hpp for the multi-sub-phase roadmap.
//
// Pre-T5a.2: `compile()` was a thin pass-through to
// `compileSeqcWithIR()`, kept byte-identical to the legacy
// `compile.cpp::runCompile()` path.
//
// T5a.2 (this revision): `compile()` now owns the outer compile flow
// — config marshalling, `AWGCompiler` construction, waveform
// registration, source compile, ELF capture, info-JSON assembly, and
// IR-sink population.  The body mirrors `compileSeqcImpl` in
// `reconstructed/src/codegen/compile_seqc.cpp` line-for-line (modulo
// the packed-string assembly, which is now done in the driver from
// separate `elf` / `infoJson` fields rather than from a `'\0'`-
// glued blob).
//
// The two anonymous-namespace helpers from `compile_seqc.cpp`
// (`parseSequencerType`, `dispatchGetAwgDeviceProps`) are not
// reachable from outside that TU, so the driver re-implements them
// here.  Both are pure dispatch on enum values; behaviour is
// preserved by construction.  Promoting them out of the anonymous
// namespace would constitute a recon edit, which T5a explicitly
// avoids per the file-level roadmap comment.
//
// Routing: gated by `SEQCC_USE_OWNED_DRIVER` (CMake option, OFF by
// default through T5a.2; flipped ON in T5a.3 after the A/B fixture
// confirms byte-identical output across the device matrix).  When
// the gate is OFF this TU is still compiled and linked but no call
// site references `SeqcDriver`.
// =============================================================================

#include "driver.hpp"

#include "zhinst/codegen/awg_compiler.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/codegen/compile_seqc.hpp"
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

    // ---- 8-12. Compile (L297-348).  ELF capture and error wrapping
    //            match compileSeqcImpl exactly; the only structural
    //            difference is that we keep `elf` / `infoJson`
    //            separate rather than packing them with a NUL byte. ----
    zhinst::AWGCompiler compiler(config);
    if (!waveformPaths.empty()) {
        compiler.addWaveforms(waveformPaths);
    }

    std::string elfData;
    std::string jsonResult;
    try {
        compiler.compileString(source);

        std::ostringstream oss;
        compiler.writeToStream(oss, "output");
        elfData = oss.str();

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

    // ---- 13. Populate IR sinks (L350-360). ----
    //
    // The driver always captures sinks (cheap shared_ptr copies that
    // outlive `compiler` because we move the result out below).  The
    // legacy `compile.cpp` path conditionally chose
    // `compileSeqc()` vs `compileSeqcWithIR()` based on whether any
    // consumer needed the IR; this driver elides that branching since
    // the cost of always populating sinks is negligible and it
    // simplifies the T5b stepwise API.
    zhinst::fillIntrospection(compiler, result.sinks);

    result.elf = std::move(elfData);
    result.infoJson = std::move(jsonResult);
    return result;
}

}  // namespace seqcc
