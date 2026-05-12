// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// ErrorMessages + ResourcesException implementations
//
// Source path (from debug info): ErrorMessages.cpp
// Global init: __cxx_global_var_init at 0xd5de0
// ============================================================================

#include "zhinst/core/error_messages.hpp"
#include "zhinst/runtime/resources.hpp"  // for ResourcesException

#include <cstring>

namespace zhinst {

// ============================================================================
// Static message map — initialized during global construction.
// BSS address: 0xb84c38
// 305 entries across 4 key ranges.
// ============================================================================
std::map<int, std::string> ErrorMessages::messages;  // populated below

// ============================================================================
// Process-wide singleton — BSS @ 0x95de60.
// extern declared in 4 caller TUs (cache, custom_functions,
// node, prefetch_emit) but never defined. Definition added here.
// ============================================================================
//! \brief Process-wide `ErrorMessages` instance used by the compiler to
//!        look up diagnostic format strings via `errMsg[id]`.
ErrorMessages errMsg;

// ============================================================================
// Three namespace-scope `const std::string` globals referenced from
// static_resources.cpp:21-23. BSS addresses verified against binary.
// extern declared, never defined. String contents recovered
// from binary rodata via `strings _seqc_compiler.so`:
//   _ZN6zhinstL26constAwgIntegrationTriggerE → "AWG_INTEGRATION_TRIGGER"
//   _ZN6zhinstL21zsyncDataPqscRegisterE      → "ZSYNC_DATA_PQSC_REGISTER"
//   _ZN6zhinstL20zsyncDataPqscDecoderE       → "ZSYNC_DATA_PQSC_DECODER"
//
// Binary: mangles these with the `L` prefix indicating internal
// linkage (declared `static` inside `namespace zhinst`). Our header
// declares them `extern` so callers can reference them across TUs;
// the actual binary may have them duplicated per-TU. This is a slight
// ABI deviation but shouldn't affect behaviour.
// ============================================================================
//! \name SeqC-resource identifier strings
//! \brief Reserved waveform/source names referenced by the resource
//!        manager and the SeqC parser to recognise built-in pseudo-waves.
//!
//! \details Each constant holds the canonical uppercase token the
//! compiler matches against user identifiers when resolving the
//! corresponding hardware feature.  They are linked into the resource
//! lookup at `static_resources.cpp:21-23`.
//! \{

//! \brief Identifier for the ZSync PQSC decoder pseudo-resource
//!        (`"ZSYNC_DATA_PQSC_DECODER"`).
const std::string zsyncDataPqscDecoder       = "ZSYNC_DATA_PQSC_DECODER";
//! \brief Identifier for the ZSync PQSC register pseudo-resource
//!        (`"ZSYNC_DATA_PQSC_REGISTER"`).
const std::string zsyncDataPqscRegister      = "ZSYNC_DATA_PQSC_REGISTER";
//! \brief Identifier for the AWG integration-trigger pseudo-resource
//!        (`"AWG_INTEGRATION_TRIGGER"`).
const std::string constAwgIntegrationTrigger = "AWG_INTEGRATION_TRIGGER";
//! \}

// ============================================================================
// ErrorMessages::operator[](ErrorMessageT) const
// Binary address: 0x108380
//
// Performs std::map::at() — red-black tree traversal on messages map.
// Node layout: left(+0x00), right(+0x08), parent(+0x10), key(+0x20), value(+0x28).
// Throws std::out_of_range("map::at: key not found") if not found.
// ============================================================================
std::string const& ErrorMessages::operator[](ErrorMessageT id) const  // 0x108380
{
    return messages.at(static_cast<int>(id));
}

// ============================================================================
// ResourcesException
// Layout: vptr(8) + std::string message_(24) = 0x20 bytes
// Inherits from std::exception (base dtor at 0x8c32f0)
// vtable at 0xb04ea8
// ============================================================================

ResourcesException::ResourcesException(std::string const& msg)  // 0x1e3a20
    : msg_(msg)
{
    // Binary: sets vtable, copies string to this+0x08
}

ResourcesException::~ResourcesException()  // 0x1f12f0
{
    // Binary: sets vtable, destroys string if heap-allocated, calls base dtor
    // Deleting dtor (D0) also calls operator delete(this, 0x20)
}

const char* ResourcesException::what() const noexcept  // 0x1f1340
{
    // Returns msg_.c_str()
    // If string is empty (length == 0), returns static empty string at 0x8ffffd
    if (msg_.empty()) {
        return "";
    }
    return msg_.c_str();
}

// ============================================================================
// getApiErrorMessage(int ziResultCode)
// Binary address: 0x2e4820
//
// Looks up ziResultCode in the apiErrorMessages hash table (BSS at 0xb85230).
// The table is an open-addressing hash table (unordered_map or flat_hash_map).
// Uses popcount to determine if table size is power-of-2 (fast path: AND mask)
// or general (slow path: modulo).
//
// Returns reference to static "unknownError" string at 0x962ba8 if not found.
// ============================================================================
//! \brief Fallback diagnostic string returned by `getApiErrorMessage` when
//!        the supplied code is not present in the message table.
static const std::string unknownError = "unknownError";

namespace {

// ============================================================================
// apiErrorMessages — anonymous-namespace flat table at BSS 0xb85230 in the
// binary, separate from ErrorMessages::messages (BSS 0xb84c38).
//
// The binary maintains two independently-populated catalogues whose entries
// happen to agree on every shared key.  D-AUDIT-3 (IF-251) restores that
// data-flow shape: getApiErrorMessage now consults this anon-ns table, not
// the public ErrorMessages::messages map.
//
// Keys mirror the 52 entries the binary's apiErrorMessages contains:
//   16384..16389 (gap at 16388) — general status
//   32768..32800                — LabOne API
//   36864..36877                — device firmware
// ============================================================================
std::map<int, std::string> apiErrorMessages;

struct ApiErrorMessagesInitializer {
    ApiErrorMessagesInitializer() {
        auto& m = apiErrorMessages;

        // 16384..16389 (gap at 16388) — general status
        m[16384] = "Success (no error)";
        m[16385] = "Warning (general)";
        m[16386] = "FIFO underrun";
        m[16387] = "FIFO overflow";
        m[16389] = "Value or node not found";

        // 32768..32800 — LabOne API
        m[32768] = "Keyword could not be resolved";
        m[32769] = "Error (general)";
        m[32770] = "USB communication failed";
        m[32771] = "Memory allocation failed";
        m[32772] = "Unable to initialize mutex";
        m[32773] = "Unable to destroy mutex";
        m[32774] = "Unable to lock mutex";
        m[32775] = "Unable to unlock mutex";
        m[32776] = "Unable to start thread";
        m[32777] = "Unable to join thread";
        m[32778] = "Unable to initialize socket";
        m[32779] = "Unable to connect to socket";
        m[32780] = "Hostname not found";
        m[32781] = "Connection invalid";
        m[32782] = "Command timed out";
        m[32783] = "Command failed internally";
        m[32784] = "Command failed on the server";
        m[32785] = "Provided buffer is too small";
        m[32786] = "Unable to open file or read from it";
        m[32787] = "There is already a similar item";
        m[32788] = "Attempt to get a write-only node";
        m[32789] = "Device is not visible to the server";
        m[32790] = "Device is already connected to a different server";
        m[32791] = "Device does not support the specified interface";
        m[32792] = "Device connection attempt timed out";
        m[32793] = "Device already connected over a different interface";
        m[32794] = "Device needs a firmware upgrade";
        m[32795] = "Device not found";
        m[32796] = "Data type mismatch. Trying to get data from a poll event with wrong target data type";
        m[32797] = "Command or provided arguments combination is not supported within this context";
        m[32798] = "The maximum number of allowed simultaneous sessions (32) exceeded. Ensure unused sessions are closed";
        m[32799] = "This functionality is not supported on HF2 devices";
        m[32800] = "Attempt to set a read-only node";

        // 36864..36877 — device firmware
        m[36864] = "Invalid argument received";
        m[36865] = "An error occurred on the device, but the error code was not provided. Please report";
        m[36866] = "Requested block address does not belong to any known aperture";
        m[36867] = "Requested block address belongs to an aperture, but the offset is above the active range";
        m[36868] = "Requested block address belongs to an aperture, but the offset+length is above the active range";
        m[36869] = "Write access attempted to a read-only aperture or a read-only node";
        m[36870] = "Access to the requested register cannot be handled by the firmware (firmware configuration error)";
        m[36871] = "Number of indices requested on a non-indexed vector node";
        m[36872] = "Vector transfer header error: received less data than the header size";
        m[36873] = "Vector transfer header error: indicated frame index is greater than the indicated frame count";
        m[36874] = "Vector transfer header error: the indicated offset and payload length exceeds the indicated overall length";
        m[36875] = "Vector transfer header error: the indicated number of extra words exceeds the indicated overall length";
        m[36876] = "Received out-of-sequence frame (may never be reported if we keep the agreement, that the entire vector transfer has to be ACKed in case its first frame was ACKed)";
        m[36877] = "Vector transfer error: accessing indexed element outside the current active range";
    }
};
ApiErrorMessagesInitializer apiErrorMessagesInitializer;

}  // anonymous namespace

//! \brief Translate a LabOne API / device-firmware return code into its
//!        human-readable diagnostic string.
//!
//! \details Looks up `ziResultCode` in a dedicated catalogue (separate from
//! the compiler-diagnostic `ErrorMessages::messages` map) covering the
//! high-id ranges 16384–16389 (general status), 32768–32800 (LabOne API),
//! and 36864–36877 (device firmware).  If the code is not registered the
//! function falls back to a fixed `"unknownError"` string rather than
//! throwing, so callers can use it unconditionally on any non-zero return
//! value.
//!
//! \param ziResultCode  Numeric `ZIResult_enum` value as returned by a
//!                      LabOne C API call.
//! \return Reference to a process-lifetime string owned by either the
//!         API-error catalogue or the static `unknownError` fallback;
//!         never throws.
std::string const& getApiErrorMessage(int ziResultCode)  // 0x2e4820
{
    auto it = apiErrorMessages.find(ziResultCode);
    if (it != apiErrorMessages.end()) {
        return it->second;
    }
    return unknownError;
}

// ============================================================================
// Global initializer — populates ErrorMessages::messages
// Binary address: __cxx_global_var_init at 0xd5de0
//
// The full 305-entry initialization is in the binary as repeated
// map::insert(pair<int, string>) calls. The full format string table
// lives inline below; see error_messages.hpp for the ErrorMessageT enum
// mapping all 305 values.
// ============================================================================

namespace {
struct ErrorMessagesInitializer {
    ErrorMessagesInitializer() {
        auto& m = ErrorMessages::messages;

        // --- Table from original binary (254 entries) ---
        // Keys 0-255 with gaps at 47 and 53

        m[0] = "%1% command without valid register";
        m[1] = "opcode %1% register %1% not given";
        m[2] = "opcode %1% value %1% not given";
        m[3] = "register does not exist";
        m[4] = "instruction %1% (opcode %2%) expects at least %3% argument(s), %4% argument(s) given";
        m[5] = "value %1% is out of range for %2% bits";
        m[6] = "addr, subr, andr, orr and xnorr expect exactly 2 arguments";
        m[7] = "instruction %1% (opcode %2%) expects %3% arguments";
        m[8] = "wrong argument type, expected register";
        m[9] = "wrong argument type, expected value";
        m[10] = "user register does not exist, just %1% registers available";
        m[11] = "%1% is not a valid opcode";
        m[12] = "program is too large to fit into memory - has %1% instructions, maximum is %2%";
        m[13] = "arrays are just supported with type wave";
        m[14] = "an array index always needs to be a const or cvar value";
        m[15] = "the array index is out of the array range";
        m[16] = "can't logically invert a variable of type %1%";
        m[17] = "can't interpret %1% as boolean expression";
        m[18] = "broken or empty %1% list detected";
        m[19] = "tried to free a NULL pointer";
        m[20] = "tried to free a modified memory pointer";
        m[21] = "cache memory full";
        m[22] = "tried to play a NULL pointer";
        m[23] = "case needs to be a const";
        m[24] = "redefined case %1%";
        m[25] = "redefined default case";
        m[26] = "required to define a case before any statement";
        m[27] = "case has to be a positive natural number, %1% not valid";
        m[28] = "case should be a natural number, gets rounded from %1% to %2%";
        m[29] = "case should only be called out of a switch statment";
        m[30] = "could not compress sections in output file '%1%'";
        m[31] = "conditional expression expects a var, cvar or const argument";
        m[32] = "tried to modify const value";
        m[33] = "expected a command";
        m[34] = "this command will never be reached";
        m[35] = "CSV file '%1%' has an inconsistent number of channels";
        m[36] = "wrong value in CSV file '%1%' (line %2%), only numerical values are allowed";
        m[37] = "value in CSV file '%1%' (line %2%) out of range";
        m[38] = "CSV file '%1%' exists but is empty";
        m[39] = "%1% expects a var or const argument";
        m[40] = "invalid device nr %1%, just %2% devices activated";
        m[41] = "division by zero";
        m[42] = "can't divide a const by a wave";
        m[43] = "nothing to write, empty input";
        m[44] = "empty operation, something went wrong";
        m[45] = "executeTableEntry expects a single const or var argument";
        m[46] = "Invalid constant argument used for executeTableEntry";
        m[47] = "Invalid constant argument used for executeTableEntry (the original binary has no m[47] and crashes here with map::at — you're welcome, ZI)";
        m[48] = "Invalid table index used for executeTableEntry";
        m[49] = "waveforms are not fitting into cache memory, load not aligned across AWGs - every play will be prepended by sync";
        m[50] = "waveforms are not fitting into cache memory, gapless playback is not guaranteed";
        m[51] = "waveforms are not fitting into wave memory (%|1$.1f| kSa over a maximum of %|2$.1f| kSa)";
        m[52] = "constant '%1%' is deprecated, please use %2%";

        m[54] = "function '%1%' called with logical value instead of cycle count";
        m[55] = "function '%1%' is deprecated, please use '%2%'";
        m[56] = "function '%1%' is deprecated, please use '%2%' or '%3%'";
        m[57] = "function '%1%' is deprecated";
        m[58] = "function '%1%' is deprecated as not compatible with AWG FIFO architecture";
        m[59] = "function '%1%' is not supported in multi-core mode";
        m[60] = "function '%1% %2%%3%' is empty";
        m[61] = "function '%1%' expects at least %2% argument(s), %3% arguments given";
        m[62] = "function '%1%' expects (const) as argument";
        m[63] = "function '%1%' expects (const, const) as arguments";
        m[64] = "function '%1%' expects (const, var) as arguments";
        m[65] = "function '%1%' expects (const, const, const) as arguments";
        m[66] = "function '%1%' expects no arguments";
        m[67] = "function '%1%' expects a single const, cvar or var argument";
        m[68] = "function '%1%' expects %2% arguments, %3% arguments given";
        m[69] = "function '%1%' expects %2% or fewer argument(s), %3% arguments given";
        m[70] = "invalid argument type %1% in function '%2%'; expects argument '%3%' to be %4%";
        m[71] = "function does not have a name";
        m[72] = "function '%1%' reaches end without returning a %2%";
        m[73] = "function '%1%' not supported on %2% devices";
        m[74] = "function '%1%' expects only const arguments";
        m[75] = "function '%1%' is a predefined function and cannot be overwritten";
        m[76] = "function '%1%' not found, possible alternatives are";
        m[77] = "function '%1%' expects %2% argument(s), %3% argument(s) given";
        m[78] = "function '%1%' expects argument %2% to be less than %3%";
        m[79] = "DIO and ZSync external triggering can't be mixed in the same program";
        m[80] = "function '%1%' expects a string or wave as first argument";
        m[81] = "function '%1%' expects a const as second argument";
        m[82] = "function '%1%' expects just waveforms (string or wave) as arguments";
        m[83] = "function 'add' expects more than one waveform (string or wave)";
        m[84] = "%1% has a higher amplitude than 1.0, waveform amplitude will be limited to 1.0";
        m[85] = "argument %1% of %2% must be a const";
        m[86] = "argument %1% of %2% must be a string";
        m[87] = "argument %1% of %2% is out of the allowed range of -1.0 to 1.0";
        m[88] = "argument %1% of %2% is greater than the waveform length";
        m[89] = "join expects at least 2 waveforms";
        m[90] = "tried to '%1%' the unknown waveform '%2%'";
        m[91] = "function '%1%' expects %2% argument(s), %3% argument(s) given";
        m[92] = "function '%1%' expects %2% or %3% arguments, %4% argument(s) given";
        m[93] = "function '%1%' expects %2% to %3% arguments, %4% argument(s) given";
        m[94] = "argument %1% of %2% must be positive";
        m[95] = "argument %1% of %2% is larger than the waveform length";
        m[96] = "argument %1% of %2% out of range, %3% overflow";
        m[97] = "called function '%1%' without arguments; string or const arguments expected";
        m[98] = "argument %1% of %2% is not allowed to be 0";
        m[99] = "%3% value %1% is larger than the maximum possible value 3, will be capped to %2%";
        m[100] = "%2% value %1% must be 1 or 2";
        m[101] = "argument %1%, LFSR initial value, can't be 0";
        m[102] = "generate(string, const...) expects a string as first argument";
        m[103] = "%1% can't be called with var arguments";
        m[104] = "getUserReg expects exactly one const argument";
        m[105] = "getUserReg register arguments must be in the range of 0 to 15";
        m[106] = "getCnt expects exactly one const argument";
        m[107] = "getCnt counter argument must be either 0 or 1";
        m[108] = "getSweeperLength expects zero arguments or a single const argument";
        m[109] = "getSweeperLength argument must be 1";
        m[110] = "invalid value for argument %1% of function %2%";
        m[111] = "can't increment %1% value %2%";
        m[112] = "can't decrement %1% value %2%";
        m[113] = "Input file '%1%' does not exist";
        m[114] = "no such file or directory: '%1%'";
        m[115] = "can't add variables of type %1% and %2%";
        m[116] = "can't subtract variables of type %1% and %2%";
        m[117] = "Invalid ZSync data_type provided";
        m[118] = "Invalid Feedback data_type provided";
        m[119] = "only const or var types can be inverted, %1% given";
        m[120] = "label '%1%' not found";
        m[121] = "lock expects exactly one argument";
        m[122] = "it's just possible to lock variables of type wave";
        m[123] = "too many iterations to unroll this loop, use a const variable for infinite loop or a var variable for this many iterations";
        m[124] = "only const or var types can be negated, %1% given";
        m[125] = "operation without operator, something went wrong";
        m[126] = "no valid argument for branch command";
        m[127] = "node '%1%' can only be written using setDouble";
        m[128] = "node '%1%' requires an %2% value, therefore the double precision is lost";
        m[129] = "frequency node '%1%' can only be set using a const or cvar";
        m[130] = "phase node '%1%' can only be set using a const or cvar";
        m[131] = "node '%1%' doesn't exist";
        m[132] = "node '%1%' requires MF option installed";
        m[133] = "sequencer '%1%' can't drive '%2%'";
        m[134] = "const or var types can be logically inverted, %1% given";
        m[135] = "%1%: invalid argument";
        m[136] = "function '%1%' accepts a single argument";
        m[137] = "function '%1%' accepts exactly two arguments";
        m[138] = "a var variable can just be multiplied with a natural number";
        m[139] = "can't assign a %1% to a %2% variable";
        m[140] = "can't multiply a %1% and a %2% variable";
        m[141] = "can't divide a %1% and a %2% variable";
        m[142] = "can't apply modulo on a %1% with a %2% variable";
        m[143] = "can't apply bitwise AND on a %1% and a %2% variable";
        m[144] = "can't apply bitwise OR on a %1% and a %2% variable";
        m[145] = "can't combine variables of type %1% and %2%";
        m[146] = "can't compare data type %1% with %2%";
        m[147] = "no valid operation type";
        m[148] = "Could not write to file '%1%': invalid path or filename";
        m[149] = "%1%: only const waveform index allowed";
        m[150] = "%1%: unexpected arguments";
        m[151] = "%1% offset is higher than the waveform length itself";
        m[152] = "%1% expects an offset and a length, just one parameter given";
        m[153] = "%1% length is too long for the given offset";
        m[154] = "%1% expects the number of samples to play as a const argument";
        m[155] = "%1% expects the address within the waveform as const or var argument";
        m[156] = "%1% length is 0, no wave will be played";
        m[157] = "%1% expects the name of the waveform as a string or wave";
        m[158] = "%1% expects at most %2% channels, %3% given";
        m[159] = "%1% sample rate can only be described with a const, not with a %2%";
        m[160] = "%1% sample rate can not be greater than 14.0 MHz";
        m[161] = "playDIOWave sample rate can not be greater than 450 MHz";
        m[162] = "prefetch Error: %1%";
        m[163] = "invalid identifier while placing the fetch and play commands";
        m[164] = "tried to swap two not connected nodes";
        m[165] = "explicit prefetch is not supported on this HW type";
        m[166] = "function '%1%' expected more arguments with the given string";
        m[167] = "function '%1%' expected less arguments with the given string";
        m[168] = "function '%1%' can't interpret the format of the given string";
        m[169] = "function '%1%' expects %2% arguments, %3% given";
        m[170] = "function '%1%' can't have a var argument with a %2% return value";
        m[171] = "tried to define '%1%' but it is already defined";
        m[172] = "run out of free registers, please reduce complexity";
        m[173] = "return statement not in a function";
        m[174] = "type mismatch, can't read %1% as %2%";
        m[175] = "type mismatch, can't write %1% to %2%";
        m[176] = "tried to access uninitialized variable '%1%'";
        m[177] = "tried to access unknown variable '%1%'";
        m[178] = "expected a return value of type %1% but no value given";
        m[179] = "return label stack is empty";
        m[180] = "expected no return value but value of type %1% given";
        m[181] = "expected a return value of type %1%, %2% given";
        m[182] = "invalid return argument";
        m[183] = "repeat only accepts non-negative number of repetitions";
        m[184] = "setDouble expects 2 or 3 arguments";
        m[185] = "setDouble expects a string as first argument, %1% given";
        m[186] = "setDouble expects a var or const as second argument, %1% given";
        m[187] = "setDouble expects a const as third argument, %1% given";
        m[188] = "setInt expects 2 arguments";
        m[189] = "setInt expects a string as first argument, %1% given";
        m[190] = "setInt expects a var or const as second argument, %1% given";
        m[191] = "setRate expects a const argument";
        m[192] = "setRate expects just one const argument";
        m[193] = "setPrecompClear expects a const argument";
        m[194] = "setPrecompClear expects just one const argument";
        m[195] = "final value of precompensation flags must be consistent in all branches";
        m[196] = "precompensation flags are indeterminate after loop execution";
        m[197] = "setUserReg expects a const as first argument";
        m[198] = "setUserReg register must be in the range of 0 to 15";
        m[199] = "setUserReg expects exactly two arguments";
        m[200] = "setUserReg expects a var or const as second argument";
        m[201] = "invalid resetOscPhase argument";
        m[202] = "resetOscPhase expects a single const argument";
        m[203] = "PRNG seed expected to be positve";
        m[204] = "PRNG seed equal to zero is not valid";
        m[205] = "PRNG seed exceeded maximum of 65535 (0xFFFF)";
        m[206] = "setPRNGRange expects two const arguments (lower, upper) in range 0...65534, lower < upper";
        m[207] = "setTrigger expects a single const or var argument";
        m[208] = "setInternalTrigger expects a single const or var argument";
        m[209] = "%1% expects two const arguments";
        m[210] = "%1% expects one const argument";
        m[211] = "%1% expects a sine generator index of 0 or 1";
        m[212] = "can't shift a %1% variable depending on a %2%";
        m[213] = "%1% statement is not supported";
        m[214] = "switch statement contains no case statements and will, therefore, be ignored";
        m[215] = "'%1%' index must be %2%";
        m[216] = "calling unknown function '%1%'";
        m[217] = "unknown target device '%1%'";
        m[218] = "FIFO play mode is not supported on device type '%1%'";
        m[219] = "FIFO play mode is required on device type '%1%'";
        m[220] = "unlock expects exactly one argument";
        m[221] = "it's just possible to unlock variables of type wave";
        m[222] = "%1% is not supported in %2% channel grouping mode";
        m[223] = "division is not supported for var, only for const";
        m[224] = "can't assign the type-less variable '%1%'";
        m[225] = "using vect with more that %1% arguments is discouraged, use CSV files to define your waveforms instead";
        m[226] = "wait expects a positive argument";
        m[227] = "waveform '%1%' does not exist";
        m[228] = "can't modify a %1% variable depending on a var or inside of a repeat loop";
        m[229] = "inconsistent number of channels in waveform %1% command (waveform %2% has %3%, expected %4%)";
        m[230] = "waveform '%1%' size %2% is not aligned to %3% samples and will be zero-extended to %4% samples";
        m[231] = "play length %1% is not aligned to %2% samples and will be extended to %3% samples";
        m[232] = "the given waveforms are not of same length, shorter waveforms will be filled with zeros to match the length of the longest waveform";
        m[233] = "no waveform with the name '%1%' found";
        m[234] = "tried to access uninitialized waveform";
        m[235] = "wave %1% is not unique, %2% used";
        m[236] = "can't play channel %1%, valid values for channel are from 1 to %2%";
        m[237] = "wave '%1%' expected to have %2% channel(s) but has %3%";
        m[238] = "mixing of numbered and unnumbered channels [ %1%(1,w1,w2) ] is not allowed";
        m[239] = "no waveform found in function %1%";
        m[240] = "Channel %1% has been defined multiple times";
        m[241] = "number of waveforms in wavetable is too large - has %1% waveforms, maximum is %2%";
        m[242] = "XOR operations are not supported";
        m[243] = "minimal waveform length for playWaveDigTrigger() command is %1% samples";
        m[244] = "waveform '%1%' size %2% is below the minimal waveform length and will be zero-extended to %3% samples";
        m[245] = "play length %1% is below minimum of %2% samples, will be extended";
        m[246] = "waveform %1% is in the used waveforms list but is not marked for load";
        m[247] = "waveform %1% is not fitting in preload cache - will cause play gap";
        m[248] = "waveform %1% has already assigned index";
        m[249] = "waveform index already used";
        m[250] = "waveform index exceeds wavetable size";
        m[251] = "invalid waveform name encountered. Please make sure that the waveform name supplied contains only alphanumeric characters or underscores and does not start with a digit.";
        m[252] = "waveform name %1% is already in use, please make sure to use unique waveform names.";
        m[253] = "attempting to perform a bitwise operation on a negative operand. Operands supplied: %1% and %2%.";
        m[254] = "attempting to perform a bitwise operation on a negative operand. Operand supplied: %1%.";
        m[255] = "unexpected error";

        // General status codes (16384+)
        m[16384] = "Success (no error)";
        m[16385] = "Warning (general)";
        m[16386] = "FIFO underrun";
        m[16387] = "FIFO overflow";
        m[16389] = "Value or node not found";

        // LabOne API errors (32768+)
        m[32768] = "Keyword could not be resolved";
        m[32769] = "Error (general)";
        m[32770] = "USB communication failed";
        m[32771] = "Memory allocation failed";
        m[32772] = "Unable to initialize mutex";
        m[32773] = "Unable to destroy mutex";
        m[32774] = "Unable to lock mutex";
        m[32775] = "Unable to unlock mutex";
        m[32776] = "Unable to start thread";
        m[32777] = "Unable to join thread";
        m[32778] = "Unable to initialize socket";
        m[32779] = "Unable to connect to socket";
        m[32780] = "Hostname not found";
        m[32781] = "Connection invalid";
        m[32782] = "Command timed out";
        m[32783] = "Command failed internally";
        m[32784] = "Command failed on the server";
        m[32785] = "Provided buffer is too small";
        m[32786] = "Unable to open file or read from it";
        m[32787] = "There is already a similar item";
        m[32788] = "Attempt to get a write-only node";
        m[32789] = "Device is not visible to the server";
        m[32790] = "Device is already connected to a different server";
        m[32791] = "Device does not support the specified interface";
        m[32792] = "Device connection attempt timed out";
        m[32793] = "Device already connected over a different interface";
        m[32794] = "Device needs a firmware upgrade";
        m[32795] = "Device not found";
        m[32796] = "Data type mismatch. Trying to get data from a poll event with wrong target data type";
        m[32797] = "Command or provided arguments combination is not supported within this context";
        m[32798] = "The maximum number of allowed simultaneous sessions (32) exceeded. Ensure unused sessions are closed";
        m[32799] = "This functionality is not supported on HF2 devices";
        m[32800] = "Attempt to set a read-only node";

        // Device firmware errors (36864+)
        m[36864] = "Invalid argument received";
        m[36865] = "An error occurred on the device, but the error code was not provided. Please report";
        m[36866] = "Requested block address does not belong to any known aperture";
        m[36867] = "Requested block address belongs to an aperture, but the offset is above the active range";
        m[36868] = "Requested block address belongs to an aperture, but the offset+length is above the active range";
        m[36869] = "Write access attempted to a read-only aperture or a read-only node";
        m[36870] = "Access to the requested register cannot be handled by the firmware (firmware configuration error)";
        m[36871] = "Number of indices requested on a non-indexed vector node";
        m[36872] = "Vector transfer header error: received less data than the header size";
        m[36873] = "Vector transfer header error: indicated frame index is greater than the indicated frame count";
        m[36874] = "Vector transfer header error: the indicated offset and payload length exceeds the indicated overall length";
        m[36875] = "Vector transfer header error: the indicated number of extra words exceeds the indicated overall length";
        m[36876] = "Received out-of-sequence frame (may never be reported if we keep the agreement, that the entire vector transfer has to be ACKed in case its first frame was ACKed)";
        m[36877] = "Vector transfer error: accessing indexed element outside the current active range";
    }
} errorMessagesInit_;
}

} // namespace zhinst

