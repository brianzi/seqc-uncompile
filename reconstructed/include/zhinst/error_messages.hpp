// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// ErrorMessages — compiler error/warning message system
//
// The ErrorMessages system uses a static std::map<int, std::string> at BSS
// address 0xb84c38, populated during global initialization from
// ErrorMessages.cpp (__cxx_global_var_init at 0xd5de0).
//
// 305 total entries across 4 key ranges:
//   1–255       : SeqC compiler errors/warnings (253 entries, gaps at 47, 53)
//   16384–16389 : General status codes (5 entries, gap at 16388)
//   32768–32800 : LabOne API/communication errors (33 entries)
//   36864–36877 : Device firmware/vector transfer errors (14 entries)
//
// Format strings use boost::format syntax: %1%, %2%, etc.
//
// The format<Args...>(ErrorMessageT, Args...) template:
//   1. Looks up the format string via inlined map::at()
//   2. Constructs boost::basic_format from the string
//   3. Feeds arguments via boost::io::detail::feed_impl
//   4. Returns formatted string via basic_format::str()
//
// ~40 template instantiations exist in the binary (format<int>,
// format<string>, format<string,int,int>, etc.)
// ============================================================================
#pragma once

#include "zhinst/exception.hpp"   // ZIAWGCompilerException (Phase 10.7d)

#include <exception>
#include <map>
#include <string>

#include <boost/format.hpp>   // boost::basic_format used by format<Args...>

namespace zhinst {

// ============================================================================
// ErrorMessageT — compiler error/warning ID enum
//
// Plain enum (not enum class) with int underlying type.
// Values 0–254 are SeqC compiler messages (verified from BSS map
// initializer at 0xd5de0 — keys 0x00..0xFE in binary).
// Values 16384+ are API/device error codes (used by getApiErrorMessage).
// Gaps: 46, 52 are unassigned.
//
// Phase 10.7e renumbering: all SeqC compiler entries were previously
// numbered 1..255 (off by +1 from the binary's actual 0..254 keys).
// Corrected to match the binary map. The aliases InvalidRegister(=0),
// ValueOverflow(=5), UnsupportedOp(=11) that previously coexisted
// as duplicates are now the canonical names — the old CmdWithoutRegister
// (=0), TooFewArguments(=4), etc. are renamed to match the binary's
// actual semantics where they differed.
// ============================================================================
enum ErrorMessageT : int {
    // --- Assembler errors (0–11) ---
    CmdWithoutRegister          = 0,   // "%1% command without valid register"
    OpcodeRegNotGiven           = 1,   // "opcode %1% register %1% not given"
    OpcodeValNotGiven           = 2,   // "opcode %1% value %1% not given"
    RegisterNotExist            = 3,   // "register does not exist"
    TooFewArguments             = 4,   // "instruction %1% (opcode %2%) expects at least %3% argument(s), %4% argument(s) given"
    ValueOutOfRange             = 5,   // "value %1% is out of range for %2% bits"
    Exactly2Args                = 6,   // "addr, subr, andr, orr and xnorr expect exactly 2 arguments"
    WrongArgCount               = 7,   // "instruction %1% (opcode %2%) expects %3% arguments"
    ExpectedRegister            = 8,   // "wrong argument type, expected register"
    ExpectedValue               = 9,   // "wrong argument type, expected value"
    UserRegNotExist             = 10,  // "user register does not exist, just %1% registers available"
    InvalidOpcode               = 11,  // "%1% is not a valid opcode"

    // --- Array errors (12–15) ---
    ArrayIndexOutOfRange        = 12,
    ProgramTooLarge             = 13,
    ArrayIndexNeedConst         = 14,
    ArraysOnlyWave              = 15,

    // --- Type/memory errors (16–22) ---
    CantInvertType              = 16,
    CantInterpretAsBool         = 17,
    BrokenList                  = 18,
    FreeNullPtr                 = 19,
    FreeModifiedPtr             = 20,
    CacheMemoryFull             = 21,
    PlayNullPtr                 = 22,

    // --- Case/switch (23–29) ---
    CaseNeedsConst              = 23,
    RedefinedCase               = 24,
    RedefinedDefaultCase        = 25,
    NeedCaseBeforeStmt          = 26,
    CasePositiveNatural         = 27,
    CaseRounded                 = 28,
    CaseOutsideSwitch           = 29,

