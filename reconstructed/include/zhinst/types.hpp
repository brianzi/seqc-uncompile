// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Forward declarations, enums, and basic types
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>

namespace zhinst {

// Forward declarations
class AsmCommands;
class AsmCommandsImpl;
class AsmCommandsImplCervino;
class AsmCommandsImplHirzel;
class Node;
class WaveformFront;
class WavetableFront;
class WaveformGenerator;
class CompilerMessageCollection;
class AWGCompilerConfig;
class DeviceConstants;
class CancelCallback;
class Resources;
class SeqCAstNode;
class CustomFunctions;

// Device type enum — bitmask-style values.
// Confirmed from getAwgDeviceTypeString (0x270080) and getAwgDeviceTypeFromString (0x270180).
// getInstance() tests (val-2) against bitmask 0x4000000040004041 to select Cervino vs Hirzel.
// Hirzel: HDAWG(2), SHFQA(8), SHFSG(16), SHFQC_SG(32), SHFLI(64)
// Cervino: UHFLI(1), UHFQA(4), GHFLI(128), VHFLI(256)
enum AwgDeviceType : int {
    UHFLI      = 1,     // codename "cervino", display "UHFLI"
    HDAWG      = 2,     // codename "hirzel",  display "HDAWG" — only Hirzel type
    UHFQA      = 4,     // codename "klausen", display "UHFQA"
    SHFQA      = 8,     // codename "grimsel_qa", display "SHFQA"
    SHFSG      = 16,    // codename "grimsel_sg", display "SHFSG"
    SHFQC_SG   = 32,    // codename "grimsel_qc_sg", display "SHFQC (SG)"
    SHFLI      = 64,    // codename "grimsel_li", display "SHFLI"
    GHFLI      = 128,   // codename "gurnigel", display "GHFLI"
    VHFLI      = 256,   // codename "maloja", display "VHFLI"
};

} // namespace zhinst
