// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// StaticResources — constructor, destructor, init, getVariable
// ============================================================================

#include "zhinst/resources.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/device_constants.hpp"

namespace zhinst {

// ============================================================================
// StaticResources::StaticResources — @0x129cb0
//
// Calls Resources("static", weak_ptr<Resources>{}) then sets up the
// std::function callback and usedSampleRate_ flag.
// ============================================================================
StaticResources::StaticResources(
    std::function<void(std::string const&)> const& logger)
    : Resources(std::string("static"), std::weak_ptr<Resources>{})
{
    // 0x129d19: mov BYTE PTR [rbx+0xd8], 0x0
    usedSampleRate_ = false;

    // 0x129d20–0x129d59: Clone the std::function into functionStorage_/functionPtr_
    // If the function's target is small enough, it's stored inline at +0xE0;
    // otherwise it's heap-allocated. functionPtr_ at +0x100 points to the target.
    // (Details of std::function internals elided — this is a bitwise copy
    //  with virtual clone call at 0x129d31: call [rdi+0x10])
}

// ============================================================================
// StaticResources::~StaticResources — D1 @0x129db0, D0 @0x129e00
//
// D1 (non-deleting):
//   1. Sets vptr to StaticResources vtable+0x10
//   2. Checks if functionPtr_ (+0x100) points to inline storage (+0xE0):
//      - If yes: calls virtual destroy via functionStorage_ vtable offset 0x20
//      - If no & not null: calls virtual destroy via offset 0x28
//   3. Tail-calls Resources::~Resources (D1 at 0x12a8f0)
//
// D0 (deleting): same cleanup then operator delete(this, 0x110)
// ============================================================================
StaticResources::~StaticResources()  // D1 @0x129db0
{
    // Destroy the std::function stored at +0xE0/+0x100
    // (implementation: virtual call on the function object to destroy it)

    // Then ~Resources() runs via tail call
}

// ============================================================================
// StaticResources::getVariable — @0x129e60 (override)
//
// Checks for special built-in variable names before delegating to base:
//
// 1. If name.length() == 0x12 (18) and name == "__device_sample_rate":
//    Sets *(rsi + 0xD8) = 1  (usedSampleRate_ on the result object)
//
// 2. Calls Resources::getVariable(name)  @0x1eb0a0
//
// 3. If result is non-null and name.length() == 0x13 (19):
//    Checks if name == "__awg_integration_..." (SSE comparison)
//    (Handles integration trigger/arm constants)
//
// 4. Also checks against static string constAwgIntegrationTrigger (BSS @0xb84690)
//    and other const-prefixed variable names.
// ============================================================================
void StaticResources::getVariable(std::string const& name)  // @0x129e60
{
    // Check for "DEVICE_SAMPLE_RATE" (18 chars)                  // 0x129e73
    if (name.size() == 18) {
        if (std::memcmp(name.data(), "DEVICE_SAMPLE_RATE", 18) == 0) {
            usedSampleRate_ = true;  // byte at offset 0xD8
        }
    }

    // Delegate to base
    Resources::getVariable(name);                                  // 0x129ea0

    // Post-check: warn about deprecated integration/trigger constants
    size_t len = name.size();

    if (len == 19) {                                               // 0x129ec0
        if (std::memcmp(name.data(), "AWG_MONITOR_TRIGGER", 19) == 0) {
            errorHandler_->report(
                ErrorMessages::format(ErrorMessageT(0x34), name, "'startQA' function"));
            return;
        }
    }

    // Check constAwgIntegrationTrigger (BSS @0xb84690)
    if (len == constAwgIntegrationTrigger.size() &&
        std::memcmp(name.data(), constAwgIntegrationTrigger.data(), len) == 0) {
        errorHandler_->report(
            ErrorMessages::format(ErrorMessageT(0x34), name, "'startQA' function"));
        return;
    }

    if (len == 19) {
        if (std::memcmp(name.data(), "AWG_INTEGRATION_ARM", 19) == 0) {
            errorHandler_->report(
                ErrorMessages::format(ErrorMessageT(0x34), name, "'startQA' function"));
            return;
        }
    }

    // Check zsyncDataPqscRegister/Decoder (BSS @0xb846a8/0xb846c0)
    if (len == zsyncDataPqscRegister.size() &&
        std::memcmp(name.data(), zsyncDataPqscRegister.data(), len) == 0) {
        errorHandler_->report(
            ErrorMessages::format(ErrorMessageT(0x34), name, "'ZSYNC_DATA_PROCESSED_A'"));
        return;
    }
    if (len == zsyncDataPqscDecoder.size() &&
        std::memcmp(name.data(), zsyncDataPqscDecoder.data(), len) == 0) {
        errorHandler_->report(
            ErrorMessages::format(ErrorMessageT(0x34), name, "'ZSYNC_DATA_PROCESSED_B'"));
        return;
    }
}

// ============================================================================
// StaticResources::init — @0x1ec8f0 (size: ~0x3D27 = ~15.6 KB)
//
// Populates the scope with built-in named constants for the target device.
// Uses Resources::addConst(name, value, VarSubType=0) extensively.
//
// Structure:
//   1. Check config.deviceType:
//      - If type == 4 (SHFSG) or type == 1 (UHFQA):
//        Add general AWG constants (shared across device types)
//      - If type == 2 (HDAWG):
//        Add HDAWG-specific constants
//
//   2. General constants added (for types 1, 4):
//      - AWG_RATE_900MHZ, AWG_RATE_450MHZ, AWG_RATE_225MHZ,
//        AWG_RATE_112MHZ, AWG_RATE_56MHZ (sampling rate dividers)
//      - Plus many more rate/trigger/integration constants
//
//   3. HDAWG constants added (type 2):
//      - AWG_RATE_600MHZ, AWG_RATE_300MHZ, AWG_RATE_150MHZ,
//        AWG_RATE_75MHZ (different frequency set)
//      - Plus HDAWG-specific trigger/DIO constants
//
//   4. Common constants for all device types:
//      - AWG_INTEGRATION_TRIGGER, AWG_INTEGRATION_ARM
//      - Various other named constants
//
// All constants are added as double values (the addConst overload taking
// double + VarSubType).
// ============================================================================
void StaticResources::init(AWGCompilerConfig const& config,
                           DeviceConstants const& deviceConstants)  // @0x1ec8f0
{
    // The function is ~15KB of repetitive addConst() calls.
    // Constants are clock divider indices (0.0-13.0) for sampling rates.

    if (config.deviceType == AwgDeviceType::HDAWG ||
        config.deviceType == AwgDeviceType::SHFSG) {
        // HDAWG/Hirzel device rates (1800MHz base, /2 divisions)
        addConst("AWG_RATE_1800MHZ", 0.0,  VarSubType(0));
        addConst("AWG_RATE_900MHZ",  1.0,  VarSubType(0));
        addConst("AWG_RATE_450MHZ",  2.0,  VarSubType(0));
        addConst("AWG_RATE_225MHZ",  3.0,  VarSubType(0));
        addConst("AWG_RATE_112MHZ",  4.0,  VarSubType(0));
        addConst("AWG_RATE_56MHZ",   5.0,  VarSubType(0));
        addConst("AWG_RATE_28MHZ",   6.0,  VarSubType(0));
        addConst("AWG_RATE_14MHZ",   7.0,  VarSubType(0));
        addConst("AWG_RATE_7MHZ",    8.0,  VarSubType(0));
        addConst("AWG_RATE_3P5MHZ",  9.0,  VarSubType(0));
        addConst("AWG_RATE_1P8MHZ",  10.0, VarSubType(0));
        addConst("AWG_RATE_880KHZ",  11.0, VarSubType(0));
        addConst("AWG_RATE_440KHZ",  12.0, VarSubType(0));
        addConst("AWG_RATE_220KHZ",  13.0, VarSubType(0));
        // ... additional rate constants for this device family
    }

    if (config.deviceType == AwgDeviceType::UHFQA ||
        config.deviceType == AwgDeviceType::UHFLI) {
        // UHF/Cervino device rates (2400MHz base, /2 divisions)
        addConst("AWG_RATE_2400MHZ",  0.0,  VarSubType(0));
        addConst("AWG_RATE_1200MHZ",  1.0,  VarSubType(0));
        addConst("AWG_RATE_600MHZ",   2.0,  VarSubType(0));
        addConst("AWG_RATE_300MHZ",   3.0,  VarSubType(0));
        addConst("AWG_RATE_150MHZ",   4.0,  VarSubType(0));
        addConst("AWG_RATE_75MHZ",    5.0,  VarSubType(0));
        addConst("AWG_RATE_37P5MHZ",  6.0,  VarSubType(0));
        addConst("AWG_RATE_18P75MHZ", 7.0,  VarSubType(0));
        addConst("AWG_RATE_9P4MHZ",   8.0,  VarSubType(0));
        addConst("AWG_RATE_4P5MHZ",   9.0,  VarSubType(0));
        addConst("AWG_RATE_2P34MHZ",  10.0, VarSubType(0));
        addConst("AWG_RATE_1P2MHZ",   11.0, VarSubType(0));
        addConst("AWG_RATE_586KHZ",   12.0, VarSubType(0));
        addConst("AWG_RATE_293KHZ",   13.0, VarSubType(0));
        // ... additional rate constants for this device family
    }

    // Common constants for all devices
    addConst("AWG_MONITOR_TRIGGER",    32.0,       VarSubType(0));
    addConst("AWG_INTEGRATION_TRIGGER", 16.0,       VarSubType(0));
    addConst("AWG_INTEGRATION_ARM",    67043328.0, VarSubType(0));  // 0x03FF0000

    // Additional constants: trigger masks, DIO bits, etc.
    // (full list is ~100+ entries covering all device-specific constants;
    //  only the rate/trigger constants are shown above — the full
    //  15KB function continues with similar addConst patterns for
    //  AWG_DIO_*, AWG_MARKER_*, QA_*, ZSYNC_* constants)
}

} // namespace zhinst
