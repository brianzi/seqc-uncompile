// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// zhinst::WaveformGenerator — waveform DSP function registry
//
// This file implements:
//   - WaveformGeneratorException (ctor, dtor, what)
//   - WaveformGeneratorValueException (ctor, dtor, what)
//   - WaveformGenerator::createDummyWaveform  @0x25be70
//   - WaveformGenerator::genericTriangle      @0x25e0c0
//   - WaveformGenerator::reverse              @0x260f20
//
// All other methods are stubbed with their binary addresses in comments.
// ============================================================================

#include "zhinst/waveform_generator.hpp"
#include "zhinst/wavetable_front.hpp"
#include "zhinst/waveform_front.hpp"
#include "zhinst/eval_results.hpp"
#include "zhinst/eval_result_value.hpp"
#include "zhinst/error_messages.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numeric>
#include <random>
#include <stdexcept>

#include "zhinst/resources.hpp"

// libc++ PRNG shim — see prng_libcxx.cpp.
extern "C" {
    void seqc_libcxx_normal_amplitude(uint64_t*, double, double, double, double*, std::size_t);
    void seqc_libcxx_uniform(uint64_t*, double, double, double*, std::size_t);
    void seqc_minstd_normal_amplitude(double, double, double, double*, std::size_t);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace zhinst {

// `floatEqual(double, double)` @0x2ec050 — exact IEEE-754 bitwise equality.
//
// Disasm shows a 5-instruction body using `cmpeqsd xmm0, xmm1` (sets all
// bits of the result to 1 if the two doubles are bitwise-equal, 0 otherwise),
// then masks the low bit. NOT a tolerance-based "approximately equal" —
// despite the name, callers use this to test for exact zero (e.g.
// `if (floatEqual(beta, 0.0))`) which `==` handles identically.
//
// Phase 20b: out-of-line definition (was forward-decl only). Defined in
// the same TU as its callers since the binary's symbol is namespace-
// scope `zhinst::floatEqual` and not part of any class.
bool floatEqual(double a, double b) {  // 0x2ec050
    return a == b;
}

// ============================================================================
// WaveformGeneratorException
// ============================================================================

WaveformGeneratorException::WaveformGeneratorException(  // 0x25ca00
    std::string const& msg)
    : message_(msg)
{}

WaveformGeneratorException::~WaveformGeneratorException() {}  // 0x25ca60

const char* WaveformGeneratorException::what() const noexcept {  // 0x261820
    if (message_.empty()) {
        return "WaveformGenerator Exception";
    }
    return message_.c_str();
}

// ============================================================================
// WaveformGeneratorValueException
// ============================================================================

WaveformGeneratorValueException::WaveformGeneratorValueException(  // 0x25c4a0
    std::string const& msg, size_t argIndex)
    : message_(msg), argIndex_(argIndex)
{}

WaveformGeneratorValueException::~WaveformGeneratorValueException() {}  // 0x25c500

const char* WaveformGeneratorValueException::what() const noexcept {  // 0x2617a0
    if (message_.empty()) {
        return "WaveformGenerator Value Exception";
    }
    return message_.c_str();
}

// ============================================================================
// WaveformGenerator — Constructor / Destructor
// ============================================================================

// Ctor @0x248200 — registers ~35 waveform functions into funcMap_
WaveformGenerator::WaveformGenerator(
    std::shared_ptr<WavetableFront> wavetableFront,
    std::function<void(std::string const&)> const& warningCallback)
    : funcMap_()
    , funcMap_maxLoadFactor_(1.0f)
    , aliasMap_()
    , aliasMap_maxLoadFactor_(1.0f)
    , createdNames_({"rand", "randomGauss", "randomUniform", "placeholder"})
    , wavetableFront_(std::move(wavetableFront))
    , pad_78_(0)
    , warningCallback_(warningCallback)
    , reserved_B0_()
{
    using namespace std::placeholders;

    // Register all waveform generation functions.
    // Each is a member function bound with this + placeholder for args.
    funcMap_["zeros"]            = std::bind(&WaveformGenerator::zeros,            this, _1);
    funcMap_["ones"]             = std::bind(&WaveformGenerator::ones,             this, _1);
    funcMap_["sine"]             = std::bind(&WaveformGenerator::sin,              this, _1);  // key is "sine", method is sin()
    funcMap_["cosine"]           = std::bind(&WaveformGenerator::cos,              this, _1);  // key is "cosine", method is cos()
    funcMap_["sinc"]             = std::bind(&WaveformGenerator::sinc,             this, _1);
    funcMap_["ramp"]             = std::bind(&WaveformGenerator::ramp,             this, _1);
    funcMap_["sawtooth"]         = std::bind(&WaveformGenerator::sawtooth,         this, _1);
    funcMap_["triangle"]         = std::bind(&WaveformGenerator::triangle,         this, _1);
    funcMap_["gauss"]            = std::bind(&WaveformGenerator::gauss,            this, _1);
    funcMap_["drag"]             = std::bind(&WaveformGenerator::drag,             this, _1);
    funcMap_["blackman"]         = std::bind(&WaveformGenerator::blackman,         this, _1);
    funcMap_["hamming"]          = std::bind(&WaveformGenerator::hamming,          this, _1);
    funcMap_["hann"]             = std::bind(&WaveformGenerator::hann,             this, _1);
    funcMap_["rect"]             = std::bind(&WaveformGenerator::rect,             this, _1);
    funcMap_["chirp"]            = std::bind(&WaveformGenerator::chirp,            this, _1);
    funcMap_["mask"]             = std::bind(&WaveformGenerator::mask,             this, _1);
    funcMap_["marker"]           = std::bind(&WaveformGenerator::marker,           this, _1);
    funcMap_["rand"]             = std::bind(&WaveformGenerator::rand,             this, _1);
    funcMap_["randomGauss"]      = std::bind(&WaveformGenerator::randomGauss,      this, _1);
    funcMap_["randomUniform"]    = std::bind(&WaveformGenerator::randomUniform,    this, _1);
    funcMap_["lfsrGaloisMarker"] = std::bind(&WaveformGenerator::lfsrGaloisMarker, this, _1);
    funcMap_["rrc"]              = std::bind(&WaveformGenerator::rrc,              this, _1);
    funcMap_["vect"]             = std::bind(&WaveformGenerator::vect,             this, _1);
    funcMap_["placeholder"]      = std::bind(&WaveformGenerator::placeholder,      this, _1);
    funcMap_["join"]             = std::bind(&WaveformGenerator::join,             this, _1);
    funcMap_["add"]              = std::bind(&WaveformGenerator::add,              this, _1);
    funcMap_["interleave"]       = std::bind(&WaveformGenerator::interleave,       this, _1);
    funcMap_["scale"]            = std::bind(&WaveformGenerator::scale,            this, _1);
    funcMap_["multiply"]         = std::bind(&WaveformGenerator::multiply,         this, _1);
    funcMap_["cut"]              = std::bind(&WaveformGenerator::cut,              this, _1);
    funcMap_["flip"]             = std::bind(&WaveformGenerator::flip,             this, _1);
    funcMap_["filter"]           = std::bind(&WaveformGenerator::filter,           this, _1);
    funcMap_["circshift"]        = std::bind(&WaveformGenerator::circshift,        this, _1);
    funcMap_["merge"]            = std::bind(&WaveformGenerator::merge,            this, _1);
    funcMap_["grow"]             = std::bind(&WaveformGenerator::grow,             this, _1);

    // aliasMap_ is intentionally empty — the binary ctor does not populate it.
    // The aliasMap_ machinery (deprecation warning + redirect in call()) exists
    // but no aliases are registered in this binary version.
}

WaveformGenerator::~WaveformGenerator() {}  // 0x127840

// ============================================================================
// Implemented methods
// ============================================================================

// createDummyWaveform @0x25be70
// Creates a zero-filled waveform of the given length by calling "zeros".
// Returns the resulting WaveformFront.
std::shared_ptr<WaveformFront> WaveformGenerator::createDummyWaveform(int length) {
    // Build a single-element vector<Value> containing the length as an int Value.
    // Disassembly literally encodes the SSO string "zeros" on the stack and a
    // stack-resident Value{type=1, which=0, storage.i=length} (~28 bytes).
    std::vector<Value> args;
    Value v;
    v.type_ = ValueType::Int;
    v.which_ = 0;           // which==0 → int variant
    v.storage_.i = length;  // set the int field
    args.push_back(v);

    // Delegate to the registered "zeros" function via call().
    auto tmp = call("zeros", args);

    // The binary then looks up the waveform by name in the wavetable
    // and marks it as used (sets +0x48 = true).  @0x25bfb4..0x25c01b
    std::optional<std::string> name;
    if (tmp) {
        name = tmp->name;
    }
    auto wf = wavetableFront_->getWaveformByName(name);
    if (wf) {
        wf->used = true;
    }
    return wf;
}

// genericTriangle @0x25e0c0
// Generates a piecewise-linear triangle waveform.
//   length:    number of samples
//   amplitude: peak amplitude (positive and negative)
//   riseRatio: fraction of the period that is the rising edge (0..1)
//   phase:     phase offset in radians
//   period:    period expressed in the same units as length (number of samples per period)
Signal WaveformGenerator::genericTriangle(int length, double amplitude,
                                           double nPeriods, double riseRatio,
                                           double phase) {
    Signal sig(static_cast<size_t>(length));

    if (length == 0) {
        return sig;
    }

    double samplesPerPeriod = static_cast<double>(length) / nPeriods;
    double fallSamples = (1.0 - riseRatio) * samplesPerPeriod;
    double riseHalfSamples = riseRatio * samplesPerPeriod * 0.5;
    double phaseOffset = (phase / (2.0 * M_PI)) * samplesPerPeriod;
    double riseHalfPlusFall = riseHalfSamples + fallSamples;

    double negAmplitude = -amplitude;
    double twoAmplitude = amplitude * 2.0;

    for (int i = 0; i < length; ++i) {
        double t = std::fmod(static_cast<double>(i) + phaseOffset, samplesPerPeriod);
        double sample;

        if (t < riseHalfSamples) {
            // First rising segment: 0 → +amplitude
            sample = (t / riseHalfSamples) * amplitude;
        } else if (t < riseHalfPlusFall) {
            // Falling segment: +amplitude → -amplitude
            sample = (1.0 - (t - riseHalfSamples) / fallSamples) * twoAmplitude + negAmplitude;
        } else {
            // Second rising segment: -amplitude → 0
            sample = ((t - riseHalfSamples - fallSamples) / riseHalfSamples) * amplitude + negAmplitude;
        }

        sig.append(sample, 0);
    }

    return sig;
}

// reverse @0x260f20
// Reverses a Signal's samples (and markers) in-place, returning a new Signal.
// For single-channel signals, it's a simple std::reverse.
// For multi-channel signals, it reverses channel-interleaved blocks.
// If sig.reserveOnly_ is true, just copies the signal unchanged.
Signal WaveformGenerator::reverse(Signal& sig) {
    if (sig.reserveOnly_) {
        return Signal(sig);
    }

    uint16_t channels = sig.channels();
    if (channels == 1) {
        // Single-channel: checkAllocation, then reverse samples and markers
        sig.checkAllocation();

        std::vector<double> reversedSamples(sig.samples_.begin(), sig.samples_.end());
        std::reverse(reversedSamples.begin(), reversedSamples.end());

        std::vector<uint8_t> reversedMarkers(sig.markers_.begin(), sig.markers_.end());
        std::reverse(reversedMarkers.begin(), reversedMarkers.end());

        return Signal(reversedSamples, reversedMarkers, sig.markerBits_);
    } else {
        // Multi-channel: reverse in blocks of 'channels' samples
        sig.checkAllocation();

        size_t totalSamples = sig.samples_.size();
        size_t totalMarkers = sig.markers_.size();
        size_t numBlocks = totalSamples / channels;

        std::vector<double> reversedSamples(totalSamples);
        std::vector<uint8_t> reversedMarkers(totalMarkers);

        for (size_t i = 0; i < numBlocks; ++i) {
            size_t srcBlock = numBlocks - 1 - i;
            for (uint16_t ch = 0; ch < channels; ++ch) {
                reversedSamples[i * channels + ch] = sig.samples_[srcBlock * channels + ch];
            }
            if (totalMarkers > 0) {
                size_t markerBlockSize = totalMarkers / numBlocks;
                for (size_t m = 0; m < markerBlockSize; ++m) {
                    reversedMarkers[i * markerBlockSize + m] =
                        sig.markers_[srcBlock * markerBlockSize + m];
                }
            }
        }

        return Signal(reversedSamples, reversedMarkers, sig.markerBits_);
    }
}

// ============================================================================
// getOrCreateWaveform / call / eval — reconstructed
// ============================================================================

// getOrCreateWaveform @0x25bca0
// If `name` is in createdNames_ (set<string> @+0x50), look up the existing
// WaveformFront via wavetableFront_->getWaveformByFunDescr(name, args) and
// bump its use-count (uint32_t at WaveformFront +0xd8).
//
// Otherwise (first time we see this name), invoke `factory(args)` to produce
// the Signal, register it via wavetableFront_->newWaveform(signal, name,
// args), and return the resulting WaveformFront.  newWaveform also inserts
// the name into createdNames_ as a side effect — that's the cache key.
//
// Throws std::bad_function_call if `factory` is empty (matches the
// __throw_bad_function_call branch in the binary).
std::shared_ptr<WaveformFront> WaveformGenerator::getOrCreateWaveform(
    std::string const& name,
    std::vector<Value> const& args,
    std::function<Signal(std::vector<Value> const&)> factory)
{
    // Binary flow at 0x25bca0:
    //   1. Check createdNames_.find(name) vs end
    //   2. If NOT in set → try getWaveformByFunDescr(name, args)
    //      - If found → increment useCount (+0xd8), return
    //   3. If in set OR getWaveformByFunDescr returned null →
    //      call factory(args), then newWaveform(signal, name, args)
    auto it = createdNames_.find(name);                                        // 0x25bcce
    if (it == createdNames_.end()) {                                           // 0x25bcda: je 0x25bdd9
        // Not in set — try existing lookup first
        auto wf = wavetableFront_->getWaveformByFunDescr(name, args);          // 0x25bdea
        if (wf) {
            // 0x25be11: incl 0xd8(%rax) — bump use-count
            // wf->useCount++;  // TODO: expose use-count field
            return wf;
        }
        // Fall through to factory path
    }

    // In set OR lookup failed: invoke factory and create new waveform
    if (!factory) {                                                            // 0x25be2d
        throw std::bad_function_call();
    }

    Signal signal = factory(args);                                             // 0x25bcfd

    return wavetableFront_->newWaveform(signal, name, args);                   // 0x25bd15
}

// call @0x25c120
// Look up a function name (with deprecated-alias resolution) and dispatch
// it through getOrCreateWaveform.  Throws WaveformGeneratorValueException
// if the name is not registered.
//
// Algorithm (mirroring the binary):
//   1. Find `name` in aliasMap_ (+0x28). If present, build an
//      ErrorMessages::format(0x37, name, alias) deprecation message and
//      invoke warningCallback_(msg) on it.  The lookup name is then the
//      value (alias) instead of the original.
//   2. Find the (possibly aliased) name in funcMap_.
//      * If absent, throw WaveformGeneratorValueException(
//            ErrorMessages::format(0xd8, name), 0).
//      * If present, clone the bound std::function and pass it as the
//        factory to getOrCreateWaveform.
std::shared_ptr<WaveformFront> WaveformGenerator::call(
    std::string const& name,
    std::vector<Value> const& args)
{
    // Resolve alias.
    std::string lookupName = name;
    auto aliasIt = aliasMap_.find(name);
    if (aliasIt != aliasMap_.end()) {
        // Emit deprecation warning through warningCallback_.
        // Format slot 0x37 takes (oldName, newName).
        std::string warning = ErrorMessages::format(
            DeprecatedFunc, name, aliasIt->second);
        if (warningCallback_) {
            warningCallback_(warning);
        } else {
            // Matches __throw_bad_function_call at 25c310 in the binary.
            throw std::bad_function_call();
        }
        lookupName = aliasIt->second;
    }

    // Lookup in function registry.
    auto funcIt = funcMap_.find(lookupName);
    if (funcIt == funcMap_.end()) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(UnknownFunction, name), 0);
    }

    // Clone the bound function and dispatch.  The disassembly explicitly
    // copies the function (via __value_func clone) before passing it to
    // getOrCreateWaveform; we do the same here implicitly by copying.
    auto factory = funcIt->second;  // copy the std::function
    return getOrCreateWaveform(lookupName, args, std::move(factory));
}

