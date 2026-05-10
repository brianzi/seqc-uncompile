// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// zhinst::WaveformGenerator — waveform DSP function registry
//
// Class size: 0xC0 (192 bytes)
//   Confirmed from __shared_ptr_emplace dealloc size 0xE0 - 0x20 header = 0xC0
//
// Ctor: 0x248200  WaveformGenerator(shared_ptr<WavetableFront>, function<void(string const&)> const&)
// Dtor: 0x127840
//
// The constructor registers 33 waveform generation functions (zeros, ones,
// sin, cos, sinc, ramp, sawtooth, triangle, gauss, drag, blackman, hamming,
// hann, rect, chirp, mask, marker, rand, randomGauss, randomUniform,
// lfsrGaloisMarker, rrc, vect, placeholder, join, add, interleave, scale,
// multiply, cut, flip, filter, circshift) into funcMap_.  merge() and grow()
// also exist as compiled methods but are NOT registered in funcMap_ — they
// are reachable only via internal recon dispatch (custom_functions_play.cpp).
// See IF-202.
//
// Each function has signature: Signal (vector<Value> const&)
// They are bound as member function pointers via std::bind.
// ============================================================================
#pragma once

#include "zhinst/waveform/signal.hpp"
#include "zhinst/ast/value.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace zhinst {

// Forward declarations
class WavetableFront;
class WaveformFront;
class EvalResults;
class CancelCallback;

// ============================================================================
// Exception classes
// ============================================================================

// WaveformGeneratorException — base exception (0x20 bytes)
//   vptr(+0x00) + string message_(+0x08)
//   Inherits from std::exception
//   vtable @0xb06710
//! Generic failure raised by `WaveformGenerator` for non-argument
//! errors (lookup miss, internal invariant violation, etc.).
class WaveformGeneratorException : public std::exception {
public:
    explicit WaveformGeneratorException(std::string const& msg);       // 0x25ca00
    ~WaveformGeneratorException() override;                            // 0x25ca60
    //! \brief Return the stored message, or the canonical literal
    //!        `"WaveformGenerator Exception"` when the message is
    //!        empty.
    //!
    //! \details Inspects `message_` and returns
    //! `message_.c_str()` when non-empty; otherwise the static
    //! fallback string is returned so callers always see a
    //! human-readable diagnostic.
    //!
    //! \return  Null-terminated diagnostic string owned by the
    //!          exception.
    const char* what() const noexcept override;                        // 0x261820

private:
    std::string message_;  // +0x08
};

// WaveformGeneratorValueException — value-related exception (0x28 bytes)
//   vptr(+0x00) + string message_(+0x08) + size_t argIndex_(+0x20)
//   Inherits from std::exception
//   vtable @0xb066d0
//! Failure raised by `WaveformGenerator` when one of the
//! `Value` arguments to a builtin waveform function is missing or
//! has the wrong type / range. Carries the offending argument index
//! so callers can attribute the error to a specific source position.
class WaveformGeneratorValueException : public std::exception {
public:
    WaveformGeneratorValueException(std::string const& msg, size_t argIndex);  // 0x25c4a0
    ~WaveformGeneratorValueException() override;                               // 0x25c500
    //! \brief Return the stored message, or the canonical literal
    //!        `"WaveformGenerator Value Exception"` when the
    //!        message is empty.
    //!
    //! \details Same fallback policy as
    //! `WaveformGeneratorException::what()`: returns
    //! `message_.c_str()` when non-empty, else the static
    //! string.  The `argIndex_` slot is **not** woven into the
    //! returned message — callers that need it must read
    //! `argIndex()` separately.
    //!
    //! \return  Null-terminated diagnostic string owned by the
    //!          exception.
    const char* what() const noexcept override;                                // 0x2617a0

    size_t argIndex() const { return argIndex_; }

private:
    std::string message_;   // +0x08
    size_t      argIndex_;  // +0x20
};

