// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Function: zhinst::compileSeqc @0xf58a0
//
// Original source: /builds/labone/labone/api/common/src/seqc_compiler.cpp
//
// Signature:
//   std::string compileSeqc(const std::string& jsonConfig,
//                           std::string sourceCode,
//                           std::string deviceId,
//                           unsigned long awgIndex,
//                           const std::string& options)
//
// Returns a pair of strings serialized: first is JSON result object,
// second is ELF binary data.
// ============================================================================

#include "zhinst/codegen/awg_compiler.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/device/awg_device_props.hpp"
#include "zhinst/device/device_type.hpp"
#include "zhinst/core/exception.hpp"
#include "zhinst/io/zi_folder.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace zhinst {

// Forward declarations for helpers used by compileSeqc
std::vector<std::string> splitDeviceOptions(std::string const& opts);  // @0x2d0460

// Anonymous namespace for local helpers
namespace {

// Map sequencer type string to AwgSequencerType enum
// Decoded from compileSeqc @0xf5b50..0xf5bce: uppercased string compared
// against "QA" and "SG"; anything else (including "AUTO") maps to Auto.
AwgSequencerType parseSequencerType(std::string const& seqStr) {
    std::string upper = boost::to_upper_copy(seqStr);
    if (upper == "QA") return AwgSequencerType::QA;
    if (upper == "SG") return AwgSequencerType::SG;
    return AwgSequencerType::Auto;
}

// Dispatch to the correct getAwgDeviceProps<> specialization based on
// the AwgDeviceType enum value.  Binary uses a jump table over the 9
// one-hot values (1,2,4,8,16,32,64,128,256).
AwgDeviceProps dispatchGetAwgDeviceProps(AwgDeviceType dt, DeviceType const& devType) {
    switch (dt) {
    case AwgDeviceType::UHFLI:    return getAwgDeviceProps<AwgDeviceType::UHFLI>(devType);
    case AwgDeviceType::HDAWG:    return getAwgDeviceProps<AwgDeviceType::HDAWG>(devType);
    case AwgDeviceType::UHFQA:    return getAwgDeviceProps<AwgDeviceType::UHFQA>(devType);
    case AwgDeviceType::SHFQA:    return getAwgDeviceProps<AwgDeviceType::SHFQA>(devType);
    case AwgDeviceType::SHFSG:    return getAwgDeviceProps<AwgDeviceType::SHFSG>(devType);
    case AwgDeviceType::SHFQC_SG: return getAwgDeviceProps<AwgDeviceType::SHFQC_SG>(devType);
    case AwgDeviceType::SHFLI:    return getAwgDeviceProps<AwgDeviceType::SHFLI>(devType);
    case AwgDeviceType::GHFLI:    return getAwgDeviceProps<AwgDeviceType::GHFLI>(devType);
    case AwgDeviceType::VHFLI:    return getAwgDeviceProps<AwgDeviceType::VHFLI>(devType);
    default:
        throw Exception("Unsupported AWG device type");
    }
}

}  // anonymous namespace

