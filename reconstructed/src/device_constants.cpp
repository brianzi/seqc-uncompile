// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// DeviceConstants factory implementation
//
// Source path (from debug info): /builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp
// Binary address: 0x2cc0c0 – 0x2cc47f (0x3c0 = 960 bytes)
//
// Switch on AwgDeviceType populates all fields of DeviceConstants.
// Throws ZIAWGCompilerException for unsupported types.
//
// Field names revised in Phase 7e based on verified consumer usage.
// ============================================================================

#include "zhinst/device_constants.hpp"
#include "zhinst/types.hpp"   // AwgDeviceType enum

#include <cstring>
#include <stdexcept>

namespace zhinst {

DeviceConstants getDeviceConstants(AwgDeviceType deviceType)  // 0x2cc0c0
{
    DeviceConstants dc;
    std::memset(&dc, 0, sizeof(dc));

    int dt = static_cast<int>(deviceType);
    dc.deviceType = dt;

    switch (dt) {

    case 1:  // UHFLI (cervino)
        dc.hasExtendedReg      = false;
        dc.waveformRegBase     = 0x95b2;
        dc.waveformMemorySize  = 0x20000;
        dc.sampleLength        = 32;
        dc.waveformAlignment   = 4096;
        dc.cachePageCount      = 4;
        dc.maxBlocks           = 2;
        dc.field_20            = 16;
        dc.field_24            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x115c;
        dc.field_3C            = 6;
        dc.waveformGranularity = 16;
        dc.waveformPageSize    = 8;
        dc.field_48            = 0;
        dc.field_4C            = 16;
        dc.bitsPerSample       = 16;
        dc.field_54            = 0;
        dc.waveformMemSize     = 1024;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 0;
        dc.samplingRate        = 1.8e9;
        dc.numOutputPorts      = 0;
        dc.numAWGCores         = 0;
        dc.hasDIO              = false;
        dc.numDIOBits          = 0;
        dc.hasPrecomp          = false;
        break;

    case 2:  // HDAWG (hirzel)
        dc.hasExtendedReg      = true;   // only device with this set
        dc.waveformRegBase     = 0x11203;
        dc.waveformMemorySize  = 0x100000;
        dc.sampleLength        = 64;
        dc.waveformAlignment   = 4096;
        dc.cachePageCount      = 4;
        dc.maxBlocks           = 2;
        dc.field_20            = 16;
        dc.field_24            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.field_3C            = 6;
        dc.waveformGranularity = 32;
        dc.waveformPageSize    = 16;
        dc.field_48            = 128;
        dc.field_4C            = 32;
        dc.bitsPerSample       = 16;
        dc.field_54            = 2;
        dc.waveformMemSize     = 16384;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.4e9;
        dc.numOutputPorts      = 10;
        dc.numAWGCores         = 5;
        dc.hasDIO              = true;
        dc.numDIOBits          = 8;
        dc.hasPrecomp          = true;
        break;

    case 4:  // UHFQA (klausen)
        dc.hasExtendedReg      = false;
        dc.waveformRegBase     = 0x95b2;
        dc.waveformMemorySize  = 0x20000;
        dc.sampleLength        = 32;
        dc.waveformAlignment   = 4096;
        dc.cachePageCount      = 4;
        dc.maxBlocks           = 2;
        dc.field_20            = 16;
        dc.field_24            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x115c;
        dc.field_3C            = 6;
        dc.waveformGranularity = 16;
        dc.waveformPageSize    = 8;
        dc.field_48            = 0;
        dc.field_4C            = 16;
        dc.bitsPerSample       = 16;
        dc.field_54            = 0;
        dc.waveformMemSize     = 1024;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 0;
        dc.samplingRate        = 1.8e9;
        dc.numOutputPorts      = 0;
        dc.numAWGCores         = 0;
        dc.hasDIO              = false;
        dc.numDIOBits          = 0;
        dc.hasPrecomp          = false;
        break;

    case 8:  // SHFQA (grimsel_qa)
        dc.hasExtendedReg      = false;
        dc.waveformRegBase     = 0x110f6;
        dc.waveformMemorySize  = 0x100000;
        dc.sampleLength        = 64;
        dc.waveformAlignment   = 4096;
        dc.cachePageCount      = 4;
        dc.maxBlocks           = 2;
        dc.field_20            = 16;
        dc.field_24            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.field_3C            = 6;
        dc.waveformGranularity = 32;
        dc.waveformPageSize    = 16;
        dc.field_48            = 128;
        dc.field_4C            = 32;
        dc.bitsPerSample       = 16;
        dc.field_54            = 2;
        dc.waveformMemSize     = 16384;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.0e9;
        dc.numOutputPorts      = 10;
        dc.numAWGCores         = 0;
        dc.hasDIO              = true;
        dc.numDIOBits          = 6;
        dc.hasPrecomp          = false;
        break;

    case 16: // SHFSG (grimsel_sg)
        dc.hasExtendedReg      = false;
        dc.waveformRegBase     = 0x11202;
        dc.waveformMemorySize  = 0x60000;
        dc.sampleLength        = 64;
        dc.waveformAlignment   = 4096;
        dc.cachePageCount      = 104;
        dc.maxBlocks           = 2;
        dc.field_20            = 16;
        dc.field_24            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.field_3C            = 6;
        dc.waveformGranularity = 32;
        dc.waveformPageSize    = 16;
        dc.field_48            = 128;
        dc.field_4C            = 16;
        dc.bitsPerSample       = 16;
        dc.field_54            = 2;
        dc.waveformMemSize     = 32768;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.0e9;
        dc.numOutputPorts      = 12;
        dc.numAWGCores         = 3;
        dc.hasDIO              = true;
        dc.numDIOBits          = 8;
        dc.hasPrecomp          = true;
        break;

    case 32: // SHFQC_SG (grimsel_qc_sg)
        dc.hasExtendedReg      = false;
        dc.waveformRegBase     = 0x11202;
        dc.waveformMemorySize  = 0x60000;
        dc.sampleLength        = 64;
        dc.waveformAlignment   = 4096;
        dc.cachePageCount      = 104;
        dc.maxBlocks           = 2;
        dc.field_20            = 16;
        dc.field_24            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.field_3C            = 6;
        dc.waveformGranularity = 32;
        dc.waveformPageSize    = 16;
        dc.field_48            = 128;
        dc.field_4C            = 16;
        dc.bitsPerSample       = 16;
        dc.field_54            = 2;
        dc.waveformMemSize     = 32768;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.0e9;
        dc.numOutputPorts      = 12;
        dc.numAWGCores         = 3;
        dc.hasDIO              = true;
        dc.numDIOBits          = 8;
        dc.hasPrecomp          = true;
        break;

    case 64: // SHFLI (grimsel_li)
        dc.hasExtendedReg      = false;
        dc.waveformRegBase     = 0x110f6;
        dc.waveformMemorySize  = 0x60000;
        dc.sampleLength        = 16;
        dc.waveformAlignment   = 16;
        dc.cachePageCount      = 0x6000;
        dc.maxBlocks           = 2;
        dc.field_20            = 16;
        dc.field_24            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.field_3C            = 6;
        dc.waveformGranularity = 32;
        dc.waveformPageSize    = 16;
        dc.field_48            = 128;
        dc.field_4C            = 16;
        dc.bitsPerSample       = 16;
        dc.field_54            = 2;
        dc.waveformMemSize     = 32768;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.0e9;
        dc.numOutputPorts      = 12;
        dc.numAWGCores         = 3;
        dc.hasDIO              = true;
        dc.numDIOBits          = 8;
        dc.hasPrecomp          = true;
        break;

    case 128: // GHFLI (gurnigel) — high-speed 12 GHz
        dc.hasExtendedReg      = false;
        dc.waveformRegBase     = 0x110f6;
        dc.waveformMemorySize  = 0x60000;
        dc.sampleLength        = 48;
        dc.waveformAlignment   = 48;
        dc.cachePageCount      = 0x2000;
        dc.maxBlocks           = 2;
        dc.field_20            = 16;
        dc.field_24            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.field_3C            = 6;
        dc.waveformGranularity = 96;
        dc.waveformPageSize    = 48;
        dc.field_48            = 384;
        dc.field_4C            = 96;
        dc.bitsPerSample       = 16;
        dc.field_54            = 2;
        dc.waveformMemSize     = 32768;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 12.0e9;
        dc.numOutputPorts      = 12;
        dc.numAWGCores         = 3;
        dc.hasDIO              = true;
        dc.numDIOBits          = 8;
        dc.hasPrecomp          = true;
        break;

    case 256: // VHFLI (maloja) — shares register config with SHFLI (64)
        dc.hasExtendedReg      = false;
        dc.waveformRegBase     = 0x110f6;
        dc.waveformMemorySize  = 0x60000;
        dc.sampleLength        = 16;
        dc.waveformAlignment   = 16;
        dc.cachePageCount      = 0x6000;
        dc.maxBlocks           = 2;
        dc.field_20            = 16;
        dc.field_24            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.field_3C            = 6;
        dc.waveformGranularity = 32;
        dc.waveformPageSize    = 16;
        dc.field_48            = 128;
        dc.field_4C            = 16;
        dc.bitsPerSample       = 16;
        dc.field_54            = 2;
        dc.waveformMemSize     = 32768;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.0e9;
        dc.numOutputPorts      = 12;
        dc.numAWGCores         = 3;
        dc.hasDIO              = true;
        dc.numDIOBits          = 8;
        dc.hasPrecomp          = true;
        break;

    default:
        // "Instantiated compiler for unsupported device type"
        // Throws ZIAWGCompilerException via boost::throw_exception
        throw std::runtime_error(
            "Instantiated compiler for unsupported device type");
    }

    return dc;
}

} // namespace zhinst