// ============================================================================
// WaveformGenerator layout (0xC0 bytes):
//
//   +0x00  unordered_map<string, function<Signal(vector<Value> const&)>>  funcMap_
//   +0x20  float                                                          field_20  (init 1.0f)
//   +0x24  (4 bytes padding)
//   +0x28  unordered_map<string, string>                                  aliasMap_
//   +0x48  float                                                          field_48  (init 1.0f)
//   +0x4C  (4 bytes padding)
//   +0x50  set<string>                                                    createdNames_
//   +0x68  shared_ptr<WavetableFront>                                     wavetableFront_
//   +0x78  (8 bytes padding/unknown)
//   +0x80  function<void(string const&)>                                  warningCallback_
//   +0xB0  weak_ptr<CancelCallback>                                       cancelCallback_ (set by Compiler::setCancelCallback @0x1234db)
//   +0xC0  END
//
// Layout verified: ctor allocates via `__shared_ptr_emplace<WaveformGenerator>`
// at 0x11d1b0 with `operator new(0xe0)`; subtract the 0x20-byte control
// block prefix → 0xC0 bytes for the WaveformGenerator body itself.
//
// `cancelCallback_`: zero-initialized in the ctor at 0x2482aa via
// `xorps xmm0; movaps [rbx+0xb0], xmm0`. Written by
// `Compiler::setCancelCallback` (binary @0x123480) which copies the
// `weak_ptr<CancelCallback>` argument here in addition to its own
// `cancelCallback_` field.  No reader has been identified yet inside
// `WaveformGenerator` — the slot exists so that a future cooperative-
// cancellation point inside a long-running waveform generator (e.g. a
// large `chirp` or `sinc` allocation) can poll the same callback the
// outer pipeline polls.  See IF-210.
//
// The funcMap_ maps function names (e.g. "zeros", "sin") to bound member
// function pointers.  aliasMap_ maps deprecated names to current names;
// call() checks aliasMap_ first and emits a warning via warningCallback_.
// ============================================================================
//! Registry and dispatcher for the SeqC built-in waveform DSP
//! functions (`zeros`, `sin`, `gauss`, `chirp`, `cut`, `add`, …).
//!
//! Each registered function takes a `vector<Value>` of arguments
//! and produces a `Signal`. `call()` resolves the function name
//! (consulting the deprecated-alias map first and warning the host
//! if a deprecated name was used), runs the generator, and routes
//! the result through `getOrCreateWaveform()` to dedupe identical
//! waveforms across the program. `eval()` wraps the result in an
//! `EvalResults` for use by the SeqC interpreter.
//!
//! Argument readers (`readDouble`, `readInt`, `readWave`, …) are
//! exposed so individual generator implementations validate their
//! inputs uniformly and raise `WaveformGeneratorValueException`
//! with the right argument index on mismatch.
class WaveformGenerator {
public:
    // --- Construction / Destruction ---
    //! \brief Construct the registry, populate `funcMap_` with the
    //!        ~33 user-callable waveform built-ins and `aliasMap_`
    //!        with the deprecated-name table, and seed
    //!        `createdNames_` with the four built-ins whose value
    //!        depends on per-call state.
    //!
    //! \details Member-init copies `wavetableFront` into
    //! `wavetableFront_` and `warningCallback` into
    //! `warningCallback_`; `cancelCallback_` is left empty until
    //! `setCancelCallback` is called.  The two libc++
    //! `unordered_map` `max_load_factor` slots are explicitly
    //! initialised to `1.0f` so the in-memory layout matches the
    //! binary's interleaved float-after-map pattern.  Function
    //! registration uses `std::bind(&WaveformGenerator::foo,
    //! this, _1)` so each `funcMap_` entry receives a stable
    //! `Signal(vector<Value> const&)` callable bound to this
    //! instance.  `createdNames_` is pre-populated with `"rand"`,
    //! `"randomGauss"`, `"randomUniform"`, and `"placeholder"` —
    //! these built-ins return a different result on every call
    //! and must therefore bypass the wavetable's deduplication
    //! cache, which `getOrCreateWaveform` arranges by always
    //! routing through the factory path when a name appears in
    //! `createdNames_`.  The two `aliasMap_` entries
    //! (`"mask"→"marker"`, `"rand"→"randomGauss"`) drive the
    //! deprecation warnings that `call` emits.  `merge` and
    //! `grow` are present as compiled methods but intentionally
    //! omitted from `funcMap_` (IF-202) because they are reached
    //! only via the play-builtin internal dispatch, not from
    //! SeqC source.
    //!
    //! \param wavetableFront    Shared handle to the wavetable
    //!                          that owns the freshly registered
    //!                          waveforms.  Stored in
    //!                          `wavetableFront_`.
    //! \param warningCallback   Invoked from `call` whenever a
    //!                          deprecated alias is used and from
    //!                          parameter readers when a value is
    //!                          clipped.  Stored in
    //!                          `warningCallback_`.
    WaveformGenerator(std::shared_ptr<WavetableFront> wavetableFront,
                      std::function<void(std::string const&)> const& warningCallback);  // 0x248200
    //! \brief Release the registered factories, the alias table,
    //!        the cached-name set, the wavetable handle, and the
    //!        installed callbacks.
    //!
    //! \details Empty body; relies on member destructors firing
    //! in reverse declaration order.  Each `funcMap_` entry
    //! drops the `shared_ptr<WaveformGenerator>` reference held
    //! by its `std::bind` capture, so once the registry is
    //! gone the wavetable handle and callbacks follow naturally.
    ~WaveformGenerator();                                                                // 0x127840

    // --- Public API ---
    //! \brief Test whether a given built-in name is currently
    //!        registered in `funcMap_`.
    //!
    //! \details \unclear  Declared in the header but the
    //! implementation is currently a stub at binary address
    //! `0x25bc60`; the recon source has no body.  Callers in
    //! `Resources::functionExists` and the SeqC AST evaluator
    //! consult `funcMap_.count(name) != 0` directly through
    //! their own copies, so the dedicated entry point is
    //! reachable but unreconstructed for now.  Once the binary
    //! is disassembled, the body is expected to delegate to
    //! `funcMap_.find(name) != funcMap_.end()`.
    //!
    //! \param name  Built-in name to test.
    //! \return  `true` when `funcMap_` holds an entry for
    //!          `name`; `false` otherwise.
    bool functionExists(std::string const& name) const;                                  // 0x25bc60

    /*! \brief Install the cancellation hook that long-running waveform
     *  generators may consult.
     *
     *  \details Stores `cb` into `cancelCallback_` (offset `+0xB0`).
     *  Currently no `WaveformGenerator` member function actually polls
     *  this slot — the writer is `Compiler::setCancelCallback`, which
     *  propagates the same `weak_ptr` it stores into its own
     *  `cancelCallback_`.  The slot exists so that a future
     *  cooperative-cancellation point inside an expensive waveform
     *  builder (e.g. `chirp`, `sinc`, large `cut`) can poll the
     *  callback without re-routing through `Compiler`.
     *
     *  \param cb  Weak handle to the user-installed cancel hook.  May
     *             be empty to detach.
     */
    void setCancelCallback(std::weak_ptr<CancelCallback> cb) {
        cancelCallback_ = std::move(cb);
    }