    // --- General compiler (30–45) ---
    ModifyConst                 = 30,
    CompressError               = 31,
    ConditionalNeedVarConst     = 32,
    ExpectedCommand             = 33,
    UnreachableCode             = 34,
    CsvInconsistentChannels     = 35,
    CsvWrongValue               = 36,
    CsvValueOutOfRange          = 37,
    CsvEmpty                    = 38,
    CantDivConstByWave          = 39,
    ExpectsVarOrConst           = 40,
    DivisionByZero              = 41,
    InvalidDeviceNr             = 42,
    EmptyInput                  = 43,
    EmptyOperation              = 44,
    ExecTableExpectsArg         = 45,
    ExecTableInvalidConst       = 46,
    UnknownError47              = 47,  // used at custom_functions_io.cpp — message string unknown
    ExecTableInvalidIndex       = 48,
    WaveNotFittingCache         = 49,
    WaveNotFittingCacheGapless  = 50,
    WaveNotFittingMemory        = 51,
    // 52 — previously listed under DeprecatedConst (53→52 in renumbering)
    // 53 — UNASSIGNED (binary has no key 0x35)

    // --- Deprecation/function validation (52–103) ---
    DeprecatedConst             = 52,  // "constant '%1%' is deprecated, please use %2%"
    FuncCalledWithLogical       = 54,
    DeprecatedFunc              = 55,
    DeprecatedFunc2             = 56,
    DeprecatedFunc3             = 57,
    DeprecatedFuncFifo          = 58,
    FuncNotMultiCore            = 59,
    FuncEmpty                   = 60,
    FuncMinArgs                 = 61,
    FuncExpectsConst            = 62,
    FuncExpectsConstConst       = 63,
    FuncExpectsConstVar         = 64,
    FuncExpects3Const           = 65,
    FuncExpectsNoArgs           = 66,
    FuncExpectsSingleArg        = 67,
    FuncExpectsNArgs            = 68,
    FuncExpectsMaxArgs          = 69,
    FuncInvalidArgType          = 70,
    FuncNoName                  = 71,
    FuncNoReturn                = 72,
    FuncNotSupported            = 73,
    FuncOnlyConst               = 74,
    FuncPredefined              = 75,
    FuncNotFound                = 76,
    FuncExactArgs               = 77,
    FuncArgLessThan             = 78,
    DioZsyncMixed               = 79,
    FuncExpectsStringOrWave     = 80,
    FuncExpectsConstSecond      = 81,
    FuncExpectsWaveforms        = 82,
    AddExpectsMultiWave         = 83,
    AmplitudeClipped            = 84,
    ArgMustBeConst              = 85,
    ArgMustBeString             = 86,
    ArgOutOfAmplRange           = 87,
    ArgGreaterThanLength        = 88,
    JoinMin2                    = 89,
    UnknownWaveform             = 90,
    FuncExactArgs2              = 91,
    FuncArgs2or3                = 92,
    FuncArgsRange               = 93,
    ArgMustBePositive           = 94,
    ArgLargerThanLength         = 95,
    ArgOverflow                 = 96,
    FuncNoArgsGiven             = 97,
    ArgNotZero                  = 98,
    ValueCapped                 = 99,
    ValueMustBe1or2             = 100,
    LfsrInitZero                = 101,
    GenerateExpectsString       = 102,
    CantCallWithVar             = 103,

    // --- Specific built-in functions (104–110) ---
    GetUserRegArgs              = 104,
    GetUserRegRange             = 105,
    GetCntArgs                  = 106,
    GetCntRange                 = 107,
    GetSweeperLenArgs           = 108,
    GetSweeperLenArg            = 109,
    InvalidArgValue             = 110,

    // --- Increment/decrement/file (111–124) ---
    CantIncrement               = 111,
    CantDecrement               = 112,
    FileNotExist                = 113,
    NoSuchFile                  = 114,
    CantAddTypes                = 115,
    CantSubtractTypes           = 116,
    InvalidZSyncData            = 117,
    InvalidFeedbackData         = 118,
    OnlyConstVarInverted        = 119,
    LabelNotFound               = 120,
    LockArgs                    = 121,
    LockOnlyWave                = 122,
    TooManyIterations           = 123,
    OnlyConstVarNegated         = 124,

