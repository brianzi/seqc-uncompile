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

#include <exception>
#include <map>
#include <string>

// Forward declare boost::basic_format to avoid full boost dependency
namespace boost { template<class Ch> class basic_format; }

namespace zhinst {

// ============================================================================
// ErrorMessageT — compiler error/warning ID enum
//
// Plain enum (not enum class) with int underlying type.
// Values 1–255 are SeqC compiler messages.
// Values 16384+ are API/device error codes (used by getApiErrorMessage).
// Gaps: 47, 53 are unassigned.
// ============================================================================
enum ErrorMessageT : int {
    // --- Assembler errors (1–12) ---
    CmdWithoutRegister          = 1,   // "%1% command without valid register"
    OpcodeRegNotGiven           = 2,   // "opcode %1% register %1% not given"
    OpcodeValNotGiven           = 3,   // "opcode %1% value %1% not given"
    RegisterNotExist            = 4,   // "register does not exist"
    TooFewArguments             = 5,   // "instruction %1% (opcode %2%) expects at least %3% argument(s), %4% argument(s) given"
    ValueOutOfRange             = 6,   // "value %1% is out of range for %2% bits"
    Exactly2Args                = 7,   // "addr, subr, andr, orr and xnorr expect exactly 2 arguments"
    WrongArgCount               = 8,   // "instruction %1% (opcode %2%) expects %3% arguments"
    ExpectedRegister            = 9,   // "wrong argument type, expected register"
    ExpectedValue               = 10,  // "wrong argument type, expected value"
    UserRegNotExist              = 11,  // "user register does not exist, just %1% registers available"
    InvalidOpcode               = 12,  // "%1% is not a valid opcode"

    // --- Array errors (13–16) ---
    ArrayIndexOutOfRange        = 13,
    ProgramTooLarge             = 14,
    ArrayIndexNeedConst         = 15,
    ArraysOnlyWave              = 16,

    // --- Type/memory errors (17–23) ---
    CantInvertType              = 17,
    CantInterpretAsBool         = 18,
    BrokenList                  = 19,
    FreeNullPtr                 = 20,
    FreeModifiedPtr             = 21,
    CacheMemoryFull             = 22,
    PlayNullPtr                 = 23,

    // --- Case/switch (24–30) ---
    CaseNeedsConst              = 24,
    RedefinedCase               = 25,
    RedefinedDefaultCase        = 26,
    NeedCaseBeforeStmt          = 27,
    CasePositiveNatural         = 28,
    CaseRounded                 = 29,
    CaseOutsideSwitch           = 30,

    // --- General compiler (31–46) ---
    ModifyConst                 = 31,
    CompressError               = 32,
    ConditionalNeedVarConst     = 33,
    ExpectedCommand             = 34,
    UnreachableCode             = 35,
    CsvInconsistentChannels     = 36,
    CsvWrongValue               = 37,
    CsvValueOutOfRange          = 38,
    CsvEmpty                    = 39,
    CantDivConstByWave          = 40,
    ExpectsVarOrConst           = 41,
    DivisionByZero              = 42,
    InvalidDeviceNr             = 43,
    EmptyInput                  = 44,
    EmptyOperation              = 45,
    ExecTableExpectsArg         = 46,
    // 47 — UNASSIGNED
    ExecTableInvalidConst       = 48,
    ExecTableInvalidIndex       = 49,
    WaveNotFittingCache         = 50,
    WaveNotFittingCacheGapless  = 51,
    WaveNotFittingMemory        = 52,
    // 53 — UNASSIGNED

    // --- Deprecation/function validation (54–104) ---
    DeprecatedConst             = 54,
    FuncCalledWithLogical       = 55,
    DeprecatedFunc              = 56,
    DeprecatedFunc2             = 57,
    DeprecatedFunc3             = 58,
    DeprecatedFuncFifo          = 59,
    FuncNotMultiCore            = 60,
    FuncEmpty                   = 61,
    FuncMinArgs                 = 62,
    FuncExpectsConst            = 63,
    FuncExpectsConstConst       = 64,
    FuncExpectsConstVar         = 65,
    FuncExpects3Const           = 66,
    FuncExpectsNoArgs           = 67,
    FuncExpectsSingleArg        = 68,
    FuncExpectsNArgs            = 69,
    FuncExpectsMaxArgs          = 70,
    FuncInvalidArgType          = 71,
    FuncNoName                  = 72,
    FuncNoReturn                = 73,
    FuncNotSupported            = 74,
    FuncOnlyConst               = 75,
    FuncPredefined              = 76,
    FuncNotFound                = 77,
    FuncExactArgs               = 78,
    FuncArgLessThan             = 79,
    DioZsyncMixed               = 80,
    FuncExpectsStringOrWave     = 81,
    FuncExpectsConstSecond      = 82,
    FuncExpectsWaveforms        = 83,
    AddExpectsMultiWave         = 84,
    AmplitudeClipped            = 85,
    ArgMustBeConst              = 86,
    ArgMustBeString             = 87,
    ArgOutOfAmplRange           = 88,
    ArgGreaterThanLength        = 89,
    JoinMin2                    = 90,
    UnknownWaveform             = 91,
    FuncExactArgs2              = 92,
    FuncArgs2or3                = 93,
    FuncArgsRange               = 94,
    ArgMustBePositive           = 95,
    ArgLargerThanLength         = 96,
    ArgOverflow                 = 97,
    FuncNoArgsGiven             = 98,
    ArgNotZero                  = 99,
    ValueCapped                 = 100,
    ValueMustBe1or2             = 101,
    LfsrInitZero                = 102,
    GenerateExpectsString       = 103,
    CantCallWithVar             = 104,