    // getOrCreateWaveform — caches waveforms by name; calls factory if not present.
    // If `name` is already in createdNames_ (+0x50 set), look up via
    // wavetableFront_->getWaveformByFunDescr() and bump its use-count
    // (WaveformFront field at +0xd8). Otherwise, invoke factory(args)→Signal,
    // then call wavetableFront_->newWaveform(signal, name, args) which creates
    // and registers a new WaveformFront.
    // Returns shared_ptr<WaveformFront>.  @0x25bca0
    /*! \brief Return the `WaveformFront` for a named built-in
     *  waveform, materialising it through `factory` on first use
     *  and serving subsequent calls from the per-generator cache.
     *
     *  \details On a cache hit (`name` is already in
     *  `createdNames_`) the existing `WaveformFront` is fetched
     *  via `wavetableFront_->getWaveformByFunDescr` and its
     *  use-count is bumped so the wavetable knows to keep it
     *  alive.  On a miss, `factory(args)` is invoked to produce
     *  the raw `Signal`, then `wavetableFront_->newWaveform`
     *  registers a fresh `WaveformFront` keyed by `(name, args)`
     *  and the result is stored in `createdNames_` so the next
     *  call hits the cache.
     *
     *  \param name     Built-in name (`"zeros"`, `"sin"`, …) that
     *                  also serves as the cache key.
     *  \param args     Argument vector forwarded to `factory` and
     *                  used as part of the cache key.
     *  \param factory  Callable that produces the underlying
     *                  `Signal` when this is the first reference.
     *                  Typically one of the bound `Signal foo(args)`
     *                  members below, supplied by `call` after
     *                  `funcMap_` resolution.
     *  \return         The cached or newly-registered
     *                  `WaveformFront`.
     */
    std::shared_ptr<WaveformFront> getOrCreateWaveform(
        std::string const& name,
        std::vector<Value> const& args,
        std::function<Signal(std::vector<Value> const&)> factory);

    // createDummyWaveform — builds vector<Value>{Value(int=length)} and calls
    // call("zeros", args). Returns the resulting WaveformFront.  @0x25be70
    //! \brief Allocate a length-sample all-zero waveform via the
    //!        registered `"zeros"` built-in and mark it as used
    //!        in the wavetable.
    //!
    //! \details Builds a one-element `vector<Value>` carrying
    //! the length as an integer `Value`, dispatches through
    //! `call("zeros", args)`, then re-fetches the resulting
    //! `WaveformFront` by name from the wavetable so its `used`
    //! flag can be set to `true`.  The `used` mark prevents the
    //! wavetable from collecting the dummy during later
    //! optimisation passes that prune unreferenced entries.
    //! Used by the SeqC frontend when a `wave w[N]` declaration
    //! needs a placeholder waveform before its real definition
    //! is encountered.
    //!
    //! \param length  Number of zero-valued samples.  Forwarded
    //!                directly to the `"zeros"` factory; values
    //!                less than 1 propagate the factory's own
    //!                `WaveformGeneratorException`.
    //! \return        The newly-registered (or cached)
    //!                `WaveformFront` with its `used` flag set.
    std::shared_ptr<WaveformFront> createDummyWaveform(int length);

    // call — alias-resolved funcMap_ dispatch.
    // 1. Look up `name` in aliasMap_ (+0x28). If present:
    //      * format an ErrorMessages::format(0x37, name, alias) deprecation
    //        message and invoke warningCallback_ on it.
    //      * Use the aliased name for the funcMap_ lookup.
    //    Otherwise use `name` directly.
    // 2. Look up the resolved name in funcMap_. If absent, throw
    //    WaveformGeneratorValueException(format(0xd8, name), 0).
    // 3. Otherwise clone the bound function and pass it as the factory to
    //    getOrCreateWaveform.
    // Returns shared_ptr<WaveformFront>.  @0x25c120
    /*! \brief Resolve a built-in waveform name (warning the host
     *  when a deprecated alias was used) and return its
     *  `WaveformFront`, materialising it on first use.
     *
     *  \details Two-step resolution that mirrors the binary:
     *    1. Look `name` up in `aliasMap_`.  When the name is a
     *       deprecated alias, format the `DeprecatedFunc`
     *       warning through `warningCallback_` (or raise
     *       `std::bad_function_call` when no callback is
     *       installed).  The alias **does not** substitute the
     *       canonical name — the lookup that follows still uses
     *       the original `name`.
     *    2. Look the original `name` up in `funcMap_`.  Both
     *       deprecated names registered today (`"mask"`,
     *       `"rand"`) have their own `funcMap_` entries
     *       installed by the constructor, so dispatch goes to
     *       the deprecated implementation while the user is
     *       warned to migrate.  When `name` has no `funcMap_`
     *       entry at all, raise
     *       `WaveformGeneratorValueException` formatted with
     *       `UnknownFunction`.
     *    3. Copy the bound `Signal foo(args)` and forward it as
     *       the `factory` argument to `getOrCreateWaveform(name,
     *       args, factory)`, which handles cache lookup or
     *       fresh registration.
     *
     *  \param name  Built-in or alias name as it appears in
     *               SeqC source.
     *  \param args  Already-evaluated `Value` arguments.
     *  \return      The cached or newly-registered
     *               `WaveformFront`.
     *  \throws WaveformGeneratorValueException  When `name` has
     *               no `funcMap_` entry.
     */
    std::shared_ptr<WaveformFront> call(std::string const& name,
                                        std::vector<Value> const& args);

    // eval — call() the named function, then wrap the resulting WaveformFront
    // in a freshly-constructed EvalResults.
    // The EvalResults gets a string-typed Value containing the function name
    // assigned to VarType=5 (eWave?), and the WaveformFront stored at +0x60.
    // Returns shared_ptr<EvalResults>.  @0x25c540
    /*! \brief Resolve and invoke a built-in waveform generator,
     *  returning the result wrapped in an `EvalResults` that the
     *  SeqC interpreter can splice into a surrounding expression.
     *
     *  \details Forwards to `call(name, args)` to obtain the
     *  underlying `WaveformFront`, then constructs a fresh
     *  `EvalResults` whose value slot stores the function name
     *  (string-typed `Value`, `VarType` 5 — the wave variant)
     *  and whose secondary waveform slot holds the
     *  `WaveformFront` itself.  This is the entry point used by
     *  the lowering frontend when a SeqC expression of the form
     *  `wave w = sin(...);` needs a value object that both
     *  carries the waveform and survives further AST evaluation.
     *
     *  \param name  Built-in name (subject to `call`'s alias
     *               resolution).
     *  \param args  Already-evaluated `Value` arguments.
     *  \return      `EvalResults` carrying the resolved
     *               `WaveformFront`.
     *  \throws WaveformGeneratorValueException  Propagated from
     *               `call` when `name` is unknown.
     */
    std::shared_ptr<EvalResults> eval(std::string const& name,
                                      std::vector<Value> const& args);