    // --- Node/oscillator (125–133) ---
    OperationWithoutOperator    = 125,
    NoValidBranchArg            = 126,
    NodeOnlySetDouble           = 127,
    NodePrecisionLoss           = 128,
    FreqNodeConstOnly           = 129,
    PhaseNodeConstOnly          = 130,
    NodeNotExist                = 131,
    NodeNeedsMFOption           = 132,
    SequencerCantDrive          = 133,

    // --- Variable arithmetic (134–147) ---
    ConstVarLogicalInvert       = 134,
    InvalidArgument             = 135,
    FuncSingleArg               = 136,
    FuncExactly2Args            = 137,
    VarMultNatural              = 138,
    CantAssignType              = 139,
    CantMultiplyTypes           = 140,
    CantDivideTypes             = 141,
    CantModuloTypes             = 142,
    CantBitAndTypes             = 143,
    CantBitOrTypes              = 144,
    CantCombineTypes            = 145,
    CantCompareTypes            = 146,
    NoValidOpType               = 147,

    // --- File/playWave (148–161) ---
    CantWriteFile               = 148,
    OnlyConstWaveIndex          = 149,
    UnexpectedArgs              = 150,
    OffsetTooHigh               = 151,
    ExpectsOffsetAndLength      = 152,
    LengthTooLong               = 153,
    ExpectsSamplesConst         = 154,
    ExpectsAddrConstOrVar       = 155,
    LengthIsZero                = 156,
    ExpectsWaveName             = 157,
    TooManyChannels             = 158,
    SampleRateConstOnly         = 159,
    SampleRateTooHigh           = 160,
    DioSampleRateTooHigh        = 161,

    // --- Prefetch/format/definition (162–171) ---
    PrefetchError               = 162,
    InvalidPrefetchId           = 163,
    SwapNotConnected            = 164,
    PrefetchNotSupported        = 165,
    FormatMoreArgs              = 166,
    FormatLessArgs              = 167,
    FormatCantInterpret         = 168,
    FormatFuncArgs              = 169,
    FormatVarReturn             = 170,
    AlreadyDefined              = 171,

    // --- Register/return/variable (172–183) ---
    OutOfRegisters              = 172,
    ReturnNotInFunc             = 173,
    TypeMismatchRead            = 174,
    TypeMismatchWrite           = 175,
    UninitializedVar            = 176,
    UnknownVar                  = 177,
    ExpectedReturnValue         = 178,
    ReturnStackEmpty            = 179,
    UnexpectedReturnValue       = 180,
    ReturnTypeMismatch          = 181,
    InvalidReturnArg            = 182,
    RepeatNonNegative           = 183,

    // --- setDouble/setInt/setRate (184–196) ---
    SetDoubleArgs               = 184,
    SetDoubleStringFirst        = 185,
    SetDoubleVarConstSecond     = 186,
    SetDoubleConstThird         = 187,
    SetIntArgs                  = 188,
    SetIntStringFirst           = 189,
    SetIntVarConstSecond        = 190,
    SetRateConst                = 191,
    SetRateOneConst             = 192,
    SetPrecompConst             = 193,
    SetPrecompOneConst          = 194,
    PrecompFlagsBranch          = 195,
    PrecompFlagsLoop            = 196,

    // --- setUserReg/PRNG/trigger (197–211) ---
    SetUserRegConstFirst        = 197,
    SetUserRegRange             = 198,
    SetUserRegArgs              = 199,
    SetUserRegVarConst          = 200,
    InvalidResetOscPhase        = 201,
    ResetOscPhaseArgs           = 202,
    PrngSeedPositive            = 203,
    PrngSeedZero                = 204,
    PrngSeedMax                 = 205,
    PrngRangeArgs               = 206,
    SetTriggerArgs              = 207,
    SetInternalTriggerArgs      = 208,
    ExpectsTwoConst             = 209,
    ExpectsOneConst             = 210,
    SineGenIndex                = 211,

    // --- Misc (212–226) ---
    CantShiftTypes              = 212,
    StatementNotSupported       = 213,
    SwitchNoCases               = 214,
    IndexMustBe                 = 215,
    UnknownFunction             = 216,
    UnknownDevice               = 217,
    FifoNotSupported            = 218,
    FifoRequired                = 219,
    UnlockArgs                  = 220,
    UnlockOnlyWave              = 221,
    NotSupportedGrouping        = 222,
    DivNotSupportedVar          = 223,
    CantAssignTypeless          = 224,
    VectTooManyArgs             = 225,
    WaitPositive                = 226,