    // --- Specific built-in functions (105–111) ---
    GetUserRegArgs              = 105,
    GetUserRegRange             = 106,
    GetCntArgs                  = 107,
    GetCntRange                 = 108,
    GetSweeperLenArgs           = 109,
    GetSweeperLenArg            = 110,
    InvalidArgValue             = 111,

    // --- Increment/decrement/file (112–125) ---
    CantIncrement               = 112,
    CantDecrement               = 113,
    FileNotExist                = 114,
    NoSuchFile                  = 115,
    CantAddTypes                = 116,
    CantSubtractTypes           = 117,
    InvalidZSyncData            = 118,
    InvalidFeedbackData         = 119,
    OnlyConstVarInverted        = 120,
    LabelNotFound               = 121,
    LockArgs                    = 122,
    LockOnlyWave                = 123,
    TooManyIterations           = 124,
    OnlyConstVarNegated         = 125,

    // --- Node/oscillator (126–134) ---
    OperationWithoutOperator    = 126,
    NoValidBranchArg            = 127,
    NodeOnlySetDouble           = 128,
    NodePrecisionLoss           = 129,
    FreqNodeConstOnly           = 130,
    PhaseNodeConstOnly          = 131,
    NodeNotExist                = 132,
    NodeNeedsMFOption           = 133,
    SequencerCantDrive          = 134,

    // --- Variable arithmetic (135–148) ---
    ConstVarLogicalInvert       = 135,
    InvalidArgument             = 136,
    FuncSingleArg               = 137,
    FuncExactly2Args            = 138,
    VarMultNatural              = 139,
    CantAssignType              = 140,
    CantMultiplyTypes           = 141,
    CantDivideTypes             = 142,
    CantModuloTypes             = 143,
    CantBitAndTypes             = 144,
    CantBitOrTypes              = 145,
    CantCombineTypes            = 146,
    CantCompareTypes            = 147,
    NoValidOpType               = 148,

    // --- File/playWave (149–162) ---
    CantWriteFile               = 149,
    OnlyConstWaveIndex          = 150,
    UnexpectedArgs              = 151,
    OffsetTooHigh               = 152,
    ExpectsOffsetAndLength      = 153,
    LengthTooLong               = 154,
    ExpectsSamplesConst         = 155,
    ExpectsAddrConstOrVar       = 156,
    LengthIsZero                = 157,
    ExpectsWaveName             = 158,
    TooManyChannels             = 159,
    SampleRateConstOnly         = 160,
    SampleRateTooHigh           = 161,
    DioSampleRateTooHigh        = 162,

    // --- Prefetch/format/definition (163–172) ---
    PrefetchError               = 163,
    InvalidPrefetchId           = 164,
    SwapNotConnected            = 165,
    PrefetchNotSupported        = 166,
    FormatMoreArgs              = 167,
    FormatLessArgs              = 168,
    FormatCantInterpret         = 169,
    FormatFuncArgs              = 170,
    FormatVarReturn             = 171,
    AlreadyDefined              = 172,

    // --- Register/return/variable (173–184) ---
    OutOfRegisters              = 173,
    ReturnNotInFunc             = 174,
    TypeMismatchRead            = 175,
    TypeMismatchWrite           = 176,
    UninitializedVar            = 177,
    UnknownVar                  = 178,
    ExpectedReturnValue         = 179,
    ReturnStackEmpty            = 180,
    UnexpectedReturnValue       = 181,
    ReturnTypeMismatch          = 182,
    InvalidReturnArg            = 183,
    RepeatNonNegative           = 184,

    // --- setDouble/setInt/setRate (185–197) ---
    SetDoubleArgs               = 185,
    SetDoubleStringFirst        = 186,
    SetDoubleVarConstSecond     = 187,
    SetDoubleConstThird         = 188,
    SetIntArgs                  = 189,
    SetIntStringFirst           = 190,
    SetIntVarConstSecond        = 191,
    SetRateConst                = 192,
    SetRateOneConst             = 193,
    SetPrecompConst             = 194,
    SetPrecompOneConst          = 195,
    PrecompFlagsBranch          = 196,
    PrecompFlagsLoop            = 197,