    // --- Parameter readers (throw WaveformGeneratorValueException on error) ---
    //! \brief Coerce a `Value` argument to `double`, rejecting
    //!        string-typed values with a formatted error
    //!        attributed to the named parameter and function.
    //!
    //! \details Returns `val.toDouble()` directly when `val` is
    //! not a string.  When `val.type_ == ValueType::String`,
    //! raises `WaveformGeneratorValueException` formatted with
    //! the `ArgMustBeConst` template (parameter and function
    //! names interpolated) and an argument-index of `0` —
    //! `readDouble` does not carry the index because every
    //! caller passes parameter strings that already encode the
    //! position (e.g. `"1 (length)"`).
    //!
    //! \param val        Argument as evaluated by the caller.
    //! \param paramName  Human-readable parameter slot used in
    //!                   the error message.
    //! \param funcName   Built-in name used in the error
    //!                   message.
    //! \return  The argument coerced to `double`.
    //! \throws WaveformGeneratorValueException  When `val` holds
    //!                   a string.
    double readDouble(Value val, std::string const& paramName,
                      std::string const& funcName);                                       // 0x25c6f0
    //! \brief Coerce a `Value` to `double` and warn the host
    //!        when its absolute value exceeds 1.0, returning
    //!        the unclipped value either way.
    //!
    //! \details Forwards to `readDouble`, then compares
    //! `std::fabs(result)` against `1.0`.  When the bound is
    //! exceeded and `warningCallback_` is set, the
    //! `AmplitudeClipped` template is formatted with
    //! `funcName` and dispatched to the host.  No actual
    //! clipping happens at the `WaveformGenerator` layer — the
    //! warning is informational; downstream code may saturate
    //! during DAC encoding.
    //!
    //! \param val        Argument as evaluated by the caller.
    //! \param paramName  Parameter slot for the inner
    //!                   `readDouble` error path.
    //! \param funcName   Built-in name used in both the inner
    //!                   error and the amplitude warning.
    //! \return  The argument coerced to `double`, unmodified.
    //! \throws WaveformGeneratorValueException  Propagated from
    //!                   `readDouble`.
    double readDoubleAmplitude(Value val, std::string const& paramName,
                               std::string const& funcName);                              // 0x25caa0
    //! \brief Coerce a `Value` argument to `int`, with explicit
    //!        rejection of `Unspecified` ("var") and `String`
    //!        types.
    //!
    //! \details Three cases:
    //!  - `val.type_ == ValueType::Unspecified` (a SeqC `var`
    //!    binding that was never assigned) raises
    //!    `WaveformGeneratorValueException` formatted with
    //!    `CantCallWithVar` carrying `argIndex` (IF-177).  This
    //!    distinguishes the case from the generic "cannot
    //!    convert" message that `Value::toInt` would otherwise
    //!    throw.
    //!  - `val.type_ == ValueType::String` raises
    //!    `WaveformGeneratorValueException` with `ArgMustBeConst`
    //!    and an argument-index of `0`.
    //!  - Any other type returns `val.toInt()` directly.
    //!
    //! \param val        Argument as evaluated by the caller.
    //! \param paramName  Parameter slot used in the
    //!                   `ArgMustBeConst` error message.
    //! \param argIndex   Position carried into the
    //!                   `CantCallWithVar` exception so the host
    //!                   can attribute the error to the right
    //!                   source position.
    //! \param funcName   Built-in name used in both error
    //!                   messages.
    //! \return  The argument coerced to `int`.
    //! \throws WaveformGeneratorValueException  When `val` is
    //!                   `Unspecified` or `String`.
    int    readInt(Value val, std::string const& paramName, int argIndex,
                   std::string const& funcName);                                          // 0x25cca0
    //! \brief Coerce a `Value` to `int` and additionally
    //!        require the result to be non-negative.
    //!
    //! \details Calls `readInt(val, paramName, argIndex,
    //! funcName)` first, then enforces `result >= 0`.  A
    //! negative result raises
    //! `WaveformGeneratorValueException` formatted with the
    //! `ArgOverflow` template — parameter name, function name,
    //! and the literal qualifier `"positive"` are interpolated
    //! — carrying `argIndex`.
    //!
    //! \param val        Argument as evaluated by the caller.
    //! \param paramName  Parameter slot used in both error
    //!                   messages.
    //! \param argIndex   Position carried into the overflow
    //!                   exception.
    //! \param funcName   Built-in name used in both error
    //!                   messages.
    //! \return  The argument coerced to `int`, guaranteed
    //!          non-negative.
    //! \throws WaveformGeneratorValueException  Propagated from
    //!                   `readInt`, or raised when the result is
    //!                   negative.
    int    readPositiveInt(Value val, std::string const& paramName, int argIndex,
                           std::string const& funcName);                                  // 0x25d490
    //! \brief Resolve a `Value` referring to a previously
    //!        defined waveform name into a fully-loaded
    //!        `WaveformFront`.
    //!
    //! \details Requires `val.type_ == 4` (the wave variant).
    //! On a type mismatch raises
    //! `WaveformGeneratorValueException` formatted with
    //! `ArgMustBeString` and `argIndex = expectedLength`.  When
    //! the type matches, fetches the waveform name via
    //! `val.toString()`, queries
    //! `wavetableFront_->waveformExists(name)` and raises
    //! `WaveformGeneratorException` formatted with
    //! `UnknownWaveform` on a miss.  On a hit, calls
    //! `getWaveformByName` to obtain the `WaveformFront` and
    //! `loadWaveform` to make sure its sample data is
    //! materialised before returning the handle.
    //!
    //! \param val             Wave-typed argument from the
    //!                        caller.
    //! \param paramName       Parameter slot used in the
    //!                        type-mismatch error.
    //! \param expectedLength  Doubles as the `argIndex` carried
    //!                        in the type-mismatch exception so
    //!                        the host can report which
    //!                        argument was wrong.
    //! \param funcName        Built-in name used in both error
    //!                        messages.
    //! \return  A `WaveformFront` with its sample data loaded.
    //! \throws WaveformGeneratorValueException  When `val` is
    //!                        not a wave reference.
    //! \throws WaveformGeneratorException  When the wavetable
    //!                        does not contain a waveform with
    //!                        the requested name.
    std::shared_ptr<WaveformFront> readWave(Value val, std::string const& paramName,
                    int expectedLength, std::string const& funcName);                     // 0x25d6f0

