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

// Named device-type bitmask combinations.
// These replace raw hex literals like static_cast<AwgDeviceType>(0x1ff).
// Values are OR-combinations of the AwgDeviceType power-of-2 enum values.
inline constexpr AwgDeviceType kDevAll        = static_cast<AwgDeviceType>(0x1FF); // all 9 devices
inline constexpr AwgDeviceType kDevAllButUHF  = static_cast<AwgDeviceType>(0x1FE); // all except UHFLI
inline constexpr AwgDeviceType kDevHirzel     = static_cast<AwgDeviceType>(0x1F2); // HDAWG + SHF* + GHFLI + VHFLI (sequencerRegBase=0x0d05)
inline constexpr AwgDeviceType kDevSHFPlus    = static_cast<AwgDeviceType>(0x1F8); // SHFQA + SHFSG + SHFQC_SG + SHFLI + GHFLI + VHFLI
inline constexpr AwgDeviceType kDevLIFamily   = static_cast<AwgDeviceType>(0x1C0); // SHFLI + GHFLI + VHFLI
inline constexpr AwgDeviceType kDevCervino    = static_cast<AwgDeviceType>(0x005); // UHFLI + UHFQA
inline constexpr AwgDeviceType kDevUHF        = static_cast<AwgDeviceType>(0x005); // alias for kDevCervino
inline constexpr AwgDeviceType kDevPreSHFLI   = static_cast<AwgDeviceType>(0x03F); // UHFLI..SHFQC_SG (first 6)
inline constexpr AwgDeviceType kDevQA         = static_cast<AwgDeviceType>(0x00C); // UHFQA + SHFQA
inline constexpr AwgDeviceType kDevHirzelAll  = static_cast<AwgDeviceType>(0x1F7); // all except UHFQA (= kDevHirzel | UHFLI)
inline constexpr AwgDeviceType kDevHirzelPlusUHFQA = static_cast<AwgDeviceType>(0x1F6); // kDevHirzel | UHFQA
inline constexpr AwgDeviceType kDevNone       = static_cast<AwgDeviceType>(0x000); // no devices

// EDirection — unified direction enum.
// Binary name: zhinst::EDirection (mangled: NS_10EDirectionE).
// Used both as AST parameter direction (eIN/eOUT/eINOUT in SeqCAstNode ctor
// signatures) and as Resources read/write direction (eIN=read, eOUT=write in
// readConst/readString/readWave/readCvar).
// str() @0x1c1730 maps: 0→"in", 1→"out", 2→"inout".
enum class EDirection : int32_t {
    eIN    = 0,   // AST: parameter is input.    Resources: read-only path.
    eOUT   = 1,   // AST: parameter is output.   Resources: write/strict path.
    eINOUT = 2,   // AST: parameter is in+out.   (Resources: not used.)
};

// Sentinel value constants (A15)
// Used across multiple subsystems for "inherit/unset/none" semantics.
constexpr int kRateInherit  = -1;   // rate = inherit global (prefetch.cpp)
constexpr int kNoWaveIndex  = -1;   // no waveform index (wavetable_front.cpp)
constexpr int kNoNodeId     = -1;   // no node ID (node.cpp)
constexpr int kNoPlayIndex  = -1;   // no play index (wave_index_tracker.cpp)

// I/Q channel tags in writeToNode (A16)
constexpr int kChannelTag_I = 0x0C;
constexpr int kChannelTag_Q = 0x0D;

} // namespace zhinst