    // --- Waveform errors (227–254) ---
    WaveformNotExist            = 227,
    CantModifyVarInRepeat       = 228,
    InconsistentChannels        = 229,
    WaveNotAligned              = 230,
    PlayLenNotAligned           = 231,
    WaveformLengthMismatch      = 232,
    WaveformNotFound            = 233,
    UninitializedWaveform       = 234,
    WaveNotUnique               = 235,
    InvalidChannel              = 236,
    WaveWrongChannels           = 237,
    MixedChannelNumbering       = 238,
    NoWaveformInFunc            = 239,
    DuplicateChannel            = 240,
    TooManyWavetableWaves       = 241,
    XorNotSupported             = 242,
    MinWaveformLength           = 243,
    WaveformBelowMin            = 244,
    PlayLenBelowMin             = 245,
    WaveUsedNotLoaded           = 246,
    WaveNotFittingPreload       = 247,
    WaveAlreadyAssigned         = 248,
    WaveIndexUsed               = 249,
    WaveNameInUse               = 250,
    WaveIndexExceedsTable       = 251,
    InvalidWaveformName         = 252,
    BitwiseNegativeOp2          = 253,
    BitwiseNegativeOp1          = 254,

    // --- General status codes (16384+) ---
    ApiSuccess                  = 16384,
    ApiWarning                  = 16385,
    ApiFifoUnderrun             = 16386,
    ApiFifoOverflow             = 16387,
    // 16388 — UNASSIGNED
    ApiNodeNotFound             = 16389,

    // --- LabOne API errors (32768+) ---
    ApiKeywordNotResolved       = 32768,
    ApiError                    = 32769,
    ApiUsbFailed                = 32770,
    ApiMemAlloc                 = 32771,
    ApiMutexInit                = 32772,
    ApiMutexDestroy             = 32773,
    ApiMutexLock                = 32774,
    ApiMutexUnlock              = 32775,
    ApiThreadStart              = 32776,
    ApiThreadJoin               = 32777,
    ApiSocketInit               = 32778,
    ApiSocketConnect            = 32779,
    ApiHostnameNotFound         = 32780,
    ApiConnectionInvalid        = 32781,
    ApiTimeout                  = 32782,
    ApiCommandFailed            = 32783,
    ApiServerFailed             = 32784,
    ApiBufferTooSmall           = 32785,
    ApiFileError                = 32786,
    ApiDuplicate                = 32787,
    ApiWriteOnly                = 32788,
    ApiDeviceNotVisible         = 32789,
    ApiDeviceOtherServer        = 32790,
    ApiInterfaceNotSupported    = 32791,
    ApiDeviceTimeout            = 32792,
    ApiDeviceOtherInterface     = 32793,
    ApiDeviceNeedsFirmware      = 32794,
    ApiDeviceNotFound           = 32795,
    ApiDataTypeMismatch         = 32796,
    ApiNotSupported             = 32797,
    ApiMaxSessions              = 32798,
    ApiHF2NotSupported          = 32799,
    ApiReadOnly                 = 32800,

    // --- Device firmware errors (36864+) ---
    FwInvalidArgument           = 36864,
    FwUnknownError              = 36865,
    FwBadAddress                = 36866,
    FwOffsetAboveRange          = 36867,
    FwOffsetLenAboveRange       = 36868,
    FwReadOnlyAperture          = 36869,
    FwCantHandle                = 36870,
    FwNonIndexedVector          = 36871,
    FwVectorHeaderShort         = 36872,
    FwVectorFrameIndex          = 36873,
    FwVectorOffsetLen           = 36874,
    FwVectorExtraWords          = 36875,
    FwOutOfSequence             = 36876,
    FwVectorOutOfRange          = 36877,