    // --- Helper methods ---
    //! \brief Synthesise a piecewise-linear triangle waveform
    //!        with configurable rise / fall ratio and phase.
    //!
    //! \details Allocates a `Signal` of `length` samples and
    //! returns it empty when `length == 0`.  Otherwise computes
    //! `samplesPerPeriod = length / nPeriods`, splits each
    //! period into a half-rise of length
    //! `riseRatio * samplesPerPeriod * 0.5`, a fall of length
    //! `(1 - riseRatio) * samplesPerPeriod`, and another
    //! half-rise of the same length.  For each output sample
    //! `i`, evaluates `t = fmod(i + phaseOffset,
    //! samplesPerPeriod)` (with `phaseOffset = phase / (2π) *
    //! samplesPerPeriod`) and dispatches into the correct
    //! linear segment: the first rise from `0` to
    //! `+amplitude`, the fall from `+amplitude` to
    //! `-amplitude`, and the trailing rise from `-amplitude`
    //! back to `0`.  Each computed sample is appended to the
    //! `Signal` with a marker byte of `0`.
    //!
    //! \param length     Number of output samples.
    //! \param amplitude  Peak amplitude (rises to `+amplitude`,
    //!                   falls to `-amplitude`).
    //! \param nPeriods   Number of full triangle periods within
    //!                   `length` samples.
    //! \param riseRatio  Fraction of the period (`0..1`) spent
    //!                   on the two combined rising segments.
    //! \param phase      Phase offset in radians.
    //! \return  The synthesised triangle `Signal`.
    Signal genericTriangle(int length, double amplitude, double nPeriods,
                           double riseRatio, double phase);                                  // 0x25e0c0
    //! \brief Shared implementation for the user-callable
    //!        `marker(...)` and the deprecated `mask(...)`
    //!        built-ins.
    //!
    //! \details Validates `args.size() == 2` and reports the
    //! standard `FuncExactArgs2` error on mismatch.  Reads
    //! `length` (positive int) from `args[0]` and `markerValue`
    //! (positive int) from `args[1]`.  When `markerValue >= 4`,
    //! invokes the warning callback with the `ValueCapped`
    //! template — `markerValue & 3` becomes the effective
    //! marker — and returns a `Signal` of `length` samples with
    //! all amplitudes set to `0.0` and the marker byte set to
    //! the (possibly truncated) value, on a single channel.
    //! `isMask` selects the parameter-name strings used in
    //! error / warning messages: `"mask"` /
    //! `"1 (length)"` / `"2 (mask)"` versus `"marker"` /
    //! `"1 (samples)"` / `"2 (markerValue)"`.
    //!
    //! \param args    Two-element argument vector.
    //! \param isMask  `true` to use the deprecated `mask`
    //!                wording in errors and warnings, `false`
    //!                for the canonical `marker` wording.
    //! \return  A length-sample, single-channel `Signal` whose
    //!          amplitudes are zero and whose marker byte is
    //!          `markerValue & 3`.
    //! \throws WaveformGeneratorException  When `args.size() != 2`.
    //! \throws WaveformGeneratorValueException  Propagated from
    //!                                          `readPositiveInt`.
    Signal markerImpl(std::vector<Value> const& args, bool isMask);                       // 0x25e230
    //! \brief Build an interleaved multi-channel `Signal` whose
    //!        samples are linearly interpolated between
    //!        per-channel start and end values, with combined
    //!        marker bits applied uniformly across all output
    //!        time steps.
    //!
    //! \details Allocates a zero-initialised
    //! `MarkerBitsPerChannel` of `markers.size()` bytes (the
    //! `markerBits` parameter is not used to size or pre-fill
    //! the per-channel marker array — only to OR into each
    //! per-step marker byte) and constructs the output
    //! `Signal(length, mbpc)`.  Returns immediately when
    //! `length == 0`.  Otherwise iterates `seg = 1..length`
    //! and for each channel `n` appends
    //! `xPoints[n] + (yPoints[n] - xPoints[n]) * seg / length`
    //! with marker `markerBits[n] | markers[n]`, producing
    //! `length * xPoints.size()` interleaved samples.
    //!
    //! \param length      Number of output time steps per
    //!                    channel.
    //! \param xPoints     Per-channel start amplitudes.
    //! \param yPoints     Per-channel end amplitudes.  Must have
    //!                    the same size as `xPoints`.
    //! \param markers     Per-channel marker bytes that are ORed
    //!                    into every emitted sample's marker.
    //! \param markerBits  Per-channel marker bytes ORed
    //!                    alongside `markers`; the names reflect
    //!                    that one source typically carries the
    //!                    user-requested bit pattern and the
    //!                    other the channel-specific marker
    //!                    overlay.
    //! \return  The interleaved `Signal`.
    Signal interpolateLinear(int length,
                             std::vector<double> const& xPoints,
                             std::vector<double> const& yPoints,
                             std::vector<uint8_t> const& markers,
                             std::vector<uint8_t> const& markerBits);                     // 0x25f410

