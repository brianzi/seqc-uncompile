# ErrorMessages format/get audit (Phase 57.A.1)

Read-only static analysis of every `ErrorMessages::format(...)` and
`ErrorMessages::get(...)` call site under `reconstructed/src/`,
cross-referenced against the format-template table in
`reconstructed/src/core/error_messages.cpp`.

Slot count = highest `%K%` (or `%|K$...|`) index appearing in the template.
boost::format requires the supplied argument count to equal the highest
referenced index; mismatches throw `boost::io::too_few_args` /
`boost::io::too_many_args` at runtime.


## Template inventory (error_messages.cpp)

| ID | Symbolic name(s) | Slots | Line | Template (truncated to 80 chars) |
|---|---|---|---|---|
| 0 | CmdWithoutRegister,InvalidRegister | 1 | 135 | `%1% command without valid register` |
| 1 | OpcodeRegNotGiven | 1 | 136 | `opcode %1% register %1% not given` |
| 2 | OpcodeValNotGiven | 1 | 137 | `opcode %1% value %1% not given` |
| 3 | RegisterNotExist | 0 | 138 | `register does not exist` |
| 4 | TooFewArguments | 4 | 139 | `instruction %1% (opcode %2%) expects at least %3% argument(s), %4% argument(s) g` |
| 5 | ValueOutOfRange,ValueOverflow | 2 | 140 | `value %1% is out of range for %2% bits` |
| 6 | Exactly2Args | 0 | 141 | `addr, subr, andr, orr and xnorr expect exactly 2 arguments` |
| 7 | WrongArgCount | 3 | 142 | `instruction %1% (opcode %2%) expects %3% arguments` |
| 8 | ExpectedRegister | 0 | 143 | `wrong argument type, expected register` |
| 9 | ExpectedValue | 0 | 144 | `wrong argument type, expected value` |
| 10 | UserRegNotExist | 1 | 145 | `user register does not exist, just %1% registers available` |
| 11 | InvalidOpcode,UnsupportedOp | 1 | 146 | `%1% is not a valid opcode` |
| 12 | ArrayIndexOutOfRange | 2 | 147 | `program is too large to fit into memory - has %1% instructions, maximum is %2%` |
| 13 | ProgramTooLarge | 0 | 148 | `arrays are just supported with type wave` |
| 14 | ArrayIndexNeedConst | 0 | 149 | `an array index always needs to be a const or cvar value` |
| 15 | ArraysOnlyWave | 0 | 150 | `the array index is out of the array range` |
| 16 | CantInvertType | 1 | 151 | `can't logically invert a variable of type %1%` |
| 17 | CantInterpretAsBool | 1 | 152 | `can't interpret %1% as boolean expression` |
| 18 | BrokenList | 1 | 153 | `broken or empty %1% list detected` |
| 19 | FreeNullPtr | 0 | 154 | `tried to free a NULL pointer` |
| 20 | FreeModifiedPtr | 0 | 155 | `tried to free a modified memory pointer` |
| 21 | CacheMemoryFull | 0 | 156 | `cache memory full` |
| 22 | PlayNullPtr | 0 | 157 | `tried to play a NULL pointer` |
| 23 | CaseNeedsConst | 0 | 158 | `case needs to be a const` |
| 24 | RedefinedCase | 1 | 159 | `redefined case %1%` |
| 25 | RedefinedDefaultCase | 0 | 160 | `redefined default case` |
| 26 | NeedCaseBeforeStmt | 0 | 161 | `required to define a case before any statement` |
| 27 | CasePositiveNatural | 1 | 162 | `case has to be a positive natural number, %1% not valid` |
| 28 | CaseRounded | 2 | 163 | `case should be a natural number, gets rounded from %1% to %2%` |
| 29 | CaseOutsideSwitch | 0 | 164 | `case should only be called out of a switch statment` |
| 30 | ModifyConst | 1 | 165 | `could not compress sections in output file '%1%'` |
| 31 | CompressError | 0 | 166 | `conditional expression expects a var, cvar or const argument` |
| 32 | ConditionalNeedVarConst | 0 | 167 | `tried to modify const value` |
| 33 | ExpectedCommand | 0 | 168 | `expected a command` |
| 34 | UnreachableCode | 0 | 169 | `this command will never be reached` |
| 35 | CsvInconsistentChannels | 1 | 170 | `CSV file '%1%' has an inconsistent number of channels` |
| 36 | CsvWrongValue | 2 | 171 | `wrong value in CSV file '%1%' (line %2%), only numerical values are allowed` |
| 37 | CsvValueOutOfRange | 2 | 172 | `value in CSV file '%1%' (line %2%) out of range` |
| 38 | CsvEmpty | 1 | 173 | `CSV file '%1%' exists but is empty` |
| 39 | CantDivConstByWave | 1 | 174 | `%1% expects a var or const argument` |
| 40 | ExpectsVarOrConst | 2 | 175 | `invalid device nr %1%, just %2% devices activated` |
| 41 | DivisionByZero | 0 | 176 | `division by zero` |
| 42 | InvalidDeviceNr | 0 | 177 | `can't divide a const by a wave` |
| 43 | EmptyInput | 0 | 178 | `nothing to write, empty input` |
| 44 | EmptyOperation | 0 | 179 | `empty operation, something went wrong` |
| 45 | ExecTableExpectsArg | 0 | 180 | `executeTableEntry expects a single const or var argument` |
| 46 | ExecTableInvalidConst | 0 | 181 | `Invalid constant argument used for executeTableEntry` |
| 47 | UnknownError47 | 0 | 182 | `Invalid constant argument used for executeTableEntry (the original binary has no` |
| 48 | ExecTableInvalidIndex | 0 | 183 | `Invalid table index used for executeTableEntry` |
| 49 | WaveNotFittingCache | 0 | 184 | `waveforms are not fitting into cache memory, load not aligned across AWGs - ever` |
| 50 | WaveNotFittingCacheGapless | 0 | 185 | `waveforms are not fitting into cache memory, gapless playback is not guaranteed` |
| 51 | WaveNotFittingMemory | 2 | 186 | `waveforms are not fitting into wave memory (%\|1$.1f\| kSa over a maximum of %\|` |
| 52 | DeprecatedConst | 2 | 187 | `constant '%1%' is deprecated, please use %2%` |
| 54 | FuncCalledWithLogical | 1 | 189 | `function '%1%' called with logical value instead of cycle count` |
| 55 | DeprecatedFunc | 2 | 190 | `function '%1%' is deprecated, please use '%2%'` |
| 56 | DeprecatedFunc2 | 3 | 191 | `function '%1%' is deprecated, please use '%2%' or '%3%'` |
| 57 | DeprecatedFunc3 | 1 | 192 | `function '%1%' is deprecated` |
| 58 | DeprecatedFuncFifo | 1 | 193 | `function '%1%' is deprecated as not compatible with AWG FIFO architecture` |
| 59 | FuncNotMultiCore | 1 | 194 | `function '%1%' is not supported in multi-core mode` |
| 60 | FuncEmpty | 3 | 195 | `function '%1% %2%%3%' is empty` |
| 61 | FuncMinArgs | 3 | 196 | `function '%1%' expects at least %2% argument(s), %3% arguments given` |
| 62 | FuncExpectsConst | 1 | 197 | `function '%1%' expects (const) as argument` |
| 63 | FuncExpectsConstConst | 1 | 198 | `function '%1%' expects (const, const) as arguments` |
| 64 | FuncExpectsConstVar | 1 | 199 | `function '%1%' expects (const, var) as arguments` |
| 65 | FuncExpects3Const | 1 | 200 | `function '%1%' expects (const, const, const) as arguments` |
| 66 | FuncExpectsNoArgs | 1 | 201 | `function '%1%' expects no arguments` |
| 67 | FuncExpectsSingleArg | 1 | 202 | `function '%1%' expects a single const, cvar or var argument` |
| 68 | FuncExpectsNArgs | 3 | 203 | `function '%1%' expects %2% arguments, %3% arguments given` |
| 69 | FuncExpectsMaxArgs | 3 | 204 | `function '%1%' expects %2% or fewer argument(s), %3% arguments given` |
| 70 | FuncInvalidArgType | 4 | 205 | `invalid argument type %1% in function '%2%'; expects argument '%3%' to be %4%` |
| 71 | FuncNoName | 0 | 206 | `function does not have a name` |
| 72 | FuncNoReturn | 2 | 207 | `function '%1%' reaches end without returning a %2%` |
| 73 | FuncNotSupported | 2 | 208 | `function '%1%' not supported on %2% devices` |
| 74 | FuncOnlyConst | 1 | 209 | `function '%1%' expects only const arguments` |
| 75 | FuncPredefined | 1 | 210 | `function '%1%' is a predefined function and cannot be overwritten` |
| 76 | FuncNotFound | 1 | 211 | `function '%1%' not found, possible alternatives are` |
| 77 | FuncExactArgs | 3 | 212 | `function '%1%' expects %2% argument(s), %3% argument(s) given` |
| 78 | FuncArgLessThan | 3 | 213 | `function '%1%' expects argument %2% to be less than %3%` |
| 79 | DioZsyncMixed | 0 | 214 | `DIO and ZSync external triggering can't be mixed in the same program` |
| 80 | FuncExpectsStringOrWave | 1 | 215 | `function '%1%' expects a string or wave as first argument` |
| 81 | FuncExpectsConstSecond | 1 | 216 | `function '%1%' expects a const as second argument` |
| 82 | FuncExpectsWaveforms | 1 | 217 | `function '%1%' expects just waveforms (string or wave) as arguments` |
| 83 | AddExpectsMultiWave | 0 | 218 | `function 'add' expects more than one waveform (string or wave)` |
| 84 | AmplitudeClipped | 1 | 219 | `%1% has a higher amplitude than 1.0, waveform amplitude will be limited to 1.0` |
| 85 | ArgMustBeConst | 2 | 220 | `argument %1% of %2% must be a const` |
| 86 | ArgMustBeString | 2 | 221 | `argument %1% of %2% must be a string` |
| 87 | ArgOutOfAmplRange | 2 | 222 | `argument %1% of %2% is out of the allowed range of -1.0 to 1.0` |
| 88 | ArgGreaterThanLength | 2 | 223 | `argument %1% of %2% is greater than the waveform length` |
| 89 | JoinMin2 | 0 | 224 | `join expects at least 2 waveforms` |
| 90 | UnknownWaveform | 2 | 225 | `tried to '%1%' the unknown waveform '%2%'` |
| 91 | FuncExactArgs2 | 3 | 226 | `function '%1%' expects %2% argument(s), %3% argument(s) given` |
| 92 | FuncArgs2or3 | 4 | 227 | `function '%1%' expects %2% or %3% arguments, %4% argument(s) given` |
| 93 | FuncArgsRange | 4 | 228 | `function '%1%' expects %2% to %3% arguments, %4% argument(s) given` |
| 94 | ArgMustBePositive | 2 | 229 | `argument %1% of %2% must be positive` |
| 95 | ArgLargerThanLength | 2 | 230 | `argument %1% of %2% is larger than the waveform length` |
| 96 | ArgOverflow | 3 | 231 | `argument %1% of %2% out of range, %3% overflow` |
| 97 | FuncNoArgsGiven | 1 | 232 | `called function '%1%' without arguments; string or const arguments expected` |
| 98 | ArgNotZero | 2 | 233 | `argument %1% of %2% is not allowed to be 0` |
| 99 | ValueCapped | 3 | 234 | `%3% value %1% is larger than the maximum possible value 3, will be capped to %2%` |
| 100 | ValueMustBe1or2 | 2 | 235 | `%2% value %1% must be 1 or 2` |
| 101 | LfsrInitZero | 1 | 236 | `argument %1%, LFSR initial value, can't be 0` |
| 102 | GenerateExpectsString | 0 | 237 | `generate(string, const...) expects a string as first argument` |
| 103 | CantCallWithVar | 1 | 238 | `%1% can't be called with var arguments` |
| 104 | GetUserRegArgs | 0 | 239 | `getUserReg expects exactly one const argument` |
| 105 | GetUserRegRange | 0 | 240 | `getUserReg register arguments must be in the range of 0 to 15` |
| 106 | GetCntArgs | 0 | 241 | `getCnt expects exactly one const argument` |
| 107 | GetCntRange | 0 | 242 | `getCnt counter argument must be either 0 or 1` |
| 108 | GetSweeperLenArgs | 0 | 243 | `getSweeperLength expects zero arguments or a single const argument` |
| 109 | GetSweeperLenArg | 0 | 244 | `getSweeperLength argument must be 1` |
| 110 | InvalidArgValue | 2 | 245 | `invalid value for argument %1% of function %2%` |
| 111 | CantIncrement | 2 | 246 | `can't increment %1% value %2%` |
| 112 | CantDecrement | 2 | 247 | `can't decrement %1% value %2%` |
| 113 | FileNotExist | 1 | 248 | `Input file '%1%' does not exist` |
| 114 | NoSuchFile | 1 | 249 | `no such file or directory: '%1%'` |
| 115 | CantAddTypes | 2 | 250 | `can't add variables of type %1% and %2%` |
| 116 | CantSubtractTypes | 2 | 251 | `can't subtract variables of type %1% and %2%` |
| 117 | InvalidZSyncData | 0 | 252 | `Invalid ZSync data_type provided` |
| 118 | InvalidFeedbackData | 0 | 253 | `Invalid Feedback data_type provided` |
| 119 | OnlyConstVarInverted | 1 | 254 | `only const or var types can be inverted, %1% given` |
| 120 | LabelNotFound | 1 | 255 | `label '%1%' not found` |
| 121 | LockArgs | 0 | 256 | `lock expects exactly one argument` |
| 122 | LockOnlyWave | 0 | 257 | `it's just possible to lock variables of type wave` |
| 123 | TooManyIterations | 0 | 258 | `too many iterations to unroll this loop, use a const variable for infinite loop ` |
| 124 | OnlyConstVarNegated | 1 | 259 | `only const or var types can be negated, %1% given` |
| 125 | OperationWithoutOperator | 0 | 260 | `operation without operator, something went wrong` |
| 126 | NoValidBranchArg | 0 | 261 | `no valid argument for branch command` |
| 127 | NodeOnlySetDouble | 1 | 262 | `node '%1%' can only be written using setDouble` |
| 128 | NodePrecisionLoss | 2 | 263 | `node '%1%' requires an %2% value, therefore the double precision is lost` |
| 129 | FreqNodeConstOnly | 1 | 264 | `frequency node '%1%' can only be set using a const or cvar` |
| 130 | PhaseNodeConstOnly | 1 | 265 | `phase node '%1%' can only be set using a const or cvar` |
| 131 | NodeNotExist | 1 | 266 | `node '%1%' doesn't exist` |
| 132 | NodeNeedsMFOption | 1 | 267 | `node '%1%' requires MF option installed` |
| 133 | SequencerCantDrive | 2 | 268 | `sequencer '%1%' can't drive '%2%'` |
| 134 | ConstVarLogicalInvert | 1 | 269 | `const or var types can be logically inverted, %1% given` |
| 135 | InvalidArgument | 1 | 270 | `%1%: invalid argument` |
| 136 | FuncSingleArg | 1 | 271 | `function '%1%' accepts a single argument` |
| 137 | FuncExactly2Args | 1 | 272 | `function '%1%' accepts exactly two arguments` |
| 138 | VarMultNatural | 0 | 273 | `a var variable can just be multiplied with a natural number` |
| 139 | CantAssignType | 2 | 274 | `can't assign a %1% to a %2% variable` |
| 140 | CantMultiplyTypes | 2 | 275 | `can't multiply a %1% and a %2% variable` |
| 141 | CantDivideTypes | 2 | 276 | `can't divide a %1% and a %2% variable` |
| 142 | CantModuloTypes | 2 | 277 | `can't apply modulo on a %1% with a %2% variable` |
| 143 | CantBitAndTypes | 2 | 278 | `can't apply bitwise AND on a %1% and a %2% variable` |
| 144 | CantBitOrTypes | 2 | 279 | `can't apply bitwise OR on a %1% and a %2% variable` |
| 145 | CantCombineTypes | 2 | 280 | `can't combine variables of type %1% and %2%` |
| 146 | CantCompareTypes | 2 | 281 | `can't compare data type %1% with %2%` |
| 147 | NoValidOpType | 0 | 282 | `no valid operation type` |
| 148 | CantWriteFile | 1 | 283 | `Could not write to file '%1%': invalid path or filename` |
| 149 | OnlyConstWaveIndex | 1 | 284 | `%1%: only const waveform index allowed` |
| 150 | UnexpectedArgs | 1 | 285 | `%1%: unexpected arguments` |
| 151 | OffsetTooHigh | 1 | 286 | `%1% offset is higher than the waveform length itself` |
| 152 | ExpectsOffsetAndLength | 1 | 287 | `%1% expects an offset and a length, just one parameter given` |
| 153 | LengthTooLong | 1 | 288 | `%1% length is too long for the given offset` |
| 154 | ExpectsSamplesConst | 1 | 289 | `%1% expects the number of samples to play as a const argument` |
| 155 | ExpectsAddrConstOrVar | 1 | 290 | `%1% expects the address within the waveform as const or var argument` |
| 156 | LengthIsZero | 1 | 291 | `%1% length is 0, no wave will be played` |
| 157 | ExpectsWaveName | 1 | 292 | `%1% expects the name of the waveform as a string or wave` |
| 158 | TooManyChannels | 3 | 293 | `%1% expects at most %2% channels, %3% given` |
| 159 | SampleRateConstOnly | 2 | 294 | `%1% sample rate can only be described with a const, not with a %2%` |
| 160 | SampleRateTooHigh | 1 | 295 | `%1% sample rate can not be greater than 14.0 MHz` |
| 161 | DioSampleRateTooHigh | 0 | 296 | `playDIOWave sample rate can not be greater than 450 MHz` |
| 162 | PrefetchError | 1 | 297 | `prefetch Error: %1%` |
| 163 | InvalidPrefetchId | 0 | 298 | `invalid identifier while placing the fetch and play commands` |
| 164 | SwapNotConnected | 0 | 299 | `tried to swap two not connected nodes` |
| 165 | PrefetchNotSupported | 0 | 300 | `explicit prefetch is not supported on this HW type` |
| 166 | FormatMoreArgs | 1 | 301 | `function '%1%' expected more arguments with the given string` |
| 167 | FormatLessArgs | 1 | 302 | `function '%1%' expected less arguments with the given string` |
| 168 | FormatCantInterpret | 1 | 303 | `function '%1%' can't interpret the format of the given string` |
| 169 | FormatFuncArgs | 3 | 304 | `function '%1%' expects %2% arguments, %3% given` |
| 170 | FormatVarReturn | 2 | 305 | `function '%1%' can't have a var argument with a %2% return value` |
| 171 | AlreadyDefined | 1 | 306 | `tried to define '%1%' but it is already defined` |
| 172 | OutOfRegisters | 0 | 307 | `run out of free registers, please reduce complexity` |
| 173 | ReturnNotInFunc | 0 | 308 | `return statement not in a function` |
| 174 | TypeMismatchRead | 2 | 309 | `type mismatch, can't read %1% as %2%` |
| 175 | TypeMismatchWrite | 2 | 310 | `type mismatch, can't write %1% to %2%` |
| 176 | UninitializedVar | 1 | 311 | `tried to access uninitialized variable '%1%'` |
| 177 | UnknownVar | 1 | 312 | `tried to access unknown variable '%1%'` |
| 178 | ExpectedReturnValue | 1 | 313 | `expected a return value of type %1% but no value given` |
| 179 | ReturnStackEmpty | 0 | 314 | `return label stack is empty` |
| 180 | UnexpectedReturnValue | 1 | 315 | `expected no return value but value of type %1% given` |
| 181 | ReturnTypeMismatch | 2 | 316 | `expected a return value of type %1%, %2% given` |
| 182 | InvalidReturnArg | 0 | 317 | `invalid return argument` |
| 183 | RepeatNonNegative | 0 | 318 | `repeat only accepts non-negative number of repetitions` |
| 184 | SetDoubleArgs | 0 | 319 | `setDouble expects 2 or 3 arguments` |
| 185 | SetDoubleStringFirst | 1 | 320 | `setDouble expects a string as first argument, %1% given` |
| 186 | SetDoubleVarConstSecond | 1 | 321 | `setDouble expects a var or const as second argument, %1% given` |
| 187 | SetDoubleConstThird | 1 | 322 | `setDouble expects a const as third argument, %1% given` |
| 188 | SetIntArgs | 0 | 323 | `setInt expects 2 arguments` |
| 189 | SetIntStringFirst | 1 | 324 | `setInt expects a string as first argument, %1% given` |
| 190 | SetIntVarConstSecond | 1 | 325 | `setInt expects a var or const as second argument, %1% given` |
| 191 | SetRateConst | 0 | 326 | `setRate expects a const argument` |
| 192 | SetRateOneConst | 0 | 327 | `setRate expects just one const argument` |
| 193 | SetPrecompConst | 0 | 328 | `setPrecompClear expects a const argument` |
| 194 | SetPrecompOneConst | 0 | 329 | `setPrecompClear expects just one const argument` |
| 195 | PrecompFlagsBranch | 0 | 330 | `final value of precompensation flags must be consistent in all branches` |
| 196 | PrecompFlagsLoop | 0 | 331 | `precompensation flags are indeterminate after loop execution` |
| 197 | SetUserRegConstFirst | 0 | 332 | `setUserReg expects a const as first argument` |
| 198 | SetUserRegRange | 0 | 333 | `setUserReg register must be in the range of 0 to 15` |
| 199 | SetUserRegArgs | 0 | 334 | `setUserReg expects exactly two arguments` |
| 200 | SetUserRegVarConst | 0 | 335 | `setUserReg expects a var or const as second argument` |
| 201 | InvalidResetOscPhase | 0 | 336 | `invalid resetOscPhase argument` |
| 202 | ResetOscPhaseArgs | 0 | 337 | `resetOscPhase expects a single const argument` |
| 203 | PrngSeedPositive | 0 | 338 | `PRNG seed expected to be positve` |
| 204 | PrngSeedZero | 0 | 339 | `PRNG seed equal to zero is not valid` |
| 205 | PrngSeedMax | 0 | 340 | `PRNG seed exceeded maximum of 65535 (0xFFFF)` |
| 206 | PrngRangeArgs | 0 | 341 | `setPRNGRange expects two const arguments (lower, upper) in range 0...65534, lowe` |
| 207 | SetTriggerArgs | 0 | 342 | `setTrigger expects a single const or var argument` |
| 208 | SetInternalTriggerArgs | 0 | 343 | `setInternalTrigger expects a single const or var argument` |
| 209 | ExpectsTwoConst | 1 | 344 | `%1% expects two const arguments` |
| 210 | ExpectsOneConst | 1 | 345 | `%1% expects one const argument` |
| 211 | SineGenIndex | 1 | 346 | `%1% expects a sine generator index of 0 or 1` |
| 212 | CantShiftTypes | 2 | 347 | `can't shift a %1% variable depending on a %2%` |
| 213 | StatementNotSupported | 1 | 348 | `%1% statement is not supported` |
| 214 | SwitchNoCases | 0 | 349 | `switch statement contains no case statements and will, therefore, be ignored` |
| 215 | IndexMustBe | 2 | 350 | `'%1%' index must be %2%` |
| 216 | UnknownFunction | 1 | 351 | `calling unknown function '%1%'` |
| 217 | UnknownDevice | 1 | 352 | `unknown target device '%1%'` |
| 218 | FifoNotSupported | 1 | 353 | `FIFO play mode is not supported on device type '%1%'` |
| 219 | FifoRequired | 1 | 354 | `FIFO play mode is required on device type '%1%'` |
| 220 | UnlockArgs | 0 | 355 | `unlock expects exactly one argument` |
| 221 | UnlockOnlyWave | 0 | 356 | `it's just possible to unlock variables of type wave` |
| 222 | NotSupportedGrouping | 2 | 357 | `%1% is not supported in %2% channel grouping mode` |
| 223 | DivNotSupportedVar | 0 | 358 | `division is not supported for var, only for const` |
| 224 | CantAssignTypeless | 1 | 359 | `can't assign the type-less variable '%1%'` |
| 225 | VectTooManyArgs | 1 | 360 | `using vect with more that %1% arguments is discouraged, use CSV files to define ` |
| 226 | WaitPositive | 0 | 361 | `wait expects a positive argument` |
| 227 | WaveformNotExist | 1 | 362 | `waveform '%1%' does not exist` |
| 228 | CantModifyVarInRepeat | 1 | 363 | `can't modify a %1% variable depending on a var or inside of a repeat loop` |
| 229 | InconsistentChannels | 4 | 364 | `inconsistent number of channels in waveform %1% command (waveform %2% has %3%, e` |
| 230 | WaveNotAligned | 4 | 365 | `waveform '%1%' size %2% is not aligned to %3% samples and will be zero-extended ` |
| 231 | PlayLenNotAligned | 3 | 366 | `play length %1% is not aligned to %2% samples and will be extended to %3% sample` |
| 232 | WaveformLengthMismatch | 0 | 367 | `the given waveforms are not of same length, shorter waveforms will be filled wit` |
| 233 | WaveformNotFound | 1 | 368 | `no waveform with the name '%1%' found` |
| 234 | UninitializedWaveform | 0 | 369 | `tried to access uninitialized waveform` |
| 235 | WaveNotUnique | 2 | 370 | `wave %1% is not unique, %2% used` |
| 236 | InvalidChannel | 2 | 371 | `can't play channel %1%, valid values for channel are from 1 to %2%` |
| 237 | WaveWrongChannels | 3 | 372 | `wave '%1%' expected to have %2% channel(s) but has %3%` |
| 238 | MixedChannelNumbering | 1 | 373 | `mixing of numbered and unnumbered channels [ %1%(1,w1,w2) ] is not allowed` |
| 239 | NoWaveformInFunc | 1 | 374 | `no waveform found in function %1%` |
| 240 | DuplicateChannel | 1 | 375 | `Channel %1% has been defined multiple times` |
| 241 | TooManyWavetableWaves | 2 | 376 | `number of waveforms in wavetable is too large - has %1% waveforms, maximum is %2` |
| 242 | XorNotSupported | 0 | 377 | `XOR operations are not supported` |
| 243 | MinWaveformLength | 1 | 378 | `minimal waveform length for playWaveDigTrigger() command is %1% samples` |
| 244 | WaveformBelowMin | 3 | 379 | `waveform '%1%' size %2% is below the minimal waveform length and will be zero-ex` |
| 245 | PlayLenBelowMin | 2 | 380 | `play length %1% is below minimum of %2% samples, will be extended` |
| 246 | WaveUsedNotLoaded | 1 | 381 | `waveform %1% is in the used waveforms list but is not marked for load` |
| 247 | WaveNotFittingPreload | 1 | 382 | `waveform %1% is not fitting in preload cache - will cause play gap` |
| 248 | WaveAlreadyAssigned | 1 | 383 | `waveform %1% has already assigned index` |
| 249 | WaveIndexUsed | 0 | 384 | `waveform index already used` |
| 250 | WaveNameInUse | 0 | 385 | `waveform index exceeds wavetable size` |
| 251 | WaveIndexExceedsTable | 0 | 386 | `invalid waveform name encountered. Please make sure that the waveform name suppl` |
| 252 | InvalidWaveformName | 1 | 387 | `waveform name %1% is already in use, please make sure to use unique waveform nam` |
| 253 | BitwiseNegativeOp2 | 2 | 388 | `attempting to perform a bitwise operation on a negative operand. Operands suppli` |
| 254 | BitwiseNegativeOp1 | 1 | 389 | `attempting to perform a bitwise operation on a negative operand. Operand supplie` |
| 255 | - | 0 | 390 | `unexpected error` |
| 16384 | ApiSuccess | 0 | 393 | `Success (no error)` |
| 16385 | ApiWarning | 0 | 394 | `Warning (general)` |
| 16386 | ApiFifoUnderrun | 0 | 395 | `FIFO underrun` |
| 16387 | ApiFifoOverflow | 0 | 396 | `FIFO overflow` |
| 16389 | ApiNodeNotFound | 0 | 397 | `Value or node not found` |
| 32768 | ApiKeywordNotResolved | 0 | 400 | `Keyword could not be resolved` |
| 32769 | ApiError | 0 | 401 | `Error (general)` |
| 32770 | ApiUsbFailed | 0 | 402 | `USB communication failed` |
| 32771 | ApiMemAlloc | 0 | 403 | `Memory allocation failed` |
| 32772 | ApiMutexInit | 0 | 404 | `Unable to initialize mutex` |
| 32773 | ApiMutexDestroy | 0 | 405 | `Unable to destroy mutex` |
| 32774 | ApiMutexLock | 0 | 406 | `Unable to lock mutex` |
| 32775 | ApiMutexUnlock | 0 | 407 | `Unable to unlock mutex` |
| 32776 | ApiThreadStart | 0 | 408 | `Unable to start thread` |
| 32777 | ApiThreadJoin | 0 | 409 | `Unable to join thread` |
| 32778 | ApiSocketInit | 0 | 410 | `Unable to initialize socket` |
| 32779 | ApiSocketConnect | 0 | 411 | `Unable to connect to socket` |
| 32780 | ApiHostnameNotFound | 0 | 412 | `Hostname not found` |
| 32781 | ApiConnectionInvalid | 0 | 413 | `Connection invalid` |
| 32782 | ApiTimeout | 0 | 414 | `Command timed out` |
| 32783 | ApiCommandFailed | 0 | 415 | `Command failed internally` |
| 32784 | ApiServerFailed | 0 | 416 | `Command failed on the server` |
| 32785 | ApiBufferTooSmall | 0 | 417 | `Provided buffer is too small` |
| 32786 | ApiFileError | 0 | 418 | `Unable to open file or read from it` |
| 32787 | ApiDuplicate | 0 | 419 | `There is already a similar item` |
| 32788 | ApiWriteOnly | 0 | 420 | `Attempt to get a write-only node` |
| 32789 | ApiDeviceNotVisible | 0 | 421 | `Device is not visible to the server` |
| 32790 | ApiDeviceOtherServer | 0 | 422 | `Device is already connected to a different server` |
| 32791 | ApiInterfaceNotSupported | 0 | 423 | `Device does not support the specified interface` |
| 32792 | ApiDeviceTimeout | 0 | 424 | `Device connection attempt timed out` |
| 32793 | ApiDeviceOtherInterface | 0 | 425 | `Device already connected over a different interface` |
| 32794 | ApiDeviceNeedsFirmware | 0 | 426 | `Device needs a firmware upgrade` |
| 32795 | ApiDeviceNotFound | 0 | 427 | `Device not found` |
| 32796 | ApiDataTypeMismatch | 0 | 428 | `Data type mismatch. Trying to get data from a poll event with wrong target data ` |
| 32797 | ApiNotSupported | 0 | 429 | `Command or provided arguments combination is not supported within this context` |
| 32798 | ApiMaxSessions | 0 | 430 | `The maximum number of allowed simultaneous sessions (32) exceeded. Ensure unused` |
| 32799 | ApiHF2NotSupported | 0 | 431 | `This functionality is not supported on HF2 devices` |
| 32800 | ApiReadOnly | 0 | 432 | `Attempt to set a read-only node` |
| 36864 | FwInvalidArgument | 0 | 435 | `Invalid argument received` |
| 36865 | FwUnknownError | 0 | 436 | `An error occurred on the device, but the error code was not provided. Please rep` |
| 36866 | FwBadAddress | 0 | 437 | `Requested block address does not belong to any known aperture` |
| 36867 | FwOffsetAboveRange | 0 | 438 | `Requested block address belongs to an aperture, but the offset is above the acti` |
| 36868 | FwOffsetLenAboveRange | 0 | 439 | `Requested block address belongs to an aperture, but the offset+length is above t` |
| 36869 | FwReadOnlyAperture | 0 | 440 | `Write access attempted to a read-only aperture or a read-only node` |
| 36870 | FwCantHandle | 0 | 441 | `Access to the requested register cannot be handled by the firmware (firmware con` |
| 36871 | FwNonIndexedVector | 0 | 442 | `Number of indices requested on a non-indexed vector node` |
| 36872 | FwVectorHeaderShort | 0 | 443 | `Vector transfer header error: received less data than the header size` |
| 36873 | FwVectorFrameIndex | 0 | 444 | `Vector transfer header error: indicated frame index is greater than the indicate` |
| 36874 | FwVectorOffsetLen | 0 | 445 | `Vector transfer header error: the indicated offset and payload length exceeds th` |
| 36875 | FwVectorExtraWords | 0 | 446 | `Vector transfer header error: the indicated number of extra words exceeds the in` |
| 36876 | FwOutOfSequence | 0 | 447 | `Received out-of-sequence frame (may never be reported if we keep the agreement, ` |
| 36877 | FwVectorOutOfRange | 0 | 448 | `Vector transfer error: accessing indexed element outside the current active rang` |