// eval @0x25c540
// Wrap a call() result in a freshly-constructed EvalResults.
//   1. result = call(name, args)              // shared_ptr<WaveformFront>
//   2. eval = make_shared<EvalResults>()      // 0x80-byte object body, 0x18 header
//   3. eval->setValue(VarType=5, Value{string=name})
//      (binary uses VarType enum value 5 — likely VarType::eWave)
//   4. Store `result` into EvalResults at +0x60 (a shared_ptr<WaveformFront>
//      member).
//   5. Return shared_ptr<EvalResults>.
//
// EvalResults layout (from binary; partial):
//     +0x00  vptr (vtable @0xb03d00 via __shared_ptr_emplace<EvalResults>)
//     +0x08  ... (zeroed in the ctor inline-fill 0x18..0x90)
//     +0x60  shared_ptr<WaveformFront>  (16 bytes: ptr + control block)
//     +0x70  ...
//     +0x90  uint64_t (zeroed)
//   Total body size: 0x98 bytes when allocated by __shared_ptr_emplace.
//
// NOTE: EvalResults is still only forward-declared in our headers, so we
// cannot construct it directly in this TU.  Once the type is reconstructed
// (Phase 15a), replace the stub body with the make_shared-based code below.
std::shared_ptr<EvalResults> WaveformGenerator::eval(
    std::string const& name,
    std::vector<Value> const& args)
{
    // 0x25c554: call(name, args) → shared_ptr<WaveformFront>
    auto wf = call(name, args);

    // 0x25c559-0x25c5b7: make_shared<EvalResults>() — zero-initialized
    auto results = std::make_shared<EvalResults>();

    // 0x25c5bf-0x25c5ec: construct Value with type_=4 (String), which_=3,
    // string content = waveform name from WaveformFront
    Value v(wf->name);  // Value(string) sets type_=4, which_=3             // 0x25c5bf

    // 0x25c5fa-0x25c5ff: setValue(VarType_Wave, value) — VarType 5 = Waveform
    results->setValue(static_cast<VarType>(5), v);                           // 0x25c5ff → 0x211b70

    // 0x25c63c-0x25c640: store wf into results->waveformFront_ (+0x48)
    results->waveformFront_ = std::move(wf);                                // 0x25c640

    return results;                                                          // 0x25c699
}

// --- Parameter readers ---
//
// These helpers extract a primitive value from a Value while emitting nicely
// formatted error messages.  The general pattern in the binary is:
//
//   try {
//     if (val.type_ == ValueType::String)
//       throw WaveformGeneratorException(format(0x55, paramName, funcName));
//     T result = val.toT();
//     /* optionally bounds-check */
//     return result;
//   } catch (WaveformGeneratorException&) {
//     throw WaveformGeneratorValueException(format(0x55|0x60, …), arg);
//   }
//
// We collapse the try/catch into a direct ValueException since the inner
// type was always converted before propagating.

double WaveformGenerator::readDouble(  // 0x25c6f0
    Value val, std::string const& paramName,
    std::string const& funcName)
{
    if (val.type_ == ValueType::String) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(ArgMustBeConst,
                                  paramName, funcName),
            0);
    }
    return val.toDouble();
}

double WaveformGenerator::readDoubleAmplitude(  // 0x25caa0
    Value val, std::string const& paramName,
    std::string const& funcName)
{
    // Calls readDouble, then checks |result| <= 1.0.  If exceeded, issues a
    // warning (not a throw) via warningCallback_ with error 0x54.
    // Binary: jump-table on val.which_ → readDouble @0x25cb50 → andpd fabs
    // @0x25cb8b → ucomisd vs 1.0 @0x25cb97 → jbe skip → format(0x54, funcName)
    // → warningCallback_() @0x25cbfd.  Returns result regardless.
    double result = readDouble(val, paramName, funcName);                       // 0x25cb50
    if (std::fabs(result) > 1.0) {                                             // 0x25cb8b–0x25cb9f
        if (warningCallback_) {
            warningCallback_(ErrorMessages::format(
                AmplitudeClipped, funcName));                   // 0x25cbe5
        }
    }
    return result;
}

int WaveformGenerator::readInt(  // 0x25cca0
    Value val, std::string const& paramName, int argIndex,
    std::string const& funcName)
{
    if (val.type_ == ValueType::String) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(ArgMustBeConst,
                                  paramName, funcName),
            0);
    }
    return val.toInt();
}

int WaveformGenerator::readPositiveInt(  // 0x25d490
    Value val, std::string const& paramName, int argIndex,
    std::string const& funcName)
{
    // Convenience wrapper: enforces result >= 0.
    int result = readInt(val, paramName, argIndex, funcName);
    if (result < 0) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(ArgOverflow,
                                  paramName, funcName,
                                  std::string("positive")),
            static_cast<size_t>(argIndex));
    }
    return result;
}

// readWave(val, paramName, expectedLength, funcName) @0x25d6f0
//
// Looks up a waveform by name from a Value and returns it as a shared_ptr<WaveformFront>.
// The Value must hold a waveform/string reference (Value type == 4).
//
// Flow:
//   1. Checks val.type_ == 4 (at 0x25d70c). If not, throws
//      WaveformGeneratorValueException(format(0x56, paramName, funcName), expectedLength).
//   2. Calls val.toString() to get the waveform name string.
//   3. Calls wavetableFront_->waveformExists(name) at 0x25d737.
//   4. If exists: calls wavetableFront_->getWaveformByName(name) at 0x25d79c
//      to get shared_ptr<WaveformFront>, then loadWaveform() at 0x25d7f9
//      to ensure the signal data is materialized. Returns the shared_ptr.
//   5. If not exists: throws WaveformGeneratorException(format(0x5a, funcName, name)).
//
// Return type is shared_ptr<WaveformFront>, NOT Signal. Callers extract
// wf->signal (at Waveform base +0x80) when they need the Signal data.
std::shared_ptr<WaveformFront> WaveformGenerator::readWave(                    // 0x25d6f0
    Value val, std::string const& paramName, int expectedLength,
    std::string const& funcName)
{
    // 0x25d70c: cmp DWORD PTR [rdx], 0x4
    if (static_cast<int>(val.type_) != 4) {
        // 0x25d842: allocate WaveformGeneratorValueException (0x20 bytes)
        // 0x25d968..0x25d984: ErrorMessages::format(0x56, paramName, funcName)
        // 0x25d930: WaveformGeneratorValueException ctor
        // 0x25d949: __cxa_throw
        std::string msg = ErrorMessages::format(
            ArgMustBeString, paramName, funcName);
        throw WaveformGeneratorValueException(msg, expectedLength);
    }

    // 0x25d72c: val.toString() → waveform name
    std::string name = val.toString();

    // 0x25d737: wavetableFront_->waveformExists(name)
    if (!wavetableFront_->waveformExists(name)) {
        // 0x25d870: allocate WaveformGeneratorException (0x28 bytes)
        // 0x25d902: val.toString() again for the error message
        // 0x25d91e: ErrorMessages::format(0x5a, funcName, waveName)
        // 0x25d93f: throw WaveformGeneratorException
        std::string waveName = val.toString();
        std::string msg = ErrorMessages::format(
            UnknownWaveform, funcName, waveName);
        throw WaveformGeneratorException(msg);
    }

    // 0x25d76a: val.toString() again for the lookup
    std::string lookupName = val.toString();

    // 0x25d79c: getWaveformByName returns shared_ptr<WaveformFront> via sret
    std::optional<std::string> optName(std::move(lookupName));
    std::shared_ptr<WaveformFront> wf =
        wavetableFront_->getWaveformByName(optName);                           // 0x25d79c

    // 0x25d7f9: loadWaveform — ensures signal data is materialized
    wavetableFront_->loadWaveform(wf);                                         // 0x25d7f9

    // 0x25d82d: return the shared_ptr (mov rax, r14; ret)
    return wf;
}

// --- Helper methods ---

// markerImpl(args, isMask) @0x25e230
//
// Shared implementation for marker() and mask() functions.
// Creates a uniform Signal of the given length with a constant marker value.
//
// Args: exactly 2 — length (int>=1) and markerValue (int>=2).
// If markerValue >= 4, issues a warning and truncates to markerValue & 3.
// isMask only controls parameter name strings ("mask"/"marker").
//
// Returns: Signal(length, 0.0, marker, channels=1) — zero samples, constant marker.
Signal WaveformGenerator::markerImpl(                                          // 0x25e230
    std::vector<Value> const& args, bool isMask)
{
    std::string funcName = isMask ? "mask" : "marker";                         // 0x25e254

    // 0x25e264: args.size() must be 2
    if (args.size() != 2) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  funcName, 2, args.size()));                  // 0x25e70a
    }

    // 0x25e3e0: read length from arg[0]
    std::string lenParam = isMask ? "1 (length)" : "1 (samples)";
    int length = readPositiveInt(args[0], lenParam, 1, funcName);              // 0x25e3e0

    // 0x25e5b4: read marker value from arg[1]
    std::string valParam = isMask ? "2 (mask)" : "2 (markerValue)";
    int markerValue = readPositiveInt(args[1], valParam, 2, funcName);         // 0x25e5b4

    // 0x25e611: range check — if markerValue >= 4, warn and truncate
    uint8_t marker = static_cast<uint8_t>(markerValue);
    if (markerValue >= 4) {                                                    // 0x25e611
        // Issue warning via warningCallback_ (+0xa0) with error 0x63
        if (warningCallback_) {
            std::string msg = ErrorMessages::format(
                ValueCapped,
                markerValue, markerValue & 3);
            warningCallback_(msg);
        }
        marker = static_cast<uint8_t>(markerValue & 3);
    }

    // 0x25e6f0: construct Signal with zero samples and constant marker
    return Signal(static_cast<size_t>(length), 0.0, marker, /*channels=*/1);   // 0x25e6f0
}

// interpolateLinear(length, xPoints, yPoints, markers, markerBits) @0x25f410
//
// Generates an interleaved multi-channel signal by linearly ramping each
// channel from xPoints[ch] to yPoints[ch] over `length` time steps.
//
// Register assignment (with hidden struct-return ptr in rdi):
//   rdi → rbx       = hidden return pointer (Signal)
//   rsi             = this (WaveformGenerator*) — unused
//   edx → [rbp-0x50]= length
//   rcx → r12       = &xPoints
//   r8  → r15       = &yPoints
//   r9  → r14       = &markers
//   [rbp+0x10]      = &markerBits
//
// Algorithm:
//   1. Allocate a zero-filled MarkerBitsPerChannel of markers.size() bytes.
//      (NOT from the markerBits parameter — the temp is zeroed.)           0x25f430–0x25f479
//   2. Construct Signal(length, mbpc).                                     0x25f48f
//   3. If length == 0, return immediately.                                 0x25f4b0
//   4. lengthDouble = (double)length.                                      0x25f4b9
//   5. Outer loop: seg = 1 .. length                                       0x25f4cc–0x25f4ef
//   6. Inner loop (n = 0 .. xPoints.size()-1):                             0x25f510–0x25f568
//        marker  = markerBits[n] | markers[n]
//        sample  = xPoints[n] + (yPoints[n] - xPoints[n]) * seg / length
//        sig.append(sample, marker)
//
// Total appended samples = length * xPoints.size() (interleaved channels).
//
Signal WaveformGenerator::interpolateLinear(                                   // 0x25f410
    int length,
    std::vector<double> const& xPoints,
    std::vector<double> const& yPoints,
    std::vector<uint8_t> const& markers,
    std::vector<uint8_t> const& markerBits)
{
    // Build a zero-filled MarkerBitsPerChannel of markers.size() bytes.     // 0x25f430
    // The binary does: size = markers.end() - markers.begin(),
    // then operator new(size) + memset(0).  This is NOT built from the
    // markerBits parameter — that is used only in the inner loop OR.
    MarkerBitsPerChannel mbpc(markers.size(), 0);                              // 0x25f434–0x25f479

    Signal sig(static_cast<size_t>(length), mbpc);                             // 0x25f48f

    if (length == 0) return sig;                                               // 0x25f4b0

    double lengthDouble = static_cast<double>(length);                         // 0x25f4b9

    // r12 = &xPoints; load begin/end pointers                                // 0x25f4c3
    size_t numChannels = xPoints.size();

    size_t seg = 1;                                                            // 0x25f4cc  edx=1
    while (true) {                                                             // outer loop 0x25f4f1
        // If xPoints is empty, skip inner loop entirely                       // 0x25f4f1: cmp end,begin
        if (numChannels > 0) {
            double segDouble = static_cast<double>(seg);                       // 0x25f4fd: cvtsi2sd rdx

            for (size_t n = 0; n < numChannels; ++n) {                         // 0x25f510 inner loop
                // Combine marker bytes from both input vectors                // 0x25f510–0x25f51f
                uint8_t marker = markerBits[n] | markers[n];

                // Linear interpolation: ramp from xPoints[n] to yPoints[n]   // 0x25f523–0x25f540
                //   xmm1 = xPoints[n]
                //   xmm0 = yPoints[n]
                //   xmm0 = (yPoints[n] - xPoints[n]) * seg / length + xPoints[n]
                double startVal = xPoints[n];                                  // 0x25f523: movsd xmm1,[rax+r13*8]
                double endVal   = yPoints[n];                                  // 0x25f52c: movsd xmm0,[rax+r13*8]
                double sample = startVal
                    + (endVal - startVal) * segDouble / lengthDouble;          // 0x25f532–0x25f540

                sig.append(sample, marker);                                    // 0x25f54a
            }
        }

        // Advance outer loop                                                  // 0x25f4e3–0x25f4ef
        if (seg == static_cast<size_t>(length)) break;                         // 0x25f4eb: cmp seg,length
        ++seg;
    }

    return sig;                                                                // 0x25f56f–0x25f580
}

// ============================================================================
// Waveform generation functions
// Each is registered in funcMap_ by the constructor.
//
// All of these share a common preamble:
//   1. Validate args.size() == expectedArgCount.  On mismatch, throw
//      WaveformGeneratorException(format(0x5b, funcName, expected, got)).
//   2. Copy each arg into a stack Value (the disassembly inlines the variant
//      copy via a jump-table on val.which_).  We use the convenient
//      readInt/readDouble/etc. helpers instead.
//   3. Allocate a vector<double> of the right length, fill it, and pass to
//      Signal(vector<double>, channels=1).
// ============================================================================

