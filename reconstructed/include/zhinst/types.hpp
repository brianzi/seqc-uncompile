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

// Device type enum — determines Cervino vs Hirzel instruction encoding.
// getInstance() tests (val-2) against bitmask 0x4000000040004041:
//   bits {0,6,14,30,62} → values {2,8,16,32,64} → Hirzel
//   Also 128 and 256 → Hirzel
//   Everything else → Cervino
enum class AwgDeviceType : int {
    HDAWG8 = 2,
    HDAWG4 = 8,
    UHFQA  = 16,
    UHFAWG = 32,
    UHFLI  = 64,
    SHFQA  = 128,
    SHFSG  = 256,
    // All other values → Cervino
};

} // namespace zhinst