## Call-site inventory

All 545 call sites under `reconstructed/src/`. Underflow/overflow/leak
are flagged in the rightmost column; rows without a flag are matched.

| File:Line | Call | Resolved id (name) | Args vs slots |
|---|---|---|---|
| src/waveform/wave_index_tracker.cpp:84 | `format(ErrorMessageT(0xF9))` | 249 (WaveIndexUsed) | args=0 slots=0 |
| src/waveform/wave_index_tracker.cpp:91 | `format(ErrorMessageT(0xFA))` | 250 (WaveNameInUse) | args=0 slots=0 |
| src/waveform/wavetable_front.cpp:308 | `format(WaveformNotFound)` | 233 (WaveformNotFound) | args=1 slots=1 |
| src/waveform/wavetable_front.cpp:316 | `format(UninitializedWaveform)` | 234 (UninitializedWaveform) | args=0 slots=0 |
| src/waveform/waveform_generator.cpp:366 | `format(DeprecatedFunc)` | 55 (DeprecatedFunc) | args=2 slots=2 |
| src/waveform/waveform_generator.cpp:381 | `format(UnknownFunction)` | 216 (UnknownFunction) | args=1 slots=1 |
| src/waveform/waveform_generator.cpp:459 | `format(ArgMustBeConst)` | 85 (ArgMustBeConst) | args=2 slots=2 |
| src/waveform/waveform_generator.cpp:478 | `format(AmplitudeClipped)` | 84 (AmplitudeClipped) | args=1 slots=1 |
| src/waveform/waveform_generator.cpp:491 | `format(ArgMustBeConst)` | 85 (ArgMustBeConst) | args=2 slots=2 |
| src/waveform/waveform_generator.cpp:506 | `format(ArgOverflow)` | 96 (ArgOverflow) | args=3 slots=3 |
| src/waveform/waveform_generator.cpp:541 | `format(ArgMustBeString)` | 86 (ArgMustBeString) | args=2 slots=2 |
| src/waveform/waveform_generator.cpp:556 | `format(UnknownWaveform)` | 90 (UnknownWaveform) | args=2 slots=2 |
| src/waveform/waveform_generator.cpp:596 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator.cpp:613 | `format(ValueCapped)` | 99 (ValueCapped) | args=2 slots=3 UNDER |
| src/waveform/waveform_generator.cpp:729 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:48 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:123 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:167 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:211 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:227 | `format(ArgMustBePositive)` | 94 (ArgMustBePositive) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:265 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:280 | `format(ArgMustBePositive)` | 94 (ArgMustBePositive) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:331 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:354 | `format(ArgLargerThanLength)` | 95 (ArgLargerThanLength) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:362 | `format(ArgNotZero)` | 98 (ArgNotZero) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:395 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:406 | `format(ArgMustBePositive)` | 94 (ArgMustBePositive) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:411 | `format(ArgMustBePositive)` | 94 (ArgMustBePositive) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:433 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:455 | `format(ArgMustBePositive)` | 94 (ArgMustBePositive) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:471 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:493 | `format(ArgMustBePositive)` | 94 (ArgMustBePositive) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:526 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:543 | `format(ArgLargerThanLength)` | 95 (ArgLargerThanLength) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:551 | `format(ArgNotZero)` | 98 (ArgNotZero) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:586 | `format(ArgLargerThanLength)` | 95 (ArgLargerThanLength) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:593 | `format(ArgNotZero)` | 98 (ArgNotZero) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:632 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:694 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:747 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:797 | `format(FuncArgs2or3)` | 92 (FuncArgs2or3) | args=4 slots=4 |
| src/waveform/waveform_generator_dsp.cpp:882 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:941 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:992 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:1051 | `format(LfsrInitZero)` | 101 (LfsrInitZero) | args=1 slots=1 |
| src/waveform/waveform_generator_dsp.cpp:1060 | `format(ValueMustBe1or2)` | 100 (ValueMustBe1or2) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:1104 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:1132 | `format(ArgLargerThanLength)` | 95 (ArgLargerThanLength) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:1201 | `format(VectTooManyArgs)` | 225 (VectTooManyArgs) | args=1 slots=1 |
| src/waveform/waveform_generator_dsp.cpp:1233 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:1280 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:1304 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:1485 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:1508 | `format(UnknownWaveform)` | 90 (UnknownWaveform) | args=2 slots=2 |
| src/waveform/waveform_generator_dsp.cpp:1531 | `format(InconsistentChannels)` | 229 (InconsistentChannels) | args=4 slots=4 |
| src/waveform/waveform_generator_dsp.cpp:1620 | `format(AmplitudeClipped)` | 84 (AmplitudeClipped) | args=1 slots=1 |
| src/waveform/waveform_generator_dsp.cpp:1946 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:2040 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=3 slots=3 |
| src/waveform/waveform_generator_dsp.cpp:2071 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=3 slots=3 |
| src/runtime/csv_parser.cpp:161 | `format(static_cast<ErrorMessageT>(0x25))` | 37 (CsvValueOutOfRange) | args=2 slots=2 |
| src/runtime/csv_parser.cpp:219 | `format(static_cast<ErrorMessageT>(0x25))` | 37 (CsvValueOutOfRange) | args=2 slots=2 |
| src/runtime/csv_parser.cpp:516 | `format(static_cast<ErrorMessageT>(0x23))` | 35 (CsvInconsistentChannels) | args=1 slots=1 |
| src/runtime/csv_parser.cpp:659 | `format(static_cast<ErrorMessageT>(0x23))` | 35 (CsvInconsistentChannels) | args=1 slots=1 |
| src/runtime/csv_parser.cpp:691 | `format(static_cast<ErrorMessageT>(0x23))` | 35 (CsvInconsistentChannels) | args=1 slots=1 |
| src/runtime/csv_parser.cpp:764 | `format(static_cast<ErrorMessageT>(0x23))` | 35 (CsvInconsistentChannels) | args=1 slots=1 |
| src/runtime/csv_parser.cpp:831 | `format(static_cast<ErrorMessageT>(0x23))` | 35 (CsvInconsistentChannels) | args=1 slots=1 |
| src/runtime/csv_parser.cpp:954 | `format(static_cast<ErrorMessageT>(0x23))` | 35 (CsvInconsistentChannels) | args=1 slots=1 |
| src/runtime/csv_parser.cpp:978 | `format(static_cast<ErrorMessageT>(0x23))` | 35 (CsvInconsistentChannels) | args=1 slots=1 |
| src/runtime/csv_parser.cpp:1028 | `format(static_cast<ErrorMessageT>(0x23))` | 35 (CsvInconsistentChannels) | args=1 slots=1 |
| src/runtime/custom_functions.cpp:333 | `format(PlayLenBelowMin)` | 245 (PlayLenBelowMin) | args=2 slots=2 |
| src/runtime/custom_functions.cpp:350 | `format(PlayLenNotAligned)` | 231 (PlayLenNotAligned) | args=3 slots=3 |
| src/runtime/custom_functions.cpp:584 | `format(FuncNotSupported)` | 73 (FuncNotSupported) | args=2 slots=2 |
| src/runtime/custom_functions.cpp:608 | `format(MinWaveformLength)` | 243 (MinWaveformLength) | args=1 slots=1 |
| src/runtime/custom_functions.cpp:634 | `format(WaveformBelowMin)` | 244 (WaveformBelowMin) | args=3 slots=3 |
| src/runtime/custom_functions.cpp:669 | `format(WaveNotAligned)` | 230 (WaveNotAligned) | args=4 slots=4 |
| src/runtime/custom_functions.cpp:735 | `format(NodeNotExist)` | 131 (NodeNotExist) | args=1 slots=1 |
| src/runtime/custom_functions.cpp:823 | `format(ExpectsWaveName)` | 157 (ExpectsWaveName) | args=1 slots=1 |
| src/runtime/custom_functions.cpp:859 | `format(TooManyChannels)` | 158 (TooManyChannels) | args=3 slots=3 |
| src/runtime/custom_functions.cpp:887 | `format(MixedChannelNumbering)` | 238 (MixedChannelNumbering) | args=1 slots=1 |
| src/runtime/custom_functions.cpp:953 | `format(MixedChannelNumbering)` | 238 (MixedChannelNumbering) | args=1 slots=1 |
| src/runtime/custom_functions.cpp:972 | `format(InvalidChannel)` | 236 (InvalidChannel) | args=2 slots=2 |
| src/runtime/custom_functions.cpp:987 | `format(DuplicateChannel)` | 240 (DuplicateChannel) | args=1 slots=1 |
| src/runtime/custom_functions.cpp:1006 | `format(WaveWrongChannels)` | 237 (WaveWrongChannels) | args=3 slots=3 |
| src/runtime/custom_functions.cpp:1050 | `format(WaveformNotFound)` | 233 (WaveformNotFound) | args=1 slots=1 |
| src/runtime/custom_functions.cpp:1062 | `format(static_cast<ErrorMessageT>(0xEB))` | 235 (WaveNotUnique) | args=2 slots=2 |
| src/runtime/custom_functions.cpp:1103 | `format(WaveformNotFound)` | 233 (WaveformNotFound) | args=1 slots=1 |
| src/runtime/custom_functions.cpp:1109 | `format(UninitializedWaveform)` | 234 (UninitializedWaveform) | args=0 slots=0 |
| src/runtime/custom_functions.cpp:1174 | `format(static_cast<ErrorMessageT>(0x9f))` | 159 (SampleRateConstOnly) | args=2 slots=2 |
| src/runtime/custom_functions.cpp:1271 | `format(MixedChannelNumbering)` | 238 (MixedChannelNumbering) | args=1 slots=1 |
| src/runtime/custom_functions.cpp:1283 | `format(DioZsyncMixed)` | 79 (DioZsyncMixed) | args=0 slots=0 |
| src/runtime/custom_functions_dio.cpp:47 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_dio.cpp:71 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_dio.cpp:95 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_dio.cpp:110 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_dio.cpp:131 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/runtime/custom_functions_dio.cpp:136 | `format(FuncArgs2or3)` | 92 (FuncArgs2or3) | args=4 slots=4 |
| src/runtime/custom_functions_dio.cpp:174 | `format(InvalidZSyncData)` | 117 (InvalidZSyncData) | args=0 slots=0 |
| src/runtime/custom_functions_dio.cpp:230 | `format(FuncExactArgs2)` | 91 (FuncExactArgs2) | args=3 slots=3 |
| src/runtime/custom_functions_dio.cpp:235 | `format(FuncArgs2or3)` | 92 (FuncArgs2or3) | args=4 slots=4 |
| src/runtime/custom_functions_dio.cpp:280 | `format(InvalidZSyncData)` | 117 (InvalidZSyncData) | args=0 slots=0 |
| src/runtime/custom_functions_dio.cpp:335 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_dio.cpp:352 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_dio.cpp:376 | `format(WaveIndexExceedsTable)` | 251 (WaveIndexExceedsTable) | args=0 slots=0 |
| src/runtime/custom_functions_dio.cpp:393 | `format(OnlyConstWaveIndex)` | 149 (OnlyConstWaveIndex) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:231 | `format(NoWaveformInFunc)` | 239 (NoWaveformInFunc) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:436 | `format(TooManyChannels)` | 158 (TooManyChannels) | args=3 slots=3 |
| src/runtime/custom_functions_play.cpp:479 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=3 slots=3 |
| src/runtime/custom_functions_play.cpp:491 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:496 | `format(IndexMustBe)` | 215 (IndexMustBe) | args=1 slots=2 UNDER |
| src/runtime/custom_functions_play.cpp:614 | `format(ErrorMessageT(0xa5))` | 165 (PrefetchNotSupported) | args=0 slots=0 |
| src/runtime/custom_functions_play.cpp:736 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=3 slots=3 |
| src/runtime/custom_functions_play.cpp:766 | `format(ExpectsOffsetAndLength)` | 152 (ExpectsOffsetAndLength) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:774 | `format(ExpectsOffsetAndLength)` | 152 (ExpectsOffsetAndLength) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:789 | `format(SampleRateTooHigh)` | 160 (SampleRateTooHigh) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:800 | `format(ExpectsSamplesConst)` | 154 (ExpectsSamplesConst) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:823 | `format(LengthIsZero)` | 156 (LengthIsZero) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:1019 | `format(ExpectsOffsetAndLength)` | 152 (ExpectsOffsetAndLength) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:1379 | `format(SequencerCantDrive)` | 133 (SequencerCantDrive) | args=2 slots=2 |
| src/runtime/custom_functions_play.cpp:1423 | `format(SequencerCantDrive)` | 133 (SequencerCantDrive) | args=2 slots=2 |
| src/runtime/custom_functions_play.cpp:1617 | `format(NodePrecisionLoss)` | 128 (NodePrecisionLoss) | args=1 slots=2 UNDER |
| src/runtime/custom_functions_play.cpp:1646 | `format(NodePrecisionLoss)` | 128 (NodePrecisionLoss) | args=1 slots=2 UNDER |
| src/runtime/custom_functions_play.cpp:1687 | `format(NodeOnlySetDouble)` | 127 (NodeOnlySetDouble) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:1710 | `format(NodeOnlySetDouble)` | 127 (NodeOnlySetDouble) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:1733 | `format(FreqNodeConstOnly)` | 129 (FreqNodeConstOnly) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:1756 | `format(PhaseNodeConstOnly)` | 130 (PhaseNodeConstOnly) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:2003 | `format(NodePrecisionLoss)` | 128 (NodePrecisionLoss) | args=2 slots=2 |
| src/runtime/custom_functions_play.cpp:2069 | `format(NodePrecisionLoss)` | 128 (NodePrecisionLoss) | args=1 slots=2 UNDER |
| src/runtime/custom_functions_play.cpp:2109 | `format(NodeOnlySetDouble)` | 127 (NodeOnlySetDouble) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:2135 | `format(PhaseNodeConstOnly)` | 130 (PhaseNodeConstOnly) | args=1 slots=1 |
| src/runtime/custom_functions_play.cpp:2274 | `format(FuncSingleArg)` | 136 (FuncSingleArg) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_play.cpp:2312 | `format(FuncInvalidArgType)` | 70 (FuncInvalidArgType) | args=1 slots=4 UNDER |
| src/runtime/custom_functions_play.cpp:2324 | `format(FormatMoreArgs)` | 166 (FormatMoreArgs) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_play.cpp:2328 | `format(FormatCantInterpret)` | 168 (FormatCantInterpret) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_playback.cpp:67 | `format(DeprecatedFuncFifo)` | 58 (DeprecatedFuncFifo) | args=1 slots=1 |
| src/runtime/custom_functions_playback.cpp:144 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=3 slots=3 |
| src/runtime/custom_functions_playback.cpp:171 | `format(SampleRateTooHigh)` | 160 (SampleRateTooHigh) | args=1 slots=1 |
| src/runtime/custom_functions_playback.cpp:443 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=3 slots=3 |
| src/runtime/custom_functions_playback.cpp:470 | `format(DioSampleRateTooHigh)` | 161 (DioSampleRateTooHigh) | args=0 slots=0 |
| src/runtime/custom_functions_playback.cpp:672 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_playback.cpp:746 | `format(FuncArgs2or3)` | 92 (FuncArgs2or3) | args=4 slots=4 |
| src/runtime/custom_functions_playback.cpp:758 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_playback.cpp:798 | `format(InvalidZSyncData)` | 117 (InvalidZSyncData) | args=0 slots=0 |
| src/runtime/custom_functions_playback.cpp:826 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_playback.cpp:838 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_playback.cpp:850 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_playback.cpp:861 | `format(FormatFuncArgs)` | 169 (FormatFuncArgs) | args=1 slots=3 UNDER |
| src/runtime/custom_functions_playback.cpp:880 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_playback.cpp:914 | `format(SetRateOneConst)` | 192 (SetRateOneConst) | args=1 slots=0 OVER |
| src/runtime/custom_functions_playback.cpp:920 | `format(SetRateConst)` | 191 (SetRateConst) | args=1 slots=0 OVER |
| src/runtime/custom_functions_playback.cpp:938 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=3 slots=3 |
| src/runtime/custom_functions_playback.cpp:942 | `format(FuncExpectsMaxArgs)` | 69 (FuncExpectsMaxArgs) | args=3 slots=3 |
| src/runtime/custom_functions_playback.cpp:991 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=3 slots=3 |
| src/runtime/custom_functions_playback.cpp:995 | `format(FuncExpectsMaxArgs)` | 69 (FuncExpectsMaxArgs) | args=3 slots=3 |
| src/runtime/custom_functions_registers.cpp:46 | `format(SetTriggerArgs)` | 207 (SetTriggerArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:62 | `format(SetTriggerArgs)` | 207 (SetTriggerArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:71 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:75 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:96 | `format(SetInternalTriggerArgs)` | 208 (SetInternalTriggerArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:112 | `format(SetInternalTriggerArgs)` | 208 (SetInternalTriggerArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:121 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:125 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:139 | `format(IndexMustBe)` | 215 (IndexMustBe) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:143 | `format(IndexMustBe)` | 215 (IndexMustBe) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:178 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:182 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:201 | `format(IndexMustBe)` | 215 (IndexMustBe) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:205 | `format(IndexMustBe)` | 215 (IndexMustBe) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:239 | `format(SetIntArgs)` | 188 (SetIntArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:245 | `format(SetIntArgs)` | 188 (SetIntArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:250 | `format(SetIntVarConstSecond)` | 190 (SetIntVarConstSecond) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:265 | `format(SetDoubleArgs)` | 184 (SetDoubleArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:280 | `format(SetDoubleArgs)` | 184 (SetDoubleArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:285 | `format(SetDoubleVarConstSecond)` | 186 (SetDoubleVarConstSecond) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:289 | `format(SetDoubleConstThird)` | 187 (SetDoubleConstThird) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:298 | `format(GenerateExpectsString)` | 102 (GenerateExpectsString) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:303 | `format(GenerateExpectsString)` | 102 (GenerateExpectsString) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:307 | `format(FuncNoArgsGiven)` | 97 (FuncNoArgsGiven) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:328 | `format(CantCallWithVar)` | 103 (CantCallWithVar) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:344 | `format(SetUserRegArgs)` | 199 (SetUserRegArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:350 | `format(SetUserRegArgs)` | 199 (SetUserRegArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:357 | `format(SetUserRegArgs)` | 199 (SetUserRegArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:374 | `format(SetUserRegArgs)` | 199 (SetUserRegArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:427 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:499 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:555 | `format(SetPrecompOneConst)` | 194 (SetPrecompOneConst) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:558 | `format(SetPrecompConst)` | 193 (SetPrecompConst) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:579 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:586 | `format(FuncCalledWithLogical)` | 54 (FuncCalledWithLogical) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:603 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:641 | `format(LockArgs)` | 121 (LockArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:644 | `format(LockOnlyWave)` | 122 (LockOnlyWave) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:650 | `format(WaveformNotExist)` | 227 (WaveformNotExist) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:662 | `format(UnlockArgs)` | 220 (UnlockArgs) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:665 | `format(UnlockOnlyWave)` | 221 (UnlockOnlyWave) | args=1 slots=0 OVER |
| src/runtime/custom_functions_registers.cpp:671 | `format(WaveformNotExist)` | 227 (WaveformNotExist) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:686 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:692 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:704 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:740 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:799 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:815 | `format(FuncExpectsMaxArgs)` | 69 (FuncExpectsMaxArgs) | args=1 slots=3 UNDER |
| src/runtime/custom_functions_registers.cpp:828 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_registers.cpp:836 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_registers.cpp:875 | `format(FuncExpectsMaxArgs)` | 69 (FuncExpectsMaxArgs) | args=1 slots=3 UNDER |
| src/runtime/custom_functions_registers.cpp:884 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_registers.cpp:921 | `format(FuncMinArgs)` | 61 (FuncMinArgs) | args=1 slots=3 UNDER |
| src/runtime/custom_functions_registers.cpp:1021 | `format(SetTriggerArgs)` | 207 (SetTriggerArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1040 | `format(PrngSeedPositive)` | 203 (PrngSeedPositive) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1043 | `format(PrngSeedZero)` | 204 (PrngSeedZero) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1046 | `format(PrngSeedMax)` | 205 (PrngSeedMax) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1065 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:1081 | `format(PrngRangeArgs)` | 206 (PrngRangeArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1096 | `format(PrngRangeArgs)` | 206 (PrngRangeArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1100 | `format(PrngRangeArgs)` | 206 (PrngRangeArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1104 | `format(PrngRangeArgs)` | 206 (PrngRangeArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1107 | `format(PrngRangeArgs)` | 206 (PrngRangeArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1111 | `format(PrngRangeArgs)` | 206 (PrngRangeArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1147 | `format(FuncExpectsMaxArgs)` | 69 (FuncExpectsMaxArgs) | args=1 slots=3 UNDER |
| src/runtime/custom_functions_registers.cpp:1153 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_registers.cpp:1175 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_registers.cpp:1190 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_registers.cpp:1210 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_registers.cpp:1219 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=0 slots=1 UNDER |
| src/runtime/custom_functions_registers.cpp:1296 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:1312 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:1326 | `format(FuncExpects3Const)` | 65 (FuncExpects3Const) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:1333 | `format(GetUserRegArgs)` | 104 (GetUserRegArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1337 | `format(GetUserRegArgs)` | 104 (GetUserRegArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1402 | `format(FuncExpectsNArgs)` | 68 (FuncExpectsNArgs) | args=3 slots=3 |
| src/runtime/custom_functions_registers.cpp:1413 | `format(FuncExpectsConstVar)` | 64 (FuncExpectsConstVar) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:1420 | `format(InvalidArgValue)` | 110 (InvalidArgValue) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:1424 | `format(InvalidArgValue)` | 110 (InvalidArgValue) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:1437 | `format(InvalidArgValue)` | 110 (InvalidArgValue) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:1505 | `format(FuncExpectsNArgs)` | 68 (FuncExpectsNArgs) | args=3 slots=3 |
| src/runtime/custom_functions_registers.cpp:1517 | `format(FuncExpectsConstVar)` | 64 (FuncExpectsConstVar) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:1525 | `format(InvalidArgValue)` | 110 (InvalidArgValue) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:1529 | `format(InvalidArgValue)` | 110 (InvalidArgValue) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:1605 | `format(FuncExpectsNArgs)` | 68 (FuncExpectsNArgs) | args=3 slots=3 |
| src/runtime/custom_functions_registers.cpp:1621 | `format(FuncExpectsConstVar)` | 64 (FuncExpectsConstVar) | args=1 slots=1 |
| src/runtime/custom_functions_registers.cpp:1644 | `format(InvalidArgValue)` | 110 (InvalidArgValue) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:1652 | `format(InvalidArgValue)` | 110 (InvalidArgValue) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:1660 | `format(InvalidArgValue)` | 110 (InvalidArgValue) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:1668 | `format(InvalidArgValue)` | 110 (InvalidArgValue) | args=2 slots=2 |
| src/runtime/custom_functions_registers.cpp:415 | `get(GetUserRegArgs)` | 104 (GetUserRegArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:436 | `get(GetUserRegRange)` | 105 (GetUserRegRange) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:490 | `get(GetSweeperLenArgs)` | 108 (GetSweeperLenArgs) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:507 | `get(GetSweeperLenArg)` | 109 (GetSweeperLenArg) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:712 | `get(GetCntRange)` | 107 (GetCntRange) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:980 | `get(UnknownError47)` | 47 (UnknownError47) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1000 | `get(ExecTableInvalidIndex)` | 48 (ExecTableInvalidIndex) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1003 | `get(ExecTableInvalidIndex)` | 48 (ExecTableInvalidIndex) | args=0 slots=0 |
| src/runtime/custom_functions_registers.cpp:1008 | `get(ExecTableExpectsArg)` | 45 (ExecTableExpectsArg) | args=0 slots=0 |
| src/runtime/custom_functions_wait.cpp:46 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:57 | `format(FuncCalledWithLogical)` | 54 (FuncCalledWithLogical) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:190 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:206 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:215 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:273 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:279 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:299 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:355 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:359 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:368 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:384 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:390 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:393 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:407 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:441 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:456 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:472 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:551 | `format(FuncExpectsNoArgs)` | 66 (FuncExpectsNoArgs) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:643 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:649 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:661 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:668 | `format(IndexMustBe)` | 215 (IndexMustBe) | args=2 slots=2 |
| src/runtime/custom_functions_wait.cpp:705 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:712 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:732 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:776 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:781 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:794 | `format(FuncExpectsConstConst)` | 63 (FuncExpectsConstConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:839 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:846 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:852 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:925 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:952 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:964 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:969 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1000 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1020 | `format(ExpectsTwoConst)` | 209 (ExpectsTwoConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1029 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1035 | `format(SineGenIndex)` | 211 (SineGenIndex) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1079 | `format(ExpectsOneConst)` | 210 (ExpectsOneConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1086 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1131 | `format(ExpectsTwoConst)` | 209 (ExpectsTwoConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1140 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1146 | `format(SineGenIndex)` | 211 (SineGenIndex) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1178 | `format(ExpectsOneConst)` | 210 (ExpectsOneConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1184 | `format(FuncExpectsConst)` | 62 (FuncExpectsConst) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1231 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1239 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:1251 | `format(FuncExpectsSingleArg)` | 67 (FuncExpectsSingleArg) | args=1 slots=1 |
| src/runtime/custom_functions_wait.cpp:761 | `get(NotSupportedGrouping)` | 222 (NotSupportedGrouping) | args=0 slots=2 LEAK |
| src/runtime/resources.cpp:505 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:512 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:567 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:574 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:582 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:588 | `format(ErrorMessageT::TypeMismatchWrite)` | None (-) | args=2 slots=None |
| src/runtime/resources.cpp:806 | `format(ErrorMessageT::UnknownVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:832 | `format(ErrorMessageT::UnknownVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:862 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:965 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:970 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1004 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1009 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1051 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1056 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1109 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1114 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1183 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1188 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1207 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1212 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1231 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1236 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1336 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1341 | `format(ErrorMessageT::AlreadyDefined)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1384 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1387 | `format(ErrorMessageT::TypeMismatchWrite)` | None (-) | args=2 slots=None |
| src/runtime/resources.cpp:1427 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1430 | `format(ErrorMessageT::TypeMismatchRead)` | None (-) | args=2 slots=None |
| src/runtime/resources.cpp:1473 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1476 | `format(ErrorMessageT::TypeMismatchWrite)` | None (-) | args=2 slots=None |
| src/runtime/resources.cpp:1482 | `format(ErrorMessageT::CantModifyVarInRepeat)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1519 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1522 | `format(ErrorMessageT::TypeMismatchWrite)` | None (-) | args=2 slots=None |
| src/runtime/resources.cpp:1528 | `format(ErrorMessageT::CantModifyVarInRepeat)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1579 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1582 | `format(ErrorMessageT::TypeMismatchWrite)` | None (-) | args=2 slots=None |
| src/runtime/resources.cpp:1588 | `format(ErrorMessageT::CantModifyVarInRepeat)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1634 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1637 | `format(ErrorMessageT::TypeMismatchWrite)` | None (-) | args=2 slots=None |
| src/runtime/resources.cpp:1643 | `format(ErrorMessageT::CantModifyVarInRepeat)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1696 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1701 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1705 | `format(ErrorMessageT::TypeMismatchRead)` | None (-) | args=2 slots=None |
| src/runtime/resources.cpp:1750 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1755 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1759 | `format(ErrorMessageT::TypeMismatchRead)` | None (-) | args=2 slots=None |
| src/runtime/resources.cpp:1817 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1822 | `format(ErrorMessageT::UninitializedVar)` | None (-) | args=1 slots=None |
| src/runtime/resources.cpp:1828 | `format(ErrorMessageT::TypeMismatchWrite)` | None (-) | args=2 slots=None |
| src/runtime/resources_function.cpp:336 | `format(FormatVarReturn)` | 170 (FormatVarReturn) | args=2 slots=2 |
| src/runtime/resources_function.cpp:360 | `format(FuncInvalidArgType)` | 70 (FuncInvalidArgType) | args=4 slots=4 |
| src/runtime/resources_function.cpp:780 | `format(AlreadyDefined)` | 171 (AlreadyDefined) | args=1 slots=1 |
| src/runtime/resources_function.cpp:837 | `format(UninitializedVar)` | 176 (UninitializedVar) | args=1 slots=1 |
| src/runtime/resources_static_global.cpp:179 | `format(DeprecatedConst)` | 52 (DeprecatedConst) | args=2 slots=2 |
| src/runtime/resources_static_global.cpp:183 | `format(DeprecatedConst)` | 52 (DeprecatedConst) | args=2 slots=2 |
| src/runtime/resources_static_global.cpp:187 | `format(DeprecatedConst)` | 52 (DeprecatedConst) | args=2 slots=2 |
| src/ast/node.cpp:368 | `format(PrefetchError)` | 162 (PrefetchError) | args=1 slots=1 |
| src/ast/seqc_ast_eval_arithmetic.cpp:429 | `format(ErrorMessageT(0xE0))` | 224 (CantAssignTypeless) | args=1 slots=1 |
| src/ast/seqc_ast_eval_arithmetic.cpp:712 | `format(ErrorMessageT(0x8b))` | 139 (CantAssignType) | args=2 slots=2 |
| src/ast/seqc_ast_eval_arithmetic.cpp:781 | `format(ErrorMessageT(0xe9))` | 233 (WaveformNotFound) | args=1 slots=1 |
| src/ast/seqc_ast_eval_arithmetic.cpp:810 | `format(ErrorMessageT(0x8b))` | 139 (CantAssignType) | args=2 slots=2 |
| src/ast/seqc_ast_eval_arithmetic.cpp:911 | `format(ErrorMessageT(0x73))` | 115 (CantAddTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_arithmetic.cpp:1165 | `format(ErrorMessageT(0x73))` | 115 (CantAddTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_arithmetic.cpp:1245 | `format(ErrorMessageT(0x74))` | 116 (CantSubtractTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_arithmetic.cpp:1520 | `format(ErrorMessageT(0x74))` | 116 (CantSubtractTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_arithmetic.cpp:1595 | `format(ErrorMessageT(0x8c))` | 140 (CantMultiplyTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_arithmetic.cpp:1725 | `format(ErrorMessageT(0x8c))` | 140 (CantMultiplyTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_arithmetic.cpp:1838 | `format(ErrorMessageT(0x8d))` | 141 (CantDivideTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_arithmetic.cpp:1955 | `format(ErrorMessageT(0x8d))` | 141 (CantDivideTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_control.cpp:98 | `format(ErrorMessageT(0x4C))` | 76 (FuncNotFound) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:133 | `format(ErrorMessageT(0x4D))` | 77 (FuncExactArgs) | args=3 slots=3 |
| src/ast/seqc_ast_eval_control.cpp:187 | `format(ErrorMessageT(0x46))` | 70 (FuncInvalidArgType) | args=4 slots=4 |
| src/ast/seqc_ast_eval_control.cpp:201 | `format(ErrorMessageT(0x46))` | 70 (FuncInvalidArgType) | args=4 slots=4 |
| src/ast/seqc_ast_eval_control.cpp:215 | `format(ErrorMessageT(0x46))` | 70 (FuncInvalidArgType) | args=4 slots=4 |
| src/ast/seqc_ast_eval_control.cpp:229 | `format(ErrorMessageT(0x46))` | 70 (FuncInvalidArgType) | args=4 slots=4 |
| src/ast/seqc_ast_eval_control.cpp:239 | `format(ErrorMessageT(0x46))` | 70 (FuncInvalidArgType) | args=4 slots=4 |
| src/ast/seqc_ast_eval_control.cpp:547 | `format(// @0x211389
            ProgramTooLarge)` | None (-) | args=0 slots=None |
| src/ast/seqc_ast_eval_control.cpp:560 | `format(// @0x2113ca
            ArrayIndexNeedConst)` | None (-) | args=0 slots=None |
| src/ast/seqc_ast_eval_control.cpp:580 | `format(// @0x21160e
            WaveformNotFound)` | None (-) | args=1 slots=None |
| src/ast/seqc_ast_eval_control.cpp:618 | `format(// @0x2118d6
            ArraysOnlyWave)` | None (-) | args=0 slots=None |
| src/ast/seqc_ast_eval_control.cpp:694 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:711 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:824 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:917 | `format(ErrorMessageT(0x1c))` | 28 (CaseRounded) | args=2 slots=2 |
| src/ast/seqc_ast_eval_control.cpp:925 | `format(ErrorMessageT(0x1b))` | 27 (CasePositiveNatural) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:1163 | `format(NeedCaseBeforeStmt)` | 26 (NeedCaseBeforeStmt) | args=0 slots=0 |
| src/ast/seqc_ast_eval_control.cpp:1244 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:1258 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:1726 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:1788 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:1875 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:2063 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:2173 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:2372 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:2730 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:2744 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:2966 | `format(ErrorMessageT(0x27))` | 39 (CantDivConstByWave) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:3076 | `format(ErrorMessageT(0xe4))` | 228 (CantModifyVarInRepeat) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:3115 | `format(ErrorMessageT(0xe4))` | 228 (CantModifyVarInRepeat) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:3138 | `format(ErrorMessageT(0xe4))` | 228 (CantModifyVarInRepeat) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:3174 | `format(ErrorMessageT(0xe4))` | 228 (CantModifyVarInRepeat) | args=1 slots=1 |
| src/ast/seqc_ast_eval_control.cpp:3405 | `format(ErrorMessageT::FuncPredefined)` | None (-) | args=1 slots=None |
| src/ast/seqc_ast_eval_control.cpp:3452 | `format(ErrorMessageT::FuncEmpty)` | None (-) | args=3 slots=None |
| src/ast/seqc_ast_eval_control.cpp:3510 | `format(ErrorMessageT::FuncNoReturn)` | None (-) | args=2 slots=None |
| src/ast/seqc_ast_eval_control.cpp:3520 | `format(ErrorMessageT::FuncEmpty)` | None (-) | args=1 slots=None |
| src/ast/seqc_ast_eval_control.cpp:3524 | `format(ErrorMessageT::FuncEmpty)` | None (-) | args=1 slots=None |
| src/ast/seqc_ast_eval_helpers.cpp:116 | `format(ErrorMessageT(0x91))` | 145 (CantCombineTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_helpers.cpp:122 | `format(ErrorMessageT(0x91))` | 145 (CantCombineTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:150 | `format(ErrorMessageT(0xd5))` | 213 (StatementNotSupported) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:168 | `format(ErrorMessageT(0xd5))` | 213 (StatementNotSupported) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:513 | `format(ErrorMessageT(0x8e))` | 142 (CantModuloTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:665 | `format(ErrorMessageT(0x6f))` | 111 (CantIncrement) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:693 | `format(ErrorMessageT(0x6f))` | 111 (CantIncrement) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:711 | `format(ErrorMessageT(0x6f))` | 111 (CantIncrement) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:718 | `format(ErrorMessageT(0x6f))` | 111 (CantIncrement) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:834 | `format(ErrorMessageT(0x70))` | 112 (CantDecrement) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:859 | `format(ErrorMessageT(0x70))` | 112 (CantDecrement) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:876 | `format(ErrorMessageT(0x70))` | 112 (CantDecrement) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:883 | `format(ErrorMessageT(0x70))` | 112 (CantDecrement) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:927 | `format(ErrorMessageT(0x7c))` | 124 (OnlyConstVarNegated) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:938 | `format(ErrorMessageT(0x7c))` | 124 (OnlyConstVarNegated) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:948 | `format(ErrorMessageT(0x7c))` | 124 (OnlyConstVarNegated) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1000 | `format(ErrorMessageT(0x7c))` | 124 (OnlyConstVarNegated) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1042 | `format(ErrorMessageT(0x77))` | 119 (OnlyConstVarInverted) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1052 | `format(ErrorMessageT(0x77))` | 119 (OnlyConstVarInverted) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1061 | `format(ErrorMessageT(0x77))` | 119 (OnlyConstVarInverted) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1119 | `format(ErrorMessageT(0xfe))` | 254 (BitwiseNegativeOp1) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1134 | `format(ErrorMessageT(0x77))` | 119 (OnlyConstVarInverted) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1176 | `format(ErrorMessageT(0x86))` | 134 (ConstVarLogicalInvert) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1187 | `format(ErrorMessageT(0x86))` | 134 (ConstVarLogicalInvert) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1197 | `format(ErrorMessageT(0x86))` | 134 (ConstVarLogicalInvert) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1278 | `format(ErrorMessageT(0x86))` | 134 (ConstVarLogicalInvert) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1326 | `format(ErrorMessageT(0xb2))` | 178 (ExpectedReturnValue) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1406 | `format(ErrorMessageT(0xb5))` | 181 (ReturnTypeMismatch) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:1439 | `format(ErrorMessageT(0xb5))` | 181 (ReturnTypeMismatch) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:1469 | `format(ErrorMessageT(0xb5))` | 181 (ReturnTypeMismatch) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:1528 | `format(ErrorMessageT(0xe9))` | 233 (WaveformNotFound) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1544 | `format(ErrorMessageT(0xb5))` | 181 (ReturnTypeMismatch) | args=2 slots=2 |
| src/ast/seqc_ast_eval_simple.cpp:1557 | `format(ErrorMessageT(0xb4))` | 180 (UnexpectedReturnValue) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1636 | `format(ErrorMessageT(0x12))` | 18 (BrokenList) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1699 | `format(ErrorMessageT(0x12))` | 18 (BrokenList) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1818 | `format(ErrorMessageT(0x12))` | 18 (BrokenList) | args=1 slots=1 |
| src/ast/seqc_ast_eval_simple.cpp:1802 | `get(0x22)` | 34 (UnreachableCode) | args=0 slots=0 |
| src/codegen/awg_assembler_opcodes.cpp:146 | `format(LabelNotFound)` | 120 (LabelNotFound) | args=1 slots=1 |
| src/codegen/awg_assembler_opcodes.cpp:153 | `format(LabelNotFound)` | 120 (LabelNotFound) | args=1 slots=1 |
| src/codegen/awg_assembler_opcodes.cpp:169 | `format(static_cast<ErrorMessageT>(5))` | 5 (ValueOutOfRange,ValueOverflow) | args=2 slots=2 |
| src/codegen/awg_assembler_opcodes.cpp:195 | `format(static_cast<ErrorMessageT>(7))` | 7 (WrongArgCount) | args=3 slots=3 |
| src/codegen/awg_assembler_opcodes.cpp:220 | `format(static_cast<ErrorMessageT>(7))` | 7 (WrongArgCount) | args=3 slots=3 |
| src/codegen/awg_assembler_opcodes.cpp:231 | `format(static_cast<ErrorMessageT>(1))` | 1 (OpcodeRegNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:241 | `format(static_cast<ErrorMessageT>(2))` | 2 (OpcodeValNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:264 | `format(static_cast<ErrorMessageT>(7))` | 7 (WrongArgCount) | args=3 slots=3 |
| src/codegen/awg_assembler_opcodes.cpp:274 | `format(static_cast<ErrorMessageT>(1))` | 1 (OpcodeRegNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:283 | `format(static_cast<ErrorMessageT>(2))` | 2 (OpcodeValNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:292 | `format(static_cast<ErrorMessageT>(2))` | 2 (OpcodeValNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:301 | `format(static_cast<ErrorMessageT>(2))` | 2 (OpcodeValNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:367 | `format(static_cast<ErrorMessageT>(1))` | 1 (OpcodeRegNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:379 | `format(static_cast<ErrorMessageT>(2))` | 2 (OpcodeValNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:390 | `format(static_cast<ErrorMessageT>(2))` | 2 (OpcodeValNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:404 | `format(static_cast<ErrorMessageT>(1))` | 1 (OpcodeRegNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:415 | `format(static_cast<ErrorMessageT>(1))` | 1 (OpcodeRegNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:430 | `format(static_cast<ErrorMessageT>(2))` | 2 (OpcodeValNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:467 | `format(static_cast<ErrorMessageT>(1))` | 1 (OpcodeRegNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:480 | `format(static_cast<ErrorMessageT>(10))` | 10 (UserRegNotExist) | args=1 slots=1 |
| src/codegen/awg_assembler_opcodes.cpp:514 | `format(static_cast<ErrorMessageT>(1))` | 1 (OpcodeRegNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:524 | `format(static_cast<ErrorMessageT>(2))` | 2 (OpcodeValNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:540 | `format(static_cast<ErrorMessageT>(7))` | 7 (WrongArgCount) | args=3 slots=3 |
| src/codegen/awg_assembler_opcodes.cpp:578 | `format(static_cast<ErrorMessageT>(1))` | 1 (OpcodeRegNotGiven) | args=2 slots=1 OVER |
| src/codegen/awg_assembler_opcodes.cpp:600 | `format(static_cast<ErrorMessageT>(4))` | 4 (TooFewArguments) | args=4 slots=4 |
| src/codegen/awg_assembler_opcodes.cpp:607 | `format(static_cast<ErrorMessageT>(7))` | 7 (WrongArgCount) | args=3 slots=3 |
| src/codegen/awg_assembler_opcodes.cpp:629 | `format(static_cast<ErrorMessageT>(7))` | 7 (WrongArgCount) | args=3 slots=3 |
| src/codegen/awg_assembler_opcodes.cpp:106 | `get(8)` | 8 (ExpectedRegister) | args=0 slots=0 |
| src/codegen/awg_assembler_opcodes.cpp:113 | `get(3)` | 3 (RegisterNotExist) | args=0 slots=0 |
| src/codegen/awg_assembler_opcodes.cpp:124 | `get(4)` | 4 (TooFewArguments) | args=0 slots=4 LEAK |
| src/codegen/awg_assembler_opcodes.cpp:176 | `get(9)` | 9 (ExpectedValue) | args=0 slots=0 |
| src/codegen/awg_assembler_opcodes.cpp:333 | `get(4)` | 4 (TooFewArguments) | args=0 slots=4 LEAK |
| src/codegen/awg_assembler_opcodes.cpp:344 | `get(6)` | 6 (Exactly2Args) | args=0 slots=0 |
| src/codegen/awg_assembler_impl_pipeline.cpp:84 | `format(FileNotExist)` | 113 (FileNotExist) | args=1 slots=1 |
| src/codegen/awg_assembler_impl_pipeline.cpp:625 | `format(CantWriteFile)` | 148 (CantWriteFile) | args=1 slots=1 |
| src/codegen/awg_compiler.cpp:70 | `format(ErrorMessageT(0x1e))` | 30 (ModifyConst) | args=1 slots=1 |
| src/codegen/awg_compiler.cpp:83 | `format(ErrorMessageT(0x1e))` | 30 (ModifyConst) | args=1 slots=1 |
| src/codegen/awg_compiler.cpp:93 | `format(ErrorMessageT(0x1e))` | 30 (ModifyConst) | args=1 slots=1 |
| src/codegen/awg_compiler.cpp:398 | `format(ErrorMessageT(0xDA))` | 218 (FifoNotSupported) | args=1 slots=1 |
| src/codegen/awg_compiler.cpp:405 | `format(ErrorMessageT(0xDB))` | 219 (FifoRequired) | args=1 slots=1 |
| src/codegen/awg_compiler.cpp:483 | `format(ErrorMessageT(0x0C))` | 12 (ArrayIndexOutOfRange) | args=2 slots=2 |
| src/codegen/awg_compiler.cpp:505 | `format(ErrorMessageT(0xF1))` | 241 (TooManyWavetableWaves) | args=2 slots=2 |
| src/codegen/awg_compiler.cpp:618 | `format(ErrorMessageT(0xe3))` | 227 (WaveformNotExist) | args=1 slots=1 |
| src/codegen/awg_compiler.cpp:662 | `format(ErrorMessageT(0xe3))` | 227 (WaveformNotExist) | args=1 slots=1 |
| src/codegen/awg_compiler.cpp:757 | `format(EmptyInput)` | 43 (EmptyInput) | args=0 slots=0 |
| src/codegen/prefetch.cpp:504 | `format(ErrorMessageT(0xC3))` | 195 (PrecompFlagsBranch) | args=0 slots=0 |
| src/codegen/prefetch.cpp:651 | `format(ErrorMessageT(0xC4))` | 196 (PrecompFlagsLoop) | args=0 slots=0 |
| src/codegen/prefetch.cpp:1828 | `format(ErrorMessageT(0xA2))` | 162 (PrefetchError) | args=1 slots=1 |
| src/codegen/prefetch.cpp:1874 | `format(ErrorMessageT(0xA2))` | 162 (PrefetchError) | args=1 slots=1 |
| src/codegen/prefetch.cpp:2352 | `format(ErrorMessageT(0x33))` | 51 (WaveNotFittingMemory) | args=2 slots=2 |
| src/codegen/prefetch_emit.cpp:106 | `format(ErrorMessageT(0xA3))` | 163 (InvalidPrefetchId) | args=0 slots=0 |
| src/asm/asm_commands.cpp:91 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:118 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:125 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:133 | `format(ErrorMessageT::ValueOverflow)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:169 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:176 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:195 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:219 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:256 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:263 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:275 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:311 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:380 | `format(ErrorMessageT(0xd8))` | 216 (UnknownFunction) | args=1 slots=1 |
| src/asm/asm_commands.cpp:401 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:452 | `format(ErrorMessageT(0x20))` | 32 (ConditionalNeedVarConst) | args=2 slots=0 OVER |
| src/asm/asm_commands.cpp:458 | `format(ErrorMessageT(0x20))` | 32 (ConditionalNeedVarConst) | args=2 slots=0 OVER |
| src/asm/asm_commands.cpp:517 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:530 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:542 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:550 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:559 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:567 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:578 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:585 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:592 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:599 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:622 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:632 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands.cpp:659 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands_impl_cervino.cpp:23 | `format(ErrorMessageT::UnsupportedOp)` | None (-) | args=1 slots=None |
| src/asm/asm_commands_impl_cervino.cpp:70 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/asm/asm_commands_impl_cervino.cpp:89 | `format(ErrorMessageT::UnsupportedOp)` | None (-) | args=1 slots=None |
| src/asm/asm_commands_impl_cervino.cpp:96 | `format(ErrorMessageT::UnsupportedOp)` | None (-) | args=1 slots=None |
| src/asm/asm_commands_impl_hirzel.cpp:70 | `format(ErrorMessageT::InvalidRegister)` | None (-) | args=1 slots=None |
| src/ast/seqc_ast_eval_impl.inc:417 | `format(VarMultNatural)` | 138 (VarMultNatural) | args=0 slots=0 |
| src/ast/seqc_ast_eval_impl.inc:565 | `format(ErrorMessageT(0x10))` | 16 (CantInvertType) | args=1 slots=1 |
| src/ast/seqc_ast_eval_impl.inc:612 | `format(ErrorMessageT(0x10))` | 16 (CantInvertType) | args=1 slots=1 |
| src/ast/seqc_ast_eval_impl.inc:672 | `format(ErrorMessageT(0x11))` | 17 (CantInterpretAsBool) | args=1 slots=1 |
| src/ast/seqc_ast_eval_impl.inc:736 | `format(ErrorMessageT(0x11))` | 17 (CantInterpretAsBool) | args=1 slots=1 |
| src/ast/seqc_ast_eval_impl.inc:987 | `format(ErrorMessageT(0x92))` | 146 (CantCompareTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_impl.inc:1252 | `format(ErrorMessageT(0x92))` | 146 (CantCompareTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_impl.inc:1540 | `format(ErrorMessageT(0x92))` | 146 (CantCompareTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_impl.inc:1870 | `format(ErrorMessageT(0xD4))` | 212 (CantShiftTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_impl.inc:2087 | `format(ErrorMessageT(0xFD))` | 253 (BitwiseNegativeOp2) | args=2 slots=2 |
| src/ast/seqc_ast_eval_impl.inc:2111 | `format(ErrorMessageT(0x8F))` | 143 (CantBitAndTypes) | args=2 slots=2 |
| src/ast/seqc_ast_eval_impl.inc:2309 | `format(ErrorMessageT(0xFD))` | 253 (BitwiseNegativeOp2) | args=2 slots=2 |
| src/ast/seqc_ast_eval_impl.inc:2333 | `format(ErrorMessageT(0x90))` | 144 (CantBitOrTypes) | args=2 slots=2 |

## Mismatches (the work items)

### Underflow (`boost::io::too_few_args` — runtime throw, **HIGH severity**)

| File:Line | ID (name) | Slots | Args | Missing arg(s) likely | IF link |
|---|---|---|---|---|---|
| src/waveform/waveform_generator.cpp:613 | 99 (ValueCapped) | 3 | 2 | funcName (passed as %3% in template "%3% value %1% is larger ...") | IF-179 |
| src/runtime/custom_functions_play.cpp:496 | 215 (IndexMustBe) | 2 | 1 | the index value/range string (template: '%1%' index must be %2%) | — |
| src/runtime/custom_functions_play.cpp:1617 | 128 (NodePrecisionLoss) | 2 | 1 | integer value (template: node '%1%' requires an %2% value, ...) | — |
| src/runtime/custom_functions_play.cpp:1646 | 128 (NodePrecisionLoss) | 2 | 1 | integer value (template: node '%1%' requires an %2% value, ...) | — |
| src/runtime/custom_functions_play.cpp:2069 | 128 (NodePrecisionLoss) | 2 | 1 | integer value (template: node '%1%' requires an %2% value, ...) | — |
| src/runtime/custom_functions_play.cpp:2274 | 136 (FuncSingleArg) | 1 | 0 | funcName (template: function '%1%' accepts a single argument) | IF-178 (printF too_few_args branch) |
| src/runtime/custom_functions_play.cpp:2312 | 70 (FuncInvalidArgType) | 4 | 1 | three more args: funcName, argName, expectedType (template has 4 slots) | — |
| src/runtime/custom_functions_play.cpp:2324 | 166 (FormatMoreArgs) | 1 | 0 | funcName (template: function '%1%' expected more arguments ...) | IF-178 |
| src/runtime/custom_functions_play.cpp:2328 | 168 (FormatCantInterpret) | 1 | 0 | funcName (template: function '%1%' can't interpret ...) | IF-178 |
| src/runtime/custom_functions_playback.cpp:861 | 169 (FormatFuncArgs) | 3 | 1 | expected-arg-count and given-arg-count (template has 3 slots) | — |
| src/runtime/custom_functions_registers.cpp:815 | 69 (FuncExpectsMaxArgs) | 3 | 1 | max-arg-count and given-arg-count (template has 3 slots) | IF-178 (startQA*) |
| src/runtime/custom_functions_registers.cpp:828 | 62 (FuncExpectsConst) | 1 | 0 | funcName (template: function '%1%' expects (const) as argument) | IF-178 |
| src/runtime/custom_functions_registers.cpp:836 | 62 (FuncExpectsConst) | 1 | 0 | funcName (template: function '%1%' expects (const) as argument) | IF-178 |
| src/runtime/custom_functions_registers.cpp:875 | 69 (FuncExpectsMaxArgs) | 3 | 1 | max-arg-count and given-arg-count (template has 3 slots) | IF-178 (startQA*) |
| src/runtime/custom_functions_registers.cpp:884 | 62 (FuncExpectsConst) | 1 | 0 | funcName (template: function '%1%' expects (const) as argument) | IF-178 |
| src/runtime/custom_functions_registers.cpp:921 | 61 (FuncMinArgs) | 3 | 1 | min-arg-count and given-arg-count (template has 3 slots) | — |
| src/runtime/custom_functions_registers.cpp:1147 | 69 (FuncExpectsMaxArgs) | 3 | 1 | max-arg-count and given-arg-count (template has 3 slots) | IF-178 (startQA*) |
| src/runtime/custom_functions_registers.cpp:1153 | 62 (FuncExpectsConst) | 1 | 0 | funcName (template: function '%1%' expects (const) as argument) | IF-178 |
| src/runtime/custom_functions_registers.cpp:1175 | 62 (FuncExpectsConst) | 1 | 0 | funcName (template: function '%1%' expects (const) as argument) | IF-178 |
| src/runtime/custom_functions_registers.cpp:1190 | 62 (FuncExpectsConst) | 1 | 0 | funcName (template: function '%1%' expects (const) as argument) | IF-178 |
| src/runtime/custom_functions_registers.cpp:1210 | 62 (FuncExpectsConst) | 1 | 0 | funcName (template: function '%1%' expects (const) as argument) | IF-178 |
| src/runtime/custom_functions_registers.cpp:1219 | 62 (FuncExpectsConst) | 1 | 0 | funcName (template: function '%1%' expects (const) as argument) | IF-178 |

### Overflow (`boost::io::too_many_args` — runtime throw, **HIGH severity**)

| File:Line | ID (name) | Slots | Args | Note |
|---|---|---|---|---|
| src/runtime/custom_functions_playback.cpp:914 | 192 (SetRateOneConst) | 0 | 1 | Template m[192] reconstructed as 0-slot constant; binary likely has %1%. Probable template-recon bug rather than call bug. |
| src/runtime/custom_functions_playback.cpp:920 | 191 (SetRateConst) | 0 | 1 | Same — m[191] template recon mismatch. |
| src/runtime/custom_functions_registers.cpp:46 | 207 (SetTriggerArgs) | 0 | 1 | Same — m[207]. |
| src/runtime/custom_functions_registers.cpp:62 | 207 (SetTriggerArgs) | 0 | 1 | Same — m[207]. |
| src/runtime/custom_functions_registers.cpp:96 | 208 (SetInternalTriggerArgs) | 0 | 1 | Same — m[208]. |
| src/runtime/custom_functions_registers.cpp:112 | 208 (SetInternalTriggerArgs) | 0 | 1 | Same — m[208]. |
| src/runtime/custom_functions_registers.cpp:239 | 188 (SetIntArgs) | 0 | 1 | Same — m[188]. |
| src/runtime/custom_functions_registers.cpp:245 | 188 (SetIntArgs) | 0 | 1 | Same — m[188]. |
| src/runtime/custom_functions_registers.cpp:265 | 184 (SetDoubleArgs) | 0 | 1 | Same — m[184]. |
| src/runtime/custom_functions_registers.cpp:280 | 184 (SetDoubleArgs) | 0 | 1 | Same — m[184]. |
| src/runtime/custom_functions_registers.cpp:344 | 199 (SetUserRegArgs) | 0 | 1 | Same — m[199]. |
| src/runtime/custom_functions_registers.cpp:350 | 199 (SetUserRegArgs) | 0 | 1 | Same — m[199]. |
| src/runtime/custom_functions_registers.cpp:357 | 199 (SetUserRegArgs) | 0 | 1 | Same — m[199]. |
| src/runtime/custom_functions_registers.cpp:374 | 199 (SetUserRegArgs) | 0 | 1 | Same — m[199]. |
| src/runtime/custom_functions_registers.cpp:555 | 194 (SetPrecompOneConst) | 0 | 1 | Same — m[194]. |
| src/runtime/custom_functions_registers.cpp:558 | 193 (SetPrecompConst) | 0 | 1 | Same — m[193]. |
| src/runtime/custom_functions_registers.cpp:641 | 121 (LockArgs) | 0 | 1 | Same — m[121]. |
| src/runtime/custom_functions_registers.cpp:644 | 122 (LockOnlyWave) | 0 | 1 | Same — m[122]. |
| src/runtime/custom_functions_registers.cpp:662 | 220 (UnlockArgs) | 0 | 1 | Same — m[220]. |
| src/runtime/custom_functions_registers.cpp:665 | 221 (UnlockOnlyWave) | 0 | 1 | Same — m[221]. |
| src/codegen/awg_assembler_opcodes.cpp:231 | 1 (OpcodeRegNotGiven) | 1 | 2 | m[1] template uses "%1% register %1%" (highest-index=1) but call passes (opcode, position) — template recon bug: should be "opcode %1% register %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:241 | 2 (OpcodeValNotGiven) | 1 | 2 | m[2] template uses "%1% value %1%" — same recon bug; should be "opcode %1% value %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:274 | 1 (OpcodeRegNotGiven) | 1 | 2 | m[1] template uses "%1% register %1%" (highest-index=1) but call passes (opcode, position) — template recon bug: should be "opcode %1% register %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:283 | 2 (OpcodeValNotGiven) | 1 | 2 | m[2] template uses "%1% value %1%" — same recon bug; should be "opcode %1% value %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:292 | 2 (OpcodeValNotGiven) | 1 | 2 | m[2] template uses "%1% value %1%" — same recon bug; should be "opcode %1% value %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:301 | 2 (OpcodeValNotGiven) | 1 | 2 | m[2] template uses "%1% value %1%" — same recon bug; should be "opcode %1% value %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:367 | 1 (OpcodeRegNotGiven) | 1 | 2 | m[1] template uses "%1% register %1%" (highest-index=1) but call passes (opcode, position) — template recon bug: should be "opcode %1% register %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:379 | 2 (OpcodeValNotGiven) | 1 | 2 | m[2] template uses "%1% value %1%" — same recon bug; should be "opcode %1% value %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:390 | 2 (OpcodeValNotGiven) | 1 | 2 | m[2] template uses "%1% value %1%" — same recon bug; should be "opcode %1% value %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:404 | 1 (OpcodeRegNotGiven) | 1 | 2 | m[1] template uses "%1% register %1%" (highest-index=1) but call passes (opcode, position) — template recon bug: should be "opcode %1% register %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:415 | 1 (OpcodeRegNotGiven) | 1 | 2 | m[1] template uses "%1% register %1%" (highest-index=1) but call passes (opcode, position) — template recon bug: should be "opcode %1% register %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:430 | 2 (OpcodeValNotGiven) | 1 | 2 | m[2] template uses "%1% value %1%" — same recon bug; should be "opcode %1% value %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:467 | 1 (OpcodeRegNotGiven) | 1 | 2 | m[1] template uses "%1% register %1%" (highest-index=1) but call passes (opcode, position) — template recon bug: should be "opcode %1% register %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:514 | 1 (OpcodeRegNotGiven) | 1 | 2 | m[1] template uses "%1% register %1%" (highest-index=1) but call passes (opcode, position) — template recon bug: should be "opcode %1% register %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:524 | 2 (OpcodeValNotGiven) | 1 | 2 | m[2] template uses "%1% value %1%" — same recon bug; should be "opcode %1% value %2% not given". |
| src/codegen/awg_assembler_opcodes.cpp:578 | 1 (OpcodeRegNotGiven) | 1 | 2 | m[1] template uses "%1% register %1%" (highest-index=1) but call passes (opcode, position) — template recon bug: should be "opcode %1% register %2% not given". |
| src/asm/asm_commands.cpp:452 | 32 (ConditionalNeedVarConst) | 0 | 2 | WRONG MESSAGE ID: call passes (double, 32). m[32]="tried to modify const value" (0 slots). The intended id is almost certainly ValueOutOfRange (=5, "value %1% is out of range for %2% bits"). 0x20 vs 0x05 swap; likely confusion between "32 bits" literal and the enum value. |
| src/asm/asm_commands.cpp:458 | 32 (ConditionalNeedVarConst) | 0 | 2 | WRONG MESSAGE ID: call passes (double, 32). m[32]="tried to modify const value" (0 slots). The intended id is almost certainly ValueOutOfRange (=5, "value %1% is out of range for %2% bits"). 0x20 vs 0x05 swap; likely confusion between "32 bits" literal and the enum value. |

### `get()` with placeholders — leaks raw `%K%` to user message (MEDIUM severity)

| File:Line | ID (name) | Slots | Note |
|---|---|---|---|
| src/runtime/custom_functions_wait.cpp:761 | 222 (NotSupportedGrouping) | 2 | m[222]="%1% is not supported in %2% channel grouping mode" — call should be `format(NotSupportedGrouping, "waitSineOscPhase", numChannelGroups)`. |
| src/codegen/awg_assembler_opcodes.cpp:124 | 4 (TooFewArguments) | 4 | m[4] = TooFewArguments, 4 slots. Both sites use `get(4)` which leaks `%1%..%4%` raw to the user (root cause of IF-175 stress-phase failure). Should be `format(TooFewArguments, instr, opcode, expected, given)`. |
| src/codegen/awg_assembler_opcodes.cpp:333 | 4 (TooFewArguments) | 4 | m[4] = TooFewArguments, 4 slots. Both sites use `get(4)` which leaks `%1%..%4%` raw to the user (root cause of IF-175 stress-phase failure). Should be `format(TooFewArguments, instr, opcode, expected, given)`. |

## IF cross-reference

- **IF-179** (markerValue / mask cap warning) — explained by:
  - `src/waveform/waveform_generator.cpp:613`  `format(ValueCapped, markerValue, markerValue & 3)` (slots=3, args=2). Missing arg is `funcName` (i.e. `"marker"` or whatever the caller resolved).
- **IF-178** (`setuserreg_oor`, `giant_expression` — `too_many_args` leak) — explained by:
  - **All 18 overflow rows in `custom_functions_registers.cpp` and `custom_functions_playback.cpp`** of the form `format(SetXxxArgs, std::string("setXxx"))` against templates m[121,122,184,188,191,192,193,194,199,207,208,220,221] which the recon stored as 0-slot constants. **Root cause is in the templates, not the call sites:** these messages are user-visible verbatim in the binary's output (`"setRate expects a const argument"` etc.), so the templates have **no** placeholder — but the recon callers pass a funcName anyway. Two consistent fixes are possible: (a) drop the `std::string("setXxx")` from every call site (the binary clearly does this — its message has no `%1%`); or (b) edit the templates to begin with `%1% ` and keep the call sites. **(a) matches the binary verbatim.**
  - Also explained: 6 underflow rows in `custom_functions_registers.cpp` (`FuncExpectsConst` / `FuncExpectsMaxArgs` / `FuncMinArgs`) where the recon dropped *too many* args off the call. These would throw `too_few_args` not `too_many_args`, so they belong to a sibling failure mode but appear in the same files.
- **IF-175** (`long_source.seqc` — raw `%1% %2% %3% %4%` leaked) — explained by:
  - `src/codegen/awg_assembler_opcodes.cpp:124` and `:333`  `errorMessage(ErrorMessages::get(4))`. m[4] = `TooFewArguments` has 4 slots; using `get()` instead of `format(...)` leaks the raw template.
  - These are the only two `get(N)` sites with a multi-slot template and exactly match the leaked text quoted in IF-175.

## New IFs surfaced by this audit (recommend filing)

The following are not covered by IF-175/178/179 but are real bugs of the same family:

1. **m[1] / m[2] template reconstruction error** (awg_assembler_opcodes.cpp lines 231,241,274,283,292,301,367,379,390,404,415,430,467,514,524,578 — 16 sites). Templates `"opcode %1% register %1% not given"` / `"opcode %1% value %1% not given"` use `%1%` twice instead of `%1%`/`%2%`. All call sites pass 2 args → `too_many_args` at runtime. Fix: correct the templates in `core/error_messages.cpp`. Severity: HIGH (every assembler register/value error throws boost exception instead of producing diagnostic).

2. **m[32] wrong-id at asm_commands.cpp:452, 458** (2 sites). `toInt32()` overflow path passes `format(ErrorMessageT(0x20), d, 32)` but m[32]="tried to modify const value" (0 slots). Almost certainly should be id=5 (`ValueOutOfRange` = `"value %1% is out of range for %2% bits"`). Severity: HIGH (every Value→int32 overflow throws boost too_many_args + reports a nonsensical error). Possible 0x20 vs 0x05 transcription error.

3. **m[222] get-leak at custom_functions_wait.cpp:761** (1 site). `waitSineOscPhase` channel-grouping check uses `get()` against a 2-slot template, leaking `%1% %2%` to the user. Fix: change to `format(NotSupportedGrouping, "waitSineOscPhase", numChannelGroups)`.

4. **m[70] FuncInvalidArgType at custom_functions_play.cpp:2312** (1 site). printF type-error path passes 1 arg against a 4-slot template (`"invalid argument type %1% in function '%2%'; expects argument '%3%' to be %4%"`). Will throw `too_few_args` whenever printF receives an unsupported-type arg. Severity: MEDIUM (rare path).

5. **m[128] NodePrecisionLoss at custom_functions_play.cpp:1617, 1646, 2069** (3 sites). Template has 2 slots (`"node '%1%' requires an %2% value, ..."`); call passes 1 (`valStr`). Missing arg is the node-name string. Severity: HIGH (any non-integer setInt warning throws).

6. **m[215] IndexMustBe at custom_functions_play.cpp:496** (1 site). 2-slot template, 1 arg passed — missing the index-range description.

7. **m[169] FormatFuncArgs at custom_functions_playback.cpp:861** (1 site, `randomSeed`). 3-slot template, 1 arg passed.

## Summary

- **Templates:** 305 entries (254 SeqC + 51 API/firmware).
- **Total call sites:** 545 (528 `format(...)`, 17 `get(...)`).
- **Underflow:** 22 sites (will throw `boost::io::too_few_args`).
- **Overflow:** 38 sites (will throw `boost::io::too_many_args`).
- **`get()` placeholder leaks:** 3 sites (will print raw `%K%` tokens).

### Fix-batching for parallel subagents (group by file)

| File | under | over | leak | Notes |
|---|---|---|---|---|
| `src/runtime/custom_functions_registers.cpp` | 12 | 18 | 0 | Largest. The 18 overflows + 6 of the 12 underflows are all "drop or keep funcName" issues against templates m[61–222]. **Recommend single subagent**: first decide whether to fix templates or call sites for the SetXxxArgs family (matching binary behaviour favours dropping the funcName arg from the call), then sweep all the FuncExpectsConst/FuncExpectsMaxArgs/FuncMinArgs sites. |
| `src/runtime/custom_functions_play.cpp` | 8 | 0 | 0 | Mostly `printF` (m[70/136/166/168]) and `setInt`-precision (m[128]) and IndexMustBe (m[215]). One subagent. |
| `src/runtime/custom_functions_playback.cpp` | 1 | 2 | 0 | m[169], m[191], m[192]. Group with `_registers.cpp` if you want all "playback/registers" in one subagent. |
| `src/runtime/custom_functions_wait.cpp` | 0 | 0 | 1 | Single get→format change. Trivial; can be merged with another agent. |
| `src/waveform/waveform_generator.cpp` | 1 | 0 | 0 | IF-179 root cause; single line fix. |
| `src/codegen/awg_assembler_opcodes.cpp` | 0 | 16 | 2 | All 18 sites trace back to templates m[1], m[2], m[4]. **The template fix is in `core/error_messages.cpp`, not in this file.** Once m[1]/m[2] are corrected to `%1% .. %2%` and the `get(4)` sites change to `format(4, instr, opcode, expected, given)`, all 18 are resolved. |
| `src/asm/asm_commands.cpp` | 0 | 2 | 0 | m[32] wrong-id; verify with GDB which message id the binary actually uses (likely 5, ValueOutOfRange). |

**Templates that themselves need fixing in `core/error_messages.cpp`:**
- m[1] — `"opcode %1% register %1% not given"` → `"opcode %1% register %2% not given"`
- m[2] — `"opcode %1% value %1% not given"` → `"opcode %1% value %2% not given"`
- m[1] / m[2] need GDB confirmation against the binary's rodata.