namespace {
    // Helper: enforce expected arg count, otherwise throw the standard error.
    inline void checkArgCount(std::vector<Value> const& args,
                              std::string const& funcName,
                              size_t expected)
    {
        if (args.size() != expected) {
            // ErrorMessage 0x5b takes (funcName, expectedCount, actualCount).
            throw WaveformGeneratorException(
                ErrorMessages::format(FuncExactArgs2,
                                      funcName.c_str(),
                                      static_cast<int>(expected),
                                      args.size()));
        }
    }
}  // namespace

// zeros(length) @0x249b90
//   length: int >= 1
// Returns a Signal of `length` samples, all zero.
Signal WaveformGenerator::zeros(std::vector<Value> const& args) {
    checkArgCount(args, "zeros", 1);
    int length = readInt(args[0], "length", 1, "zeros");
    std::vector<double> samples(static_cast<size_t>(length), 0.0);
    return Signal(samples, /*channels=*/1);
}

// ones(length) @0x249e10
//   length: int >= 1
// Returns a Signal of `length` samples, all one.
Signal WaveformGenerator::ones(std::vector<Value> const& args) {
    checkArgCount(args, "ones", 1);
    int length = readInt(args[0], "length", 1, "ones");
    std::vector<double> samples(static_cast<size_t>(length), 1.0);
    return Signal(samples, /*channels=*/1);
}

// rect(length, amplitude) @0x250770
//   length:    int >= 1
//   amplitude: double in [-1.0, 1.0]
// Returns a Signal of `length` samples, all `amplitude`.
//
// Disassembly note: rect uses readDoubleAmplitude for the amplitude param,
// which adds a |amp| <= 1.0 bounds check on top of readDouble.
Signal WaveformGenerator::rect(std::vector<Value> const& args) {           // 0x250770
    checkArgCount(args, "rect", 2);
    int length      = readInt(args[0], "length", 1, "rect");
    double amplitude = readDoubleAmplitude(args[1], "amplitude", "rect");
    std::vector<double> samples(static_cast<size_t>(length), amplitude);
    return Signal(samples, /*channels=*/1);
}

// scale(signal, factor) @0x258270
//   signal: a Signal value
//   factor: double scalar multiplier
// Returns a Signal whose samples are signal.samples * factor.
//
// Skeleton: extracts the wave via readWave (length=-1 → "any length"),
// allocates a same-sized output vector, multiplies each sample.
// Markers and reserveOnly_ are copied through unchanged.
Signal WaveformGenerator::scale(std::vector<Value> const& args) {           // 0x258270
    checkArgCount(args, "scale", 2);
    Signal sig    = readWave(args[0], "wave", -1, "scale")->signal;
    double factor = readDouble(args[1], "factor", "scale");

    if (sig.reserveOnly_) {
        return sig;
    }
    sig.checkAllocation();

    std::vector<double> out(sig.samples_.size());
    for (size_t i = 0; i < sig.samples_.size(); ++i) {
        out[i] = sig.samples_[i] * factor;
    }
    return Signal(out, sig.markers_, sig.markerBits_);
}

// add(signal_a, signal_b, ...) @0x256ff0
//   Takes 2+ signals of the same length and sums them sample-by-sample.
//   Markers are OR'd together, markerBits unchanged.
//
// Skeleton: validates all args are signals and same length, then accumulates.
// The full disassembly handles channel mismatches and markers carefully;
// this reconstruction covers the common single-channel case.
Signal WaveformGenerator::add(std::vector<Value> const& args) {             // 0x256ff0
    if (args.size() < 2) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "add", 2, args.size()));
    }

    Signal first = readWave(args[0], "wave_1", -1, "add")->signal;
    if (first.reserveOnly_) {
        return first;  // matches the disassembly's reserveOnly_ short-circuit
    }
    first.checkAllocation();

    std::vector<double> sum = first.samples_;
    std::vector<uint8_t> markers = first.markers_;

    for (size_t i = 1; i < args.size(); ++i) {
        std::string paramName = "wave_" + std::to_string(i + 1);
        Signal s = readWave(args[i], paramName, static_cast<int>(sum.size()), "add")->signal;
        s.checkAllocation();
        for (size_t j = 0; j < sum.size(); ++j) {
            sum[j] += s.samples_[j];
        }
        // OR markers if same length.
        if (s.markers_.size() == markers.size()) {
            for (size_t j = 0; j < markers.size(); ++j) {
                markers[j] |= s.markers_[j];
            }
        }
        // OR markerBits_ from each signal into the accumulator          // 0x2573c0
        for (size_t j = 0; j < s.markerBits_.size() && j < first.markerBits_.size(); ++j) {
            first.markerBits_[j] |= s.markerBits_[j];
        }
    }

    return Signal(sum, markers, first.markerBits_);
}

// gauss(length, amplitude, position, width) @0x24ddb0
//   length:    int >= 1
//   amplitude: double in [-1.0, 1.0]
//   position:  double — center sample index
//   width:     double — sigma in samples
// Returns a Gaussian envelope: amplitude * exp(-((i-position)^2) / (2*width^2)).
Signal WaveformGenerator::gauss(std::vector<Value> const& args) {           // 0x24ddb0
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "gauss", 4, args.size()));
    }

    int length;
    double amp, position, width;

    if (args.size() == 4) {
        length   = readInt(args[0], "length", 1, "gauss");
        amp      = readDoubleAmplitude(args[1], "amplitude", "gauss");
        position = readDouble(args[2], "position", "gauss");
        width    = readDouble(args[3], "width", "gauss");
    } else {
        // 3-arg form: gauss(length, position, width) — amplitude defaults to 1.0
        length   = readInt(args[0], "length", 1, "gauss");              // 0x24ddf9
        position = readDouble(args[1], "position", "gauss");
        width    = readDouble(args[2], "width", "gauss");
        amp      = 1.0;
    }

    std::vector<double> samples(static_cast<size_t>(length));
    double twoSigmaSq = 2.0 * width * width;
    if (twoSigmaSq == 0.0) {
        // Degenerate case: matches the binary which would emit NaN/Inf samples.
        // We mirror that by leaving the vector zero-initialized.
        return Signal(samples, /*channels=*/1);
    }
    for (int i = 0; i < length; ++i) {
        double dx = static_cast<double>(i) - position;
        samples[static_cast<size_t>(i)] = amp * std::exp(-(dx * dx) / twoSigmaSq);
    }
    return Signal(samples, /*channels=*/1);
}

// ============================================================================
// sin(length, amplitude, phase[, nPeriods]) @0x24a0f0
//   length:    int >= 1 (readPositiveInt)
//   amplitude: double in [-1.0, 1.0]
//   phase:     double (radians)
//   nPeriods:  double >= 0 (default 1.0)
// Loop: theta = 2*nPeriods*PI*i/length + phase; sample = amplitude * sin(theta)
Signal WaveformGenerator::sin(std::vector<Value> const& args) {                  // 0x24a0f0
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "sine", 3, args.size()));
    }

    int length;
    double amplitude, phase;

    if (args.size() == 4) {
        length    = readPositiveInt(args[0], "1 (length)", 1, "sine");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "sine");
        phase     = readDouble(args[2], "3 (phase)", "sine");
        double nPeriods  = readDouble(args[3], "4 (nPeriods)", "sine");

        // Validate nPeriods >= 0                                                // 0x24a963
        if (nPeriods < 0.0) {
            throw WaveformGeneratorValueException(
                ErrorMessages::format(ArgMustBePositive,
                                      "nPeriods", "sine"), 3);
        }

        Signal sig(static_cast<size_t>(length));                                 // 0x24a974
        if (length == 0) return sig;

        // Precompute: twoNPeriodsPi = 2 * nPeriods * PI                        // 0x24a983-0x24a98f
        double twoNPeriodsPi = 2.0 * nPeriods * M_PI;
        double dLength = static_cast<double>(length);

        for (int i = 0; i < length; ++i) {                                      // 0x24a9b0
            double theta = static_cast<double>(i) * twoNPeriodsPi / dLength + phase;
            sig.append(amplitude * std::sin(theta), 0);                          // 0x24a9d9
        }
        return sig;                                                              // 0x24a9e6
    } else {
        // 3-arg case: sine(length, amplitude, phase)
        // Produces a constant waveform: every sample = sin(amplitude + phase)
        // (GDB-verified: binary computes sin(amp+phase) for all samples)
        length    = readPositiveInt(args[0], "1 (length)", 1, "sine");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "sine");
        phase     = readDouble(args[2], "3 (phase)", "sine");

        Signal sig(static_cast<size_t>(length));
        if (length == 0) return sig;

        double value = std::sin(amplitude + phase);
        for (int i = 0; i < length; ++i) {
            sig.append(value, 0);
        }
        return sig;
    }
}
// cos(length, amplitude, phase[, nPeriods]) @0x24abd0
//   Identical to sin() but calls std::cos instead of std::sin.
Signal WaveformGenerator::cos(std::vector<Value> const& args) {                  // 0x24abd0
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "cosine", 3, args.size()));
    }

    int length;
    double amplitude, phase;

    if (args.size() == 4) {
        length    = readPositiveInt(args[0], "1 (length)", 1, "cosine");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "cosine");
        phase     = readDouble(args[2], "3 (phase)", "cosine");
        double nPeriods  = readDouble(args[3], "4 (nPeriods)", "cosine");

        if (nPeriods < 0.0) {
            throw WaveformGeneratorValueException(
                ErrorMessages::format(ArgMustBePositive,
                                      "nPeriods", "cosine"), 3);
        }

        Signal sig(static_cast<size_t>(length));
        if (length == 0) return sig;

        double twoNPeriodsPi = 2.0 * nPeriods * M_PI;
        double dLength = static_cast<double>(length);

        for (int i = 0; i < length; ++i) {                                      // 0x24b560
            double theta = static_cast<double>(i) * twoNPeriodsPi / dLength + phase;
            sig.append(amplitude * std::cos(theta), 0);                          // 0x24b589
        }
        return sig;
    } else {
        // 3-arg case: cosine(length, amplitude, phase)
        // Produces a constant waveform: every sample = cos(amplitude + phase)
        // (GDB-verified: same pattern as sine 3-arg)
        length    = readPositiveInt(args[0], "1 (length)", 1, "cosine");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "cosine");
        phase     = readDouble(args[2], "3 (phase)", "cosine");

        Signal sig(static_cast<size_t>(length));
        if (length == 0) return sig;

        double value = std::cos(amplitude + phase);
        for (int i = 0; i < length; ++i) {
            sig.append(value, 0);
        }
        return sig;
    }
}
// sinc(length, amplitude, position, beta) @0x24b6e0
//   length:    int >= 1 (readPositiveInt)
//   amplitude: double in [-1.0, 1.0]
//   position:  int >= 2 (readPositiveInt)
//   beta:      double (readDouble)
//
// The binary accepts 3 or 4 args (cmp rax,0x4/0x3), but the 3-arg path reads
// args[3] out of bounds — effectively UB. In practice sinc always takes 4 args.
// The error message (0x5b) reports "expected 3 arguments" because the binary's
// fallthrough happens to use the 3-arg error format from the size-check code.
//
// Loop: counter starts at -position, runs for length iterations.
//   If counter == 0: output amplitude (sinc(0) = 1).
//   Else: x = 2*PI*beta*counter/length; output amplitude * sin(x) / x.
// Warning if position > length (error 0x5f).
// Error if beta == 0 (floatEqual, error 0x62).
Signal WaveformGenerator::sinc(std::vector<Value> const& args) {                 // 0x24b6e0
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "sinc", 3, args.size()));
    }

    int length, position;
    double amplitude, beta;

    if (args.size() == 4) {
        length    = readPositiveInt(args[0], "1 (length)", 1, "sinc");           // 0x24b890
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "sinc");       // 0x24bae2
        position  = readPositiveInt(args[2], "2 (position)", 2, "sinc");         // 0x24bc0b
        beta      = readDouble(args[3], "4 (beta)", "sinc");                     // 0x24bf66
    } else {
        // 3-arg path: sinc(length, position, beta) — amplitude defaults to 1.0
        length    = readPositiveInt(args[0], "1 (length)", 1, "sinc");           // 0x24b9b6
        amplitude = 1.0;
        position  = readPositiveInt(args[1], "2 (position)", 2, "sinc");
        beta      = readDouble(args[2], "3 (beta)", "sinc");
    }

    // Warn if position > length                                                 // 0x24bf9c
    if (static_cast<unsigned int>(position) > static_cast<unsigned int>(length)) {
        if (warningCallback_) {
            warningCallback_(ErrorMessages::format(
                ArgLargerThanLength, "position", "sinc"));
        }
    }

    // beta == 0 → throw                                                         // 0x24bff3
    if (floatEqual(beta, 0.0)) {
        throw WaveformGeneratorException(
            ErrorMessages::format(ArgNotZero,
                                  "beta", "sinc"));
    }

    Signal sig(static_cast<size_t>(length));                                     // 0x24c006
    if (length == 0) return sig;

    // Precompute: twoPiBeta = 2 * beta * PI                                     // 0x24c019
    double twoPiBeta = 2.0 * beta * M_PI;
    double dLength = static_cast<double>(length);

    int counter = -position;                                                     // neg r13d @0x24c03a
    for (int remaining = length; remaining > 0; --remaining, ++counter) {        // 0x24c057
        if (counter == 0) {
            sig.append(amplitude, 0);                                            // 0x24c04a — sinc(0) = 1
        } else {
            double x = static_cast<double>(counter) * twoPiBeta / dLength;       // 0x24c064-0x24c069
            sig.append(amplitude * std::sin(x) / x, 0);                          // 0x24c07e-0x24c090
        }
    }
    return sig;                                                                  // 0x24c097
}