    //! \brief Return a `Signal` whose samples (and per-sample
    //!        markers) are reversed from `sig`, preserving
    //!        channel interleaving for multi-channel inputs.
    //!
    //! \details `sig.reserveOnly_` short-circuits to a plain
    //! copy of the input.  For single-channel signals,
    //! materialises the data via `checkAllocation`, copies
    //! samples and markers into local vectors, applies
    //! `std::reverse` to each, and constructs the result
    //! `Signal`.  For multi-channel signals, performs a
    //! block-reverse: each block of `channels` interleaved
    //! samples is treated as a unit, with marker bytes
    //! reversed in matching block sizes.  Markers carry along
    //! with their owning sample block.
    //!
    //! \param sig  Source signal.  Mutated only via
    //!             `checkAllocation` (which lazily fills its
    //!             sample buffer) — the sample data itself is
    //!             not modified.
    //! \return  A new `Signal` whose `samples_` is the reverse
    //!          of `sig.samples_` (block-wise for multi-channel)
    //!          and whose `markers_` follows the same
    //!          permutation.
    Signal reverse(Signal& sig);                                                          // 0x260f20

    // --- Waveform generation functions ---
    // Each takes vector<Value> const& and returns Signal.
    // Registered in funcMap_ by the constructor.

    //! \brief Constant-zero waveform of the given length.
    //! \details One-argument factory: `zeros(length)` returns a
    //!          single-channel `Signal` of `length` samples, every
    //!          sample equal to 0.0.  No marker bits are set.
    //! \param args  Single argument: integer `length` >= 1.
    //! \return  Signal of `length` zero-valued samples.
    //! \throws WaveformGeneratorException  on wrong argument count
    //!         or non-positive `length`.
    Signal zeros(std::vector<Value> const& args);            // 0x249b90

    //! \brief Constant-one waveform of the given length.
    //! \details One-argument factory: `ones(length)` returns a
    //!          single-channel `Signal` of `length` samples, every
    //!          sample equal to 1.0.
    //! \param args  Single argument: integer `length` >= 1.
    //! \return  Signal of `length` samples, all set to 1.0.
    //! \throws WaveformGeneratorException  on wrong argument count
    //!         or non-positive `length`.
    Signal ones(std::vector<Value> const& args);             // 0x249e10

    /*! \brief Sinusoidal waveform.
     *
     *  \details Three- or four-argument factory:
     *  - `sin(length, amplitude, phase)`:
     *    constant waveform; every sample equals
     *    `sin(amplitude + phase)`.  This is a degenerate
     *    binary-faithful behaviour, not the typical use.
     *  - `sin(length, amplitude, phase, nPeriods)`:
     *    sample[i] = `amplitude * sin(2*pi*nPeriods*i/length + phase)`.
     *
     *  All read-error messages report the function as `"sine"`.
     *
     *  \param args  3 or 4 arguments.  See per-overload list above.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     *  \throws WaveformGeneratorValueException  if `nPeriods < 0`
     *          (4-arg overload only).
     */
    Signal sin(std::vector<Value> const& args);              // 0x24a0f0

    /*! \brief Cosinusoidal waveform.
     *
     *  \details Identical structure to `sin`, but uses `std::cos`:
     *  - `cos(length, amplitude, phase)`:
     *    constant waveform; every sample equals
     *    `cos(amplitude + phase)`.
     *  - `cos(length, amplitude, phase, nPeriods)`:
     *    sample[i] = `amplitude * cos(2*pi*nPeriods*i/length + phase)`.
     *
     *  All read-error messages report the function as `"cosine"`.
     *
     *  \param args  3 or 4 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     *  \throws WaveformGeneratorValueException  if `nPeriods < 0`
     *          (4-arg overload only).
     */
    Signal cos(std::vector<Value> const& args);              // 0x24abd0

    /*! \brief Sinc-function envelope.
     *
     *  \details Three- or four-argument factory:
     *  - `sinc(length, position, beta)`:
     *    amplitude defaults to 1.0.
     *  - `sinc(length, amplitude, position, beta)`.
     *
     *  Computes, for `i` in `[0, length)`, with `c = i - position`:
     *  - `c == 0`:  sample = `amplitude` (the limit `sinc(0) = 1`).
     *  - otherwise: `x = 2*pi*beta*c/length`,
     *    sample = `amplitude * sin(x) / x`.
     *
     *  Emits a warning via the warning callback if `position > length`.
     *
     *  \param args  3 or 4 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or if `beta == 0`.
     *  \binarynote The 4-argument overload reports its third
     *          parameter in error messages as `"3 (position)"` and
     *          its fourth as `"3 (beta)"` — both literal strings
     *          are arity-blind and do not track the user-visible
     *          argument index.  See IF-230.
     */
    Signal sinc(std::vector<Value> const& args);             // 0x24b6e0

    /*! \brief Linear ramp from `startLevel` to `endLevel`.
     *
     *  \details Three-argument factory: `ramp(length, startLevel, endLevel)`.
     *  Both levels are bounded to `|level| <= 1.0` (else
     *  `WaveformGeneratorValueException`).
     *  - `length >= 2`: sample[i] = `startLevel + i * (endLevel - startLevel) / (length - 1)`.
     *  - `length == 1`: a single sample equal to `endLevel`.
     *
     *  \param args  Three arguments: integer `length` >= 1, two
     *          doubles `startLevel` and `endLevel` each in `[-1, 1]`.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     *  \throws WaveformGeneratorValueException  if either level is
     *          outside `[-1, 1]`.
     */
    Signal ramp(std::vector<Value> const& args);             // 0x24c2c0

    /*! \brief Sawtooth waveform (rising-edge only).
     *
     *  \details Three- or four-argument factory; in both cases
     *  delegates to `genericTriangle` with `riseRatio = 1.0`:
     *  - `sawtooth(length, amplitude, phase)`:
     *    `nPeriods` defaults to 1.0.
     *  - `sawtooth(length, amplitude, phase, nPeriods)`:
     *    explicit period count.
     *
     *  \param args  3 or 4 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     *  \throws WaveformGeneratorValueException  if `nPeriods < 0`.
     */
    Signal sawtooth(std::vector<Value> const& args);         // 0x24c8b0

