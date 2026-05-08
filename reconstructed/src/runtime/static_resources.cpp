// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// StaticResources — constructor, destructor, init, getVariable
// ============================================================================

#include "zhinst/runtime/resources.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/device/device_constants.hpp"

namespace zhinst {

// ============================================================================
// Deprecated/relocated variable-name BSS strings used by the deprecation
// post-check in StaticResources::getVariable. Definitions provided by
// `src/error_messages.cpp`. The header
// `zhinst/error_messages.hpp` already exposes these `extern`; the local
// declarations below are kept for documentation of the binary BSS
// addresses and are redundant-but-compatible with the header.
// ============================================================================
extern const std::string constAwgIntegrationTrigger;  // BSS @0xb84690 — "AWG_INTEGRATION_TRIGGER"
extern const std::string zsyncDataPqscRegister;       // BSS @0xb846a8 — "ZSYNC_DATA_PQSC_REGISTER"
extern const std::string zsyncDataPqscDecoder;        // BSS @0xb846c0 — "ZSYNC_DATA_PQSC_DECODER"

// ============================================================================
// StaticResources constructor, destructor, errorReportTarget, and getVariable
// are defined in resources_static_global.cpp (the more detailed reconstruction).
// Only StaticResources::init lives in this file.
// ============================================================================

// ============================================================================
// StaticResources::init — @0x1ec8f0 (size: 0x4854, ends at 0x1f1144)
//
// Populates the scope with built-in named constants for the target device.
// Uses Resources::addConst(name, value, VarSubType) extensively.
//
// Structure:
//   1. Device-type dispatch on config.deviceType (at r14):
//      - Types 1, 4 (HD): AWG_RATE (1800MHz base) + MONITOR/INTEGRATION consts
//      - Type 2 (Cervino/SHF): AWG_RATE (2400MHz base)
//      - Types 8, 16, 32: AWG_RATE (2000MHz base)
//      - Type 0x40, 0x100: AWG_RATE (2000MHz base, shared)
//      - Type 0x80: AWG_RATE (6000MHz base)
//   2. Second dispatch: QA_INT/QA_GEN (types 4, 8) or ZSYNC_DATA (types 2, 16, 32)
//   3. Common section: trigger indices, trigger bitmasks, channel/marker,
//      suppress/enable, osc phase, math constants, booleans
//
// Total addConst calls: 212 in full function
// For Cervino (type 2): 121 calls (14 rate + 5 ZSYNC + 102 common)
// ============================================================================
void StaticResources::init(AWGCompilerConfig const& config,
                           DeviceConstants const& deviceConstants)  // @0x1ec8f0
{
    // ================================================================
    // Phase 1: Device-type-specific AWG_RATE constants
    // ================================================================

    if (config.deviceType == UHFQA /*Cervino/klausen*/ ||
        config.deviceType == UHFLI /*Cervino*/) {                          // 0x1ec917–0x1ec91f
        // --- Cervino rates (1800MHz base) --- 0x1ec925–0x1eccfe
        addConst("AWG_RATE_1800MHZ", 0.0,  VarSubType(0));       // 0x1ec947
        addConst("AWG_RATE_900MHZ",  1.0,  VarSubType(0));       // 0x1ec9a7
        addConst("AWG_RATE_450MHZ",  2.0,  VarSubType(0));       // 0x1ec9ec
        addConst("AWG_RATE_225MHZ",  3.0,  VarSubType(0));       // 0x1eca2c
        addConst("AWG_RATE_112MHZ",  4.0,  VarSubType(0));       // 0x1eca6c
        addConst("AWG_RATE_56MHZ",   5.0,  VarSubType(0));       // 0x1ecab2
        addConst("AWG_RATE_28MHZ",   6.0,  VarSubType(0));
        addConst("AWG_RATE_14MHZ",   7.0,  VarSubType(0));
        addConst("AWG_RATE_7MHZ",    8.0,  VarSubType(0));
        addConst("AWG_RATE_3P5MHZ",  9.0,  VarSubType(0));
        addConst("AWG_RATE_1P8MHZ",  10.0, VarSubType(0));
        addConst("AWG_RATE_880KHZ",  11.0, VarSubType(0));
        addConst("AWG_RATE_440KHZ",  12.0, VarSubType(0));
        addConst("AWG_RATE_220KHZ",  13.0, VarSubType(0));       // 0x1ecce2

        // HD-only trigger/integration constants               0x1eccfe–0x1ecd44
        addConst("AWG_MONITOR_TRIGGER",     32.0,         VarSubType(0));  // 0x1ecd12
        addConst("AWG_INTEGRATION_TRIGGER", 16.0,         VarSubType(0));  // 0x1ecd2b
        addConst("AWG_INTEGRATION_ARM",     67043328.0,   VarSubType(0));  // 0x1ecd44
    }

    if (config.deviceType == HDAWG /*Hirzel*/) {                // 0x1ecd4c
        // --- Hirzel rates (2400MHz base) --- 0x1ecd55–0x1ed0f8
        addConst("AWG_RATE_2400MHZ",  0.0,  VarSubType(0));      // 0x1ecd77
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
        addConst("AWG_RATE_293KHZ",   13.0, VarSubType(0));      // 0x1ed0f8
    }

    // --- 2.0 GHz rate variants (certain HDAWG revisions) ---     0x1ed14a–0x1ed4e9
    // Condition: deviceType is one of {0, 8, 16, 32}. Verified at
    // 0x1ed136-0x1ed144: `movabs rdx, 0x100010100; bt rdx, rcx` where
    // rcx = config.deviceType. Bits 0, 8, 16, 32 of the 64-bit constant
    // correspond to the four supported device-type indices.
    {
        const uint32_t deviceType = static_cast<uint32_t>(config.deviceType);
        constexpr uint64_t kRate2GHzDeviceMask = 0x100010100ULL;
        const bool supports2GHz =
            (deviceType <= 32) &&
            ((kRate2GHzDeviceMask >> deviceType) & 1ULL);
        if (supports2GHz) {
        addConst("AWG_RATE_2000MHZ",   0.0,  VarSubType(0));      // 0x1ed164
        addConst("AWG_RATE_1000MHZ",   1.0,  VarSubType(0));
        addConst("AWG_RATE_500MHZ",    2.0,  VarSubType(0));
        addConst("AWG_RATE_250MHZ",    3.0,  VarSubType(0));
        addConst("AWG_RATE_125MHZ",    4.0,  VarSubType(0));
        addConst("AWG_RATE_62P5MHZ",   5.0,  VarSubType(0));
        addConst("AWG_RATE_31P25MHZ",  6.0,  VarSubType(0));
        addConst("AWG_RATE_15P63MHZ",  7.0,  VarSubType(0));
        addConst("AWG_RATE_7P81MHZ",   8.0,  VarSubType(0));
        addConst("AWG_RATE_3P9MHZ",    9.0,  VarSubType(0));
        addConst("AWG_RATE_1P95MHZ",   10.0, VarSubType(0));
        addConst("AWG_RATE_976KHZ",    11.0, VarSubType(0));
        addConst("AWG_RATE_488KHZ",    12.0, VarSubType(0));
        addConst("AWG_RATE_244KHZ",    13.0, VarSubType(0));      // 0x1ed4e9
        }
    }

    // --- SHF rates (SHFQA=0x100, SHFSG=0x40: 2.0 GHz, 4 steps) --- 0x1ee1f9–0x1ee2fb
    if (config.deviceType == SHFLI || config.deviceType == VHFLI) {
        addConst("AWG_RATE_2000MHZ",   0.0,  VarSubType(0));      // 0x1ee213
        addConst("AWG_RATE_1000MHZ",   1.0,  VarSubType(0));
        addConst("AWG_RATE_500MHZ",    2.0,  VarSubType(0));
        addConst("AWG_RATE_250MHZ",    3.0,  VarSubType(0));      // 0x1ee2fb
    }

    // --- SHFQC rates (device==0x80: 6.0 GHz base, 4 steps) ---  0x1ee309–0x1ee40e
    if (config.deviceType == GHFLI) {
        addConst("AWG_RATE_6000MHZ",   0.0,  VarSubType(0));      // 0x1ee323
        addConst("AWG_RATE_3000MHZ",   1.0,  VarSubType(0));
        addConst("AWG_RATE_1500MHZ",   2.0,  VarSubType(0));
        addConst("AWG_RATE_750MHZ",    3.0,  VarSubType(0));      // 0x1ee40e
    }

    // ================================================================
    // Phase 2: QA_INT / QA_GEN channel bitmask constants
    // ================================================================

    // --- HDAWG4 (awg_count==4): 10 QA_INT channels ---           0x1ed4ff–0x1ed86f
    if (deviceConstants.deviceType == UHFQA) {
        addConst("QA_INT_0",     1.0,     VarSubType(0));          // 0x1ed519
        addConst("QA_INT_1",     2.0,     VarSubType(0));
        addConst("QA_INT_2",     4.0,     VarSubType(0));
        addConst("QA_INT_3",     8.0,     VarSubType(0));
        addConst("QA_INT_4",     16.0,    VarSubType(0));
        addConst("QA_INT_5",     32.0,    VarSubType(0));
        addConst("QA_INT_6",     64.0,    VarSubType(0));
        addConst("QA_INT_7",     128.0,   VarSubType(0));
        addConst("QA_INT_8",     256.0,   VarSubType(0));
        addConst("QA_INT_9",     512.0,   VarSubType(0));
        addConst("QA_INT_ALL",   1023.0,  VarSubType(0));          // 2^10 - 1
        addConst("QA_INT_NONE",  0.0,     VarSubType(0));
        addConst("ZSYNC_DATA_RAW", 0.0,   VarSubType(0));          // 0x1ed86f
    }

    // --- HDAWG8 (awg_count==8): 16 QA_INT + 16 QA_GEN ---       0x1ed87b–0x1ee1e6
    if (deviceConstants.deviceType == SHFQA) {
        addConst("QA_INT_0",     1.0,     VarSubType(0));          // 0x1ed895
        addConst("QA_INT_1",     2.0,     VarSubType(0));
        addConst("QA_INT_2",     4.0,     VarSubType(0));
        addConst("QA_INT_3",     8.0,     VarSubType(0));
        addConst("QA_INT_4",     16.0,    VarSubType(0));
        addConst("QA_INT_5",     32.0,    VarSubType(0));
        addConst("QA_INT_6",     64.0,    VarSubType(0));
        addConst("QA_INT_7",     128.0,   VarSubType(0));
        addConst("QA_INT_8",     256.0,   VarSubType(0));
        addConst("QA_INT_9",     512.0,   VarSubType(0));
        addConst("QA_INT_10",    1024.0,  VarSubType(0));
        addConst("QA_INT_11",    2048.0,  VarSubType(0));
        addConst("QA_INT_12",    4096.0,  VarSubType(0));
        addConst("QA_INT_13",    8192.0,  VarSubType(0));
        addConst("QA_INT_14",    16384.0, VarSubType(0));
        addConst("QA_INT_15",    32768.0, VarSubType(0));
        addConst("QA_INT_ALL",   65535.0, VarSubType(0));          // 2^16 - 1
        addConst("QA_INT_NONE",  0.0,     VarSubType(0));

        addConst("QA_GEN_0",    1.0,     VarSubType(0));
        addConst("QA_GEN_1",    2.0,     VarSubType(0));
        addConst("QA_GEN_2",    4.0,     VarSubType(0));
        addConst("QA_GEN_3",    8.0,     VarSubType(0));
        addConst("QA_GEN_4",    16.0,    VarSubType(0));
        addConst("QA_GEN_5",    32.0,    VarSubType(0));
        addConst("QA_GEN_6",    64.0,    VarSubType(0));
        addConst("QA_GEN_7",    128.0,   VarSubType(0));
        addConst("QA_GEN_8",    256.0,   VarSubType(0));
        addConst("QA_GEN_9",    512.0,   VarSubType(0));
        addConst("QA_GEN_10",   1024.0,  VarSubType(0));
        addConst("QA_GEN_11",   2048.0,  VarSubType(0));
        addConst("QA_GEN_12",   4096.0,  VarSubType(0));
        addConst("QA_GEN_13",   8192.0,  VarSubType(0));
        addConst("QA_GEN_14",   16384.0, VarSubType(0));
        addConst("QA_GEN_15",   32768.0, VarSubType(0));
        addConst("QA_GEN_ALL",  65535.0, VarSubType(0));           // 2^16 - 1
        addConst("QA_GEN_NONE", 0.0,     VarSubType(0));
        addConst("ZSYNC_DATA_RAW", 0.0,  VarSubType(0));          // 0x1ee1e6
    }

    if (config.deviceType == HDAWG || config.deviceType == SHFSG ||
        config.deviceType == SHFQC_SG) {                                // 0x1ee411 bitmap
        // ZSYNC_DATA constants — values computed from deviceConstants  0x1ee430–0x1ee5fd
        int n = deviceConstants.execTableIndexBits;  // byte at offset 0x78
        int base = 1 << n;
        addConst("ZSYNC_DATA_RAW",          (double)(base),     VarSubType(0));  // 0x1ee46c
        addConst("ZSYNC_DATA_PQSC_REGISTER",(double)(base + 1), VarSubType(0));  // 0x1ee4dc
        addConst("ZSYNC_DATA_PROCESSED_A",  (double)(base + 1), VarSubType(0));  // 0x1ee527
        addConst("ZSYNC_DATA_PQSC_DECODER", (double)(base + 2), VarSubType(0));  // 0x1ee597
        addConst("ZSYNC_DATA_PROCESSED_B",  (double)(base + 2), VarSubType(0));  // 0x1ee5e2

        if (config.deviceType == SHFQC_SG) {                            // 0x1ee5fe
            addConst("QA_DATA_RAW",       (double)(base + 3), VarSubType(0));    // 0x1ee639
            addConst("QA_DATA_PROCESSED_D", (double)(base + 4), VarSubType(0));    // 0x1ee67f
        }
    }

    // ================================================================
    // Phase 3: Common constants (all device types)  0x1ee69b–0x1f05ed
    // ================================================================

    // --- Device sample rate ---                                 0x1ee69b
    // Verified at 0x1ee6a1-0x1ee6b7 + 0x1f0618-0x1f062b:
    //   xmm0 = config.deviceSampleRate
    //   if (isnan(xmm0)) {
    //       if (config.deviceType == 2) {
    //           // skip addConst entirely
    //           goto next_const;
    //       }
    //       xmm0 = deviceConstants->samplingRate;  // [DC+0x70]
    //   }
    //   addConst("DEVICE_SAMPLE_RATE", xmm0, 0);
    // The previous reconstruction's "sampleRate = 2.4e9" branch was wrong:
    // the binary skips the addConst when type==2 + NaN, it does not
    // substitute a literal value.
    double sampleRate = config.deviceSampleRate;  // r14[0x8]
    bool emitSampleRate = true;
    if (std::isnan(sampleRate)) {
        if (config.deviceType == HDAWG) {
            emitSampleRate = false;                                 // 0x1f061c → 0x1ee6bc
        } else {
            sampleRate = deviceConstants.samplingRate;              // 0x1f0622 → DC+0x70
        }
    }
    if (emitSampleRate) {
        addConst("DEVICE_SAMPLE_RATE", sampleRate, VarSubType(0));   // 0x1ee6b7
    }

    // --- Trigger index constants (sequential indices 0–31) ---  0x1ee6ec–0x1ef1f8
    addConst("AWG_ANA_TRIGGER1_INDEX",         0.0,  VarSubType(0));  // 0x1ee6ec
    addConst("AWG_ANA_TRIGGER2_INDEX",         1.0,  VarSubType(0));  // 0x1ee73a
    addConst("AWG_DIG_TRIGGER1_INDEX",         2.0,  VarSubType(0));  // 0x1ee77e
    addConst("AWG_DIG_TRIGGER2_INDEX",         3.0,  VarSubType(0));  // 0x1ee7c2
    addConst("AWG_DEMOD_TRIGGER1_INDEX",       4.0,  VarSubType(0));  // 0x1ee81f
    addConst("AWG_DEMOD_TRIGGER2_INDEX",       5.0,  VarSubType(0));  // 0x1ee87c
    addConst("AWG_DEMOD_TRIGGER3_INDEX",       6.0,  VarSubType(0));  // 0x1ee8e3
    addConst("AWG_DEMOD_TRIGGER4_INDEX",       7.0,  VarSubType(0));  // 0x1ee94a
    addConst("AWG_DEMOD_TRIGGER5_INDEX",       8.0,  VarSubType(0));  // 0x1ee9b1
    addConst("AWG_DEMOD_TRIGGER6_INDEX",       9.0,  VarSubType(0));  // 0x1eea18
    addConst("AWG_DEMOD_TRIGGER7_INDEX",       10.0, VarSubType(0));  // 0x1eea7f
    addConst("AWG_DEMOD_TRIGGER8_INDEX",       11.0, VarSubType(0));  // 0x1eeae6
    addConst("AWG_DEMODRATEMAX_TRIGGER1_INDEX",12.0, VarSubType(0));  // 0x1eeb4a
    addConst("AWG_DEMODRATE_TRIGGER1_INDEX",   13.0, VarSubType(0));  // 0x1eebae
    addConst("AWG_DEMODRATE_TRIGGER2_INDEX",   14.0, VarSubType(0));  // 0x1eec12
    addConst("AWG_DEMODRATE_TRIGGER3_INDEX",   15.0, VarSubType(0));  // 0x1eec76
    addConst("AWG_DEMODRATE_TRIGGER4_INDEX",   16.0, VarSubType(0));  // 0x1eecda
    addConst("AWG_DEMODRATE_TRIGGER5_INDEX",   17.0, VarSubType(0));  // 0x1eed3e
    addConst("AWG_DEMODRATE_TRIGGER6_INDEX",   18.0, VarSubType(0));  // 0x1eeda2
    addConst("AWG_DEMODRATE_TRIGGER7_INDEX",   19.0, VarSubType(0));  // 0x1eee06
    addConst("AWG_DEMODRATE_TRIGGER8_INDEX",   20.0, VarSubType(0));  // 0x1eee6a
    addConst("AWG_MDS_RE_TRIGGER_INDEX",       21.0, VarSubType(0));  // 0x1eeed1
    addConst("AWG_MDS_FE_TRIGGER_INDEX",       22.0, VarSubType(0));  // 0x1eef2e
    addConst("AWG_MDS_LVL1_TRIGGER_INDEX",     23.0, VarSubType(0));  // 0x1eef92
    addConst("AWG_MDS_LVL0_TRIGGER_INDEX",     24.0, VarSubType(0));  // 0x1eeff6
    addConst("AWG_ZSYNC_TRIGGER_INDEX",        25.0, VarSubType(0));  // 0x1ef053
    addConst("AWG_CNT_TRIGGER0_INDEX",         26.0, VarSubType(0));  // 0x1ef0a1
    addConst("AWG_CNT_TRIGGER1_INDEX",         27.0, VarSubType(0));  // 0x1ef0e5
    addConst("AWG_MAP_TRIGGER_INDEX",          28.0, VarSubType(0));  // 0x1ef129
    addConst("AWG_WAIT_TRIGGER_INDEX",         29.0, VarSubType(0));  // 0x1ef16d
    addConst("AWG_TS_TRIGGER_INDEX",           30.0, VarSubType(0));  // 0x1ef1b4
    addConst("AWG_TIME_TRIGGER_INDEX",         31.0, VarSubType(0));  // 0x1ef1f8

    // --- Grid trigger ---                                       0x1ef24e
    addConst("AWG_GRID_TRIGGER",               32.0, VarSubType(0));  // 0x1ef24e

    // --- Trigger bitmask values (powers of 2) ---               0x1ef298–0x1ef458
    addConst("AWG_TRIGGER1",                   1.0,      VarSubType(0));  // 0x1ef298
    addConst("AWG_TRIGGER2",                   2.0,      VarSubType(0));  // 0x1ef2d8
    addConst("AWG_TRIGGER3",                   4.0,      VarSubType(0));  // 0x1ef318
    addConst("AWG_TRIGGER4",                   8.0,      VarSubType(0));  // 0x1ef358
    addConst("AWG_TRIGGER5",                   16.0,     VarSubType(0));  // 0x1ef398
    addConst("AWG_TRIGGER6",                   32.0,     VarSubType(0));  // 0x1ef3d8
    addConst("AWG_TRIGGER7",                   64.0,     VarSubType(0));  // 0x1ef418
    addConst("AWG_TRIGGER8",                   128.0,    VarSubType(0));  // 0x1ef458

    // --- Analog/Digital trigger bitmasks ---                    0x1ef498–0x1ef558
    addConst("AWG_ANA_TRIGGER1",               1.0,      VarSubType(0));  // 0x1ef498
    addConst("AWG_ANA_TRIGGER2",               2.0,      VarSubType(0));  // 0x1ef4d8
    addConst("AWG_DIG_TRIGGER1",               4.0,      VarSubType(0));  // 0x1ef518
    addConst("AWG_DIG_TRIGGER2",               8.0,      VarSubType(0));  // 0x1ef558

    // --- Demod trigger bitmasks ---                             0x1ef59e–0x1ef788
    addConst("AWG_DEMOD_TRIGGER1",             16.0,     VarSubType(0));  // 0x1ef59e
    addConst("AWG_DEMOD_TRIGGER2",             32.0,     VarSubType(0));  // 0x1ef5e4
    addConst("AWG_DEMOD_TRIGGER3",             64.0,     VarSubType(0));  // 0x1ef62a
    addConst("AWG_DEMOD_TRIGGER4",             128.0,    VarSubType(0));  // 0x1ef670
    addConst("AWG_DEMOD_TRIGGER5",             256.0,    VarSubType(0));  // 0x1ef6b6
    addConst("AWG_DEMOD_TRIGGER6",             512.0,    VarSubType(0));  // 0x1ef6fc
    addConst("AWG_DEMOD_TRIGGER7",             1024.0,   VarSubType(0));  // 0x1ef742
    addConst("AWG_DEMOD_TRIGGER8",             2048.0,   VarSubType(0));  // 0x1ef788

    // --- Demod rate trigger bitmasks ---                        0x1ef7ec–0x1efa5c
    addConst("AWG_DEMODRATEMAX_TRIGGER1",      4096.0,   VarSubType(0));  // 0x1ef7ec
    addConst("AWG_DEMODRATE_TRIGGER1",         8192.0,   VarSubType(0));  // 0x1ef83a
    addConst("AWG_DEMODRATE_TRIGGER2",         16384.0,  VarSubType(0));  // 0x1ef888
    addConst("AWG_DEMODRATE_TRIGGER3",         32768.0,  VarSubType(0));  // 0x1ef8d6
    addConst("AWG_DEMODRATE_TRIGGER4",         65536.0,  VarSubType(0));  // 0x1ef924
    addConst("AWG_DEMODRATE_TRIGGER5",         131072.0, VarSubType(0));  // 0x1ef972
    addConst("AWG_DEMODRATE_TRIGGER6",         262144.0, VarSubType(0));  // 0x1ef9c0
    addConst("AWG_DEMODRATE_TRIGGER7",         524288.0, VarSubType(0));  // 0x1efa0e
    addConst("AWG_DEMODRATE_TRIGGER8",         1048576.0,VarSubType(0));  // 0x1efa5c

    // --- MDS / special trigger bitmasks ---                     0x1efaa2–0x1efc8e
    addConst("AWG_MDS_RE_TRIGGER",             2097152.0,    VarSubType(0));  // 0x1efaa2
    addConst("AWG_MDS_FE_TRIGGER",             4194304.0,    VarSubType(0));  // 0x1efae8
    addConst("AWG_MDS_LVL1_TRIGGER",           8388608.0,    VarSubType(0));  // 0x1efb2f
    addConst("AWG_MDS_LVL0_TRIGGER",           16777216.0,   VarSubType(0));  // 0x1efb76
    addConst("AWG_MAP_TRIGGER",                268435456.0,  VarSubType(0));  // 0x1efbc7
    addConst("AWG_WAIT_TRIGGER",               536870912.0,  VarSubType(0));  // 0x1efc07
    addConst("AWG_TS_TRIGGER",                 1073741824.0, VarSubType(0));  // 0x1efc4e
    addConst("AWG_TIME_TRIGGER",               -2147483648.0,VarSubType(0));  // 0x1efc8e

    // --- Channel/Marker (skipped by type 8, je at 0x1efcae) --- 0x1efcdd–0x1efda2
    if (config.deviceType != AwgDeviceType::SHFQA) {
        addConst("AWG_CHAN1",                  1.0,  VarSubType(0));  // 0x1efcdd
        addConst("AWG_CHAN2",                  2.0,  VarSubType(0));  // 0x1efd18
        addConst("AWG_MARKER1",               1.0,  VarSubType(0));  // 0x1efd62
        addConst("AWG_MARKER2",               2.0,  VarSubType(0));  // 0x1efda2
    }

    // --- Suppress/Enable ---                                    0x1efe06–0x1f00ba
    addConst("AWG_SUPPRESS_CHAN1_SIGOUT1",     1.0,  VarSubType(0));  // 0x1efe06
    addConst("AWG_SUPPRESS_CHAN1_SIGOUT2",     2.0,  VarSubType(0));  // 0x1efe6a
    addConst("AWG_SUPPRESS_CHAN2_SIGOUT1",     4.0,  VarSubType(0));  // 0x1efece
    addConst("AWG_SUPPRESS_CHAN2_SIGOUT2",     8.0,  VarSubType(0));  // 0x1eff32
    addConst("AWG_ENABLE_CHAN1_SIGOUT1",       1.0,  VarSubType(0));  // 0x1eff99
    addConst("AWG_ENABLE_CHAN1_SIGOUT2",       2.0,  VarSubType(0));  // 0x1f0000
    addConst("AWG_ENABLE_CHAN2_SIGOUT1",       4.0,  VarSubType(0));  // 0x1f005d
    addConst("AWG_ENABLE_CHAN2_SIGOUT2",       8.0,  VarSubType(0));  // 0x1f00ba

    // --- OSC phase / USERREG (skipped by type 8, je at 0x1f00da) --- 0x1f010b–0x1f021e
    if (config.deviceType != AwgDeviceType::SHFQA) {
        addConst("AWG_OSC_PHASE_START",        1.0,  VarSubType(0));  // 0x1f010b
        addConst("AWG_OSC_PHASE_MIDDLE",       0.0,  VarSubType(0));  // 0x1f0150
        addConst("AWG_USERREG_SWEEP_COUNT0",   35.0, VarSubType(0));  // 0x1f01b7
        addConst("AWG_USERREG_SWEEP_COUNT1",   36.0, VarSubType(0));  // 0x1f021e
    }

    // --- Math constants ---                                     0x1f0256–0x1f0571
    addConst("M_E",          2.718281828459045,   VarSubType(0));  // 0x1f0256
    addConst("M_LOG2E",      1.4426950408889634,  VarSubType(0));  // 0x1f0299
    addConst("M_LOG10E",     0.4342944819032518,  VarSubType(0));  // 0x1f02dc
    addConst("M_LN2",        0.6931471805599453,  VarSubType(0));  // 0x1f031a
    addConst("M_LN10",       2.302585092994046,   VarSubType(0));  // 0x1f035c
    addConst("M_PI",         3.141592653589793,    VarSubType(0));  // 0x1f0398
    addConst("M_PI_2",       1.5707963267948966,  VarSubType(0));  // 0x1f03da
    addConst("M_PI_4",       0.7853981633974483,  VarSubType(0));  // 0x1f041c
    addConst("M_1_PI",       0.3183098861837907,  VarSubType(0));  // 0x1f045e
    addConst("M_2_PI",       0.6366197723675814,  VarSubType(0));  // 0x1f04a0
    addConst("M_2_SQRTPI",   1.1283791670955126,  VarSubType(0));  // 0x1f04e9
    addConst("M_SQRT2",      1.4142135623730951,  VarSubType(0));  // 0x1f052c
    addConst("M_SQRT1_2",    0.7071067811865476,  VarSubType(0));  // 0x1f0571

    // --- Boolean constants (VarSubType(1)!) ---                 0x1f05b0–0x1f05ed
    addConst("true",          1.0,  VarSubType(1));               // 0x1f05b0
    addConst("false",         0.0,  VarSubType(1));               // 0x1f05ed
}

} // namespace zhinst