// ramp(length, startLevel, endLevel) @0x24c2c0
//   length:     int >= 1 (readInt)
//   startLevel: double, |val| <= 1.0
//   endLevel:   double, |val| <= 1.0
// If length >= 2: sample[i] = start + i * (end - start) / (length - 1)
// If length == 1: single sample = endLevel.
Signal WaveformGenerator::ramp(std::vector<Value> const& args) {                 // 0x24c2c0
    // Exact arg count check via vector byte size (0x78 = 3 * sizeof(Value))
    if (args.size() != 3) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "ramp", 3, args.size()));
    }

    int length       = readInt(args[0], "1 (length)", 1, "ramp");                // 0x24c3c1
    double startLevel = readDouble(args[1], "2 (startLevel)", "ramp");           // 0x24c4dd
    double endLevel   = readDouble(args[2], "3 (endLevel)", "ramp");             // 0x24c5e8

    // Validate |startLevel| <= 1.0 and |endLevel| <= 1.0                       // 0x24c617-0x24c64a
    if (std::fabs(startLevel) > 1.0) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(ArgMustBePositive,
                                  "startLevel", "ramp"), 1);
    }
    if (std::fabs(endLevel) > 1.0) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(ArgMustBePositive,
                                  "endLevel", "ramp"), 2);
    }

    Signal sig(static_cast<size_t>(length));                                     // 0x24c656
    if (length >= 2) {
        double delta = endLevel - startLevel;                                    // 0x24c666
        double divisor = static_cast<double>(length - 1);                        // 0x24c673-0x24c67e
        for (int i = 0; i < length; ++i) {                                       // 0x24c690
            double sample = startLevel + static_cast<double>(i) * delta / divisor;
            sig.append(sample, 0);                                               // 0x24c6b2
        }
    } else if (length == 1) {
        sig.append(endLevel, 0);                                                 // 0x24c6cf
    }
    return sig;                                                                  // 0x24c6d6
}
// sawtooth(length, amplitude, phase[, nPeriods]) @0x24c8b0
//   Delegates to genericTriangle with riseRatio = 1.0.
Signal WaveformGenerator::sawtooth(std::vector<Value> const& args) {             // 0x24c8b0
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "sawtooth", 3, args.size()));
    }

    double nPeriods = 1.0;                                                       // 3-arg default: 1.0 (GDB-verified)
    int length;
    double amplitude, phase;

    if (args.size() == 4) {
        length    = readPositiveInt(args[0], "1 (length)", 1, "sawtooth");       // 0x24ca67
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "sawtooth");   // 0x24ccbb
        phase     = readDouble(args[2], "3 (phase)", "sawtooth");                // 0x24cdd6
        nPeriods  = readDouble(args[3], "4 (nPeriods)", "sawtooth");             // 0x24d0df
    } else {
        length    = readPositiveInt(args[0], "1 (length)", 1, "sawtooth");       // 0x24cb93
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "sawtooth");
        phase     = readDouble(args[2], "3 (phase)", "sawtooth");
    }

    // Validate nPeriods >= 0                                                    // 0x24d063
    if (nPeriods < 0.0) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(ArgMustBePositive,
                                  "nPeriods", "sawtooth"), 3);
    }

    if (args.size() == 4) {
        return genericTriangle(length, amplitude, nPeriods, 1.0, phase);         // 0x24d14e — 4-arg path
    } else {
        // 3-arg path: binary swaps amp↔nPeriods and phase↔phase in registers
        return genericTriangle(length, nPeriods, phase, 1.0, amplitude);         // 0x24d14e — 3-arg path (GDB-verified)
    }
}
// triangle(length, amplitude, phase[, nPeriods]) @0x24d330
//   Delegates to genericTriangle with riseRatio = 0.5.
Signal WaveformGenerator::triangle(std::vector<Value> const& args) {             // 0x24d330
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "triangle", 3, args.size()));
    }

    double nPeriods = 1.0;                                                       // 3-arg default: 1.0 (GDB-verified)
    int length;
    double amplitude, phase;

    if (args.size() == 4) {
        length    = readPositiveInt(args[0], "1 (length)", 1, "triangle");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "triangle");
        phase     = readDouble(args[2], "3 (phase)", "triangle");
        nPeriods  = readDouble(args[3], "4 (nPeriods)", "triangle");
    } else {
        length    = readPositiveInt(args[0], "1 (length)", 1, "triangle");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "triangle");
        phase     = readDouble(args[2], "3 (phase)", "triangle");
    }

    // Validate nPeriods >= 0                                                    // 0x24dae3
    if (nPeriods < 0.0) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(ArgMustBePositive,
                                  "nPeriods", "triangle"), 3);
    }

    if (args.size() == 4) {
        return genericTriangle(length, amplitude, nPeriods, 0.5, phase);         // 0x24dbce — 4-arg path
    } else {
        // 3-arg path: binary swaps amp↔nPeriods and phase↔phase in registers
        return genericTriangle(length, nPeriods, phase, 0.5, amplitude);         // 0x24dbce — 3-arg path (GDB-verified)
    }
}
// drag(length, amplitude, position, sigma) @0x24e950
// drag(length, position, sigma)            — amplitude defaults to 1.0
//
// Computes a Gaussian-derivative (DRAG) pulse:
//   sample[i] = amp * (-(i - position) / sigma) * exp(-(i-position)^2 / (2*sigma^2))
//
// The amplitude is folded into a precomputed coefficient: amp * sigma.
// The derivative factor per sample is (position - i) / sigma^2.
// Combined: (position - i) / sigma^2 * amp * sigma = amp * (position - i) / sigma
//           = amp * (-(i - position) / sigma).
// Then multiplied by exp(-dx^2 / (2*sigma^2)).
//
// Disassembly notes:
//   - 3-arg: length, position, sigma.  Amplitude defaults to 1.0 from rodata @0x956030.
//   - 4-arg: length, amplitude(bounded), position, sigma.
//   - If sigma == 0 (floatEqual), throws WaveformGeneratorException(0x62).
//   - Position > length triggers a warning via the logger callback (error 0x5f).
//   - Constant @0x9562d0 = -1.0 used for precomputing -1.0/sigma.
//   - Sign-flip xorpd @0x24f2d5 negates (position - i) → (i - position).
Signal WaveformGenerator::drag(std::vector<Value> const& args) {               // 0x24e950
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "drag", 3, args.size()));
    }

    double amplitude = 1.0;
    double position, sigma;

    if (args.size() == 4) {
        int length = readPositiveInt(args[0], "1 (length)", 1, "drag");
        amplitude  = readDoubleAmplitude(args[1], "2 (amplitude)", "drag");
        position   = readDouble(args[2], "3 (position)", "drag");
        sigma      = readDouble(args[3], "4 (sigma)", "drag");

        // Warn if position > length                                    // 0x24f1f7
        if (position > static_cast<double>(length)) {
            // logger warning via error 0x5f — "position" / "drag"
            if (warningCallback_) {
                warningCallback_(ErrorMessages::format(
                    ArgLargerThanLength, "position", "drag"));
            }
        }

        // sigma == 0 → throw                                          // 0x24f257
        if (floatEqual(sigma, 0.0)) {
            throw WaveformGeneratorException(
                ErrorMessages::format(ArgNotZero,
                                      "sigma", "drag"));
        }

        Signal sig(static_cast<size_t>(length));                       // 0x24f273
        if (length == 0) return sig;

        // Precompute                                                   // 0x24f281
        // Binary loads exp(-0.5) from rodata @0x9562d0 (NOT -1.0 as originally assumed).
        // This normalizes the DRAG pulse so its peak = amplitude.
        double normFactor = std::exp(-0.5);          // @0x9562d0
        double scaledInv  = normFactor / sigma;      // exp(-0.5) / sigma
        double ampCoeff   = amplitude / scaledInv;   // amplitude * sigma / exp(-0.5)
        double sigmaSq    = sigma * sigma;
        double twoSigmaSq = sigmaSq + sigmaSq;      // 2 * sigma^2

        for (int i = 0; i < length; ++i) {                             // 0x24f2c0
            double dx  = position - static_cast<double>(i);
            double neg = -dx;                      // i - position (xorpd sign flip)
            double expArg = neg * dx / twoSigmaSq; // -(dx^2) / (2*sigma^2)
            double deriv  = dx / sigmaSq * ampCoeff;
            sig.append(std::exp(expArg) * deriv, 0);                   // 0x24f30d
        }
        return sig;                                                    // 0x24f31a
    }

    // 3-arg path: drag(length, position, sigma)
    int length = readPositiveInt(args[0], "1 (length)", 1, "drag");
    position   = readDouble(args[1], "2 (position)", "drag");
    sigma      = readDouble(args[2], "3 (sigma)", "drag");

    // Warn if position > length                                        // 0x24f102
    amplitude = 1.0;  // default from rodata @0x956030
    if (position > static_cast<double>(length)) {
        if (warningCallback_) {
            warningCallback_(ErrorMessages::format(
                ArgLargerThanLength, "position", "drag"));
        }
    }

    if (floatEqual(sigma, 0.0)) {                                      // 0x24f257
        throw WaveformGeneratorException(
            ErrorMessages::format(ArgNotZero,
                                  "sigma", "drag"));
    }

    Signal sig(static_cast<size_t>(length));                           // 0x24f273
    if (length == 0) return sig;

    double normFactor2 = std::exp(-0.5);
    double scaledInv2  = normFactor2 / sigma;
    double ampCoeff2   = amplitude / scaledInv2;
    double sigmaSq     = sigma * sigma;
    double twoSigmaSq  = sigmaSq + sigmaSq;

    for (int i = 0; i < length; ++i) {                                 // 0x24f2c0
        double dx  = position - static_cast<double>(i);
        double neg = -dx;
        double expArg = neg * dx / twoSigmaSq;
        double deriv  = dx / sigmaSq * ampCoeff2;
        sig.append(std::exp(expArg) * deriv, 0);
    }
    return sig;
}
// blackman(length, amplitude, alpha) @0x24f530
// blackman(length, alpha)            — amplitude defaults to 1.0
//
// Generalized Blackman window:
//   w[n] = amplitude * ((1-alpha)*0.5 - 0.5*cos(2*pi*n/(N-1)) + alpha*0.5*cos(4*pi*n/(N-1)))
//
// Standard Blackman uses alpha=0.16 → a0=0.42, a1=0.5, a2=0.08.
//
// Disassembly notes:
//   - 2-arg: length, alpha.  Amplitude defaults to 1.0 from rodata @0x956030.
//   - 3-arg: length, amplitude(bounded), alpha.
//   - Pre-loop: base = (1.0 - alpha) * 0.5; scaledAlpha = alpha * 0.5. (@0x24fb58)
//   - Loop: cos(2*pi*n/(N-1)) * 0.5 subtracted, cos(4*pi*n/(N-1)) * scaledAlpha added.
//   - Constants: @0x9562c8=0.5, @0x9562d8=2*pi, @0x9562e0=0.5, @0x9562e8=4*pi.
Signal WaveformGenerator::blackman(std::vector<Value> const& args) {           // 0x24f530
    if (args.size() != 2 && args.size() != 3) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "blackman", 2, args.size()));
    }

    double amplitude = 1.0;
    double alpha;

    if (args.size() == 3) {
        int length  = readPositiveInt(args[0], "1 (length)", 1, "blackman");
        amplitude   = readDoubleAmplitude(args[1], "2 (amplitude)", "blackman");
        alpha       = readDouble(args[2], "3 (alpha)", "blackman");

        Signal sig(static_cast<size_t>(length));                       // 0x24fb4a
        if (length == 0) return sig;

        double base        = (1.0 - alpha) * 0.5;                     // 0x24fb58
        double Nm1         = static_cast<double>(length - 1);
        double scaledAlpha = alpha * 0.5;

        for (int n = 0; n < length; ++n) {                            // 0x24fba0
            double nd = static_cast<double>(n);
            double w  = base
                      - 0.5 * std::cos(2.0 * M_PI * nd / Nm1)        // 0x24fbb0 → cos
                      + scaledAlpha * std::cos(4.0 * M_PI * nd / Nm1);// 0x24fbef → cos
            sig.append(w * amplitude, 0);                              // 0x24fc0b
        }
        return sig;
    }

    // 2-arg: blackman(length, alpha), amplitude = 1.0
    int length = readPositiveInt(args[0], "1 (length)", 1, "blackman");
    alpha      = readDouble(args[1], "2 (alpha)", "blackman");

    Signal sig(static_cast<size_t>(length));
    if (length == 0) return sig;

    double base        = (1.0 - alpha) * 0.5;
    double Nm1         = static_cast<double>(length - 1);
    double scaledAlpha = alpha * 0.5;

    for (int n = 0; n < length; ++n) {
        double nd = static_cast<double>(n);
        double w  = base
                  - 0.5 * std::cos(2.0 * M_PI * nd / Nm1)
                  + scaledAlpha * std::cos(4.0 * M_PI * nd / Nm1);
        sig.append(w * amplitude, 0);
    }
    return sig;
}
// hamming(length, amplitude) @0x24fd20
// hamming(length)            — amplitude defaults to 1.0
//
// Hamming window:  w[n] = amplitude * (0.54 - 0.46 * cos(2*pi*n / (N-1)))
//
// Disassembly notes:
//   - 1-arg: length only.  Amplitude defaults to 1.0 from rodata @0x956030.
//   - 2-arg: length, amplitude(bounded).
//   - Loop @0x250120: cos(2*pi*n/(N-1)), multiply by -0.46 (@0x9562f0),
//     add 0.54 (@0x9562f8), multiply by amplitude.
Signal WaveformGenerator::hamming(std::vector<Value> const& args) {            // 0x24fd20
    if (args.size() != 1 && args.size() != 2) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "hamming", 1, args.size()));
    }

    double amplitude = 1.0;

    if (args.size() == 2) {
        int length = readPositiveInt(args[0], "1 (length)", 1, "hamming");
        amplitude  = readDoubleAmplitude(args[1], "2 (amplitude)", "hamming");

        Signal sig(static_cast<size_t>(length));                       // 0x2500f3
        if (length == 0) return sig;

        double Nm1 = static_cast<double>(length - 1);                 // 0x250101
        for (int n = 0; n < length; ++n) {                            // 0x250120
            double nd = static_cast<double>(n);
            double w  = 0.54 - 0.46 * std::cos(2.0 * M_PI * nd / Nm1);
            sig.append(w * amplitude, 0);                              // 0x250157
        }
        return sig;
    }

    // 1-arg: hamming(length), amplitude = 1.0
    int length = readPositiveInt(args[0], "1 (length)", 1, "hamming");

    Signal sig(static_cast<size_t>(length));
    if (length == 0) return sig;

    double Nm1 = static_cast<double>(length - 1);
    for (int n = 0; n < length; ++n) {
        double nd = static_cast<double>(n);
        double w  = 0.54 - 0.46 * std::cos(2.0 * M_PI * nd / Nm1);
        sig.append(w * amplitude, 0);
    }
    return sig;
}
// hann(length, amplitude) @0x250250
// hann(length)            — amplitude defaults to 0.5
//
// Hann window:  w[n] = amplitude * (1.0 - cos(2*pi*n / (N-1)))
//
// Note: the standard Hann is 0.5*(1-cos(...)). Here amplitude defaults to 0.5
// from rodata @0x9562c8, and the formula is amp*(1-cos(...)).  For 2-arg form,
// the user-supplied amplitude is multiplied by 0.5 at @0x2505fe before the loop.
//
// Disassembly notes:
//   - 1-arg: length.  Default amplitude = 0.5 (from @0x9562c8).
//   - 2-arg: length, amplitude(bounded).  After readDoubleAmplitude, the result
//     is multiplied by 0.5 (@0x250603: mulsd xmm0, [0x9562c8]).
//   - Loop @0x250640: cos(2*pi*n/(N-1)), then (1.0 - cos) * amp.
Signal WaveformGenerator::hann(std::vector<Value> const& args) {               // 0x250250
    if (args.size() != 1 && args.size() != 2) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "hann", 1, args.size()));
    }

    double amplitude;

    if (args.size() == 2) {
        int length = readPositiveInt(args[0], "1 (length)", 1, "hann");
        amplitude  = readDoubleAmplitude(args[1], "2 (amplitude)", "hann");
        amplitude *= 0.5;                                              // 0x2505fe

        Signal sig(static_cast<size_t>(length));                       // 0x250616
        if (length == 0) return sig;

        double Nm1 = static_cast<double>(length - 1);                 // 0x250624
        for (int n = 0; n < length; ++n) {                            // 0x250640
            double nd = static_cast<double>(n);
            double w  = 1.0 - std::cos(2.0 * M_PI * nd / Nm1);       // 0x250658 → cos
            sig.append(w * amplitude, 0);                              // 0x250677
        }
        return sig;
    }

    // 1-arg: hann(length), amplitude = 0.5
    int length = readPositiveInt(args[0], "1 (length)", 1, "hann");
    amplitude  = 0.5;                                                  // from @0x9562c8

    Signal sig(static_cast<size_t>(length));
    if (length == 0) return sig;

    double Nm1 = static_cast<double>(length - 1);
    for (int n = 0; n < length; ++n) {
        double nd = static_cast<double>(n);
        double w  = 1.0 - std::cos(2.0 * M_PI * nd / Nm1);
        sig.append(w * amplitude, 0);
    }
    return sig;
}
// chirp @0x250bb0
//   3 args: (length, startFreq, stopFreq) — amplitude = 1.0, phase = 0
//   4 args: (length, startFreq, stopFreq, phase) — amplitude = 1.0
//   5 args: (length, amplitude, startFreq, stopFreq, phase)
// Linear frequency sweep: instantaneous freq = startFreq + freqRate * i
//   where freqRate = (stopFreq - startFreq) / length.
// Loop: theta = 2*PI*startFreq*i + phase + PI*freqRate*i^2
//       sample = amplitude * sin(theta)
// After loop: zero-pad until (total samples) % 16 == 0.
Signal WaveformGenerator::chirp(std::vector<Value> const& args) {                // 0x250bb0
    if (args.size() < 3 || args.size() > 5) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncArgs2or3,
                                  "chirp", 3, 4, args.size()));
    }

    int length;
    double amplitude, startFreq, stopFreq, phase;

    if (args.size() == 5) {
        length    = readPositiveInt(args[0], "1 (length)", 1, "chirp");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "chirp");
        startFreq = readDouble(args[2], "3 (startFrequency)", "chirp");
        stopFreq  = readDouble(args[3], "4 (stopFrequency)", "chirp");
        phase     = readDouble(args[4], "5 (phase)", "chirp");
    } else if (args.size() == 4) {
        // 4 args: chirp(length, startFreq, stopFreq, phase) — amplitude defaults to 1.0
        length    = readPositiveInt(args[0], "1 (length)", 1, "chirp");
        amplitude = 1.0;
        startFreq = readDouble(args[1], "2 (startFrequency)", "chirp");
        stopFreq  = readDouble(args[2], "3 (stopFrequency)", "chirp");
        phase     = readDouble(args[3], "4 (phase)", "chirp");
    } else {
        // 3 args: chirp(length, startFreq, stopFreq) — amplitude defaults to 1.0
        length    = readPositiveInt(args[0], "1 (length)", 1, "chirp");
        amplitude = 1.0;
        startFreq = readDouble(args[1], "2 (startFrequency)", "chirp");
        stopFreq  = readDouble(args[2], "3 (stopFrequency)", "chirp");
        phase     = 0.0;
    }

    Signal sig(static_cast<size_t>(length));                                     // 0x251a10
    if (length == 0) return sig;

    // Precompute                                                                // 0x251a26-0x251a55
    double freqRate = (stopFreq - startFreq) / static_cast<double>(length);
    double twoPi = 2.0 * M_PI;                                                  // @0x9562d8
    double f0 = twoPi * startFreq;       // 2*PI*startFreq                       // 0x251a40
    double k  = twoPi * freqRate * 0.5;  // PI * freqRate                        // 0x251a4d-0x251a55

    for (int i = 0; i < length; ++i) {                                           // 0x251a60
        double di = static_cast<double>(i);
        double theta = f0 * di + phase + k * di * di;                            // 0x251a68-0x251a7f
        sig.append(amplitude * std::sin(theta), 0);                              // 0x251a92
    }

    // Zero-pad to 16-sample alignment                                           // 0x251a9f
    if ((length & 0xf) != 0) {
        int padded = length + 1;                                                 // 0x251aa5: inc r14d
        do {
            sig.append(0.0, 0);                                                  // 0x251ab0-0x251ab9
        } while ((padded++ & 0xf) != 0);                                         // 0x251ac3: test then inc
    }

    return sig;                                                                  // 0x251acc
}
// mask(args) @0x251cb0 — 1-line wrapper: tail-calls markerImpl(args, true)
Signal WaveformGenerator::mask(std::vector<Value> const& args) {               // 0x251cb0
    return markerImpl(args, /*isMask=*/true);                                  // 0x251cbe
}
// marker(args) @0x251cd0 — 1-line wrapper: tail-calls markerImpl(args, false)
Signal WaveformGenerator::marker(std::vector<Value> const& args) {             // 0x251cd0
    return markerImpl(args, /*isMask=*/false);                                 // 0x251cdb
}
// rand(length, amplitude, mean, stddev) @0x251cf0
// rand(length, amplitude, mean)          — stddev defaults to 1.0
//
// Generates Gaussian random noise using the TLS mt19937_64 PRNG
// (GlobalResources::random) with std::normal_distribution.
//
// Disassembly notes:
//   - 3-arg path starts at 0x251d31: (length, amplitude, mean), stddev = 1.0
//   - 4-arg path starts at 0x251d6e: (length, amplitude, mean, stddev)
//   - readPositiveInt for "1 (length)" min=1
//   - readDoubleAmplitude for "2 (amplitude)"
//   - readDouble for mean (param name "2 (mean)" or "3 (mean)" via inc trick)
//   - readDouble for stddev (param name "3 (standard deviation)" or "4 (...)")
//   - Default stddev = 1.0 loaded from rodata @0x956030
//   - Signal::Signal(size_t) at 0x25254e constructs output
//   - Generation loop at 0x252580+ uses an inline Park-Miller MINSTD LCG
//     (multiplier 48271, modulus 2^31-1, state reset to 1 each call) with
//     a Marsaglia polar Box-Muller transform.  This is the ONLY rand*
//     function that does NOT use the MT19937_64 in GlobalResources::random.
//   - Samples are: amplitude * (mean + stddev * normal_polar())
Signal WaveformGenerator::rand(std::vector<Value> const& args) {               // 0x251cf0
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "rand", 4, args.size()));
    }

    int length;
    double amplitude;
    double mean;
    double stddev = 1.0;  // default from rodata @0x956030

    if (args.size() == 4) {                                                    // 0x251d6e
        length    = readPositiveInt(args[0], "1 (length)", 1, "rand");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "rand");
        mean      = readDouble(args[2], "2 (mean)", "rand");                   // 0x252205
        stddev    = readDouble(args[3], "3 (standard deviation)", "rand");      // 0x25244d
    } else {  // 3 args                                                        // 0x251d31
        length    = readPositiveInt(args[0], "1 (length)", 1, "rand");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "rand");
        mean      = readDouble(args[2], "3 (mean)", "rand");                   // 0x252321
        // stddev remains 1.0; the binary loads 1.0 from @0x956030 and
        // branches past the 4th readDouble.                                   // 0x252452
    }

    Signal sig(static_cast<size_t>(length));                                   // 0x25254e
    if (length == 0) return sig;

    // The binary's `rand` uses a Park-Miller MINSTD LCG (multiplier 48271,
    // modulus 2^31 - 1) with state RESET TO 1 at the start of each call,
    // then a Marsaglia polar Box-Muller transform.  It does NOT use the
    // shared MT19937_64 in GlobalResources::random — that's reserved for
    // randomGauss/randomUniform (and re-seeded by randomSeed()).
    // See prng_libcxx.cpp for algorithm details and binary cross-refs.
    std::vector<double> samples(length);
    seqc_minstd_normal_amplitude(amplitude, mean, stddev,
                                  samples.data(), static_cast<size_t>(length));
    for (int i = 0; i < length; ++i) {                                        // 0x252580
        sig.append(samples[i], 0);
    }
    return sig;
}

