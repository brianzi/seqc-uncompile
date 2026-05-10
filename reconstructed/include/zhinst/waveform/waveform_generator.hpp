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
    const char* what() const noexcept override;                        // 0x261820
    // Returns message_.c_str() if non-empty, else "WaveformGenerator Exception"

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
    const char* what() const noexcept override;                                // 0x2617a0
    // Returns message_.c_str() if non-empty, else "WaveformGenerator Value Exception"

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
//   +0xB0  shared_ptr<void>                                               field_B0_ (zero-initialized in ctor; NO SETTER FOUND in binary)
//   +0xC0  END
//
// Layout verified: ctor allocates via `__shared_ptr_emplace<WaveformGenerator>`
// at 0x11d1b0 with `operator new(0xe0)`; subtract the 0x20-byte control
// block prefix → 0xC0 bytes for the WaveformGenerator body itself.
//
// `field_B0_`: zero-initialized in the ctor at 0x2482aa via
// `xorps xmm0; movaps [rbx+0xb0], xmm0`, but  investigation
// found NO instruction in the binary that writes to a WaveformGenerator
// instance's +0xB0/+0xB8 offsets. Earlier `+0xB0` read sightings inside
// WaveformGenerator methods were misattributed: those reads target the
// union body of `Value` parameter objects (which have a tag at +0xA8 and
// a 16-byte union storage at +0xB0), not the WaveformGenerator `this`.
// The slot is therefore reserved-but-unused — likely a feature that was
// compiled out, or a hook that an external API was supposed to install
// but the dynamic loader never reaches that code path. Kept in the layout
// for byte-fidelity.
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
    WaveformGenerator(std::shared_ptr<WavetableFront> wavetableFront,
                      std::function<void(std::string const&)> const& warningCallback);  // 0x248200
    ~WaveformGenerator();                                                                // 0x127840

    // --- Public API ---
    bool functionExists(std::string const& name) const;                                  // 0x25bc60

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
    /*! \brief Resolve a built-in waveform name (with deprecation
     *  alias support) and return its `WaveformFront`, materialising
     *  it on first use.
     *
     *  \details Three-step resolution:
     *    1. Look `name` up in `aliasMap_`.  When the name is a
     *       deprecated alias for a current built-in, format the
     *       `DeprecatedFunc` warning through `warningCallback_`
     *       and substitute the canonical name for the lookup
     *       that follows.
     *    2. Look the (resolved) name up in `funcMap_`.  When
     *       absent, raise `WaveformGeneratorValueException`
     *       formatted with `UnknownFunction`.
     *    3. Clone the bound `Signal foo(args)` and forward it as
     *       the `factory` argument to `getOrCreateWaveform(name,
     *       args, factory)`, which handles cache lookup or
     *       fresh registration.
     *
     *  \param name  Built-in or alias name as it appears in
     *               SeqC source.
     *  \param args  Already-evaluated `Value` arguments.
     *  \return      The cached or newly-registered
     *               `WaveformFront`.
     *  \throws WaveformGeneratorValueException  When `name`
     *               (after alias resolution) is not registered
     *               in `funcMap_`.
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
    double readDouble(Value val, std::string const& paramName,
                      std::string const& funcName);                                       // 0x25c6f0
    double readDoubleAmplitude(Value val, std::string const& paramName,
                               std::string const& funcName);                              // 0x25caa0
    int    readInt(Value val, std::string const& paramName, int argIndex,
                   std::string const& funcName);                                          // 0x25cca0
    int    readPositiveInt(Value val, std::string const& paramName, int argIndex,
                           std::string const& funcName);                                  // 0x25d490
    std::shared_ptr<WaveformFront> readWave(Value val, std::string const& paramName,
                    int expectedLength, std::string const& funcName);                     // 0x25d6f0

    // --- Helper methods ---
    Signal genericTriangle(int length, double amplitude, double nPeriods,
                           double riseRatio, double phase);                                  // 0x25e0c0
    Signal markerImpl(std::vector<Value> const& args, bool isMask);                       // 0x25e230
    Signal interpolateLinear(int length,
                             std::vector<double> const& xPoints,
                             std::vector<double> const& yPoints,
                             std::vector<uint8_t> const& markers,
                             std::vector<uint8_t> const& markerBits);                     // 0x25f410

    Signal reverse(Signal& sig);                                                          // 0x260f20

    // --- Waveform generation functions ---
    // Each takes vector<Value> const& and returns Signal.
    // Registered in funcMap_ by the constructor.
    Signal zeros(std::vector<Value> const& args);            // 0x249b90
    Signal ones(std::vector<Value> const& args);             // 0x249e10
    Signal sin(std::vector<Value> const& args);              // 0x24a0f0
    Signal cos(std::vector<Value> const& args);              // 0x24abd0
    Signal sinc(std::vector<Value> const& args);             // 0x24b6e0
    Signal ramp(std::vector<Value> const& args);             // 0x24c2c0
    Signal sawtooth(std::vector<Value> const& args);         // 0x24c8b0
    Signal triangle(std::vector<Value> const& args);         // 0x24d330
    Signal gauss(std::vector<Value> const& args);            // 0x24ddb0
    Signal drag(std::vector<Value> const& args);             // 0x24e950
    Signal blackman(std::vector<Value> const& args);         // 0x24f530
    Signal hamming(std::vector<Value> const& args);          // 0x24fd20
    Signal hann(std::vector<Value> const& args);             // 0x250250
    Signal rect(std::vector<Value> const& args);             // 0x250770
    Signal chirp(std::vector<Value> const& args);            // 0x250bb0
    Signal mask(std::vector<Value> const& args);             // 0x251cb0
    Signal marker(std::vector<Value> const& args);           // 0x251cd0
    Signal rand(std::vector<Value> const& args);             // 0x251cf0
    Signal randomGauss(std::vector<Value> const& args);      // 0x252930
    Signal randomUniform(std::vector<Value> const& args);    // 0x253440
    Signal lfsrGaloisMarker(std::vector<Value> const& args); // 0x253bc0
    Signal rrc(std::vector<Value> const& args);              // 0x254290
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

    // +0xB0: Dead/vestigial shared_ptr — no setter exists in binary
    std::shared_ptr<void>                                    reserved_B0_;       // +0xB0
};

} // namespace zhinst
