// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// ErrorMessages + ResourcesException implementations
//
// Source path (from debug info): ErrorMessages.cpp
// Global init: __cxx_global_var_init at 0xd5de0
// ============================================================================

#include "zhinst/error_messages.hpp"
#include "zhinst/resources.hpp"  // for ResourcesException

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
// Phase 20a: extern declared in 4 caller TUs (cache, custom_functions,
// node, prefetch_emit) but never defined. Definition added here.
// ============================================================================
ErrorMessages errMsg;

// ============================================================================
// Three namespace-scope `const std::string` globals referenced from
// static_resources.cpp:21-23. BSS addresses verified against binary.
// Phase 20a: extern declared, never defined. String contents recovered
// from binary rodata via `strings _seqc_compiler.so`:
//   _ZN6zhinstL26constAwgIntegrationTriggerE → "AWG_INTEGRATION_TRIGGER"
//   _ZN6zhinstL21zsyncDataPqscRegisterE      → "ZSYNC_DATA_PQSC_REGISTER"
//   _ZN6zhinstL20zsyncDataPqscDecoderE       → "ZSYNC_DATA_PQSC_DECODER"
//
// NOTE: the binary mangles these with the `L` prefix indicating internal
// linkage (declared `static` inside `namespace zhinst`). Our header
// declares them `extern` so callers can reference them across TUs;
// the actual binary may have them duplicated per-TU. This is a slight
// ABI deviation but shouldn't affect behaviour.
// ============================================================================
const std::string zsyncDataPqscDecoder       = "ZSYNC_DATA_PQSC_DECODER";
const std::string zsyncDataPqscRegister      = "ZSYNC_DATA_PQSC_REGISTER";
const std::string constAwgIntegrationTrigger = "AWG_INTEGRATION_TRIGGER";

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
// ErrorMessages::format(ErrorMessageT) — no-arg version
// Binary address: 0x15d0d0
//
// Looks up format string, wraps in boost::basic_format, returns str().
// No argument feeding since there are no parameters.
// ============================================================================
std::string ErrorMessages::format(ErrorMessageT id)  // 0x15d0d0
{
    // In the binary this inlines the map lookup and constructs
    // boost::basic_format, then immediately calls .str().
    // Simplified reconstruction:
    return messages.at(static_cast<int>(id));
}

// ============================================================================
// Template instantiation pattern (documented, not all reconstructed)
//
// ~40 instantiations exist in the binary. Each follows this pattern:
//
//   std::string ErrorMessages::format<Args...>(ErrorMessageT id, Args... args) {
//       auto& fmt_str = messages.at(static_cast<int>(id));
//       boost::basic_format<char> fmt(fmt_str);      // 0x10fdc0
//       // For each arg:
//       //   boost::io::detail::feed_impl(fmt, arg);  // 0x112bc0
//       (fmt % ... % args);
//       return fmt.str();                             // 0x114470
//   }
//
// Known instantiations (address — template args):
//   0x15d0d0 — <>                    (no-arg, see above)
//   0x171d40 — <int>
//   0x122520 — <int, int>
//   0x15b240 — <int, int, int>
//   0x25e980 — <int, int, string>
//   0x16b8f0 — <int, string>
//   0x16e110 — <int, char const*>
//   0x171090 — <int, short>
//   0x28a530 — <unsigned long>
//   0x107b40 — <unsigned long, unsigned long>
//   0x16d7b0 — <unsigned long, char const*>
//   0x106170 — <string>
//   0x2896a0 — <string, int, int>
//   0x15b7d0 — <string, int, int, unsigned int>
//   0x16d5c0 — <string, int, int, unsigned long>
//   0x163570 — <string, int, unsigned int>
//   0x15d220 — <string, int, unsigned long>
//   0x15b610 — <string, int, string>
//   0x171e20 — <string, int, unsigned short>
//   0x2b8a00 — <string, unsigned long>
//   0x210220 — <string, unsigned long, unsigned long>
//   0x12a370 — <string, char const*>
//   0x25ed90 — <string, char const*, unsigned long>
//   0x15a0f0 — <string, string>
//   0x16d140 — <string, string, int, string>
//   0x16d380 — <string, string, unsigned long, string>
//   0x25d2d0 — <string, string, char const*>
//   0x15a290 — <string, string, string>
//   0x1ea500 — <string, string, string, char const*>
//   0x2103e0 — <string, string, string, string>
//   0x170cf0 — <string, short, int>
//   0x163f10 — <string, short, unsigned short>
//   0x15c0d0 — <char const*>
//   0x15cf50 — <char const*, int, int, unsigned long>
//   0x15cdf0 — <char const*, int, unsigned long>
//   0x164430 — <char const*, string>
//   0x25f060 — <char const*, string, unsigned short, short>
//   0x1640e0 — <char const*, char const*>
//   0x1cc810 — <double, double>
//   0x21b560 — <double, int>
// ============================================================================

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
static const std::string unknownError = "unknownError";