// randomGauss(length, amplitude, mean, stddev) @0x252930
// randomGauss(length, amplitude, mean)          — stddev defaults to 1.0
//
// Identical structure to rand() but with function name "randomGauss" in error
// messages. The disassembly is nearly identical (same TLS mt19937_64,
// same normal_distribution, same 3-or-4-arg paths).
//
// Disassembly notes:
//   - Function name string "randomGauss" built at 0x252952
//   - 3-arg path at 0x252991: (length, amplitude, mean), stddev = 1.0
//   - 4-arg path at 0x25298b: (length, amplitude, mean, stddev)
//   - readPositiveInt for "1 (length)" min=1 at 0x252ae6
//   - readDoubleAmplitude for "2 (amplitude)" at 0x252d10
//   - readDouble for "2 (mean)" at 0x252e19
//   - readDouble for "3 (standard deviation)" at 0x253045
//   - Default stddev = 1.0 from rodata @0x956030
Signal WaveformGenerator::randomGauss(std::vector<Value> const& args) {        // 0x252930
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "randomGauss", 4, args.size()));
    }

    int length;
    double amplitude;
    double mean;
    double stddev = 1.0;  // default from rodata @0x956030

    if (args.size() == 4) {                                                    // 0x25298b
        length    = readPositiveInt(args[0], "1 (length)", 1, "randomGauss");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "randomGauss");
        mean      = readDouble(args[2], "2 (mean)", "randomGauss");
        stddev    = readDouble(args[3], "3 (standard deviation)", "randomGauss");
    } else {  // 3 args                                                        // 0x252991
        length    = readPositiveInt(args[0], "1 (length)", 1, "randomGauss");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "randomGauss");
        mean      = readDouble(args[2], "3 (mean)", "randomGauss");
    }

    Signal sig(static_cast<size_t>(length));                                   // 0x2531c5
    if (length == 0) return sig;

    std::vector<double> samples(length);
    seqc_libcxx_normal_amplitude(GlobalResources::random, amplitude, mean, stddev,
                                  samples.data(), static_cast<size_t>(length));
    for (int i = 0; i < length; ++i) {
        sig.append(samples[i], 0);
    }
    return sig;
}

// randomUniform(length, amplitude) @0x253440
// randomUniform(length)            — amplitude defaults to 1.0
//
// Generates uniformly-distributed random noise in [-amplitude, +amplitude]
// using the TLS mt19937_64 PRNG with uniform_real_distribution.
//
// Disassembly notes:
//   - 1-arg path at 0x2534a4: length only, amplitude = 1.0
//   - 2-arg path at 0x2534a2: (length, amplitude)
//   - readPositiveInt for "1 (length)" min=1
//   - readDoubleAmplitude for "2 (amplitude)" (2-arg only)
//   - Default amplitude = 1.0 from rodata @0x956030
//   - After parameter read, accesses GlobalResources::random TLS @0x253814
//   - Loop at 0x2538d0: generates uniform random in [-amp, +amp]
//   - XOR negate amplitude for min: xorpd with sign-mask @0x8fc5e0
//   - Range = 2*amplitude (addsd xmm0,xmm0 at 0x2538a1)
Signal WaveformGenerator::randomUniform(std::vector<Value> const& args) {      // 0x253440
    if (args.size() != 1 && args.size() != 2) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "randomUniform", 2, args.size()));
    }

    double amplitude = 1.0;  // default from rodata @0x956030

    int length;
    if (args.size() == 2) {                                                    // 0x2534a2
        length    = readPositiveInt(args[0], "1 (length)", 1, "randomUniform");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "randomUniform");
    } else {  // 1 arg                                                         // 0x2534a4
        length    = readPositiveInt(args[0], "1 (length)", 1, "randomUniform");
    }

    Signal sig(static_cast<size_t>(length));                                   // 0x25382e
    if (length == 0) return sig;

    // The binary computes min = -amplitude, max = +amplitude
    // and uses uniform_real_distribution<double>(min, max).
    std::vector<double> samples(length);
    seqc_libcxx_uniform(GlobalResources::random, -amplitude, amplitude,
                         samples.data(), static_cast<size_t>(length));
    for (int i = 0; i < length; ++i) {                                        // 0x2538d0
        sig.append(samples[i], 0);
    }
    return sig;
}