    // Aliases for asm_commands.cpp (verified from binary throw sites):
    //   prf: esi=0  → CmdWithoutRegister (= 0)
    //   wvfs: esi=5 → ValueOutOfRange    (= 5)
    //   wvfs/wvft/wwvfq cervino: esi=0xb → InvalidOpcode (= 11)
    //
    // After the Phase 10.7e renumbering these aliases are now redundant —
    // they resolve to the same values as the canonical names above:
    //   InvalidRegister ≡ CmdWithoutRegister  = 0
    //   ValueOverflow   ≡ ValueOutOfRange     = 5
    //   UnsupportedOp   ≡ InvalidOpcode       = 11
    // Kept as aliases for source compatibility (src/ uses these names).
    InvalidRegister             = 0,    // alias for CmdWithoutRegister
    ValueOverflow               = 5,    // alias for ValueOutOfRange
    UnsupportedOp               = 11,   // alias for InvalidOpcode
};

// ============================================================================
// ErrorMessages — static message store + format<>() template
//
// Static member: std::map<int, std::string> messages (BSS at 0xb84c38)
// Initialized in __cxx_global_var_init at 0xd5de0
//
// operator[] does std::map::at() (red-black tree traversal).
// format<>() inlines the map lookup, wraps in boost::format, feeds args.
// ============================================================================
class ErrorMessages {
public:
    // Lookup a format string by enum value.
    // Throws std::out_of_range if key not found.
    // Binary address: 0x108380
    std::string const& operator[](ErrorMessageT id) const;

    // Static lookup by integer key (used as ErrorMessages::get(n) in opcodes code)
    static std::string const& get(int id) { return messages.at(id); }

    // Format a message with arguments (variadic template).
    // ~54 instantiations in binary. Pattern (verified — see WP-A in
    // notes/linker_resolution.md):
    //   1. messages.at(id) → format string
    //   2. boost::basic_format(str)
    //   3. feed args via boost::io::detail::feed_impl (operator% chain)
    //   4. return basic_format::str()
    //
    // The binary splits this into two functions: the outer creates the
    // format object and calls the inner to feed args.  Explicit
    // instantiations live in error_messages.cpp.

    // Inner helper — feeds args into an existing boost::format object.
    // Binary splits the first arg out of the pack (affects mangling):
    //   format<T, Args...>(BF&, T, Args...) not format<Args...>(BF&, Args...)
    template <typename T, typename... Args>
    static std::string format(boost::format& fmt, T arg, Args... args) {
        fmt % arg;
        return format(fmt, args...);
    }

    // Base case: no more args to feed.
    static std::string format(boost::format& fmt) {
        return fmt.str();
    }

    // Outer entry — creates the format object, delegates to inner.
    template <typename... Args>
    static std::string format(ErrorMessageT id, Args... args) {
        boost::format fmt(messages.at(static_cast<int>(id)));
        return format(fmt, args...);
    }

    // Static message map (BSS at 0xb84c38)
    static std::map<int, std::string> messages;
};

// Process-wide singleton instance, BSS at 0x95de60.
// Declared `extern` in 4 TUs (cache, custom_functions, node, prefetch_emit);
// definition lives in error_messages.cpp.
extern ErrorMessages errMsg;

// Three `extern const std::string` namespace globals referenced from
// static_resources.cpp:21-23 (BSS @ 0xb84690 / 0xb846a8 / 0xb846c0).
// Definitions live in error_messages.cpp.
extern const std::string zsyncDataPqscDecoder;
extern const std::string zsyncDataPqscRegister;
extern const std::string constAwgIntegrationTrigger;

// NOTE: ResourcesException is defined in resources.hpp (not duplicated here).

// Free function: look up API error message by ZIResult_enum code.
// Uses a separate hash table (apiErrorMessages, BSS at 0xb85230).
// Returns static "unknownError" string if not found.
// Binary address: 0x2e4820
std::string const& getApiErrorMessage(int ziResultCode);

// ============================================================================
// ZIAWGCompilerException — thrown on compiler/assembler errors
//
// As of Phase 10.7d this class is fully reconstructed in
// include/zhinst/exception.hpp (re-included at the top of this file).
// It inherits from zhinst::Exception (verified MI layout: std::bad_exception
// + boost::exception, total size 0x60). The two binary ctors (default at
// 0x2e72f0 and string at 0x2e7360) are reproduced there.
//
// This file no longer declares the class — only includes the canonical
// definition for source-level compatibility with existing #include
// "zhinst/error_messages.hpp" sites that throw ZIAWGCompilerException.
// ============================================================================

} // namespace zhinst