// ============================================================================
// compileSeqc @0xf58a0
//
// Binary flow (approx 5KB, 0xf58a0..0xf6d00):
//
//   1. Parse 1st arg (jsonConfig) as boost::json::object       @0xf5928
//   2. Extract "sequencer" field, uppercase, map to enum       @0xf5b50
//   3. Uppercase deviceId (arg3) and options (arg5) strings    @0xf5c20
//   4. Construct DeviceType(deviceId, options)                 @0xf5c80
//      → code() → toAwgDeviceType(code, seqType)
//      → dispatchGetAwgDeviceProps(awgDT, devType)
//   5. Build waveform search path:
//      ZiFolder::ziFolder(Data) → folderPath() / "awg" / "waves"
//   6. Extract "samplerate", "filename", "wavepath", "waveforms" from JSON
//   7. Populate AWGCompilerConfig on stack                     @0xf6300
//   8. Create AWGCompiler(config)                              @0xf6500
//   9. compiler.addWaveforms(waveformPaths)                    @0xf6560
//  10. compiler.compileString(sourceCode)                      @0xf6580
//  11. compiler.writeToStream(oss, "outputut")                 @0xf65a0
//  12. Seek stream to get ELF binary data
//  13. Build result boost::json::object:
//        "messages"    ← compiler.getCompileReport()
//        "maxelfsize"  ← AwgDeviceProps.maxElfSize
//        "wavemem"     ← compiler.getJsonWaveformMemoryInfo()
//  14. Serialize JSON → first return string; ELF → second
//  15. On exception (zhinst::Exception): get report, prepend
//      "Compilation failed: ", re-throw
// ============================================================================
//! \brief Compile a SeqC source file for a Zurich Instruments AWG and
//!        return the produced ELF together with a JSON status report.
//!
//! \details This is the C++ entry point that backs the Python
//! `compile_seqc(...)` binding.  The high-level pipeline is:
//!   1. Parse `jsonConfig` as a Boost.JSON object and extract the
//!      sequencer hint, samplerate, optional debug filename, optional
//!      wave-search path, and a `';'`-separated waveform path list.
//!   2. Resolve the target device by uppercasing `deviceId`/`options`,
//!      constructing a `DeviceType`, mapping it to an `AwgDeviceType`
//!      via the sequencer hint, and looking up the per-device
//!      `AwgDeviceProps` (throws `Exception` on an unsupported
//!      device/sequencer combination).
//!   3. Build a default waveform search path under the LabOne data
//!      folder (`<data>/awg/waves`) unless `wavepath` overrides it.
//!   4. Populate an `AWGCompilerConfig` from the device properties and
//!      JSON fields, construct an `AWGCompiler`, register waveform
//!      paths, and run `compileString(sourceCode)` followed by
//!      `writeToStream` to obtain the ELF image.
//!   5. Build a JSON result object carrying the compile report
//!      (`"messages"`), the device's maximum ELF size
//!      (`"maxelfsize"`), and the wave-memory usage info
//!      (`"wavemem"`).
//!
//! On a `zhinst::Exception` from the compiler the function prepends
//! `"Compilation failed: "` to the compile report (or to `what()` if
//! the report is empty) and re-throws.
//!
//! \param jsonConfig  Stringified JSON object with the per-call
//!                    configuration (`sequencer`, `samplerate`,
//!                    `filename`, `wavepath`, `waveforms`).  Invalid
//!                    JSON is treated as an empty object.
//! \param sourceCode  SeqC source text to compile.
//! \param deviceId    Device-type identifier (case-insensitive),
//!                    e.g. `"HDAWG8"`, `"SHFQA4"`.
//! \param awgIndex    Zero-based AWG core index inside the target
//!                    device.
//! \param options     Newline-separated device options string
//!                    (case-insensitive).
//! \return A single `std::string` packing two payloads concatenated
//!         with a `'\0'` separator: the serialized JSON status report
//!         followed by the binary ELF image.  The Python binding
//!         unpacks this into a `(elf_bytes, info_dict)` tuple.
//!
//! \verifyme The exact packing and the `outputut`/`output` stream
//! label are taken from the binary; small wording differences from
//! the canonical LabOne source are flagged in
//! `reconstructed/notes/elf_format.md`.
std::string compileSeqc(std::string const& jsonConfig,   // @0xf58a0
                        std::string sourceCode,
                        std::string deviceId,
                        unsigned long awgIndex,
                        std::string const& options)
{
    // --- 1. Parse JSON config ---
    boost::json::value jv;
    try {
        jv = boost::json::parse(jsonConfig);
    } catch (...) {
        // If JSON parsing fails, use an empty object
        jv = boost::json::object{};
    }
    auto& jobj = jv.as_object();

    // --- 2. Extract sequencer type ---
    AwgSequencerType seqType = AwgSequencerType::Auto;
    if (auto* seqVal = jobj.if_contains("sequencer")) {
        if (seqVal->is_string()) {
            seqType = parseSequencerType(std::string(seqVal->as_string()));
        }
    }

    // --- 3. Uppercase deviceId and options ---
    // Binary: std::toupper with locale @0xf5c20
    boost::to_upper(deviceId);
    std::string upperOptions = boost::to_upper_copy(options);

    // --- 4. Construct DeviceType, get AwgDeviceType ---
    DeviceType devType(deviceId, upperOptions);
    DeviceTypeCode dtCode = devType.code();
    AwgDeviceType awgDT = toAwgDeviceType(dtCode, seqType);

    if (static_cast<int>(awgDT) == 0) {
        // Unsupported device/sequencer combination
        std::string errMsg = makeUnsupportedAwgSequencerErrorMessage(dtCode, seqType);
        throw Exception(errMsg);
    }

    AwgDeviceProps props = dispatchGetAwgDeviceProps(awgDT, devType);

    // --- 5. Build waveform search path ---
    // ZiFolder::ziFolder(Data) → folderPath("", "") / "awg" / "waves"
    ZiFolder dataFolder = ZiFolder::ziFolder(ZiFolder::DirectoryType::Data);
    std::string wavePath = dataFolder.folderPath("", "");
    boost::filesystem::path waveSearchPath(wavePath);
    waveSearchPath /= "awg";
    waveSearchPath /= "waves";

    // --- 6. Extract config fields from JSON ---
    double sampleRate = std::numeric_limits<double>::quiet_NaN();
    if (auto* srVal = jobj.if_contains("samplerate")) {
        if (srVal->is_double()) {
            sampleRate = srVal->as_double();
        } else if (srVal->is_int64()) {
            sampleRate = static_cast<double>(srVal->as_int64());
        }
    }

    std::string filename;
    if (auto* fnVal = jobj.if_contains("filename")) {
        if (fnVal->is_string()) {
            filename = std::string(fnVal->as_string());
        }
    }

    std::string configWavePath;
    if (auto* wpVal = jobj.if_contains("wavepath")) {
        if (wpVal->is_string()) {
            configWavePath = std::string(wpVal->as_string());
        }
    }

    // "waveforms" — JSON string split by ';' into vector
    std::vector<std::string> waveformPaths;
    if (auto* wfVal = jobj.if_contains("waveforms")) {
        if (wfVal->is_string()) {
            std::string wfStr(wfVal->as_string());
            boost::split(waveformPaths, wfStr, boost::is_any_of(";"),
                         boost::token_compress_on);
            // Remove empty strings from split
            waveformPaths.erase(
                std::remove_if(waveformPaths.begin(), waveformPaths.end(),
                               [](std::string const& s) { return s.empty(); }),
                waveformPaths.end());
        }
    }

    // --- 7. Populate AWGCompilerConfig ---
    AWGCompilerConfig config{};
    config.deviceType = props.deviceType;                       // +0x00
    config.sampleFormat = SampleFormat(props.sampleFormat);     // +0x04
    config.deviceSampleRate = sampleRate;                       // +0x08
    config.addressImpl = props.addressImpl;                     // +0x10
    config.channelsPerGroup[0] = 0x0002;                        // +0x14
    config.channelsPerGroup[1] = 0x0004;                        // +0x16
    config.isHirzel = props.isHirzel;                           // +0x18
    config.cacheType = (props.deviceType == AwgDeviceType::HDAWG) ? 1 : 0;  // +0x19 — cmp $0x2 at 0xf6798, sete at 0xf67da
    config.numChannelGroups = 1;                                // +0x1C
    config.awgIndex = static_cast<int32_t>(awgIndex);           // +0x20
    config.deviceIndex = 0;                                     // +0x24
    config.optimizationFlags = 0xFF;                                   // +0x88 — binary hardcodes 0xFF
    config.numCores = 0;                                        // +0x94 — binary sets 0 (not 1)
    config.loopUnrollLimit = 0x20000;                           // +0x98 — 131072; compile-time loop unroll limit

    // String fields
    if (!filename.empty()) {
        config.debugDumpPath = filename;                             // +0x30
    }
    if (!configWavePath.empty()) {
        config.searchPath = boost::filesystem::path(configWavePath);  // +0xA8
    } else {
        config.searchPath = waveSearchPath;
    }

    // splitDeviceOptions for additional config setup
    // @0xf63xx: calls splitDeviceOptions on the options string
    // (This populates includePaths or other config fields depending on content)

    // --- 8. Create AWGCompiler ---
    AWGCompiler compiler(config);

    // --- 9. Add waveforms ---
    if (!waveformPaths.empty()) {
        compiler.addWaveforms(waveformPaths);
    }

    // --- 10-12. Compile, write to stream, capture ELF ---
    std::string elfData;
    std::string jsonResult;
    try {
        compiler.compileString(sourceCode);                      // @0xf6580

        // Write ELF to ostringstream
        std::ostringstream oss;
        compiler.writeToStream(oss, "output");                    // @0xf65a0
        elfData = oss.str();

        // --- 13. Build result JSON ---
        std::string compileReport = compiler.getCompileReport();
        std::string waveMemInfo = compiler.getJsonWaveformMemoryInfo();

        boost::json::object result;
        result["messages"] = compileReport;
        result["maxelfsize"] = props.maxElfSize;

        // Parse waveMemInfo as JSON and embed as object
        if (!waveMemInfo.empty()) {
            try {
                result["wavemem"] = boost::json::parse(waveMemInfo);
            } catch (...) {
                result["wavemem"] = boost::json::object{};
            }
        } else {
            result["wavemem"] = boost::json::object{};
        }

        // --- 14. Serialize ---
        jsonResult = boost::json::serialize(result);

    } catch (zhinst::Exception const& ex) {                      // @0xf6800
        // Get compile report even on failure
        std::string report = compiler.getCompileReport();
        std::string msg = "Compilation failed: ";
        if (!report.empty()) {                                   // @0xf7974
            msg += report;
        } else {
            msg += ex.what();
            msg += "\n";
        }
        throw Exception(msg);
    }

    // Return format: JSON result string + '\0' separator + ELF binary data
    // The binary packs these as: first string = JSON, second = ELF
    // Exact serialization: return as single string with JSON first, then
    // a null separator, then ELF data.
    // @0xf6a80: the binary constructs the return string as JSON + '\0' + ELF
    std::string packed;
    packed.reserve(jsonResult.size() + 1 + elfData.size());
    packed.append(jsonResult);
    packed.push_back('\0');
    packed.append(elfData);

    return packed;
}

}  // namespace zhinst