// lfsrGaloisMarker(samples, marker, polynomial, initial) @0x253bc0
//
// Generates a Linear Feedback Shift Register (Galois configuration)
// marker pattern. The output signal has zero-valued samples but the
// marker bits encode the LFSR bit stream.
//
// Disassembly notes:
//   - checkArgCount 4 args (error 0x5b thrown at 0x25408e if wrong count)
//   - readPositiveInt "1 (samples)"    min=1  at 0x253cc8 → length
//   - readPositiveInt "2 (marker)"     min=2  at 0x253dd1 → markerBit
//   - readPositiveInt "3 (polynomial)" min=3  at 0x253ee9 → poly
//   - readPositiveInt "4 (initial)"    min=4  at 0x253feb → initial
//   - Error 0x65 thrown at 0x254160 if initial==0 ("initial must be non-zero")
//   - Error 0x64 thrown at 0x254111 if markerBit validation fails
//   - Signal::Signal(size_t) at 0x254027
//   - Core LFSR loop at 0x254056:
//       shr r15d, 1        ; state >>= 1
//       test bl, 0x1       ; test LSB of old state
//       je skip_xor        ; if LSB==0, append(0.0, 0)
//       append(0.0, marker); xor r15d, r13d  ; state ^= poly
//     Loop continues for `length` iterations.
Signal WaveformGenerator::lfsrGaloisMarker(std::vector<Value> const& args) {   // 0x253bc0
    checkArgCount(args, "lfsrGaloisMarker", 4);

    int length   = readPositiveInt(args[0], "1 (samples)", 1, "lfsrGaloisMarker");    // 0x253cc8
    int marker   = readPositiveInt(args[1], "2 (marker)", 2, "lfsrGaloisMarker");     // 0x253dd1
    int poly     = readPositiveInt(args[2], "3 (polynomial)", 3, "lfsrGaloisMarker"); // 0x253ee9
    int initial  = readPositiveInt(args[3], "4 (initial)", 4, "lfsrGaloisMarker");    // 0x253feb

    if (initial == 0) {                                                        // 0x254018 → 0x254160
        throw WaveformGeneratorException(
            ErrorMessages::format(LfsrInitZero,
                                  "lfsrGaloisMarker"));
    }

    // Marker must be exactly 1 or 2 (bit position).
    // Binary encodes this as: (unsigned)(marker - 3) <= 0xfffffffd → throw.
    // Valid: marker ∈ {1, 2}.  @0x253e0b–0x253e14.
    if (marker < 1 || marker > 2) {                                            // 0x253e14 → 0x254111
        throw WaveformGeneratorException(
            ErrorMessages::format(ValueMustBe1or2,
                                  marker, "lfsrGaloisMarker"));                // 0x254129
    }
    uint8_t markerByte = static_cast<uint8_t>(marker);

    Signal sig(static_cast<size_t>(length));                                   // 0x254027
    if (length == 0) return sig;

    uint32_t state = static_cast<uint32_t>(initial);                           // r15d = initial at 0x254035
    for (int i = 0; i < length; ++i) {                                         // 0x254056
        uint32_t lsb = state & 1;
        state >>= 1;                                                           // shr r15d, 1
        if (lsb) {
            sig.append(0.0, markerByte);                                       // 0x254068
            state ^= static_cast<uint32_t>(poly);                              // xor r15d, r13d
        } else {
            sig.append(0.0, 0);                                                // 0x254049
        }
    }
    return sig;
}
// rrc(length, amplitude, position, beta, width) @0x254290
// rrc(length, amplitude, position, beta)        — width defaults to 1.0
// rrc(length, position, beta)                   — amplitude=1.0, width=1.0
//
// Root-raised cosine (RRC) filter impulse response:
//   t = (i - position) * width     (width = 1/samplesPerSymbol)
//
//   t == 0:       h = 1 - beta + 4*beta/pi
//   t == ±1/(4β): h = (beta/sqrt(2)) * ((1+2/pi)*sin(pi/(4β)) + (1-2/pi)*cos(pi/(4β)))
//                   = (beta/2) * (sqrt(2)*(1+2/pi)*sin(pi/(4β)) + sqrt(2)*(1-2/pi)*cos(pi/(4β)))
//   otherwise:    h = (sin((1-β)πt) + 4βt*cos((1+β)πt)) / (πt * (1 - (4βt)^2))
//
//   result = amplitude * h
//
// Disassembly notes:
//   - 3/4/5-arg forms; missing params default to 1.0.
//   - Position > length triggers warning (error 0x5f).
//   - Constants: @0x956098=4.0, @0x956068=pi, @0x9562a8=2.0, @0x956278=pi,
//     @0x956310=sqrt(2)*(1-2/pi)(?), @0x956318=sqrt(2)*(1+2/pi)(?).
//   - Special cases checked via floatEqual() at @0x25529a, @0x2552bc, @0x2552d2.
Signal WaveformGenerator::rrc(std::vector<Value> const& args) {                // 0x254290
    if (args.size() < 3 || args.size() > 5) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "rrc", 3, args.size()));
    }

    int length;
    double amplitude = 1.0;
    double position, beta, width = 1.0;

    if (args.size() == 5) {
        length    = readPositiveInt(args[0], "1 (length)", 1, "rrc");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "rrc");
        position  = readDouble(args[2], "3 (position)", "rrc");
        beta      = readDouble(args[3], "4 (beta)", "rrc");
        width     = readDouble(args[4], "5 (width)", "rrc");
    } else if (args.size() == 4) {
        length    = readPositiveInt(args[0], "1 (length)", 1, "rrc");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "rrc");
        position  = readDouble(args[2], "3 (position)", "rrc");
        beta      = readDouble(args[3], "4 (beta)", "rrc");
    } else {
        length   = readPositiveInt(args[0], "1 (length)", 1, "rrc");
        position = readDouble(args[1], "2 (position)", "rrc");
        beta     = readDouble(args[2], "3 (position)", "rrc");
    }

    // Warn if position > length                                        // 0x254e4b
    if (position > static_cast<double>(length)) {
        if (warningCallback_) {
            warningCallback_(ErrorMessages::format(
                ArgLargerThanLength, "position", "rrc"));
        }
    }

    Signal sig(static_cast<size_t>(length));                           // 0x2551cd
    if (length == 0) return sig;

    // Precompute                                                       // 0x2551e0
    double fourBeta       = 4.0 * beta;
    double invFourBeta    = 1.0 / fourBeta;       // t1 = 1/(4β)
    double piOverFourBeta = M_PI / fourBeta;       // π/(4β)
    double oneMinusBeta   = 1.0 - beta;
    double onePlusBeta    = 1.0 + beta;            // 1 + β
    double betaHalf       = beta / 2.0;            // β/2
    double piOverFourBetaAlt = M_PI / fourBeta;    // same as piOverFourBeta
    // t==0 value: fourBeta/pi + 1 - beta = 4β/π + 1 - β               // stored @ [2a8]
    double tZeroValue     = fourBeta / M_PI + oneMinusBeta;

    for (int i = 0; i < length; ++i) {                                 // 0x255280
        double t = (static_cast<double>(i) - position) * width;
        double h;

        if (floatEqual(t, 0.0)) {                                     // 0x25529a
            // Special case: t == 0
            h = tZeroValue;                                            // 0x2553b0
        } else if (floatEqual(t, invFourBeta) ||
                   floatEqual(t, -invFourBeta)) {                      // 0x2552bc, 0x2552d2
            // Special case: t == ±1/(4β)
            double s = std::sin(piOverFourBeta);                       // 0x2552e3
            double c = std::cos(piOverFourBeta);                       // 0x2552f5
            // h = betaHalf * (C2*sin + C1*cos)
            // where C1 = sqrt(2)*(1-2/pi), C2 = sqrt(2)*(1+2/pi) [rodata @956310, @956318]
            // This equals: (β/√2) * ((1+2/π)*sin(π/(4β)) + (1-2/π)*cos(π/(4β)))
            static const double C1 = std::sqrt(2.0) * (1.0 - 2.0 / M_PI);  // @0x956310
            static const double C2 = std::sqrt(2.0) * (1.0 + 2.0 / M_PI);  // @0x956318
            h = betaHalf * (C2 * s + C1 * c);                         // 0x255313
        } else {
            // General case                                            // 0x25531d
            double piT        = t * M_PI;
            double sinTerm    = std::sin(oneMinusBeta * piT);          // 0x25533e
            double fourBetaT  = t * fourBeta;                          // 0x255350
            double cosTerm    = std::cos(onePlusBeta * piT);           // 0x25536d
            double numerator  = sinTerm + fourBetaT * cosTerm;         // 0x25537f
            double fourBetaT2 = fourBetaT * fourBetaT;                 // 0x255387
            double denominator = piT * (1.0 - fourBetaT2);            // 0x25539b
            h = numerator / denominator;                               // 0x2553a3
        }

        sig.append(h * amplitude, 0);                                  // 0x2553be
    }
    return sig;                                                        // 0x2553d4
}
// vect(d1, d2, ...) @0x255570
//
// Builds a Signal directly from numeric arguments. Each arg is read as
// a double and appended to the output sample vector. Maximum 100 elements
// (error 0xe1 thrown at 0x2555b8 if >= 101 args).
//
// Disassembly notes:
//   - Arg count check: (size / sizeof(Value)) >= 0x65 (101) → error 0xe1
//   - Signal::Signal(size_t) at 0x25560b — creates output with args.size() slots
//   - Loop at 0x255656: iterates args, reads each as double via Value union
//     type dispatch (int→cvtsi2sd, double→direct, bool→movzx, string→error)
//   - Each extracted double is appended via Signal::append(double, 0)
//   - Param names are dynamic: to_string(i+1) + " (waveform)"
Signal WaveformGenerator::vect(std::vector<Value> const& args) {               // 0x255570
    if (args.size() >= 101) {                                                  // 0x2555a4
        throw WaveformGeneratorException(
            ErrorMessages::format(VectTooManyArgs, 100));
    }

    Signal sig(args.size());                                                   // 0x25560b
    if (args.empty()) return sig;

    for (size_t i = 0; i < args.size(); ++i) {                                // 0x255656
        std::string paramName = std::to_string(i + 1) + " (waveform)";
        // The binary does inline Value type dispatch here to extract a double.
        // For reconstruction we use readDouble which does the same thing.
        double val = readDouble(args[i], paramName, "vect");
        sig.append(val, 0);
    }
    return sig;
}

// placeholder(length, [marker0], [marker1]) @0x255850
//
// Creates a Signal with reserveOnly_=true (lazy allocation).
// Used as a placeholder for waveforms whose contents will be filled later.
//
// Disassembly notes:
//   - Accepts 0-3 args (at 0x255898: if args empty or size >= 4, throws 0x5b)
//     Wait — actually the check is: if args.size()==0 → error; size >= 4 → error.
//   - readPositiveInt "1 (samples)" min=1 at 0x25597a → length
//   - If >= 2 args: readInt "2 (marker0)" min=2 at 0x255a9c → marker0 (0 or non-0)
//   - If >= 3 args: readInt "3 (marker1)" min=3 at 0x255bb2 → marker1
//   - After reading markers, constructs Signal via Signal(ReserveOnly, length, markerBits)
//     at 0x255c09 (or similar ctor that sets reserveOnly_=true)
Signal WaveformGenerator::placeholder(std::vector<Value> const& args) {        // 0x255850
    if (args.empty() || args.size() >= 4) {                                    // 0x255898
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "placeholder", 1, args.size()));
    }

    int length = readPositiveInt(args[0], "1 (samples)", 1, "placeholder");    // 0x25597a

    bool hasMarker0 = false;
    bool hasMarker1 = false;
    if (args.size() >= 2) {                                                    // 0x2559c6
        int m0 = readInt(args[1], "2 (marker0)", 2, "placeholder");            // 0x255a9c
        hasMarker0 = (m0 != 0);
    }
    if (args.size() >= 3) {                                                    // 0x255aeb
        int m1 = readInt(args[2], "3 (marker1)", 3, "placeholder");            // 0x255bb2
        hasMarker1 = (m1 != 0);
    }

    // Build markerBits vector — always single-channel for placeholder.
    // Marker0 = bit 0, Marker1 = bit 1, combined into one byte.
    // Binary @0x255bf0: or r13b,r15b then stores single byte.
    MarkerBitsPerChannel markerBits;
    uint8_t bits = 0;
    if (hasMarker0) bits |= 1;
    if (hasMarker1) bits |= 2;
    if (bits != 0) markerBits.push_back(bits);

    // Construct with reserveOnly=true
    ReserveOnly tag;
    return Signal(tag, static_cast<size_t>(length), markerBits);               // 0x255c09
}

// join(sig1, sig2, ...) @0x255da0
//
// Concatenates multiple signals by appending their samples end-to-end.
// All signals must have the same channel count.
//
// Disassembly notes (4688 bytes, complex):
//   - No fixed arg count check — must have >= 1 arg
//   - First arg read at offset 0x00 in args vector
//   - Signal::Signal(size_t, MarkerBitsPerChannel const&) at 0x25636b — output
//   - Signal::append(Signal&) called at 0x256408 to concatenate each signal
//   - Signal::checkAllocation() called multiple times (0x256416, 0x256429, etc.)
//   - For reserveOnly_ signals: copies through with reserveOnly_ semantics
//   - Channel count validation between signals
Signal WaveformGenerator::join(std::vector<Value> const& args) {               // 0x255da0
    if (args.size() < 1) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "join", 1, args.size()));
    }

    // Binary @0x255e82: iterates all args; type==4 (String) → wave,
    // type!=4 → numeric value (interpolation/target length).
    // Collect wave signals and optional numeric parameter.
    std::vector<std::pair<size_t, Signal>> waves;   // (arg index, signal)
    int requestedLength = 0;

    for (size_t i = 0; i < args.size(); ++i) {
        if (static_cast<int>(args[i].type_) == 4) {
            // String → wave reference
            std::string paramName = "wave_" + std::to_string(i + 1);
            Signal s = readWave(args[i], paramName, -1, "join")->signal;
            waves.emplace_back(i, std::move(s));
        } else {
            // Non-String → numeric parameter (interpolation length)
            requestedLength = args[i].toInt();
        }
    }

    if (waves.empty()) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncExactArgs2,
                                  "join", 1, args.size()));
    }

    auto& first = waves[0].second;
    size_t interpLen = (requestedLength > 0 && waves.size() > 1)
                           ? static_cast<size_t>(requestedLength)
                           : 0;

    if (first.reserveOnly_) {                                                  // reserveOnly short-circuit
        size_t totalLength = first.length_;
        for (size_t i = 1; i < waves.size(); ++i) {
            totalLength += waves[i].second.length_;
        }
        if (interpLen > 0) {
            // Per join boundary: interpLen interp samples.
            // Plus one trailing block of interpLen zeros after last wave.
            totalLength += waves.size() * interpLen;
        }
        ReserveOnly tag;
        return Signal(tag, totalLength, first.markerBits_);
    }

    first.checkAllocation();

    // Compute total length (frames) for all wave signals.
    size_t totalLength = first.length_;
    for (size_t i = 1; i < waves.size(); ++i) {
        waves[i].second.checkAllocation();
        totalLength += waves[i].second.length_;
    }
    if (interpLen > 0) {
        totalLength += waves.size() * interpLen;
    }

    // Create output signal seeded from first wave's markerBits (channels
    // derived from markerBits.size()). This matches the original
    // Signal::Signal(size_t, MarkerBitsPerChannel) ctor at 0x25636b.
    Signal result(totalLength, first.markerBits_);                             // 0x25636b

    if (interpLen == 0) {
        // No interpolation: pure append. Preserves original semantics
        // (markerBits OR-merged across waves, length recomputed by append).
        for (size_t i = 0; i < waves.size(); ++i) {
            auto& s = waves[i].second;
            s.checkAllocation();
            result.append(s);
        }
        return result;
    }

    // Interpolation path: emit per-wave samples followed by either an
    // interpolation ramp (between waves) or a zero-pad block (after last).
    // Re-init result.samples_/markers_ as empty (the ctor reserved them).
    uint16_t channels = std::max<uint16_t>(first.channels_, 1);
    result.channels_ = channels;
    result.samples_.clear();
    result.markers_.clear();

    for (size_t wi = 0; wi < waves.size(); ++wi) {
        auto& s = waves[wi].second;
        s.checkAllocation();

        // Append this wave's samples + markers.
        size_t nSamples = s.length_ * channels;
        result.samples_.insert(result.samples_.end(),
                               s.samples_.begin(),
                               s.samples_.begin() + nSamples);
        if (!s.markers_.empty()) {
            result.markers_.insert(result.markers_.end(),
                                   s.markers_.begin(),
                                   s.markers_.begin() + nSamples);
        } else {
            result.markers_.insert(result.markers_.end(), nSamples, uint8_t(0));
        }

        // OR this wave's markerBits into the result's markerBits.
        size_t mbSize = result.markerBits_.size();
        for (size_t i = 0; i < mbSize && i < s.markerBits_.size(); ++i) {
            result.markerBits_[i] |= s.markerBits_[i];
        }

        if (wi + 1 < waves.size()) {
            // Linear ramp from this wave's last frame to next wave's first frame.
            auto& next = waves[wi + 1].second;
            next.checkAllocation();
            for (size_t k = 0; k < interpLen; ++k) {
                double t = static_cast<double>(k + 1) /
                           static_cast<double>(interpLen);
                for (uint16_t c = 0; c < channels; ++c) {
                    size_t lastIdx = (s.length_ - 1) * channels + c;
                    size_t firstIdx = c;
                    double a = s.samples_[lastIdx];
                    double b = next.samples_[firstIdx];
                    result.samples_.push_back(a + t * (b - a));
                    result.markers_.push_back(0);
                }
            }
        } else {
            // Trailing zero-pad of interpLen frames after the last wave.
            for (size_t k = 0; k < interpLen; ++k) {
                for (uint16_t c = 0; c < channels; ++c) {
                    result.samples_.push_back(0.0);
                    result.markers_.push_back(0);
                }
            }
        }
    }

    result.length_ = result.samples_.size() / channels;
    return result;
}