    /*! \brief Symmetric triangle waveform.
     *
     *  \details Three- or four-argument factory; delegates to
     *  `genericTriangle` with `riseRatio = 0.5`:
     *  - `triangle(length, amplitude, phase)`:
     *    `nPeriods` defaults to 1.0.
     *  - `triangle(length, amplitude, phase, nPeriods)`.
     *
     *  \param args  3 or 4 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     *  \throws WaveformGeneratorValueException  if `nPeriods < 0`.
     */
    Signal triangle(std::vector<Value> const& args);         // 0x24d330

    /*! \brief Gaussian envelope.
     *
     *  \details Three- or four-argument factory:
     *  - `gauss(length, position, width)`:
     *    amplitude defaults to 1.0.
     *  - `gauss(length, amplitude, position, width)`.
     *
     *  Computes sample[i] = `amplitude * exp(-((i - position)^2) / (2 * width^2))`.
     *  If `width == 0` the output is left zero-initialised (matches the
     *  binary's NaN/Inf-producing degenerate case, suppressed here).
     *
     *  \param args  3 or 4 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     */
    Signal gauss(std::vector<Value> const& args);            // 0x24ddb0

    /*! \brief DRAG (derivative-of-Gaussian) pulse.
     *
     *  \details Three- or four-argument factory:
     *  - `drag(length, position, sigma)`:
     *    amplitude defaults to 1.0.
     *  - `drag(length, amplitude, position, sigma)`.
     *
     *  Computes the derivative of a Gaussian, normalised so its peak
     *  equals `amplitude`:
     *  sample[i] = `(amplitude * (i - position) / sigma) * exp(-0.5) * exp(-(i - position)^2 / (2 * sigma^2))`,
     *  expressed in the binary's instruction order via the precomputed
     *  coefficients `exp(-0.5) / sigma` and `amplitude * sigma / exp(-0.5)`.
     *
     *  Emits a warning via the warning callback if `position > length`.
     *
     *  \param args  3 or 4 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or if `sigma == 0`.
     */
    Signal drag(std::vector<Value> const& args);             // 0x24e950

    /*! \brief Generalised Blackman window.
     *
     *  \details Two- or three-argument factory:
     *  - `blackman(length, alpha)`:
     *    amplitude defaults to 1.0.
     *  - `blackman(length, amplitude, alpha)`.
     *
     *  Computes
     *  `w[n] = a * ((1 - alpha) / 2 - 0.5 * cos(2*pi*n / (N - 1))
     *               + (alpha / 2) * cos(4*pi*n / (N - 1)))`,
     *  where `a` is the supplied or default amplitude and `N == length`.
     *  Standard Blackman uses `alpha = 0.16`.
     *
     *  \param args  2 or 3 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     */
    Signal blackman(std::vector<Value> const& args);         // 0x24f530

    /*! \brief Hamming window.
     *
     *  \details One- or two-argument factory:
     *  - `hamming(length)`:
     *    amplitude defaults to 1.0.
     *  - `hamming(length, amplitude)`.
     *
     *  Computes `w[n] = a * (0.54 - 0.46 * cos(2*pi*n / (N - 1)))`.
     *
     *  \param args  1 or 2 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     */
    Signal hamming(std::vector<Value> const& args);          // 0x24fd20

    /*! \brief Hann window.
     *
     *  \details One- or two-argument factory:
     *  - `hann(length)`:
     *    amplitude defaults to 0.5.
     *  - `hann(length, amplitude)`:
     *    the supplied amplitude is internally multiplied by 0.5
     *    before the per-sample loop, matching the binary.
     *
     *  Computes `w[n] = a' * (1 - cos(2*pi*n / (N - 1)))`,
     *  where `a'` is the (possibly halved) amplitude.  The conventional
     *  Hann window factor of 0.5 is therefore present in both
     *  overloads.
     *
     *  \param args  1 or 2 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     */
    Signal hann(std::vector<Value> const& args);             // 0x250250

    /*! \brief Constant-amplitude rectangular waveform.
     *
     *  \details Two-argument factory: `rect(length, amplitude)`.
     *  Returns a single-channel `Signal` of `length` samples, every
     *  sample equal to `amplitude`.  The amplitude is read with
     *  `readDoubleAmplitude`, which warns (but does not clip) when
     *  `|amplitude| > 1`.
     *
     *  \param args  Two arguments: integer `length` >= 1 and
     *          double `amplitude`.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     */
    Signal rect(std::vector<Value> const& args);             // 0x250770
    Signal chirp(std::vector<Value> const& args);            // 0x250bb0

    //! \brief Deprecated alias for `marker`; thin wrapper that
    //!        forwards to `markerImpl(args, isMask=true)`.
    //! \details Registered in `funcMap_` directly (not via
    //!          `aliasMap_`) so the call dispatches here even
    //!          though `aliasMap_` also records `"mask" -> "marker"`
    //!          for the deprecation warning.  The `isMask` flag
    //!          changes the user-visible function name in error
    //!          messages produced by `markerImpl` from `"marker"`
    //!          to `"mask"`.
    //! \param args  Same shape as `marker`.
    //! \return  Same as `marker(args)`.
    Signal mask(std::vector<Value> const& args);             // 0x251cb0

