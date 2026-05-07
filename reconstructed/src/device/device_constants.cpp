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
// Field names revised based on verified consumer usage.
// Default-branch throw reconstructed in uses
// BOOST_THROW_EXCEPTION(ZIAWGCompilerException(...)) to match the
// boost::throw_exception call at 0x2cc44d with source_location
// (file=constants.cpp, function=getDeviceConstants, line=312, column=65).
// ============================================================================

#include "zhinst/device/device_constants.hpp"
#include "zhinst/core/types.hpp"      // AwgDeviceType enum
#include "zhinst/core/exception.hpp"  // ZIAWGCompilerException

#include <boost/throw_exception.hpp>

#include <cstring>

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
        dc.sineNodeBase            = 16;
        dc.waveformElfAlignment            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x115c;
        dc.triggerLatencyCycles            = 6;
        dc.maxWaveformLength = 16;
        dc.grainSize    = 8;
        dc.playMinSamples            = 0;
        dc.waveformMinSamples            = 16;
        dc.bitsPerSample       = 16;
        dc.numCounters            = 0;
        dc.maxProgramSize     = 1024;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 0;
        dc.samplingRate        = 1.8e9;
        dc.execTableIndexBits      = 0;
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
        dc.sineNodeBase            = 16;
        dc.waveformElfAlignment            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.triggerLatencyCycles            = 6;
        dc.maxWaveformLength = 32;
        dc.grainSize    = 16;
        dc.playMinSamples            = 128;
        dc.waveformMinSamples            = 32;
        dc.bitsPerSample       = 16;
        dc.numCounters            = 2;
        dc.maxProgramSize     = 16384;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.4e9;
        dc.execTableIndexBits      = 10;
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
        dc.sineNodeBase            = 16;
        dc.waveformElfAlignment            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x115c;
        dc.triggerLatencyCycles            = 6;
        dc.maxWaveformLength = 16;
        dc.grainSize    = 8;
        dc.playMinSamples            = 0;
        dc.waveformMinSamples            = 16;
        dc.bitsPerSample       = 16;
        dc.numCounters            = 0;
        dc.maxProgramSize     = 1024;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 0;
        dc.samplingRate        = 1.8e9;
        dc.execTableIndexBits      = 0;
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
        dc.sineNodeBase            = 16;
        dc.waveformElfAlignment            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.triggerLatencyCycles            = 6;
        dc.maxWaveformLength = 32;
        dc.grainSize    = 16;
        dc.playMinSamples            = 128;
        dc.waveformMinSamples            = 32;
        dc.bitsPerSample       = 16;
        dc.numCounters            = 2;
        dc.maxProgramSize     = 16384;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.0e9;
        dc.execTableIndexBits      = 10;
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
        dc.sineNodeBase            = 16;
        dc.waveformElfAlignment            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.triggerLatencyCycles            = 6;
        dc.maxWaveformLength = 32;
        dc.grainSize    = 16;
        dc.playMinSamples            = 128;
        dc.waveformMinSamples            = 16;
        dc.bitsPerSample       = 16;
        dc.numCounters            = 2;
        dc.maxProgramSize     = 32768;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.0e9;
        dc.execTableIndexBits      = 12;
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
        dc.sineNodeBase            = 16;
        dc.waveformElfAlignment            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.triggerLatencyCycles            = 6;
        dc.maxWaveformLength = 32;
        dc.grainSize    = 16;
        dc.playMinSamples            = 128;
        dc.waveformMinSamples            = 16;
        dc.bitsPerSample       = 16;
        dc.numCounters            = 2;
        dc.maxProgramSize     = 32768;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.0e9;
        dc.execTableIndexBits      = 12;
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
        dc.sineNodeBase            = 16;
        dc.waveformElfAlignment            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.triggerLatencyCycles            = 6;
        dc.maxWaveformLength = 32;
        dc.grainSize    = 16;
        dc.playMinSamples            = 128;
        dc.waveformMinSamples            = 16;
        dc.bitsPerSample       = 16;
        dc.numCounters            = 2;
        dc.maxProgramSize     = 32768;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.0e9;
        dc.execTableIndexBits      = 12;
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
        dc.sineNodeBase            = 16;
        dc.waveformElfAlignment            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.triggerLatencyCycles            = 6;
        dc.maxWaveformLength = 96;
        dc.grainSize    = 48;
        dc.playMinSamples            = 384;
        dc.waveformMinSamples            = 96;
        dc.bitsPerSample       = 16;
        dc.numCounters            = 2;
        dc.maxProgramSize     = 32768;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 12.0e9;
        dc.execTableIndexBits      = 12;
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
        dc.sineNodeBase            = 16;
        dc.waveformElfAlignment            = 64;
        dc.registerDepth       = 16;
        dc.memoryDepth         = 1024;
        dc.sequencerRegBase    = 0x0d05;
        dc.triggerLatencyCycles            = 6;
        dc.maxWaveformLength = 32;
        dc.grainSize    = 16;
        dc.playMinSamples            = 128;
        dc.waveformMinSamples            = 16;
        dc.bitsPerSample       = 16;
        dc.numCounters            = 2;
        dc.maxProgramSize     = 32768;
        dc.maxSequenceLen      = 16000;
        dc.seqClockDivider     = 1165;
        dc.samplingRate        = 2.0e9;
        dc.execTableIndexBits      = 12;
        dc.numAWGCores         = 3;
        dc.hasDIO              = true;
        dc.numDIOBits          = 8;
        dc.hasPrecomp          = true;
        break;

    default:
        // .rodata 0x90b170 = "Instantiated compiler for unsupported device type"
        // Throw site at 0x2cc3f7..0x2cc44d:
        //   - constructs ZIAWGCompilerException(string) at [rbp-0x98]
        //   - calls boost::throw_exception<ZIAWGCompilerException>(ex, src_loc)
        //     @ 0x270ab0 with source_location{file=constants.cpp,
        //     function="DeviceConstants zhinst::getDeviceConstants(AwgDeviceType)",
        //     line=312, column=65}.
        BOOST_THROW_EXCEPTION(ZIAWGCompilerException(
            "Instantiated compiler for unsupported device type"));
    }

    return dc;
}

} // namespace zhinst
