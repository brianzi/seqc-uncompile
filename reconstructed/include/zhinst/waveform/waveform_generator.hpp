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
    //! \brief Construct the exception with a pre-formatted
    //!        diagnostic message.
    //!
    //! \details The message is copied into `message_`; `what()`
    //! later returns it verbatim, falling back to a static
    //! literal only when `msg` is empty.  Callers typically
    //! produce `msg` via `ErrorMessages::format(...)` so the
    //! resulting text is already localised and templated.
    //!
    //! \param msg  Pre-formatted diagnostic, owned-by-copy.
    explicit WaveformGeneratorException(std::string const& msg);       // 0x25ca00
    //! \brief Release the embedded `message_` storage.
    //!
    //! \details Empty body; `message_`'s destructor handles
    //! the heap buffer for long-form strings.  Declared so the
    //! vtable slot is emitted in this translation unit, matching
    //! the binary's vtable at `0xb06710`.
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
    //! \brief Construct the exception with a pre-formatted
    //!        message and the offending argument's position.
    //!
    //! \details Copies `msg` into `message_` and stores
    //! `argIndex` into `argIndex_` for later retrieval via
    //! `argIndex()`.  The host catches this exception type at
    //! the SeqC AST evaluator boundary and pairs `argIndex`
    //! with the call-site's argument-list source positions to
    //! attribute the error to a specific token.
    //!
    //! \param msg       Pre-formatted diagnostic (typically
    //!                  produced by `ErrorMessages::format`),
    //!                  owned-by-copy.
    //! \param argIndex  Zero-based position of the offending
    //!                  argument in the original call.
    WaveformGeneratorValueException(std::string const& msg, size_t argIndex);  // 0x25c4a0
    //! \brief Release the embedded `message_` storage.
    //!
    //! \details Empty body; `message_`'s destructor frees any
    //! long-form heap buffer.  Declared so the vtable slot is
    //! emitted here, matching the binary's vtable at `0xb066d0`.
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

    //! \brief Position of the argument whose value caused the
    //!        exception.
    //!
    //! \details Returns the `argIndex` captured at construction
    //! time.  Combined with the SeqC call site's argument-list
    //! source positions, the host can highlight the exact
    //! offending token in user diagnostics.  The accessor is
    //! `inline` and never throws.
    //!
    //! \return  Zero-based position of the offending argument
    //!          in the original call.
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
     *  - `gauss(length, position, sigma)`:
     *    amplitude defaults to 1.0.
     *  - `gauss(length, amplitude, position, sigma)`.
     *
     *  Computes sample[i] = `amplitude * exp(-((i - position)^2) / (2 * sigma^2))`.
     *  If `sigma == 0` the output is left zero-initialised (matches the
     *  binary's NaN/Inf-producing degenerate case, suppressed here).
     *
     *  \param args  3 or 4 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count.
     *  \binarynote The user-visible parameter name in error messages is
     *              `sigma`, not `width`; the recon previously used
     *              `"width"` (see IF-232).
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

    /*! \brief Linear-frequency-sweep (chirp) sinusoid.
     *
     *  \details Three-, four-, or five-argument factory:
     *  - `chirp(length, startFrequency, stopFrequency)`:
     *    amplitude defaults to 1.0, initial phase defaults to 0.
     *  - `chirp(length, startFrequency, stopFrequency, initialPhase)`:
     *    amplitude defaults to 1.0.
     *  - `chirp(length, amplitude, startFrequency, stopFrequency, initialPhase)`.
     *
     *  Computes, for `i` in `[0, length)` with sampling rate baked
     *  into the (already-normalised) `startFrequency` /
     *  `stopFrequency`:
     *  - `freqRate = (stopFrequency - startFrequency) / length`
     *  - `theta = 2*pi*startFrequency*i + pi*freqRate*i*i + initialPhase`
     *  - `sample[i] = amplitude * sin(theta)`
     *
     *  The output is zero-padded with samples of value 0 to a
     *  16-sample alignment boundary (so a request for 70 samples
     *  yields a 80-sample `Signal`).
     *
     *  \param args  3, 4, or 5 arguments.
     *  \return  Signal of `length` samples (16-sample-aligned, single channel).
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or via `read*` for invalid argument types.
     */
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

    /*! \brief Gaussian-noise waveform driven by an inline Park-Miller
     *         MINSTD generator (deterministic per call).
     *
     *  \details Three- or four-argument factory:
     *  - `rand(length, mean, stddev)`: amplitude defaults to 1.0.
     *  - `rand(length, amplitude, mean, stddev)`.
     *
     *  Each output sample is `amplitude * (mean + stddev * z)` where
     *  `z` is a standard normal drawn via the Marsaglia polar
     *  Box-Muller transform from a Park-Miller MINSTD LCG (multiplier
     *  48271, modulus 2^31 - 1).  The PRNG state is **reset to 1 at
     *  the start of every call**, so the same `(length, mean, stddev,
     *  amplitude)` arguments always yield the same samples.  This is
     *  the only `rand`-family factory that does not share the global
     *  `mt19937_64` instance.
     *
     *  \param args  3 or 4 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or via `read*` for invalid argument types.
     *  \binarynote The 3-argument overload is `rand(length, mean,
     *          stddev)` — `amplitude` is the slot that defaults to
     *          1.0, **not** `stddev`.  Mirrors the same convention
     *          used by `randomGauss`.  See IF-205 / IF-231.
     */
    Signal rand(std::vector<Value> const& args);             // 0x251cf0

    /*! \brief Gaussian-noise waveform driven by the shared
     *         `mt19937_64` PRNG.
     *
     *  \details Three- or four-argument factory:
     *  - `randomGauss(length, mean, stddev)`: amplitude defaults to 1.0.
     *  - `randomGauss(length, amplitude, mean, stddev)`.
     *
     *  Each output sample is `amplitude * (mean + stddev * z)` where
     *  `z` is drawn from `std::normal_distribution<double>` over the
     *  thread-local `GlobalResources::random` engine.  Re-seedable via
     *  the `randomSeed` user function; consecutive calls within one
     *  compile session advance the same shared engine.
     *
     *  \param args  3 or 4 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or via `read*` for invalid argument types.
     *  \binarynote The 3-argument overload is `randomGauss(length,
     *          mean, stddev)` — `amplitude` is the slot that defaults
     *          to 1.0, **not** `stddev`.  See IF-205.
     */
    Signal randomGauss(std::vector<Value> const& args);      // 0x252930

    /*! \brief Uniform-noise waveform on `[-amplitude, +amplitude]`.
     *
     *  \details One- or two-argument factory:
     *  - `randomUniform(length)`: amplitude defaults to 1.0.
     *  - `randomUniform(length, amplitude)`.
     *
     *  Each output sample is drawn from
     *  `std::uniform_real_distribution<double>(-amplitude, +amplitude)`
     *  over the thread-local `GlobalResources::random` engine
     *  (shared with `randomGauss`).
     *
     *  \param args  1 or 2 arguments.
     *  \return  Signal of `length` samples (single channel).
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or via `read*` for invalid argument types.
     */
    Signal randomUniform(std::vector<Value> const& args);    // 0x253440

    /*! \brief Pseudo-random marker pattern from a Galois-form LFSR.
     *
     *  \details Four-argument factory
     *  `lfsrGaloisMarker(samples, markerBit, polynomial, initial)`.
     *  Iterates a 32-bit state register `length` times: at each step
     *  the LSB is tested, the state is shifted right by one, and if
     *  the original LSB was set the state is XORed with `polynomial`.
     *  The output `Signal` has all sample values 0; its marker stream
     *  encodes the LFSR output by setting bit `markerBit` (1 or 2)
     *  whenever the original LSB was set.
     *
     *  Argument constraints:
     *  - `samples >= 1`,
     *  - `markerBit` must be 1 or 2 (any other value throws),
     *  - `polynomial >= 1`,
     *  - `initial >= 1` (zero would lock the LFSR; a dedicated
     *    error message is emitted).
     *
     *  \param args  Exactly 4 arguments.
     *  \return  Signal of `samples` zero-valued samples whose marker
     *          stream carries the LFSR sequence.
     *  \throws WaveformGeneratorException  on wrong argument count,
     *          `markerBit` outside `{1, 2}`, or `initial == 0`.
     */
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

    /*! \brief Reservation stub for a yet-to-be-defined waveform.
     *
     *  \details One- to three-argument factory:
     *  - `placeholder(samples)`,
     *  - `placeholder(samples, marker0)`,
     *  - `placeholder(samples, marker0, marker1)`.
     *
     *  Returns a `Signal` constructed in *reserve-only* mode: the
     *  sample buffer is not allocated, only the length and marker
     *  shape are recorded so downstream prefetch / wave-table-IR
     *  passes can lay out the slot.  Each marker argument is read
     *  as an integer that is interpreted as a boolean (zero → marker
     *  bit absent, non-zero → bit set).  The combined marker bits
     *  are stored as a single byte (`markerBits[0] = (marker0 ?
     *  0x1 : 0) | (marker1 ? 0x2 : 0)`).
     *
     *  The marker-bits vector is **always** allocated with one
     *  element even when both markers are absent — downstream
     *  consumers (`Signal::append`, `join`, `merge`) iterate
     *  `markerBits_` unconditionally and would otherwise read OOB.
     *
     *  \param args  1, 2, or 3 arguments.
     *  \return  Signal in reserve-only mode with the requested
     *          length and marker layout.
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or via `read*` for invalid argument types.
     */
    Signal placeholder(std::vector<Value> const& args);      // 0x255850

    /*! \brief Concatenate two or more waveforms end-to-end with
     *         optional linear interpolation between them.
     *
     *  \details Variadic factory accepting one or more waveform
     *  arguments and at most one trailing integer.  Waveform
     *  arguments are appended in order; the optional integer (if
     *  present and `> 0`) requests an interpolation length applied
     *  between consecutive waveforms.
     *
     *  Without an interpolation length the output is the plain
     *  concatenation of every input's samples and markers; the
     *  output's `markerBits_` is the bitwise OR of every input's
     *  marker-bits, and the length is the sum of input lengths.
     *
     *  With an interpolation length `L > 0` and `N` waveforms the
     *  output length becomes `(sum of input lengths) + N * L`.
     *  Between every pair of consecutive waveforms the function
     *  inserts an `L`-frame linear ramp from the previous wave's
     *  last frame to the next wave's first frame (per channel,
     *  marker bytes set to 0).  After the final wave it appends an
     *  `L`-frame zero-pad block.  The channel count is
     *  `max(first.channels_, 1)`.
     *
     *  Reserve-only inputs participate via `Signal::checkAllocation`,
     *  which materialises them as zero-filled regions; the result
     *  is always a fully concrete `Signal`.  See IF-181 for the
     *  GDB verification of that behaviour.
     *
     *  \param args  At least one waveform argument; an optional
     *          trailing non-string argument is consumed as an
     *          interpolation length (frames).
     *  \return  Concatenated `Signal`.
     *  \throws WaveformGeneratorException  if no waveform arguments
     *          are present, or via `readWave` for malformed
     *          waveform references.
     */
    Signal join(std::vector<Value> const& args);             // 0x255da0

    /*! \brief Pointwise sum of two or more waveforms.
     *
     *  \details Variadic factory accepting two or more waveform
     *  arguments.  Reads the first waveform with arbitrary length;
     *  every subsequent waveform must match the first waveform's
     *  length (enforced by `readWave`'s `expectedLength`
     *  parameter).  The output's samples are the elementwise sum
     *  of every input's samples; per-sample markers are bitwise
     *  OR'd across inputs that have a matching `markers_` length;
     *  the output `markerBits_` is the bitwise OR of every
     *  input's marker-bits.
     *
     *  Reserve-only inputs are materialised via `checkAllocation`
     *  (zero-filled) before being summed — there is no early
     *  return for the all-reserve-only case (see IF-188).
     *
     *  \param args  At least two waveform arguments of equal length.
     *  \return  Single-channel `Signal` whose samples are the sum
     *          of every input's samples.
     *  \throws WaveformGeneratorException  on fewer than two
     *          arguments, or via `readWave` for length mismatches.
     */
    Signal add(std::vector<Value> const& args);              // 0x256ff0

    /*! \brief Interleave the channels of several single-channel
     *         waveforms into a single logical channel.
     *
     *  \details Thin wrapper around `merge`: invokes `merge(args)`
     *  to produce an `N`-channel `Signal`, then sets
     *  `channels_ = 1` so downstream consumers treat the
     *  interleaved sample stream as a single channel.  The
     *  output's `markerBits_` is collapsed to a single byte (zero
     *  if `merge` produced none), and `length_` is recomputed as
     *  `samples_.size()` to reflect the interleaved layout.
     *
     *  Equivalent to `merge` for argument-validation purposes —
     *  see `merge` for the accepted argument shapes (including
     *  the optional trailing integer length hint and the
     *  all-reserve-only short-circuit).
     *
     *  \param args  Forwarded to `merge`.
     *  \return  Single-channel `Signal` whose samples are the
     *          channel-interleaved concatenation of `merge`'s
     *          output.
     *  \throws WaveformGeneratorException  via `merge`.
     */
    Signal interleave(std::vector<Value> const& args);       // 0x258140

    /*! \brief Multiply every sample of a waveform by a scalar.
     *
     *  \details Two-argument factory `scale(waveform, factor)`.
     *  Reads the waveform (any length) and a numeric `factor`,
     *  then returns a `Signal` whose samples are
     *  `waveform.samples_[i] * factor`.  Markers and `markerBits_`
     *  are copied through unchanged.
     *
     *  Reserve-only inputs are returned by value without
     *  materialising the sample buffer (the scaling is deferred
     *  until the placeholder is resolved).
     *
     *  \param args  Exactly 2 arguments: a waveform and a numeric
     *          scaling factor.
     *  \return  Scaled `Signal` with the same length, markers, and
     *          marker-bits layout as the input.
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or via `read*` for invalid argument types.
     */
    Signal scale(std::vector<Value> const& args);            // 0x258270

    /*! \brief Pointwise product of two or more waveforms.
     *
     *  \details Variadic factory accepting two or more waveform
     *  arguments.  All inputs must agree on `channels_` (otherwise
     *  raises `InconsistentChannels`); the output length is the
     *  maximum of the inputs' sample counts and the output
     *  `markerBits_` is the bitwise OR of every input's
     *  marker-bits.  For each output sample the function multiplies
     *  the corresponding samples across every input (per-sample
     *  markers are byte-multiplied, which acts as logical AND for
     *  `0/1`-valued markers); inputs that are too short for a given
     *  index contribute a `0.0` product and a zero marker.
     *
     *  Reserve-only inputs participate as zero-filled buffers; if
     *  every input is reserve-only the result is a reserve-only
     *  `Signal` of `maxNSamples` frames.  When any output sample's
     *  absolute value exceeds `1.0` the function emits an
     *  `AmplitudeClipped` warning via `warningCallback_`.
     *
     *  \param args  At least two waveform arguments.
     *  \return  `Signal` whose samples are the elementwise product
     *          of the inputs.
     *  \throws WaveformGeneratorException  on fewer than two
     *          arguments, non-waveform arguments, unknown
     *          waveforms, or channel-count mismatches.
     */
    Signal multiply(std::vector<Value> const& args);         // 0x258750

    /*! \brief Extract a contiguous slice of a waveform.
     *
     *  \details Three-argument factory `cut(waveform, from, to)`,
     *  where `from` and `to` are 1-based inclusive sample indices
     *  (each `>= 1` and `< length(waveform)`).  Returns the slice
     *  `[from, to]` as a new `Signal` with `(to - from + 1)`
     *  samples.
     *
     *  When `from == to` the function returns a fully zeroed
     *  `Signal` (no samples, no markers, `channels_ = 0`,
     *  `length_ = 0`) regardless of the input's `reserveOnly_`
     *  flag — this is intentional and propagates a distinct
     *  `PlayConfig` through the prefetch / codegen pipeline so
     *  empty plays correctly invalidate the global CWVF (see
     *  IF-176 for the full rationale and downstream side effects).
     *
     *  Reserve-only inputs (other than the `from == to` case
     *  above) yield a reserve-only `Signal` of the requested cut
     *  length, preserving the input's `markerBits_`.
     *
     *  \param args  Exactly 3 arguments: a waveform plus two
     *          positive integer indices.
     *  \return  Slice of the input waveform as a new `Signal`.
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or via `read*` for invalid argument types.
     *  \throws WaveformGeneratorValueException  if `from` or `to`
     *          is `>= length(waveform)` (template
     *          `ArgGreaterThanLength`).
     */
    Signal cut(std::vector<Value> const& args);              // 0x2598d0

    /*! \brief Reverse the sample order of a waveform.
     *
     *  \details One-argument factory; thin wrapper that reads the
     *  waveform via `readWave` (any length) and returns
     *  `reverse(sig)`.  See the private `reverse` helper for the
     *  exact reversal semantics — including the multi-channel
     *  block-reverse where each frame of `channels_` consecutive
     *  samples is treated as a unit, and marker bytes follow
     *  their associated sample frames.
     *
     *  \param args  Exactly 1 waveform argument.
     *  \return  Reversed `Signal`.
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or via `readWave` for malformed waveform references.
     */
    Signal flip(std::vector<Value> const& args);             // 0x25a310

    /*! \brief Apply a discrete IIR/FIR transfer function to a waveform.
     *
     *  \details Three-argument factory `filter(b, a, x)` where
     *  `b` and `a` are coefficient waveforms (numerator and
     *  denominator of the transfer function `H(z) = B(z) / A(z)`)
     *  and `x` is the input signal.  Computes
     *
     *  ```
     *  y[n] = (sum_{k=0}^{Nb-1} b[k] * x[n-k]
     *        - sum_{k=1}^{Na-1} a[k] * y[n-k]) / a[0]
     *  ```
     *
     *  with implicit zero-padding for negative indices.  Operates
     *  on single-channel waveforms only.
     *
     *  Validation:
     *  - `b` must have at least one sample,
     *  - `a` must have at least one sample,
     *  - `a[0]` must be non-zero,
     *  - `x` must be single-channel.
     *
     *  If `x` has zero samples the function returns a copy of `x`
     *  unchanged.
     *
     *  \param args  Exactly 3 arguments (`b`, `a`, `x`); enforced
     *          by the dispatcher rather than re-checked here.
     *  \return  Single-channel `Signal` of the same length as `x`.
     *  \throws WaveformGeneratorValueException  on validation
     *          failures (empty `a` or `b`, zero leading `a`,
     *          multi-channel `x`).
     */
    Signal filter(std::vector<Value> const& args);           // 0x25a540

    /*! \brief Circularly left-shift a waveform's samples (and markers).
     *
     *  \details Two-argument factory `circshift(waveform, shift)`.
     *  Returns a copy of the input `waveform` whose samples are
     *  rotated left by `shift` positions: the first `shift` samples
     *  are moved to the end.  For multi-channel waveforms the
     *  rotation operates on whole frames (each frame = `channels_`
     *  consecutive samples).  Marker bits are rotated by `shift mod
     *  markers.size()` so they stay aligned with their associated
     *  samples.  The shift is normalised modulo
     *  `samples.size() / channels`, so a shift equal to the number
     *  of frames returns a copy unchanged.
     *
     *  Reserve-only inputs (from `placeholder`) are returned by
     *  value without rotation, since their sample buffer has not yet
     *  been allocated.
     *
     *  \param args  Exactly 2 arguments: a waveform and an integer
     *          shift `>= 1` (validated by `readPositiveInt`).
     *  \return  Rotated copy of the input waveform.
     *  \throws WaveformGeneratorException  on wrong argument count
     *          or via `read*` for invalid argument types.
     */
    Signal circshift(std::vector<Value> const& args);        // 0x25b0e0

    /*! \brief Build a multi-channel `Signal` by interleaving
     *         several single-channel waveforms.
     *
     *  \details Variadic factory accepting two or more waveform
     *  arguments and at most one trailing integer.  The trailing
     *  integer (if present) is interpreted as a requested-length
     *  hint that participates in the all-reserve-only output
     *  length — it does not truncate or extend the materialised
     *  output.
     *
     *  Each waveform argument becomes one channel of the output;
     *  `channels_` is therefore the number of waveform arguments.
     *  The output length is the maximum of the inputs' sample
     *  counts; shorter inputs contribute `0.0` samples and zero
     *  markers for the missing frames.  Per-frame samples are
     *  interleaved channel-by-channel and each channel keeps its
     *  own marker byte (markers are *not* OR'd onto channel 0).
     *  The output `markerBits_` is the concatenation of every
     *  input's `markerBits_`.
     *
     *  When every input is reserve-only the function short-circuits
     *  to a reserve-only `Signal` whose length is the maximum of
     *  the input lengths and the requested-length hint, with
     *  `channels_` set to the number of inputs.  See IF-162 for
     *  the GDB-verified length-accumulation semantics.
     *
     *  Internal helper for `interleave` (which forwards to `merge`
     *  and then collapses the result back to a single logical
     *  channel).
     *
     *  \param args  At least two waveform arguments; an optional
     *          trailing `Int` is consumed as a length hint.
     *  \return  Multi-channel `Signal`.
     *  \throws WaveformGeneratorException  on fewer than two
     *          waveform arguments or via `readWave` for malformed
     *          waveform references.
     */
    Signal merge(std::vector<Value> const& args);            // 0x25f5c0

    /*! \brief Zero-pad a waveform to a target length.
     *
     *  \details Two-argument factory `grow(waveform, length)`.
     *  Returns a copy of `waveform` extended with zero-valued
     *  samples (and zero marker bytes) so that the output has
     *  exactly `length` frames.  When `length == 0` the input is
     *  returned unchanged; when `length` equals the input's
     *  current frame count the input is returned unchanged; when
     *  `length` is *less than* the input's current frame count the
     *  function raises an error (`grow` only extends, never
     *  shrinks).
     *
     *  Reserve-only inputs short-circuit to a reserve-only
     *  `Signal` of the requested length, preserving the input's
     *  `markerBits_`.  The output's `channels_` matches the
     *  input's `channels_`.
     *
     *  \param args  Exactly 2 arguments: a waveform and an integer
     *          target length in frames.
     *  \return  Zero-padded `Signal` of `length` frames.
     *  \throws WaveformGeneratorException  on fewer than two
     *          arguments, on `length < currentFrames`, or via
     *          `read*` for invalid argument types.
     */
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