    /*! \brief Marker-only waveform.
     *
     *  \details Thin wrapper that forwards to
     *  `markerImpl(args, isMask=false)`.  The implementation builds
     *  a `Signal` whose sample data is zero and whose marker bits
     *  carry the supplied marker value.  Marker values >= 4 trigger
     *  a `ValueCapped` warning and are masked with `markerValue & 3`.
     *
     *  \param args  See `markerImpl` for the accepted shapes.
     *  \return  Signal whose marker bits encode the requested pattern.
     *  \throws WaveformGeneratorException  on argument validation
     *          failures inside `markerImpl`.
     */
    Signal marker(std::vector<Value> const& args);           // 0x251cd0
    Signal rand(std::vector<Value> const& args);             // 0x251cf0
    Signal randomGauss(std::vector<Value> const& args);      // 0x252930
    Signal randomUniform(std::vector<Value> const& args);    // 0x253440
    Signal lfsrGaloisMarker(std::vector<Value> const& args); // 0x253bc0

    /*! \brief Root-raised-cosine (RRC) filter impulse response.
     *
     *  \details Three-, four-, or five-argument factory:
     *  - `rrc(length, position, beta)`:
     *    amplitude defaults to 1.0, width defaults to 1.0.
     *  - `rrc(length, amplitude, position, beta)`:
     *    width defaults to 1.0.
     *  - `rrc(length, amplitude, position, beta, width)`.
     *
     *  Computes, for `i` in `[0, length)`, with `t = (i - position) * width`
     *  (where `width = 1 / samples_per_symbol`):
     *  - `t == 0`: `h = 1 - beta + 4*beta/pi`.
     *  - `t == ±1/(4*beta)`: closed-form
     *    `h = (beta/sqrt(2)) * ((1+2/pi)*sin(pi/(4*beta)) + (1-2/pi)*cos(pi/(4*beta)))`.
     *  - otherwise: standard RRC formula
     *    `h = (sin((1-beta)*pi*t) + 4*beta*t*cos((1+beta)*pi*t)) / (pi*t*(1 - (4*beta*t)^2))`.
     *
     *  Each output sample is multiplied by `amplitude`.  Emits a
     *  warning via the warning callback if `position > length`.
     *
     *  \param args  3, 4, or 5 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     *  \binarynote The 3-argument overload reports its `position` and
     *          `beta` parameters in error messages as
     *          `"3 (position)"` and `"4 (beta)"` even though the
     *          user passes them in slots 2 and 3 — the literal
     *          strings are arity-blind and do not track the
     *          user-visible argument index.  See IF-230.
     */
    Signal rrc(std::vector<Value> const& args);              // 0x254290

    /*! \brief Build a `Signal` directly from numeric arguments.
     *
     *  \details Variadic factory: each argument is read as a `double`
     *  and appended to the output sample vector in order.  At most
     *  100 elements are allowed; passing 101 or more arguments throws
     *  `WaveformGeneratorException` with the `VectTooManyArgs` error
     *  message.
     *
     *  Per-argument parameter names are constructed as
     *  `"<i+1> (waveform)"` for the `i`th argument (1-based), so an
     *  out-of-range value is reported with its position in the call.
     *
     *  \param args  Up to 100 numeric values (integers, doubles, or
     *          booleans).  Strings are rejected with a `readDouble`
     *          error.
     *  \return  Signal of `args.size()` samples (single channel).
     *          An empty argument list yields an empty `Signal`.
     *  \throws WaveformGeneratorException  if `args.size() >= 101`,
     *          or via `readDouble` for non-numeric arguments.
     */
    Signal vect(std::vector<Value> const& args);             // 0x255570
    Signal placeholder(std::vector<Value> const& args);      // 0x255850
    Signal join(std::vector<Value> const& args);             // 0x255da0
    Signal add(std::vector<Value> const& args);              // 0x256ff0
    Signal interleave(std::vector<Value> const& args);       // 0x258140
    Signal scale(std::vector<Value> const& args);            // 0x258270
    Signal multiply(std::vector<Value> const& args);         // 0x258750
    Signal cut(std::vector<Value> const& args);              // 0x2598d0
    Signal flip(std::vector<Value> const& args);             // 0x25a310
    Signal filter(std::vector<Value> const& args);           // 0x25a540
    Signal circshift(std::vector<Value> const& args);        // 0x25b0e0
    Signal merge(std::vector<Value> const& args);            // 0x25f5c0
    Signal grow(std::vector<Value> const& args);             // 0x260640

private:
    // +0x00: Function registry — maps name to bound member function
    std::unordered_map<std::string,
        std::function<Signal(std::vector<Value> const&)>>    funcMap_;           // +0x00

    // Binary: On libc++, unordered_map is 0x28 bytes (0x20 hash table state +
    // 4-byte max_load_factor float + 4 padding). The floats below are the
    // trailing max_load_factor of the preceding map, not separate fields.
    // We declare them explicitly for layout fidelity with the binary.
    float funcMap_maxLoadFactor_{1.0f};  // +0x20  (libc++ unordered_map internal)

    // +0x28: Alias map — deprecated names → current names (call() warns then redirects)
    std::unordered_map<std::string, std::string>             aliasMap_;          // +0x28

    float aliasMap_maxLoadFactor_{1.0f}; // +0x48  (libc++ unordered_map internal)

    // +0x50: Set of waveform names that have already been generated by call().
    // Used by getOrCreateWaveform() to short-circuit cached lookups.
    std::set<std::string>                                    createdNames_;      // +0x50

    // +0x68: Shared pointer to wavetable front-end
    std::shared_ptr<WavetableFront>                          wavetableFront_;    // +0x68

    // +0x78: 8 bytes — padding between wavetableFront_ and warningCallback_
    uint64_t pad_78_{0};  // +0x78

    // +0x80: Warning callback (invoked for deprecated function aliases)
    std::function<void(std::string const&)>                  warningCallback_;   // +0x80

    // +0xB0: Cancellation hook propagated by Compiler::setCancelCallback.
    // No reader inside WaveformGenerator has been identified yet; the
    // slot exists to let a future cooperative-cancellation point poll
    // the callback without re-routing through Compiler. See IF-210.
    std::weak_ptr<CancelCallback>                            cancelCallback_;    // +0xB0
};

} // namespace zhinst