// interleave(sig1, sig2) @0x258140
//
// Interleaves two single-channel signals into a multi-channel signal.
// This is a thin wrapper: calls merge(args) then sets the interleaved flag.
//
// Disassembly notes:
//   - Tail-calls merge() at 0x258154
//   - After merge returns, sets result.channels_ = 1 at 0x258159
//     (mov WORD PTR [rbx+0x48], 0x1) — this effectively marks it as
//     interleaved rather than multi-channel. The samples are already
//     interleaved by merge; setting channels_=1 means "treat the
//     interleaved samples as a single logical channel."
//   - Then manipulates the markers vector (extending/truncating)
//
// NOTE: The marker manipulation after merge (0x25815f-0x25822e) is
// complex — involves vector reallocation. Needs more analysis.
Signal WaveformGenerator::interleave(std::vector<Value> const& args) {         // 0x258140
    Signal result = merge(args);                                               // 0x258154
    result.channels_ = 1;                                                      // 0x258159: WORD PTR [rbx+0x48] = 1

    // The binary then adjusts the markerBits_ vector length.
    // At 0x25815f: r14 = result.markerBits_.begin() (+0x30), rdi = result.markerBits_.end() (+0x38)
    // It ensures markerBits_.size() == 1 (either truncating or extending with 0).
    // This is because after setting channels_=1, only one marker-bits entry is needed.
    if (result.markerBits_.size() > 1) {
        result.markerBits_.resize(1);
    } else if (result.markerBits_.empty()) {
        result.markerBits_.push_back(0);
    }

    // 0x258232-25823d: length_ = samples_.size()
    // Binary: mov 0x8(%rbx),%rax; sub (%rbx),%rax; sar $0x3,%rax; mov %rax,0x50(%rbx)
    // This overwrites length_ with the actual sample count AFTER markerBits resize.
    result.length_ = result.samples_.size();

    return result;
}
// multiply(sig1, sig2, ...) @0x258750
//
// Pointwise multiplication of two or more waveform signals.
// Does NOT use readWave(); manually loads waveforms via wavetableFront_.
//
// Phase 1 (0x258826..0x258d0b): Load all waveforms by name:
//   - Each arg must be type 4 (wave ref), else error AddExpectsMultiWave (83)
//   - waveformExists check, else WaveformGeneratorValueException UnknownWaveform (90)
//   - getWaveformByName + loadWaveform
//   - Track maxNSamples across all waveforms
//   - Validate channels_ match (first sets expected; others must match or
//     InconsistentChannels (229) error)
//   - markerBits_ OR'd across all waveforms
//   - allReserveOnly = AND of all reserveOnly_ flags
//
// Phase 2 (0x258d0b..0x258d3c): Construct output:
//   - If allReserveOnly → Signal(ReserveOnly, maxNSamples, markerBits)
//   - Else → Signal(maxNSamples, markerBits)
//
// Phase 3 (0x258d3c..0x2593a1): Per-sample multiply loop:
//   - totalSamples = maxNSamples * channels (flat interleaved)
//   - For each sample index, iterate all waveforms:
//     - reserveOnly waveforms get samples/markers padded to channels*nSamples (zeros)
//     - If waveform too short for index → product=0, marker=0, break
//     - product *= wf.samples_[idx]; marker *= wf.markers_[idx] (byte mul)
//   - Signal::append(product, marker) at 0x259384
//   - Track |product|>1.0 → AmplitudeClipped (84) warning at 0x2593ba
Signal WaveformGenerator::multiply(std::vector<Value> const& args) {           // 0x258750
    // --- Arg count check ---                                                     // 0x258795
    if (args.size() < 2) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncMinArgs,
                                  "multiply", 2, args.size()));                    // 0x25965e
    }

    // --- Phase 1: Load all waveforms, validate channels, collect metadata ---
    std::vector<std::shared_ptr<WaveformFront>> waveforms;                         // [rbp-0xf0]
    MarkerBitsPerChannel markerBits;                                               // [rbp-0xb0]

    bool allReserveOnly = true;                                                    // 0x2587d5: sil=1
    int16_t channels = 0;                                                          // [rbp-0x68]
    int maxNSamples = 0;                                                           // r8d

    for (size_t i = 0; i < args.size(); ++i) {                                    // 0x258826
        // Each arg must be type 4 (wave reference)                                // 0x258838
        if (static_cast<int>(args[i].type_) != 4) {
            throw WaveformGeneratorException(                                      // 0x259545
                errMsg[AddExpectsMultiWave]);
        }

        // Check waveform exists                                                   // 0x258862
        std::string name = args[i].toString();
        if (!wavetableFront_->waveformExists(name)) {
            throw WaveformGeneratorValueException(                                 // 0x2594ce
                ErrorMessages::format(UnknownWaveform,
                                      "multiply", args[i].toString()),
                i + 1);                                                            // 0x259515: rdx = i+1
        }

        // Load waveform                                                           // 0x2588f3
        std::optional<std::string> optName(name);
        auto wf = wavetableFront_->getWaveformByName(optName);
        wavetableFront_->loadWaveform(wf);                                         // 0x258968

        // Track max nSamples                                                      // 0x2589a3
        Signal const& sig = wf->signal;
        int nSamples = static_cast<int>(sig.length_);
        if (nSamples > maxNSamples) {
            maxNSamples = nSamples;
        }

        uint16_t sigChannels = sig.channels_;                                      // 0x2589b1

        if (i != 0) {                                                              // 0x2589bd
            // Check channel count matches first waveform                          // 0x2589c7
            if (static_cast<int16_t>(channels) != static_cast<int>(sigChannels)) {
                throw WaveformGeneratorValueException(                             // 0x259593
                    ErrorMessages::format(InconsistentChannels,
                                          "multiply", args[i].toString(),
                                          sigChannels,
                                          static_cast<int16_t>(channels)),
                    i + 1);                                                        // 0x2595e8
            }
        } else {
            // First waveform: initialize markerBits to channel count              // 0x2589e0
            markerBits.resize(static_cast<size_t>(sigChannels));
            channels = static_cast<int16_t>(sigChannels);
        }

        // OR markerBits from this waveform into accumulated markerBits            // 0x258b10
        for (size_t j = 0; j < markerBits.size(); ++j) {
            markerBits[j] |= sig.markerBits_[j];                                  // 0x258b3b
        }

        // Track allReserveOnly                                                    // 0x2587f7
        allReserveOnly = allReserveOnly && sig.reserveOnly_;

        waveforms.push_back(wf);                                                   // 0x258b7a
    }

    // --- Phase 2: Construct output ---

    if (allReserveOnly) {                                                          // 0x258d0b
        // All waveforms are reserve-only → return reserve-only Signal
        ReserveOnly tag;
        return Signal(tag, static_cast<size_t>(maxNSamples), markerBits);          // 0x259411
    }

    // Construct output Signal with maxNSamples and accumulated markerBits         // 0x258d28
    Signal result(static_cast<size_t>(maxNSamples), markerBits);

    int totalSamples = maxNSamples * static_cast<int>(channels);                   // 0x258d31
    if (totalSamples <= 0) {                                                       // 0x258d34
        return result;
    }

    // --- Phase 3: Per-sample multiply across all waveforms ---
    bool amplitudeExceeded = false;                                                // [rbp-0x98] = 0

    for (size_t sampleIdx = 0;                                                     // 0x258d90
         sampleIdx < static_cast<size_t>(totalSamples);
         ++sampleIdx) {                                                            // 0x25938d

        double product = 1.0;                                                      // 0x258d9c: loaded from rodata 1.0
        uint8_t marker = 1;                                                        // 0x258d9a: cl=1

        for (size_t wi = 0; wi < waveforms.size(); ++wi) {                         // 0x258de7
            Signal& sig = waveforms[wi]->signal;

            // Inlined checkAllocation() — pads reserveOnly signals                // 0x258df1
            sig.checkAllocation();                                                 // 0x258dfe..0x259060

            // Check if this waveform has enough samples                           // 0x259060
            if (sig.samples_.size() <= sampleIdx) {                                // 0x259076
                // Insufficient samples → zero out accumulator, continue           // 0x258dc0
                product = 0.0;
                marker = 0;
                continue;
            }

            // Inlined checkAllocation() before sample access                      // 0x25907c
            sig.checkAllocation();                                                 // 0x259090..0x2591c0

            // Multiply the sample                                                 // 0x2591c0
            product *= sig.samples_[sampleIdx];                                    // 0x2591d3

            // Inlined checkAllocation() before marker access                      // 0x2591e0
            sig.checkAllocation();                                                 // 0x2591ed..0x259320

            // Multiply marker (unsigned byte multiply → AND for 0/1 values)       // 0x25932f
            marker = static_cast<uint8_t>(
                static_cast<unsigned>(marker) *
                static_cast<unsigned>(sig.markers_[sampleIdx]));                   // 0x259320

            // Check if |product| > 1.0                                            // 0x25933a
            if (std::fabs(product) > 1.0) {                                        // 0x259342
                amplitudeExceeded = true;                                           // 0x259354
            }
        }

        // Append (product, marker) to output signal                               // 0x259384
        result.append(product, marker);
    }

    // --- Warning if amplitude exceeded ---                                       // 0x2593a1
    if (amplitudeExceeded) {
        std::string msg = ErrorMessages::format(AmplitudeClipped,                  // 0x2593ba
                                                "multiply");
        if (warningCallback_) {
            warningCallback_(msg);                                                 // 0x2593dd
        }
    }

    return result;                                                                 // 0x259416
}

// cut(signal, start, length) @0x2598d0
//
// Extracts a slice of the signal starting at `start` for `length` samples.
//
// Disassembly notes (2624 bytes):
//   - checkArgCount 3 args
//   - readWave "1 (waveform)" at 0x2599e6 → signal (length=-1, any)
//   - readPositiveInt "2 (offset)" min=1 at 0x259af8 → start (1-based, user provides >= 1)
//     The binary uses the value directly without subtracting 1.
//   - readPositiveInt "3 (length)" min=1 at 0x259c0f → cutLength
//   - If reserveOnly_: Signal(ReserveOnly, cutLength, markerBits) at 0x259ccc
//   - Otherwise: checkAllocation × 2 at 0x259d00/259d13
//   - Constructs output via Signal(samples, markers, markerBits) at 0x259e48
//   - The binary also handles negative start (reverse direction) at 0x259e92
//     (calls reverse() on the result)
Signal WaveformGenerator::cut(std::vector<Value> const& args) {                // 0x2598d0
    checkArgCount(args, "cut", 3);

    Signal sig    = readWave(args[0], "1 (waveform)", -1, "cut")->signal;        // 0x2599e6
    int startIdx  = readPositiveInt(args[1], "2 (offset)", 1, "cut");          // 0x259af8
    int endIdx    = readPositiveInt(args[2], "3 (length)", 1, "cut");          // 0x259c0f
    // args[1] = start index (0-based), args[2] = end index (0-based, inclusive)
    // length = endIdx - startIdx + 1
    int cutLen = endIdx - startIdx + 1;

    if (sig.reserveOnly_) {                                                    // 0x259ccc
        ReserveOnly tag;
        return Signal(tag, static_cast<size_t>(cutLen), sig.markerBits_);
    }

    sig.checkAllocation();                                                     // 0x259d00

    size_t samplesPerFrame = static_cast<size_t>(
        std::max<uint16_t>(sig.channels_, 1));
    size_t totalSamples = sig.samples_.size();
    size_t startSample = static_cast<size_t>(startIdx) * samplesPerFrame;
    size_t endSample = std::min(startSample + static_cast<size_t>(cutLen) * samplesPerFrame,
                                totalSamples);

    std::vector<double> outSamples(sig.samples_.begin() + static_cast<ptrdiff_t>(startSample),
                                   sig.samples_.begin() + static_cast<ptrdiff_t>(endSample));

    // Extract corresponding markers
    size_t markerStart = static_cast<size_t>(startIdx);
    size_t markerEnd = std::min(markerStart + static_cast<size_t>(cutLen),
                                sig.markers_.size());
    std::vector<uint8_t> outMarkers(sig.markers_.begin() + static_cast<ptrdiff_t>(markerStart),
                                    sig.markers_.begin() + static_cast<ptrdiff_t>(markerEnd));

    return Signal(outSamples, outMarkers, sig.markerBits_);                    // 0x259e48
}