    // --- setUserReg/PRNG/trigger (198–212) ---
    SetUserRegConstFirst        = 198,
    SetUserRegRange             = 199,
    SetUserRegArgs              = 200,
    SetUserRegVarConst          = 201,
    InvalidResetOscPhase        = 202,
    ResetOscPhaseArgs           = 203,
    PrngSeedPositive            = 204,
    PrngSeedZero                = 205,
    PrngSeedMax                 = 206,
    PrngRangeArgs               = 207,
    SetTriggerArgs              = 208,
    SetInternalTriggerArgs      = 209,
    ExpectsTwoConst             = 210,
    ExpectsOneConst             = 211,
    SineGenIndex                = 212,

    // --- Misc (213–227) ---
    CantShiftTypes              = 213,
    StatementNotSupported       = 214,
    SwitchNoCases               = 215,
    IndexMustBe                 = 216,
    UnknownFunction             = 217,
    UnknownDevice               = 218,
    FifoNotSupported            = 219,
    FifoRequired                = 220,
    UnlockArgs                  = 221,
    UnlockOnlyWave              = 222,
    NotSupportedGrouping        = 223,
    DivNotSupportedVar          = 224,
    CantAssignTypeless          = 225,
    VectTooManyArgs             = 226,
    WaitPositive                = 227,

    // --- Waveform errors (228–255) ---
    WaveformNotExist            = 228,
    CantModifyVarInRepeat       = 229,
    InconsistentChannels        = 230,
    WaveNotAligned              = 231,
    PlayLenNotAligned           = 232,
    WaveformLengthMismatch      = 233,
    WaveformNotFound            = 234,
    UninitializedWaveform       = 235,
    WaveNotUnique               = 236,
    InvalidChannel              = 237,
    WaveWrongChannels           = 238,
    MixedChannelNumbering       = 239,
    NoWaveformInFunc            = 240,
    DuplicateChannel            = 241,
    TooManyWavetableWaves       = 242,
    XorNotSupported             = 243,
    MinWaveformLength           = 244,
    WaveformBelowMin            = 245,
    PlayLenBelowMin             = 246,
    WaveUsedNotLoaded           = 247,
    WaveNotFittingPreload       = 248,
    WaveAlreadyAssigned         = 249,
    WaveIndexUsed               = 250,
    WaveNameInUse               = 251,
    WaveIndexExceedsTable       = 252,
    InvalidWaveformName         = 253,
    BitwiseNegativeOp2          = 254,
    BitwiseNegativeOp1          = 255,

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

    // TODO: These names are used in asm_commands.cpp / asm_commands_impl_*.cpp
    // but their numeric values are unknown. They may be aliases for existing
    // enum values above (e.g., InvalidRegister might be RegisterNotExist=4).
    // Added here as placeholders to allow compilation.
    InvalidRegister             = -1,   // PLACEHOLDER — real value unknown
    UnsupportedOp               = -2,   // PLACEHOLDER — real value unknown
    ValueOverflow               = -3,   // PLACEHOLDER — real value unknown
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

    // Format a message with no arguments.
    // Binary address: 0x15d0d0
    // NOTE: made static for compilation — underlying map is static (BSS).
    // In the binary these are const member functions on a global instance.
    static std::string format(ErrorMessageT id);

    // Format a message with arguments (variadic template).
    // ~40 instantiations in binary. Pattern:
    //   1. map::at(id) → format string
    //   2. boost::basic_format(str)
    //   3. feed args via boost::io::detail::feed_impl
    //   4. return basic_format::str()
    template <typename... Args>
    static std::string format(ErrorMessageT id, Args&&... args);

    // Static message map (BSS at 0xb84c38)
    static std::map<int, std::string> messages;
};

// NOTE: ResourcesException is defined in resources.hpp (not duplicated here).

// Free function: look up API error message by ZIResult_enum code.
// Uses a separate hash table (apiErrorMessages, BSS at 0xb85230).
// Returns static "unknownError" string if not found.
// Binary address: 0x2e4820
std::string const& getApiErrorMessage(int ziResultCode);

// ============================================================================
// ZIAWGCompilerException — thrown on compiler/assembler errors
//
// TODO: exact layout and vtable address not yet confirmed.
// Used in awg_assembler_impl_pipeline.cpp and other compilation paths.
// ============================================================================
class ZIAWGCompilerException : public std::exception {
public:
    explicit ZIAWGCompilerException(std::string const& msg) : message_(msg) {}
    explicit ZIAWGCompilerException(int code, std::string const& msg)
        : code_(code), message_(msg) {}
    ~ZIAWGCompilerException() override = default;
    const char* what() const noexcept override { return message_.c_str(); }
    int code() const { return code_; }
private:
    int code_ = 0;
    std::string message_;
};

} // namespace zhinst