std::string const& getApiErrorMessage(int ziResultCode)  // 0x2e4820
{
    // Simplified: the binary does hash table lookup on apiErrorMessages.
    // The table contains entries for keys 16384-16389, 32768-32800, 36864-36877.
    // These are the same keys that appear in ErrorMessages::messages,
    // but stored in a separate hash table for fast O(1) lookup.
    auto it = ErrorMessages::messages.find(ziResultCode);
    if (it != ErrorMessages::messages.end()) {
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

        // Assembler errors (1-12)
        m[1]  = "%1% command without valid register";
        m[2]  = "opcode %1% register %1% not given";
        m[3]  = "opcode %1% value %1% not given";
        m[4]  = "register does not exist";
        m[5]  = "instruction %1% (opcode %2%) expects at least %3% argument(s), %4% argument(s) given";
        m[6]  = "value %1% is out of range for %2% bits";
        m[7]  = "addr, subr, andr, orr and xnorr expect exactly 2 arguments";
        m[8]  = "instruction %1% (opcode %2%) expects %3% arguments";
        m[9]  = "wrong argument type, expected register";
        m[10] = "wrong argument type, expected value";
        m[11] = "user register does not exist, just %1% registers available";
        m[12] = "%1% is not a valid opcode";

        // Array/program errors (13-16)
        m[13] = "the array index is out of the array range";
        m[14] = "program is too large to fit into memory - has %1% instructions, maximum is %2%";
        m[15] = "an array index always needs to be a const or cvar value";
        m[16] = "arrays are just supported with type wave";

        // Type/memory errors (17-23)
        m[17] = "can't logically invert a variable of type %1%";
        m[18] = "can't interpret %1% as boolean expression";
        m[19] = "broken or empty %1% list detected";
        m[20] = "tried to free a NULL pointer";
        m[21] = "tried to free a modified memory pointer";
        m[22] = "cache memory full";
        m[23] = "tried to play a NULL pointer";

        // Case/switch (24-30)
        m[24] = "case needs to be a const";
        m[25] = "redefined case %1%";
        m[26] = "redefined default case";
        m[27] = "required to define a case before any statement";
        m[28] = "case has to be a positive natural number, %1% not valid";
        m[29] = "case should be a natural number, gets rounded from %1% to %2%";
        m[30] = "case should only be called out of a switch statment";  // sic: typo in binary

        // General compiler (31-52)
        m[31] = "tried to modify const value";
        m[32] = "could not compress sections in output file '%1%'";
        m[33] = "conditional expression expects a var, cvar or const argument";
        m[34] = "expected a command";
        m[35] = "this command will never be reached";
        m[36] = "CSV file '%1%' has an inconsistent number of channels";
        m[37] = "wrong value in CSV file '%1%' (line %2%), only numerical values are allowed";
        m[38] = "value in CSV file '%1%' (line %2%) out of range";
        m[39] = "CSV file '%1%' exists but is empty";
        m[40] = "can't divide a const by a wave";
        m[41] = "%1% expects a var or const argument";
        m[42] = "division by zero";
        m[43] = "invalid device nr %1%, just %2% devices activated";
        m[44] = "nothing to write, empty input";
        m[45] = "empty operation, something went wrong";
        m[46] = "executeTableEntry expects a single const or var argument";
        // 47 — UNASSIGNED
        m[48] = "Invalid constant argument used for executeTableEntry";
        m[49] = "Invalid table index used for executeTableEntry";
        m[50] = "waveforms are not fitting into cache memory, load not aligned across AWGs - every play will be prepended by sync";
        m[51] = "waveforms are not fitting into cache memory, gapless playback is not guaranteed";
        m[52] = "waveforms are not fitting into wave memory (%|1$.1f| kSa over a maximum of %|2$.1f| kSa)";
        // 53 — UNASSIGNED

        // Function validation (54-104)
        m[54]  = "constant '%1%' is deprecated, please use %2%";
        m[55]  = "function '%1%' called with logical value instead of cycle count";
        m[56]  = "function '%1%' is deprecated, please use '%2%'";
        m[57]  = "function '%1%' is deprecated, please use '%2%' or '%3%'";
        m[58]  = "function '%1%' is deprecated";
        m[59]  = "function '%1%' is deprecated as not compatible with AWG FIFO architecture";
        m[60]  = "function '%1%' is not supported in multi-core mode";
        m[61]  = "function '%1% %2%%3%' is empty";
        m[62]  = "function '%1%' expects at least %2% argument(s), %3% arguments given";
        m[63]  = "function '%1%' expects (const) as argument";
        m[64]  = "function '%1%' expects (const, const) as arguments";
        m[65]  = "function '%1%' expects (const, var) as arguments";
        m[66]  = "function '%1%' expects (const, const, const) as arguments";
        m[67]  = "function '%1%' expects no arguments";
        m[68]  = "function '%1%' expects a single const, cvar or var argument";
        m[69]  = "function '%1%' expects %2% arguments, %3% arguments given";
        m[70]  = "function '%1%' expects %2% or fewer argument(s), %3% arguments given";
        m[71]  = "invalid argument type %1% in function '%2%'; expects argument '%3%' to be %4%";
        m[72]  = "function does not have a name";
        m[73]  = "function '%1%' reaches end without returning a %2%";
        m[74]  = "function '%1%' not supported on %2% devices";
        m[75]  = "function '%1%' expects only const arguments";
        m[76]  = "function '%1%' is a predefined function and cannot be overwritten";
        m[77]  = "function '%1%' not found, possible alternatives are";
        m[78]  = "function '%1%' expects %2% argument(s), %3% argument(s) given";
        m[79]  = "function '%1%' expects argument %2% to be less than %3%";
        m[80]  = "DIO and ZSync external triggering can't be mixed in the same program";
        m[81]  = "function '%1%' expects a string or wave as first argument";
        m[82]  = "function '%1%' expects a const as second argument";
        m[83]  = "function '%1%' expects just waveforms (string or wave) as arguments";
        m[84]  = "function 'add' expects more than one waveform (string or wave)";
        m[85]  = "%1% has a higher amplitude than 1.0, waveform amplitude will be limited to 1.0";
        m[86]  = "argument %1% of %2% must be a const";
        m[87]  = "argument %1% of %2% must be a string";
        m[88]  = "argument %1% of %2% is out of the allowed range of -1.0 to 1.0";
        m[89]  = "argument %1% of %2% is greater than the waveform length";
        m[90]  = "join expects at least 2 waveforms";
        m[91]  = "tried to '%1%' the unknown waveform '%2%'";
        m[92]  = "function '%1%' expects %2% argument(s), %3% argument(s) given";
        m[93]  = "function '%1%' expects %2% or %3% arguments, %4% argument(s) given";
        m[94]  = "function '%1%' expects %2% to %3% arguments, %4% argument(s) given";
        m[95]  = "argument %1% of %2% must be positive";
        m[96]  = "argument %1% of %2% is larger than the waveform length";
        m[97]  = "argument %1% of %2% out of range, %3% overflow";
        m[98]  = "called function '%1%' without arguments; string or const arguments expected";
        m[99]  = "argument %1% of %2% is not allowed to be 0";
        m[100] = "%3% value %1% is larger than the maximum possible value 3, will be capped to %2%";
        m[101] = "%2% value %1% must be 1 or 2";
        m[102] = "argument %1%, LFSR initial value, can't be 0";
        m[103] = "generate(string, const...) expects a string as first argument";
        m[104] = "%1% can't be called with var arguments";

        // Built-in function validation (105-111)
        m[105] = "getUserReg expects exactly one const argument";
        m[106] = "getUserReg register arguments must be in the range of 0 to 15";
        m[107] = "getCnt expects exactly one const argument";
        m[108] = "getCnt counter argument must be either 0 or 1";
        m[109] = "getSweeperLength expects zero arguments or a single const argument";
        m[110] = "getSweeperLength argument must be 1";
        m[111] = "invalid value for argument %1% of function %2%";

        // Increment/file/type (112-125)
        m[112] = "can't increment %1% value %2%";
        m[113] = "can't decrement %1% value %2%";
        m[114] = "Input file '%1%' does not exist";
        m[115] = "no such file or directory: '%1%'";
        m[116] = "can't add variables of type %1% and %2%";
        m[117] = "can't subtract variables of type %1% and %2%";
        m[118] = "Invalid ZSync data_type provided";
        m[119] = "Invalid Feedback data_type provided";
        m[120] = "only const or var types can be inverted, %1% given";
        m[121] = "label '%1%' not found";
        m[122] = "lock expects exactly one argument";
        m[123] = "it's just possible to lock variables of type wave";
        m[124] = "too many iterations to unroll this loop, use a const variable for infinite loop or a var variable for this many iterations";
        m[125] = "only const or var types can be negated, %1% given";

        // Node/oscillator (126-134)
        m[126] = "operation without operator, something went wrong";
        m[127] = "no valid argument for branch command";
        m[128] = "node '%1%' can only be written using setDouble";
        m[129] = "node '%1%' requires an %2% value, therefore the double precision is lost";
        m[130] = "frequency node '%1%' can only be set using a const or cvar";
        m[131] = "phase node '%1%' can only be set using a const or cvar";
        m[132] = "node '%1%' doesn't exist";
        m[133] = "node '%1%' requires MF option installed";
        m[134] = "sequencer '%1%' can't drive '%2%'";

        // Variable arithmetic (135-148)
        m[135] = "const or var types can be logically inverted, %1% given";
        m[136] = "%1%: invalid argument";
        m[137] = "function '%1%' accepts a single argument";
        m[138] = "function '%1%' accepts exactly two arguments";
        m[139] = "a var variable can just be multiplied with a natural number";
        m[140] = "can't assign a %1% to a %2% variable";
        m[141] = "can't multiply a %1% and a %2% variable";
        m[142] = "can't divide a %1% and a %2% variable";
        m[143] = "can't apply modulo on a %1% with a %2% variable";
        m[144] = "can't apply bitwise AND on a %1% and a %2% variable";
        m[145] = "can't apply bitwise OR on a %1% and a %2% variable";
        m[146] = "can't combine variables of type %1% and %2%";
        m[147] = "can't compare data type %1% with %2%";
        m[148] = "no valid operation type";

        // File/playWave (149-162)
        m[149] = "Could not write to file '%1%': invalid path or filename";
        m[150] = "%1%: only const waveform index allowed";
        m[151] = "%1%: unexpected arguments";
        m[152] = "%1% offset is higher than the waveform length itself";
        m[153] = "%1% expects an offset and a length, just one parameter given";
        m[154] = "%1% length is too long for the given offset";
        m[155] = "%1% expects the number of samples to play as a const argument";
        m[156] = "%1% expects the address within the waveform as const or var argument";
        m[157] = "%1% length is 0, no wave will be played";
        m[158] = "%1% expects the name of the waveform as a string or wave";
        m[159] = "%1% expects at most %2% channels, %3% given";
        m[160] = "%1% sample rate can only be described with a const, not with a %2%";
        m[161] = "%1% sample rate can not be greater than 14.0 MHz";
        m[162] = "playDIOWave sample rate can not be greater than 450 MHz";

        // Prefetch/format (163-172)
        m[163] = "prefetch Error: %1%";
        m[164] = "invalid identifier while placing the fetch and play commands";
        m[165] = "tried to swap two not connected nodes";
        m[166] = "explicit prefetch is not supported on this HW type";
        m[167] = "function '%1%' expected more arguments with the given string";
        m[168] = "function '%1%' expected less arguments with the given string";
        m[169] = "function '%1%' can't interpret the format of the given string";
        m[170] = "function '%1%' expects %2% arguments, %3% given";
        m[171] = "function '%1%' can't have a var argument with a %2% return value";
        m[172] = "tried to define '%1%' but it is already defined";

        // Register/return (173-184)
        m[173] = "run out of free registers, please reduce complexity";
        m[174] = "return statement not in a function";
        m[175] = "type mismatch, can't read %1% as %2%";
        m[176] = "type mismatch, can't write %1% to %2%";
        m[177] = "tried to access uninitialized variable '%1%'";
        m[178] = "tried to access unknown variable '%1%'";
        m[179] = "expected a return value of type %1% but no value given";
        m[180] = "return label stack is empty";
        m[181] = "expected no return value but value of type %1% given";
        m[182] = "expected a return value of type %1%, %2% given";
        m[183] = "invalid return argument";
        m[184] = "repeat only accepts non-negative number of repetitions";

        // setDouble/setInt/setRate (185-197)
        m[185] = "setDouble expects 2 or 3 arguments";
        m[186] = "setDouble expects a string as first argument, %1% given";
        m[187] = "setDouble expects a var or const as second argument, %1% given";
        m[188] = "setDouble expects a const as third argument, %1% given";
        m[189] = "setInt expects 2 arguments";
        m[190] = "setInt expects a string as first argument, %1% given";
        m[191] = "setInt expects a var or const as second argument, %1% given";
        m[192] = "setRate expects a const argument";
        m[193] = "setRate expects just one const argument";
        m[194] = "setPrecompClear expects a const argument";
        m[195] = "setPrecompClear expects just one const argument";
        m[196] = "final value of precompensation flags must be consistent in all branches";
        m[197] = "precompensation flags are indeterminate after loop execution";

        // setUserReg/PRNG/trigger (198-212)
        m[198] = "setUserReg expects a const as first argument";
        m[199] = "setUserReg register must be in the range of 0 to 15";
        m[200] = "setUserReg expects exactly two arguments";
        m[201] = "setUserReg expects a var or const as second argument";
        m[202] = "invalid resetOscPhase argument";
        m[203] = "resetOscPhase expects a single const argument";
        m[204] = "PRNG seed expected to be positve";  // sic: typo in binary
        m[205] = "PRNG seed equal to zero is not valid";
        m[206] = "PRNG seed exceeded maximum of 65535 (0xFFFF)";
        m[207] = "setPRNGRange expects two const arguments (lower, upper) in range 0...65534, lower < upper";
        m[208] = "setTrigger expects a single const or var argument";
        m[209] = "setInternalTrigger expects a single const or var argument";
        m[210] = "%1% expects two const arguments";
        m[211] = "%1% expects one const argument";
        m[212] = "%1% expects a sine generator index of 0 or 1";

        // Misc (213-227)
        m[213] = "can't shift a %1% variable depending on a %2%";
        m[214] = "%1% statement is not supported";
        m[215] = "switch statement contains no case statements and will, therefore, be ignored";
        m[216] = "'%1%' index must be %2%";
        m[217] = "calling unknown function '%1%'";
        m[218] = "unknown target device '%1%'";
        m[219] = "FIFO play mode is not supported on device type '%1%'";
        m[220] = "FIFO play mode is required on device type '%1%'";
        m[221] = "unlock expects exactly one argument";
        m[222] = "it's just possible to unlock variables of type wave";
        m[223] = "%1% is not supported in %2% channel grouping mode";
        m[224] = "division is not supported for var, only for const";
        m[225] = "can't assign the type-less variable '%1%'";
        m[226] = "using vect with more that %1% arguments is discouraged, use CSV files to define your waveforms instead";
        m[227] = "wait expects a positive argument";

        // Waveform errors (228-255)
        m[228] = "waveform '%1%' does not exist";
        m[229] = "can't modify a %1% variable depending on a var or inside of a repeat loop";
        m[230] = "inconsistent number of channels in waveform %1% command (waveform %2% has %3%, expected %4%)";
        m[231] = "waveform '%1%' size %2% is not aligned to %3% samples and will be zero-extended to %4% samples";
        m[232] = "play length %1% is not aligned to %2% samples and will be extended to %3% samples";
        m[233] = "the given waveforms are not of same length, shorter waveforms will be filled with zeros to match the length of the longest waveform";
        m[234] = "no waveform with the name '%1%' found";
        m[235] = "tried to access uninitialized waveform";
        m[236] = "wave %1% is not unique, %2% used";
        m[237] = "can't play channel %1%, valid values for channel are from 1 to %2%";
        m[238] = "wave '%1%' expected to have %2% channel(s) but has %3%";
        m[239] = "mixing of numbered and unnumbered channels [ %1%(1,w1,w2) ] is not allowed";
        m[240] = "no waveform found in function %1%";
        m[241] = "Channel %1% has been defined multiple times";
        m[242] = "number of waveforms in wavetable is too large - has %1% waveforms, maximum is %2%";
        m[243] = "XOR operations are not supported";
        m[244] = "minimal waveform length for playWaveDigTrigger() command is %1% samples";
        m[245] = "waveform '%1%' size %2% is below the minimal waveform length and will be zero-extended to %3% samples";
        m[246] = "play length %1% is below minimum of %2% samples, will be extended";
        m[247] = "waveform %1% is in the used waveforms list but is not marked for load";
        m[248] = "waveform %1% is not fitting in preload cache - will cause play gap";
        m[249] = "waveform %1% has already assigned index";
        m[250] = "waveform index already used";
        m[251] = "waveform name %1% is already in use, please make sure to use unique waveform names.";
        m[252] = "waveform index exceeds wavetable size";
        m[253] = "invalid waveform name encountered. Please make sure that the waveform name supplied contains only alphanumeric characters or underscores and does not start with a digit.";
        m[254] = "attempting to perform a bitwise operation on a negative operand. Operands supplied: %1% and %2%.";
        m[255] = "attempting to perform a bitwise operation on a negative operand. Operand supplied: %1%.";

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
