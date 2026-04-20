// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// DeviceConstants factory implementation
//
// Source path (from debug info): /builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp
// Binary address: 0x2cc0c0 – 0x2cc47f (0x3c0 = 960 bytes)
//
// Switch on AwgDeviceType populates all fields of DeviceConstants.
// Throws ZIAWGCompilerException for unsupported types.
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
        dc.hasExtendedReg    = false;
        dc.waveformReg       = { 0x95b2,  0x20000, 32, 4096 };
        dc.commandTableReg   = { 4, 2, 16, 64 };
        dc.numOutputChannels = 16;
        dc.registerSpace     = 1024;
        dc.sequencerReg      = { 0x115c, 6, 16, 8 };
        dc.auxReg            = { 0, 16, 16, 0 };
        dc.waveformMemSize   = 1024;
        dc.maxSequenceLen    = 16000;
        dc.seqClockDivider   = 0;
        dc.samplingRate      = 1.8e9;
        dc.numOutputPorts    = 0;
        dc.numAWGCores       = 0;
        dc.hasDIO            = false;
        dc.numDIOBits        = 0;
        dc.hasPrecomp        = false;
        break;

    case 2:  // HDAWG (hirzel)
        dc.hasExtendedReg    = true;   // only device with this set
        dc.waveformReg       = { 0x11203, 0x100000, 64, 4096 };
        dc.commandTableReg   = { 4, 2, 16, 64 };
        dc.numOutputChannels = 16;
        dc.registerSpace     = 1024;
        dc.sequencerReg      = { 0x0d05, 6, 32, 16 };
        dc.auxReg            = { 128, 32, 16, 2 };
        dc.waveformMemSize   = 16384;
        dc.maxSequenceLen    = 16000;
        dc.seqClockDivider   = 1165;
        dc.samplingRate      = 2.4e9;
        dc.numOutputPorts    = 10;
        dc.numAWGCores       = 5;
        dc.hasDIO            = true;
        dc.numDIOBits        = 8;
        dc.hasPrecomp        = true;
        break;

    case 4:  // UHFQA (klausen)
        dc.hasExtendedReg    = false;
        dc.waveformReg       = { 0x95b2,  0x20000, 32, 4096 };
        dc.commandTableReg   = { 4, 2, 16, 64 };
        dc.numOutputChannels = 16;
        dc.registerSpace     = 1024;
        dc.sequencerReg      = { 0x115c, 6, 16, 8 };
        dc.auxReg            = { 0, 16, 16, 0 };
        dc.waveformMemSize   = 1024;
        dc.maxSequenceLen    = 16000;
        dc.seqClockDivider   = 0;
        dc.samplingRate      = 1.8e9;
        dc.numOutputPorts    = 0;
        dc.numAWGCores       = 0;
        dc.hasDIO            = false;
        dc.numDIOBits        = 0;
        dc.hasPrecomp        = false;
        break;

    case 8:  // SHFQA (grimsel_qa)
        dc.hasExtendedReg    = false;
        dc.waveformReg       = { 0x110f6, 0x100000, 64, 4096 };
        dc.commandTableReg   = { 4, 2, 16, 64 };
        dc.numOutputChannels = 16;
        dc.registerSpace     = 1024;
        dc.sequencerReg      = { 0x0d05, 6, 32, 16 };
        dc.auxReg            = { 128, 32, 16, 2 };
        dc.waveformMemSize   = 16384;
        dc.maxSequenceLen    = 16000;
        dc.seqClockDivider   = 1165;
        dc.samplingRate      = 2.0e9;
        dc.numOutputPorts    = 10;
        dc.numAWGCores       = 0;
        dc.hasDIO            = true;
        dc.numDIOBits        = 6;
        dc.hasPrecomp        = true;
        break;

    case 16: // SHFSG (grimsel_sg)
        dc.hasExtendedReg    = false;
        dc.waveformReg       = { 0x11202, 0x60000, 64, 4096 };
        dc.commandTableReg   = { 104, 2, 16, 64 };
        dc.numOutputChannels = 16;
        dc.registerSpace     = 1024;
        dc.sequencerReg      = { 0x0d05, 6, 32, 16 };
        dc.auxReg            = { 128, 16, 16, 2 };
        dc.waveformMemSize   = 32768;
        dc.maxSequenceLen    = 16000;
        dc.seqClockDivider   = 1165;
        dc.samplingRate      = 2.0e9;
        dc.numOutputPorts    = 12;
        dc.numAWGCores       = 3;
        dc.hasDIO            = true;
        dc.numDIOBits        = 8;
        dc.hasPrecomp        = true;
        break;

    case 32: // SHFQC_SG (grimsel_qc_sg)
        dc.hasExtendedReg    = false;
        dc.waveformReg       = { 0x11202, 0x60000, 64, 4096 };
        dc.commandTableReg   = { 104, 2, 16, 64 };
        dc.numOutputChannels = 16;
        dc.registerSpace     = 1024;
        dc.sequencerReg      = { 0x0d05, 6, 32, 16 };
        dc.auxReg            = { 128, 16, 16, 2 };
        dc.waveformMemSize   = 32768;
        dc.maxSequenceLen    = 16000;
        dc.seqClockDivider   = 1165;
        dc.samplingRate      = 2.0e9;
        dc.numOutputPorts    = 12;
        dc.numAWGCores       = 3;
        dc.hasDIO            = true;
        dc.numDIOBits        = 8;
        dc.hasPrecomp        = true;
        break;

    case 64: // SHFLI (grimsel_li)
        dc.hasExtendedReg    = false;
        dc.waveformReg       = { 0x110f6, 0x60000, 16, 16 };
        dc.commandTableReg   = { 0x6000, 2, 16, 64 };
        dc.numOutputChannels = 16;
        dc.registerSpace     = 1024;
        dc.sequencerReg      = { 0x0d05, 6, 32, 16 };
        dc.auxReg            = { 128, 16, 16, 2 };
        dc.waveformMemSize   = 32768;
        dc.maxSequenceLen    = 16000;
        dc.seqClockDivider   = 1165;
        dc.samplingRate      = 2.0e9;
        dc.numOutputPorts    = 12;
        dc.numAWGCores       = 3;
        dc.hasDIO            = true;
        dc.numDIOBits        = 8;
        dc.hasPrecomp        = true;
        break;

    case 128: // GHFLI (gurnigel) — high-speed 12 GHz
        dc.hasExtendedReg    = false;
        dc.waveformReg       = { 0x110f6, 0x60000, 48, 48 };
        dc.commandTableReg   = { 0x2000, 2, 16, 64 };
        dc.numOutputChannels = 16;
        dc.registerSpace     = 1024;
        dc.sequencerReg      = { 0x0d05, 6, 96, 48 };
        dc.auxReg            = { 384, 96, 16, 2 };
        dc.waveformMemSize   = 32768;
        dc.maxSequenceLen    = 16000;
        dc.seqClockDivider   = 1165;
        dc.samplingRate      = 12.0e9;
        dc.numOutputPorts    = 12;
        dc.numAWGCores       = 3;
        dc.hasDIO            = true;
        dc.numDIOBits        = 8;
        dc.hasPrecomp        = true;
        break;

    case 256: // VHFLI (maloja) — shares register config with SHFLI (64)
        dc.hasExtendedReg    = false;
        dc.waveformReg       = { 0x110f6, 0x60000, 16, 16 };
        dc.commandTableReg   = { 0x6000, 2, 16, 64 };
        dc.numOutputChannels = 16;
        dc.registerSpace     = 1024;
        dc.sequencerReg      = { 0x0d05, 6, 32, 16 };
        dc.auxReg            = { 128, 16, 16, 2 };
        dc.waveformMemSize   = 32768;
        dc.maxSequenceLen    = 16000;
        dc.seqClockDivider   = 1165;
        dc.samplingRate      = 2.0e9;
        dc.numOutputPorts    = 12;
        dc.numAWGCores       = 3;
        dc.hasDIO            = true;
        dc.numDIOBits        = 8;
        dc.hasPrecomp        = true;
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