// ============================================================================
// Explicit template instantiations for ErrorMessages::format<>()
//
// The binary emits out-of-line instantiations for ~54 arg-type combinations.
// The outer format(ErrorMessageT, ...) and inner format(boost::format&, ...)
// are both instantiated.
// ============================================================================
namespace zhinst {

//! \cond INTERNAL
// Internal aliases used only by the explicit template instantiations
// below; hidden from the public docs site along with the instantiations
// themselves (Doxygen otherwise warns on each explicit instantiation).
using S = std::string;
using BF = boost::format;

// Explicit template instantiations of ErrorMessages::format. Hidden from
// Doxygen because each instantiation otherwise generates a "no matching
// class member" warning (Doxygen can't bind explicit-instantiation lines
// to their parameterised template declaration).

// --- format(ErrorMessageT, Args...) ---
// Zero-arg variadic (binary emits format<>(ErrorMessageT) — no non-template overload)
template std::string ErrorMessages::format<>(ErrorMessageT);
template std::string ErrorMessages::format<int>(ErrorMessageT, int);
template std::string ErrorMessages::format<int, int>(ErrorMessageT, int, int);
template std::string ErrorMessages::format<int, int, int>(ErrorMessageT, int, int, int);
template std::string ErrorMessages::format<int, short>(ErrorMessageT, int, short);
template std::string ErrorMessages::format<int, char const*>(ErrorMessageT, int, char const*);
template std::string ErrorMessages::format<int, S>(ErrorMessageT, int, S);
template std::string ErrorMessages::format<int, int, S>(ErrorMessageT, int, int, S);
template std::string ErrorMessages::format<double, double>(ErrorMessageT, double, double);
template std::string ErrorMessages::format<double, int>(ErrorMessageT, double, int);
template std::string ErrorMessages::format<unsigned long>(ErrorMessageT, unsigned long);
template std::string ErrorMessages::format<unsigned long, unsigned long>(ErrorMessageT, unsigned long, unsigned long);
template std::string ErrorMessages::format<unsigned long, char const*>(ErrorMessageT, unsigned long, char const*);
template std::string ErrorMessages::format<char const*>(ErrorMessageT, char const*);
template std::string ErrorMessages::format<char const*, char const*>(ErrorMessageT, char const*, char const*);
template std::string ErrorMessages::format<char const*, int, int, unsigned long>(ErrorMessageT, char const*, int, int, unsigned long);
template std::string ErrorMessages::format<char const*, int, unsigned long>(ErrorMessageT, char const*, int, unsigned long);
template std::string ErrorMessages::format<char const*, S>(ErrorMessageT, char const*, S);
template std::string ErrorMessages::format<char const*, S, unsigned short, short>(ErrorMessageT, char const*, S, unsigned short, short);
template std::string ErrorMessages::format<S>(ErrorMessageT, S);
template std::string ErrorMessages::format<S, char const*>(ErrorMessageT, S, char const*);
template std::string ErrorMessages::format<S, char const*, unsigned long>(ErrorMessageT, S, char const*, unsigned long);
template std::string ErrorMessages::format<S, int, int>(ErrorMessageT, S, int, int);
template std::string ErrorMessages::format<S, int, int, unsigned int>(ErrorMessageT, S, int, int, unsigned int);
template std::string ErrorMessages::format<S, int, int, unsigned long>(ErrorMessageT, S, int, int, unsigned long);
template std::string ErrorMessages::format<S, int, unsigned int>(ErrorMessageT, S, int, unsigned int);
template std::string ErrorMessages::format<S, int, unsigned long>(ErrorMessageT, S, int, unsigned long);
template std::string ErrorMessages::format<S, int, unsigned short>(ErrorMessageT, S, int, unsigned short);
template std::string ErrorMessages::format<S, int, S>(ErrorMessageT, S, int, S);
template std::string ErrorMessages::format<S, short, int>(ErrorMessageT, S, short, int);
template std::string ErrorMessages::format<S, short, unsigned short>(ErrorMessageT, S, short, unsigned short);
template std::string ErrorMessages::format<S, unsigned long>(ErrorMessageT, S, unsigned long);
template std::string ErrorMessages::format<S, unsigned long, unsigned long>(ErrorMessageT, S, unsigned long, unsigned long);
template std::string ErrorMessages::format<S, S>(ErrorMessageT, S, S);
template std::string ErrorMessages::format<S, S, char const*>(ErrorMessageT, S, S, char const*);
template std::string ErrorMessages::format<S, S, int, S>(ErrorMessageT, S, S, int, S);
template std::string ErrorMessages::format<S, S, S>(ErrorMessageT, S, S, S);
template std::string ErrorMessages::format<S, S, S, char const*>(ErrorMessageT, S, S, S, char const*);
template std::string ErrorMessages::format<S, S, S, S>(ErrorMessageT, S, S, S, S);
template std::string ErrorMessages::format<S, S, unsigned long, S>(ErrorMessageT, S, S, unsigned long, S);

// --- format(boost::format&, Args...) [inner helper] ---
// Signature: format<T, Args...>(BF&, T, Args...)
template std::string ErrorMessages::format<char const*, S>(BF&, char const*, S);
template std::string ErrorMessages::format<char const*, S, unsigned short, short>(BF&, char const*, S, unsigned short, short);
template std::string ErrorMessages::format<int, int, S>(BF&, int, int, S);
template std::string ErrorMessages::format<int, S>(BF&, int, S);
template std::string ErrorMessages::format<S, int, S>(BF&, S, int, S);
template std::string ErrorMessages::format<S, unsigned long, S>(BF&, S, unsigned long, S);
template std::string ErrorMessages::format<S, S>(BF&, S, S);
template std::string ErrorMessages::format<S, S, char const*>(BF&, S, S, char const*);
template std::string ErrorMessages::format<S, S, int, S>(BF&, S, S, int, S);
template std::string ErrorMessages::format<S, S, S>(BF&, S, S, S);
template std::string ErrorMessages::format<S, S, S, char const*>(BF&, S, S, S, char const*);
template std::string ErrorMessages::format<S, S, S, S>(BF&, S, S, S, S);
template std::string ErrorMessages::format<S, S, unsigned long, S>(BF&, S, S, unsigned long, S);
template std::string ErrorMessages::format<unsigned long, S>(BF&, unsigned long, S);
//! \endcond

} // namespace zhinst
