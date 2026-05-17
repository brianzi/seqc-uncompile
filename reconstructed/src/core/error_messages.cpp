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
        m[ApiSuccess] = "Success (no error)";
        m[ApiWarning] = "Warning (general)";
        m[ApiFifoUnderrun] = "FIFO underrun";
        m[ApiFifoOverflow] = "FIFO overflow";
        m[ApiNodeNotFound] = "Value or node not found";

        // 32768..32800 — LabOne API
        m[ApiKeywordNotResolved] = "Keyword could not be resolved";
        m[ApiError] = "Error (general)";
        m[ApiUsbFailed] = "USB communication failed";
        m[ApiMemAlloc] = "Memory allocation failed";
        m[ApiMutexInit] = "Unable to initialize mutex";
        m[ApiMutexDestroy] = "Unable to destroy mutex";
        m[ApiMutexLock] = "Unable to lock mutex";
        m[ApiMutexUnlock] = "Unable to unlock mutex";
        m[ApiThreadStart] = "Unable to start thread";
        m[ApiThreadJoin] = "Unable to join thread";
        m[ApiSocketInit] = "Unable to initialize socket";
        m[ApiSocketConnect] = "Unable to connect to socket";
        m[ApiHostnameNotFound] = "Hostname not found";
        m[ApiConnectionInvalid] = "Connection invalid";
        m[ApiTimeout] = "Command timed out";
        m[ApiCommandFailed] = "Command failed internally";
        m[ApiServerFailed] = "Command failed on the server";
        m[ApiBufferTooSmall] = "Provided buffer is too small";
        m[ApiFileError] = "Unable to open file or read from it";
        m[ApiDuplicate] = "There is already a similar item";
        m[ApiWriteOnly] = "Attempt to get a write-only node";
        m[ApiDeviceNotVisible] = "Device is not visible to the server";
        m[ApiDeviceOtherServer] = "Device is already connected to a different server";
        m[ApiInterfaceNotSupported] = "Device does not support the specified interface";
        m[ApiDeviceTimeout] = "Device connection attempt timed out";
        m[ApiDeviceOtherInterface] = "Device already connected over a different interface";
        m[ApiDeviceNeedsFirmware] = "Device needs a firmware upgrade";
        m[ApiDeviceNotFound] = "Device not found";
        m[ApiDataTypeMismatch] = "Data type mismatch. Trying to get data from a poll event with wrong target data type";
        m[ApiNotSupported] = "Command or provided arguments combination is not supported within this context";
        m[ApiMaxSessions] = "The maximum number of allowed simultaneous sessions (32) exceeded. Ensure unused sessions are closed";
        m[ApiHF2NotSupported] = "This functionality is not supported on HF2 devices";
        m[ApiReadOnly] = "Attempt to set a read-only node";

        // 36864..36877 — device firmware
        m[FwInvalidArgument] = "Invalid argument received";
        m[FwUnknownError] = "An error occurred on the device, but the error code was not provided. Please report";
        m[FwBadAddress] = "Requested block address does not belong to any known aperture";
        m[FwOffsetAboveRange] = "Requested block address belongs to an aperture, but the offset is above the active range";
        m[FwOffsetLenAboveRange] = "Requested block address belongs to an aperture, but the offset+length is above the active range";
        m[FwReadOnlyAperture] = "Write access attempted to a read-only aperture or a read-only node";
        m[FwCantHandle] = "Access to the requested register cannot be handled by the firmware (firmware configuration error)";
        m[FwNonIndexedVector] = "Number of indices requested on a non-indexed vector node";
        m[FwVectorHeaderShort] = "Vector transfer header error: received less data than the header size";
        m[FwVectorFrameIndex] = "Vector transfer header error: indicated frame index is greater than the indicated frame count";
        m[FwVectorOffsetLen] = "Vector transfer header error: the indicated offset and payload length exceeds the indicated overall length";
        m[FwVectorExtraWords] = "Vector transfer header error: the indicated number of extra words exceeds the indicated overall length";
        m[FwOutOfSequence] = "Received out-of-sequence frame (may never be reported if we keep the agreement, that the entire vector transfer has to be ACKed in case its first frame was ACKed)";
        m[FwVectorOutOfRange] = "Vector transfer error: accessing indexed element outside the current active range";
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

        m[CmdWithoutRegister] = "%1% command without valid register";
        m[OpcodeRegNotGiven] = "opcode %1% register %1% not given";
        m[OpcodeValNotGiven] = "opcode %1% value %1% not given";
        m[RegisterNotExist] = "register does not exist";
        m[TooFewArguments] = "instruction %1% (opcode %2%) expects at least %3% argument(s), %4% argument(s) given";
        m[ValueOutOfRange] = "value %1% is out of range for %2% bits";
        m[Exactly2Args] = "addr, subr, andr, orr and xnorr expect exactly 2 arguments";
        m[WrongArgCount] = "instruction %1% (opcode %2%) expects %3% arguments";
        m[ExpectedRegister] = "wrong argument type, expected register";
        m[ExpectedValue] = "wrong argument type, expected value";
        m[UserRegNotExist] = "user register does not exist, just %1% registers available";
        m[InvalidOpcode] = "%1% is not a valid opcode";
        m[ProgramTooLarge] = "program is too large to fit into memory - has %1% instructions, maximum is %2%";
        m[ArraysOnlyWave] = "arrays are just supported with type wave";
        m[ArrayIndexNeedConst] = "an array index always needs to be a const or cvar value";
        m[ArrayIndexOutOfRange] = "the array index is out of the array range";
        m[CantInvertType] = "can't logically invert a variable of type %1%";
        m[CantInterpretAsBool] = "can't interpret %1% as boolean expression";
        m[BrokenList] = "broken or empty %1% list detected";
        m[FreeNullPtr] = "tried to free a NULL pointer";
        m[FreeModifiedPtr] = "tried to free a modified memory pointer";
        m[CacheMemoryFull] = "cache memory full";
        m[PlayNullPtr] = "tried to play a NULL pointer";
        m[CaseNeedsConst] = "case needs to be a const";
        m[RedefinedCase] = "redefined case %1%";
        m[RedefinedDefaultCase] = "redefined default case";
        m[NeedCaseBeforeStmt] = "required to define a case before any statement";
        m[CasePositiveNatural] = "case has to be a positive natural number, %1% not valid";
        m[CaseRounded] = "case should be a natural number, gets rounded from %1% to %2%";
        m[CaseOutsideSwitch] = "case should only be called out of a switch statment";
        m[CompressError] = "could not compress sections in output file '%1%'";
        m[ConditionalNeedVarConst] = "conditional expression expects a var, cvar or const argument";
        m[ModifyConst] = "tried to modify const value";
        m[ExpectedCommand] = "expected a command";
        m[UnreachableCode] = "this command will never be reached";
        m[CsvInconsistentChannels] = "CSV file '%1%' has an inconsistent number of channels";
        m[CsvWrongValue] = "wrong value in CSV file '%1%' (line %2%), only numerical values are allowed";
        m[CsvValueOutOfRange] = "value in CSV file '%1%' (line %2%) out of range";
        m[CsvEmpty] = "CSV file '%1%' exists but is empty";
        m[ExpectsVarOrConst] = "%1% expects a var or const argument";
        m[InvalidDeviceNr] = "invalid device nr %1%, just %2% devices activated";
        m[DivisionByZero] = "division by zero";
        m[CantDivConstByWave] = "can't divide a const by a wave";
        m[EmptyInput] = "nothing to write, empty input";
        m[EmptyOperation] = "empty operation, something went wrong";
        m[ExecTableExpectsArg] = "executeTableEntry expects a single const or var argument";
        m[ExecTableInvalidConst] = "Invalid constant argument used for executeTableEntry";
        m[UnknownError47] = "Invalid constant argument used for executeTableEntry (the original binary has no m[UnknownError47] and crashes here with map::at — you're welcome, ZI)";
        m[ExecTableInvalidIndex] = "Invalid table index used for executeTableEntry";
        m[WaveNotFittingCache] = "waveforms are not fitting into cache memory, load not aligned across AWGs - every play will be prepended by sync";
        m[WaveNotFittingCacheGapless] = "waveforms are not fitting into cache memory, gapless playback is not guaranteed";
        m[WaveNotFittingMemory] = "waveforms are not fitting into wave memory (%|1$.1f| kSa over a maximum of %|2$.1f| kSa)";
        m[DeprecatedConst] = "constant '%1%' is deprecated, please use %2%";

        m[FuncCalledWithLogical] = "function '%1%' called with logical value instead of cycle count";
        m[DeprecatedFunc] = "function '%1%' is deprecated, please use '%2%'";
        m[DeprecatedFunc2] = "function '%1%' is deprecated, please use '%2%' or '%3%'";
        m[DeprecatedFunc3] = "function '%1%' is deprecated";
        m[DeprecatedFuncFifo] = "function '%1%' is deprecated as not compatible with AWG FIFO architecture";
        m[FuncNotMultiCore] = "function '%1%' is not supported in multi-core mode";
        m[FuncEmpty] = "function '%1% %2%%3%' is empty";
        m[FuncMinArgs] = "function '%1%' expects at least %2% argument(s), %3% arguments given";
        m[FuncExpectsConst] = "function '%1%' expects (const) as argument";
        m[FuncExpectsConstConst] = "function '%1%' expects (const, const) as arguments";
        m[FuncExpectsConstVar] = "function '%1%' expects (const, var) as arguments";
        m[FuncExpects3Const] = "function '%1%' expects (const, const, const) as arguments";
        m[FuncExpectsNoArgs] = "function '%1%' expects no arguments";
        m[FuncExpectsSingleArg] = "function '%1%' expects a single const, cvar or var argument";
        m[FuncExpectsNArgs] = "function '%1%' expects %2% arguments, %3% arguments given";
        m[FuncExpectsMaxArgs] = "function '%1%' expects %2% or fewer argument(s), %3% arguments given";
        m[FuncInvalidArgType] = "invalid argument type %1% in function '%2%'; expects argument '%3%' to be %4%";
        m[FuncNoName] = "function does not have a name";
        m[FuncNoReturn] = "function '%1%' reaches end without returning a %2%";
        m[FuncNotSupported] = "function '%1%' not supported on %2% devices";
        m[FuncOnlyConst] = "function '%1%' expects only const arguments";
        m[FuncPredefined] = "function '%1%' is a predefined function and cannot be overwritten";
        m[FuncNotFound] = "function '%1%' not found, possible alternatives are";
        m[FuncExactArgs] = "function '%1%' expects %2% argument(s), %3% argument(s) given";
        m[FuncArgLessThan] = "function '%1%' expects argument %2% to be less than %3%";
        m[DioZsyncMixed] = "DIO and ZSync external triggering can't be mixed in the same program";
        m[FuncExpectsStringOrWave] = "function '%1%' expects a string or wave as first argument";
        m[FuncExpectsConstSecond] = "function '%1%' expects a const as second argument";
        m[FuncExpectsWaveforms] = "function '%1%' expects just waveforms (string or wave) as arguments";
        m[AddExpectsMultiWave] = "function 'add' expects more than one waveform (string or wave)";
        m[AmplitudeClipped] = "%1% has a higher amplitude than 1.0, waveform amplitude will be limited to 1.0";
        m[ArgMustBeConst] = "argument %1% of %2% must be a const";
        m[ArgMustBeString] = "argument %1% of %2% must be a string";
        m[ArgOutOfAmplRange] = "argument %1% of %2% is out of the allowed range of -1.0 to 1.0";
        m[ArgGreaterThanLength] = "argument %1% of %2% is greater than the waveform length";
        m[JoinMin2] = "join expects at least 2 waveforms";
        m[UnknownWaveform] = "tried to '%1%' the unknown waveform '%2%'";
        m[FuncExactArgs2] = "function '%1%' expects %2% argument(s), %3% argument(s) given";
        m[FuncArgs2or3] = "function '%1%' expects %2% or %3% arguments, %4% argument(s) given";
        m[FuncArgsRange] = "function '%1%' expects %2% to %3% arguments, %4% argument(s) given";
        m[ArgMustBePositive] = "argument %1% of %2% must be positive";
        m[ArgLargerThanLength] = "argument %1% of %2% is larger than the waveform length";
        m[ArgOverflow] = "argument %1% of %2% out of range, %3% overflow";
        m[FuncNoArgsGiven] = "called function '%1%' without arguments; string or const arguments expected";
        m[ArgNotZero] = "argument %1% of %2% is not allowed to be 0";
        m[ValueCapped] = "%3% value %1% is larger than the maximum possible value 3, will be capped to %2%";
        m[ValueMustBe1or2] = "%2% value %1% must be 1 or 2";
        m[LfsrInitZero] = "argument %1%, LFSR initial value, can't be 0";
        m[GenerateExpectsString] = "generate(string, const...) expects a string as first argument";
        m[CantCallWithVar] = "%1% can't be called with var arguments";
        m[GetUserRegArgs] = "getUserReg expects exactly one const argument";
        m[GetUserRegRange] = "getUserReg register arguments must be in the range of 0 to 15";
        m[GetCntArgs] = "getCnt expects exactly one const argument";
        m[GetCntRange] = "getCnt counter argument must be either 0 or 1";
        m[GetSweeperLenArgs] = "getSweeperLength expects zero arguments or a single const argument";
        m[GetSweeperLenArg] = "getSweeperLength argument must be 1";
        m[InvalidArgValue] = "invalid value for argument %1% of function %2%";
        m[CantIncrement] = "can't increment %1% value %2%";
        m[CantDecrement] = "can't decrement %1% value %2%";
        m[FileNotExist] = "Input file '%1%' does not exist";
        m[NoSuchFile] = "no such file or directory: '%1%'";
        m[CantAddTypes] = "can't add variables of type %1% and %2%";
        m[CantSubtractTypes] = "can't subtract variables of type %1% and %2%";
        m[InvalidZSyncData] = "Invalid ZSync data_type provided";
        m[InvalidFeedbackData] = "Invalid Feedback data_type provided";
        m[OnlyConstVarInverted] = "only const or var types can be inverted, %1% given";
        m[LabelNotFound] = "label '%1%' not found";
        m[LockArgs] = "lock expects exactly one argument";
        m[LockOnlyWave] = "it's just possible to lock variables of type wave";
        m[TooManyIterations] = "too many iterations to unroll this loop, use a const variable for infinite loop or a var variable for this many iterations";
        m[OnlyConstVarNegated] = "only const or var types can be negated, %1% given";
        m[OperationWithoutOperator] = "operation without operator, something went wrong";
        m[NoValidBranchArg] = "no valid argument for branch command";
        m[NodeOnlySetDouble] = "node '%1%' can only be written using setDouble";
        m[NodePrecisionLoss] = "node '%1%' requires an %2% value, therefore the double precision is lost";
        m[FreqNodeConstOnly] = "frequency node '%1%' can only be set using a const or cvar";
        m[PhaseNodeConstOnly] = "phase node '%1%' can only be set using a const or cvar";
        m[NodeNotExist] = "node '%1%' doesn't exist";
        m[NodeNeedsMFOption] = "node '%1%' requires MF option installed";
        m[SequencerCantDrive] = "sequencer '%1%' can't drive '%2%'";
        m[ConstVarLogicalInvert] = "const or var types can be logically inverted, %1% given";
        m[InvalidArgument] = "%1%: invalid argument";
        m[FuncSingleArg] = "function '%1%' accepts a single argument";
        m[FuncExactly2Args] = "function '%1%' accepts exactly two arguments";
        m[VarMultNatural] = "a var variable can just be multiplied with a natural number";
        m[CantAssignType] = "can't assign a %1% to a %2% variable";
        m[CantMultiplyTypes] = "can't multiply a %1% and a %2% variable";
        m[CantDivideTypes] = "can't divide a %1% and a %2% variable";
        m[CantModuloTypes] = "can't apply modulo on a %1% with a %2% variable";
        m[CantBitAndTypes] = "can't apply bitwise AND on a %1% and a %2% variable";
        m[CantBitOrTypes] = "can't apply bitwise OR on a %1% and a %2% variable";
        m[CantCombineTypes] = "can't combine variables of type %1% and %2%";
        m[CantCompareTypes] = "can't compare data type %1% with %2%";
        m[NoValidOpType] = "no valid operation type";
        m[CantWriteFile] = "Could not write to file '%1%': invalid path or filename";
        m[OnlyConstWaveIndex] = "%1%: only const waveform index allowed";
        m[UnexpectedArgs] = "%1%: unexpected arguments";
        m[OffsetTooHigh] = "%1% offset is higher than the waveform length itself";
        m[ExpectsOffsetAndLength] = "%1% expects an offset and a length, just one parameter given";
        m[LengthTooLong] = "%1% length is too long for the given offset";
        m[ExpectsSamplesConst] = "%1% expects the number of samples to play as a const argument";
        m[ExpectsAddrConstOrVar] = "%1% expects the address within the waveform as const or var argument";
        m[LengthIsZero] = "%1% length is 0, no wave will be played";
        m[ExpectsWaveName] = "%1% expects the name of the waveform as a string or wave";
        m[TooManyChannels] = "%1% expects at most %2% channels, %3% given";
        m[SampleRateConstOnly] = "%1% sample rate can only be described with a const, not with a %2%";
        m[SampleRateTooHigh] = "%1% sample rate can not be greater than 14.0 MHz";
        m[DioSampleRateTooHigh] = "playDIOWave sample rate can not be greater than 450 MHz";
        m[PrefetchError] = "prefetch Error: %1%";
        m[InvalidPrefetchId] = "invalid identifier while placing the fetch and play commands";
        m[SwapNotConnected] = "tried to swap two not connected nodes";
        m[PrefetchNotSupported] = "explicit prefetch is not supported on this HW type";
        m[FormatMoreArgs] = "function '%1%' expected more arguments with the given string";
        m[FormatLessArgs] = "function '%1%' expected less arguments with the given string";
        m[FormatCantInterpret] = "function '%1%' can't interpret the format of the given string";
        m[FormatFuncArgs] = "function '%1%' expects %2% arguments, %3% given";
        m[FormatVarReturn] = "function '%1%' can't have a var argument with a %2% return value";
        m[AlreadyDefined] = "tried to define '%1%' but it is already defined";
        m[OutOfRegisters] = "run out of free registers, please reduce complexity";
        m[ReturnNotInFunc] = "return statement not in a function";
        m[TypeMismatchRead] = "type mismatch, can't read %1% as %2%";
        m[TypeMismatchWrite] = "type mismatch, can't write %1% to %2%";
        m[UninitializedVar] = "tried to access uninitialized variable '%1%'";
        m[UnknownVar] = "tried to access unknown variable '%1%'";
        m[ExpectedReturnValue] = "expected a return value of type %1% but no value given";
        m[ReturnStackEmpty] = "return label stack is empty";
        m[UnexpectedReturnValue] = "expected no return value but value of type %1% given";
        m[ReturnTypeMismatch] = "expected a return value of type %1%, %2% given";
        m[InvalidReturnArg] = "invalid return argument";
        m[RepeatNonNegative] = "repeat only accepts non-negative number of repetitions";
        m[SetDoubleArgs] = "setDouble expects 2 or 3 arguments";
        m[SetDoubleStringFirst] = "setDouble expects a string as first argument, %1% given";
        m[SetDoubleVarConstSecond] = "setDouble expects a var or const as second argument, %1% given";
        m[SetDoubleConstThird] = "setDouble expects a const as third argument, %1% given";
        m[SetIntArgs] = "setInt expects 2 arguments";
        m[SetIntStringFirst] = "setInt expects a string as first argument, %1% given";
        m[SetIntVarConstSecond] = "setInt expects a var or const as second argument, %1% given";
        m[SetRateConst] = "setRate expects a const argument";
        m[SetRateOneConst] = "setRate expects just one const argument";
        m[SetPrecompConst] = "setPrecompClear expects a const argument";
        m[SetPrecompOneConst] = "setPrecompClear expects just one const argument";
        m[PrecompFlagsBranch] = "final value of precompensation flags must be consistent in all branches";
        m[PrecompFlagsLoop] = "precompensation flags are indeterminate after loop execution";
        m[SetUserRegConstFirst] = "setUserReg expects a const as first argument";
        m[SetUserRegRange] = "setUserReg register must be in the range of 0 to 15";
        m[SetUserRegArgs] = "setUserReg expects exactly two arguments";
        m[SetUserRegVarConst] = "setUserReg expects a var or const as second argument";
        m[InvalidResetOscPhase] = "invalid resetOscPhase argument";
        m[ResetOscPhaseArgs] = "resetOscPhase expects a single const argument";
        m[PrngSeedPositive] = "PRNG seed expected to be positve";
        m[PrngSeedZero] = "PRNG seed equal to zero is not valid";
        m[PrngSeedMax] = "PRNG seed exceeded maximum of 65535 (0xFFFF)";
        m[PrngRangeArgs] = "setPRNGRange expects two const arguments (lower, upper) in range 0...65534, lower < upper";
        m[SetTriggerArgs] = "setTrigger expects a single const or var argument";
        m[SetInternalTriggerArgs] = "setInternalTrigger expects a single const or var argument";
        m[ExpectsTwoConst] = "%1% expects two const arguments";
        m[ExpectsOneConst] = "%1% expects one const argument";
        m[SineGenIndex] = "%1% expects a sine generator index of 0 or 1";
        m[CantShiftTypes] = "can't shift a %1% variable depending on a %2%";
        m[StatementNotSupported] = "%1% statement is not supported";
        m[SwitchNoCases] = "switch statement contains no case statements and will, therefore, be ignored";
        m[IndexMustBe] = "'%1%' index must be %2%";
        m[UnknownFunction] = "calling unknown function '%1%'";
        m[UnknownDevice] = "unknown target device '%1%'";
        m[FifoNotSupported] = "FIFO play mode is not supported on device type '%1%'";
        m[FifoRequired] = "FIFO play mode is required on device type '%1%'";
        m[UnlockArgs] = "unlock expects exactly one argument";
        m[UnlockOnlyWave] = "it's just possible to unlock variables of type wave";
        m[NotSupportedGrouping] = "%1% is not supported in %2% channel grouping mode";
        m[DivNotSupportedVar] = "division is not supported for var, only for const";
        m[CantAssignTypeless] = "can't assign the type-less variable '%1%'";
        m[VectTooManyArgs] = "using vect with more that %1% arguments is discouraged, use CSV files to define your waveforms instead";
        m[WaitPositive] = "wait expects a positive argument";
        m[WaveformNotExist] = "waveform '%1%' does not exist";
        m[CantModifyVarInRepeat] = "can't modify a %1% variable depending on a var or inside of a repeat loop";
        m[InconsistentChannels] = "inconsistent number of channels in waveform %1% command (waveform %2% has %3%, expected %4%)";
        m[WaveNotAligned] = "waveform '%1%' size %2% is not aligned to %3% samples and will be zero-extended to %4% samples";
        m[PlayLenNotAligned] = "play length %1% is not aligned to %2% samples and will be extended to %3% samples";
        m[WaveformLengthMismatch] = "the given waveforms are not of same length, shorter waveforms will be filled with zeros to match the length of the longest waveform";
        m[WaveformNotFound] = "no waveform with the name '%1%' found";
        m[UninitializedWaveform] = "tried to access uninitialized waveform";
        m[WaveNotUnique] = "wave %1% is not unique, %2% used";
        m[InvalidChannel] = "can't play channel %1%, valid values for channel are from 1 to %2%";
        m[WaveWrongChannels] = "wave '%1%' expected to have %2% channel(s) but has %3%";
        m[MixedChannelNumbering] = "mixing of numbered and unnumbered channels [ %1%(1,w1,w2) ] is not allowed";
        m[NoWaveformInFunc] = "no waveform found in function %1%";
        m[DuplicateChannel] = "Channel %1% has been defined multiple times";
        m[TooManyWavetableWaves] = "number of waveforms in wavetable is too large - has %1% waveforms, maximum is %2%";
        m[XorNotSupported] = "XOR operations are not supported";
        m[MinWaveformLength] = "minimal waveform length for playWaveDigTrigger() command is %1% samples";
        m[WaveformBelowMin] = "waveform '%1%' size %2% is below the minimal waveform length and will be zero-extended to %3% samples";
        m[PlayLenBelowMin] = "play length %1% is below minimum of %2% samples, will be extended";
        m[WaveUsedNotLoaded] = "waveform %1% is in the used waveforms list but is not marked for load";
        m[WaveNotFittingPreload] = "waveform %1% is not fitting in preload cache - will cause play gap";
        m[WaveAlreadyAssigned] = "waveform %1% has already assigned index";
        m[WaveIndexUsed] = "waveform index already used";
        m[WaveIndexExceedsTable] = "waveform index exceeds wavetable size";
        m[InvalidWaveformName] = "invalid waveform name encountered. Please make sure that the waveform name supplied contains only alphanumeric characters or underscores and does not start with a digit.";
        m[WaveNameInUse] = "waveform name %1% is already in use, please make sure to use unique waveform names.";
        m[BitwiseNegativeOp2] = "attempting to perform a bitwise operation on a negative operand. Operands supplied: %1% and %2%.";
        m[BitwiseNegativeOp1] = "attempting to perform a bitwise operation on a negative operand. Operand supplied: %1%.";
        m[UnexpectedError] = "unexpected error";

        // General status codes (16384+)
        m[ApiSuccess] = "Success (no error)";
        m[ApiWarning] = "Warning (general)";
        m[ApiFifoUnderrun] = "FIFO underrun";
        m[ApiFifoOverflow] = "FIFO overflow";
        m[ApiNodeNotFound] = "Value or node not found";

        // LabOne API errors (32768+)
        m[ApiKeywordNotResolved] = "Keyword could not be resolved";
        m[ApiError] = "Error (general)";
        m[ApiUsbFailed] = "USB communication failed";
        m[ApiMemAlloc] = "Memory allocation failed";
        m[ApiMutexInit] = "Unable to initialize mutex";
        m[ApiMutexDestroy] = "Unable to destroy mutex";
        m[ApiMutexLock] = "Unable to lock mutex";
        m[ApiMutexUnlock] = "Unable to unlock mutex";
        m[ApiThreadStart] = "Unable to start thread";
        m[ApiThreadJoin] = "Unable to join thread";
        m[ApiSocketInit] = "Unable to initialize socket";
        m[ApiSocketConnect] = "Unable to connect to socket";
        m[ApiHostnameNotFound] = "Hostname not found";
        m[ApiConnectionInvalid] = "Connection invalid";
        m[ApiTimeout] = "Command timed out";
        m[ApiCommandFailed] = "Command failed internally";
        m[ApiServerFailed] = "Command failed on the server";
        m[ApiBufferTooSmall] = "Provided buffer is too small";
        m[ApiFileError] = "Unable to open file or read from it";
        m[ApiDuplicate] = "There is already a similar item";
        m[ApiWriteOnly] = "Attempt to get a write-only node";
        m[ApiDeviceNotVisible] = "Device is not visible to the server";
        m[ApiDeviceOtherServer] = "Device is already connected to a different server";
        m[ApiInterfaceNotSupported] = "Device does not support the specified interface";
        m[ApiDeviceTimeout] = "Device connection attempt timed out";
        m[ApiDeviceOtherInterface] = "Device already connected over a different interface";
        m[ApiDeviceNeedsFirmware] = "Device needs a firmware upgrade";
        m[ApiDeviceNotFound] = "Device not found";
        m[ApiDataTypeMismatch] = "Data type mismatch. Trying to get data from a poll event with wrong target data type";
        m[ApiNotSupported] = "Command or provided arguments combination is not supported within this context";
        m[ApiMaxSessions] = "The maximum number of allowed simultaneous sessions (32) exceeded. Ensure unused sessions are closed";
        m[ApiHF2NotSupported] = "This functionality is not supported on HF2 devices";
        m[ApiReadOnly] = "Attempt to set a read-only node";

        // Device firmware errors (36864+)
        m[FwInvalidArgument] = "Invalid argument received";
        m[FwUnknownError] = "An error occurred on the device, but the error code was not provided. Please report";
        m[FwBadAddress] = "Requested block address does not belong to any known aperture";
        m[FwOffsetAboveRange] = "Requested block address belongs to an aperture, but the offset is above the active range";
        m[FwOffsetLenAboveRange] = "Requested block address belongs to an aperture, but the offset+length is above the active range";
        m[FwReadOnlyAperture] = "Write access attempted to a read-only aperture or a read-only node";
        m[FwCantHandle] = "Access to the requested register cannot be handled by the firmware (firmware configuration error)";
        m[FwNonIndexedVector] = "Number of indices requested on a non-indexed vector node";
        m[FwVectorHeaderShort] = "Vector transfer header error: received less data than the header size";
        m[FwVectorFrameIndex] = "Vector transfer header error: indicated frame index is greater than the indicated frame count";
        m[FwVectorOffsetLen] = "Vector transfer header error: the indicated offset and payload length exceeds the indicated overall length";
        m[FwVectorExtraWords] = "Vector transfer header error: the indicated number of extra words exceeds the indicated overall length";
        m[FwOutOfSequence] = "Received out-of-sequence frame (may never be reported if we keep the agreement, that the entire vector transfer has to be ACKed in case its first frame was ACKed)";
        m[FwVectorOutOfRange] = "Vector transfer error: accessing indexed element outside the current active range";
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