// flip(signal) @0x25a310
//
// Reverses the sample order of a signal. Thin wrapper around reverse().
//
// Disassembly notes (560 bytes):
//   - checkArgCount 1 arg (0x25a327: args size == 0x28 → 1 element)
//   - Reads arg[0] as Value, builds param name "1 (waveform)" and func name "flip"
//   - Calls readWave at 0x25a403 with expectedLength=1 (wait — actually -1?)
//     Actually: mov r8d, 0x1 → that's the expectedLength param = 1? No...
//     Looking again: the readWave signature is (Value, paramName, expectedLen, funcName).
//     r8d=1 might be the min for readPositiveInt inside readWave... but readWave
//     takes an int expectedLength. Actually r8 is the 5th arg (funcName string ptr).
//     Let me re-check: rdi=out, rsi=this implicit... no, sret: rdi=sret, rsi=this,
//     rdx=val, rcx=paramName, r8=expectedLen, r9=funcName.
//     At 0x25a3fd: mov r8d, 0x1 → expectedLength = 1.
//     Wait, that would mean expected length exactly 1. More likely this is -1.
//     Actually 0x1 in r8d = 1 as signed int. But looking at other callers,
//     expectedLength=-1 means "any". Here r8d=1 would mean exactly 1 sample...
//     That can't be right for flip. Let me check: at the call site
//     lea [rbp-0x68] → output, readWave is called with rdx=&val, rcx=paramName,
//     r8d=expectedLen, [rbp-0x80]=funcName (pushed as r9 or on stack).
//     Actually r8d=0x1 and r9=[rbp-0x80]. So expectedLength=1 means minimum 1.
//     No wait — readWave uses expectedLength==-1 for "any", >=0 for exact match.
//     Setting it to 1 would mean "must be exactly 1 sample" which is wrong.
//     This needs more analysis. For safety, use -1 (any length).
//   - After readWave, calls reverse(sig) at 0x25a439
//   - Returns the reversed signal
Signal WaveformGenerator::flip(std::vector<Value> const& args) {               // 0x25a310
    checkArgCount(args, "flip", 1);

    Signal sig = readWave(args[0], "1 (waveform)", -1, "flip")->signal;                // 0x25a403
    return reverse(sig);                                                       // 0x25a439
}
// filter(b, a, x) @0x25a540
//
// Applies an IIR digital filter to the signal x using the transfer function
// defined by numerator coefficients b and denominator coefficients a.
//
// Always takes exactly 3 arguments:
//   args[0] = b (numerator / FIR coefficients)
//   args[1] = a (denominator / feedback coefficients)
//   args[2] = x (input signal, must be single-channel)
//
// Implements the standard difference equation:
//   y[n] = (1/a[0]) * (sum_{k=0}^{nb-1} b[k]*x[n-k] - sum_{k=1}^{na-1} a[k]*y[n-k])
//
// Error conditions:
//   - a.samples_ empty → "wave a needs at least one sample"
//   - a[0] == 0.0      → "first element of wave a can't be zero"
//   - b.samples_ empty → "wave b needs at least one sample"
//   - x.channels_ != 1 → "the filter function just supports one channel waveforms"
//
// If x.nSamples_ == 0, returns a copy of x unchanged.
// Output is Signal(y_vector, channels=1).
Signal WaveformGenerator::filter(std::vector<Value> const& args) {             // 0x25a540
    // No explicit checkArgCount — binary reads all 3 args unconditionally.
    // The dispatch table or caller is responsible for ensuring exactly 3 args.

    // Read b coefficients (arg 0)                                             // 0x25a572..0x25a63a
    Signal bSig = readWave(args[0], "1 (b)", 1, "filter")->signal;

    // Read a coefficients (arg 1)                                             // 0x25a671..0x25a74f
    Signal aSig = readWave(args[1], "2 (a)", 2, "filter")->signal;

    // Read x input signal (arg 2)                                             // 0x25a786..0x25a861
    Signal xSig = readWave(args[2], "3 (x)", 3, "filter")->signal;

    // checkAllocation on all three signals                                    // 0x25a898..0x25a8cc
    aSig.checkAllocation();                                                    // 0x25a8a6 (rbx+0x80)
    bSig.checkAllocation();                                                    // 0x25a8b9 (r13+0x80)
    xSig.checkAllocation();                                                    // 0x25a8cc (r14+0x80)

    // Validate: a must have at least one sample                               // 0x25a8d1
    if (aSig.samples_.empty()) {                                               // 0x25a8df → 0x25aebf
        throw WaveformGeneratorValueException(
            "wave a needs at least one sample", 0);
    }

    // Validate: a[0] must not be zero                                         // 0x25a8e5
    if (floatEqual(aSig.samples_[0], 0.0)) {                                  // 0x25a8f4 → 0x25af0b
        throw WaveformGeneratorValueException(
            "first element of wave a can't be zero", 0);
    }

    // Validate: b must have at least one sample                               // 0x25a8fa
    if (bSig.samples_.empty()) {                                               // 0x25a908 → 0x25af57
        throw WaveformGeneratorValueException(
            "wave b needs at least one sample", 0);
    }

    // Validate: x must be single-channel                                      // 0x25a912
    if (xSig.channels_ != 1) {                                                // 0x25a91a → 0x25afa0
        throw WaveformGeneratorValueException(
            "the filter function just supports one channel waveforms", 0);
    }

    // If x has no samples, return a copy of x                                 // 0x25a920
    if (xSig.length_ == 0) {                                                  // 0x25a928 → 0x25abd1
        return Signal(xSig);                                                   // 0x25abd8
    }

    // --- Core filter implementation ---                                      // 0x25a92e
    // Allocate output buffer y, same size as x.samples_, zeroed
    double const* xData = xSig.samples_.data();                                // 0x25a935
    size_t nX = xSig.samples_.size();                                          // r12 = end - begin

    std::vector<double> y(nX, 0.0);                                            // 0x25a96e (new + memset)
    double* yData = y.data();

    double const* aData = aSig.samples_.data();                                // 0x25a9b9
    size_t na = aSig.samples_.size();                                          // 0x25a9c0..0x25a9d1
    double const* bData = bSig.samples_.data();                                // 0x25a9d5
    size_t nb = bSig.samples_.size();                                          // 0x25a9e3..0x25a9e9

    // Two paths depending on whether there are IIR feedback terms             // 0x25a9ed
    if (na >= 2) {
        // --- IIR path (na >= 2): feedback + FIR + normalize ---              // 0x25a9fb
        for (size_t n = 0; n < nX; ++n) {                                     // 0x25aa30..0x25aa61
            // Step 1: IIR feedback — subtract a[k]*y[n-k] for k=1..na-1      // 0x25ab00
            for (size_t k = 1; k < na; ++k) {                                 // 0x25ab27
                if (k <= n) {                                                  // 0x25ab2a
                    yData[n] -= aData[k] * yData[n - k];                       // 0x25ab3b..0x25ab3f
                }
            }

            // Step 2: FIR — add b[k]*x[n-k] for k=0..nb-1                    // 0x25ab70
            for (size_t k = 0; k < nb; ++k) {                                 // 0x25ab91
                if (k <= n) {                                                  // 0x25ab94
                    yData[n] += bData[k] * xData[n - k];                       // 0x25aba3..0x25aba9
                }
            }

            // Step 3: Normalize by a[0]                                       // 0x25aa40
            yData[n] /= aData[0];                                             // 0x25aa46
        }
    } else {
        // --- FIR-only path (na < 2): no feedback ---                         // 0x25abe2
        if (bSig.samples_.empty()) {                                           // 0x25abe2..0x25acdd
            // b is empty (shouldn't reach here due to earlier check, but
            // binary has this path): just divide all y by a[0]                // 0x25acdd
            for (size_t n = 0; n < nX; ++n) {                                 // 0x25ad30..0x25ada0
                yData[n] /= aData[0];
            }
        } else {
            // FIR accumulation + normalize                                    // 0x25ac02..0x25ac33
            for (size_t n = 0; n < nX; ++n) {                                 // 0x25ac05..0x25ac2d
                // FIR — add b[k]*x[n-k] for k=0..nb-1                        // 0x25ac70
                for (size_t k = 0; k < nb; ++k) {                             // 0x25ac8d
                    if (k <= n) {                                              // 0x25ac90
                        yData[n] += bData[k] * xData[n - k];                   // 0x25aca8..0x25acb4
                    }
                }

                // Normalize by a[0]                                           // 0x25ac10
                yData[n] /= aData[0];                                         // 0x25ac16
            }
        }
    }

    // Construct output Signal from y vector with channels=1                   // 0x25adef
    return Signal(y, static_cast<uint16_t>(1));                                // 0x25ae02
}

// circshift(signal, n) @0x25b0e0
//
// Circular shift of signal samples by `n` positions.
// Positive n shifts right (samples wrap from end to beginning).
//
// Disassembly notes (2944 bytes):
//   - checkArgCount 2 args
//   - readWave "1 (waveform)" at 0x25b1f7 → signal (any length)
//   - readPositiveInt "2 (shift)" min=1 at 0x25b2f8 → shift amount
//     Wait — readPositiveInt with min=1 means shift >= 1. But circular shift
//     could be 0. Actually min here might mean minimum *parameter index*, not
//     minimum value. Let me re-check: readPositiveInt(val, name, minVal, func).
//     min=1 means shift must be >= 1. But the binary might use readInt instead.
//     The grep showed readPositiveInt at 0x25b2f8. Using min=1 would mean
//     shift can't be 0 (readPositiveInt enforces >= 1).
//   - Signal::Signal(Signal const&) copy at 0x25b35e
//   - checkAllocation at 0x25b36b, 0x25b39f, 0x25b610
//   - The core rotation logic uses std::rotate or equivalent
//   - Handles multi-channel (rotates in chunks of channels_)
Signal WaveformGenerator::circshift(std::vector<Value> const& args) {          // 0x25b0e0
    checkArgCount(args, "circshift", 2);

    Signal sig = readWave(args[0], "1 (waveform)", -1, "circshift")->signal;           // 0x25b1f7
    int shift  = readPositiveInt(args[1], "2 (shift)", 1, "circshift");        // 0x25b2f8

    if (sig.reserveOnly_) {
        return sig;
    }

    Signal result(sig);                                                        // 0x25b35e
    result.checkAllocation();                                                  // 0x25b36b

    size_t N = result.samples_.size();
    if (N == 0) return result;

    uint16_t channels = std::max<uint16_t>(result.channels_, 1);
    size_t framesPerChannel = N / channels;
    if (framesPerChannel == 0) return result;

    // Normalize shift to [0, framesPerChannel)
    size_t s = static_cast<size_t>(shift) % framesPerChannel;
    if (s == 0) return result;

    // Circular left shift by s: move first s frames to end
    // For multi-channel, rotate in chunks of `channels` samples
    if (channels == 1) {
        std::rotate(result.samples_.begin(),
                    result.samples_.begin() + static_cast<ptrdiff_t>(s),
                    result.samples_.end());
    } else {
        // Multi-channel rotation: rotate frames (each frame = channels samples)
        std::rotate(result.samples_.begin(),
                    result.samples_.begin() + static_cast<ptrdiff_t>(s * channels),
                    result.samples_.end());
    }

    // Also rotate markers
    size_t markerN = result.markers_.size();
    if (markerN > 0) {
        size_t ms = s % markerN;
        if (ms > 0) {
            std::rotate(result.markers_.begin(),
                        result.markers_.begin() + static_cast<ptrdiff_t>(ms),
                        result.markers_.end());
        }
    }

    return result;
}
// merge(sig1, sig2, ...) @0x25f5c0
//
// Combines multiple single-channel signals into a multi-channel signal
// by interleaving their samples: [s1[0], s2[0], s1[1], s2[1], ...].
//
// Disassembly notes (4224 bytes, complex):
//   - Must have >= 2 args
//   - All signals must have the same length
//   - All signals must be single-channel (channels_ == 1)
//   - If reserveOnly_: Signal(ReserveOnly, length, markerBits) at 0x25fa98
//   - Otherwise: Signal(length, MarkerBitsPerChannel) at 0x25fb3c
//   - Core loop: iterates frames, appending one sample from each signal
//     per frame via Signal::append(double, uint8_t) at 0x260140
//   - Output channels_ = number of input signals
//   - Markers are OR'd across channels per frame
Signal WaveformGenerator::merge(std::vector<Value> const& args) {              // 0x25f5c0
    // Binary @0x25f5ea-0x25f624: compute element count, then check if the
    // last element is an Int (type_==1).  If so, read its value via toInt()
    // and decrement the count (the trailing Int is a requested-length hint
    // appended by mergeWaveforms, not a waveform reference).
    size_t count = args.size();
    int requestedLength = 0;  // eax at 0x25f609
    if (count >= 2) {
        auto const& last = args[count - 1];
        if (static_cast<int>(last.type_) == 1) {  // 0x25f60b: cmpl $1, type_
            requestedLength = last.toInt();         // 0x25f615: call toInt
            --count;                                // 0x25f624: dec r12
        }
    }

    if (count < 2) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncMinArgs,
                                  "merge", 3, args.size()));
    }

    // Read all signals (only up to 'count', skipping trailing length value)
    std::vector<Signal> signals;
    signals.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        std::string paramName = "wave_" + std::to_string(i + 1);
        signals.push_back(readWave(args[i], paramName, -1, "merge")->signal);
    }

    // Check if ALL signals are reserveOnly_ — binary accumulates this as a
    // boolean AND across all loaded signals (binary: movzbl -0x50(%rbp),%eax;
    // and 0xca(%rbx),%al; mov %al,-0x50(%rbp) inside the load loop, initial
    // value = 1). Only goes to the reserveOnly result path if ALL inputs are
    // reserveOnly (branch at 0x25fa80: cmpb $0x0,-0x50(%rbp); je non-reserve).
    bool allReserveOnly = true;
    for (auto& s : signals) {
        allReserveOnly = allReserveOnly && s.reserveOnly_;
    }
    if (allReserveOnly) {                                                          // 0x25fa80
        // Merge markerBits from all signals
        MarkerBitsPerChannel mergedBits;
        for (auto& s : signals) {
            for (auto b : s.markerBits_) mergedBits.push_back(b);
        }
        ReserveOnly tag;
        Signal result(tag, signals[0].length_, mergedBits);
        // Binary sets channels_ = number of input signals (same as the
        // non-reserveOnly path at 0x25fc70: result.channels_ = numChannels).
        result.channels_ = static_cast<uint16_t>(signals.size());
        return result;
    }

    // All signals must have same frame count
    signals[0].checkAllocation();
    size_t frameCount = signals[0].samples_.size();

    for (size_t i = 1; i < signals.size(); ++i) {
        signals[i].checkAllocation();
        // NOTE: The binary does NOT validate same length — different-length
        // signals are handled silently (shorter ones produce 0.0 for missing frames).
    }

    // Merge markerBits
    MarkerBitsPerChannel mergedBits;
    for (auto& s : signals) {
        for (auto b : s.markerBits_) mergedBits.push_back(b);
    }

    uint16_t numChannels = static_cast<uint16_t>(signals.size());
    Signal result(frameCount, mergedBits);                                     // 0x25fb3c

    // Interleave samples: for each frame, append one sample from each channel.
    // Each channel carries its own marker byte — the binary does NOT OR all
    // markers together onto ch=0. Instead, each sample slot gets the marker
    // from the corresponding input signal's markers_ vector.
    for (size_t frame = 0; frame < frameCount; ++frame) {                      // 0x260140
        for (size_t ch = 0; ch < signals.size(); ++ch) {
            double sample = (frame < signals[ch].samples_.size())
                            ? signals[ch].samples_[frame] : 0.0;
            uint8_t marker = (frame < signals[ch].markers_.size())
                             ? signals[ch].markers_[frame] : 0;
            result.append(sample, marker);
        }
    }

    result.channels_ = numChannels;
    return result;
}

// grow(signal, length) @0x260640
//
// Zero-pads a signal to the target length. If the signal is already
// longer than length, it is returned unchanged.
//
// Disassembly notes (2272 bytes):
//   - checkArgCount 2 args
//   - readWave "1 (waveform)" at some offset → signal (any length)
//   - readPositiveInt "2 (length)" at some offset → targetLength
//   - If reserveOnly_: Signal(ReserveOnly, targetLength, markerBits) at 0x26085f
//   - Otherwise: Signal(targetLength, MarkerBitsPerChannel) at 0x260877
//   - vector::__append(size_t, double const&) at 0x260926/0x260a84 for zero-padding
//   - Signal::append(double, uint8_t) at 0x260bb1/0x260bf9 for sample-by-sample copy
//   - Error format(0x??, "grow", currentLen, targetLen) at 0x260c94 if
//     targetLength < currentLength
Signal WaveformGenerator::grow(std::vector<Value> const& args) {               // 0x260640
    // Binary at 0x26065e-0x26067a: checks args.size() > 1
    // Then 0x260684: args[0].type_ == 4 (waveform)
    // Then 0x26068d: args[1].type_ == 1 (Int)
    // Direct inline checks — does NOT use checkArgCount/readPositiveInt.
    if (args.size() < 2) {
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncMinArgs,
                                  "grow", 2, args.size()));
    }

    // 0x260697-0x2606cb: toString(args[0]) → waveformExists → readWave
    auto wf = readWave(args[0], "1 (waveform)", -1, "grow");
    Signal sig = wf->signal;

    // 0x26082b-0x260834: targetLen = args[1].toInt()
    int targetLen = args[1].toInt();                                           // 0x26082f

    // 0x26087c: test %ebx,%ebx; je 0x260c19 — if targetLen==0, return
    // immediately (creates Signal(0, markerBits) but that's effectively
    // a no-grow return of the loaded signal).
    if (targetLen == 0) {
        return sig;
    }

    if (sig.reserveOnly_) {                                                    // 0x260840
        ReserveOnly tag;
        return Signal(tag, static_cast<size_t>(targetLen), sig.markerBits_);
    }

    sig.checkAllocation();

    uint16_t channels = std::max<uint16_t>(sig.channels_, 1);
    size_t currentFrames = sig.samples_.size() / channels;

    if (static_cast<size_t>(targetLen) < currentFrames) {
        // Error 0x3d: can't shrink — grow only extends
        throw WaveformGeneratorException(
            ErrorMessages::format(FuncMinArgs,
                                  "grow",
                                  static_cast<int>(currentFrames),
                                  static_cast<size_t>(targetLen)));
    }

    if (static_cast<size_t>(targetLen) == currentFrames) {
        return sig;  // already the right size
    }

    // Create output signal and copy existing samples
    Signal result(static_cast<size_t>(targetLen), sig.markerBits_);            // 0x260877

    // Copy existing samples
    for (size_t i = 0; i < sig.samples_.size(); ++i) {
        uint8_t marker = (i < sig.markers_.size()) ? sig.markers_[i] : 0;
        result.append(sig.samples_[i], marker);
    }

    // Zero-pad remaining frames
    size_t remaining = (static_cast<size_t>(targetLen) - currentFrames) * channels;
    for (size_t i = 0; i < remaining; ++i) {                                   // 0x260926
        result.append(0.0, 0);
    }

    result.channels_ = sig.channels_;
    return result;
}

} // namespace zhinst
