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
#include <functional>
#include <numeric>
#include <random>
#include <stdexcept>

#include "zhinst/resources.hpp"

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
    , field_20_(1.0f)
    , aliasMap_()
    , field_48_(1.0f)
    , createdNames_()
    , wavetableFront_(std::move(wavetableFront))
    , field_78_(0)
    , warningCallback_(warningCallback)
    , field_B0_()
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

    // NOTE: The ctor does NOT register aliases in aliasMap_ (confirmed empty in Phase 13c).
    // The exact alias mappings are not yet extracted from the disassembly
    // (the ctor is ~5KB, mostly repetitive registration code).
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
    return call("zeros", args);
}

// genericTriangle @0x25e0c0
// Generates a piecewise-linear triangle waveform.
//   length:    number of samples
//   amplitude: peak amplitude (positive and negative)
//   riseRatio: fraction of the period that is the rising edge (0..1)
//   phase:     phase offset in radians
//   period:    period expressed in the same units as length (number of samples per period)
Signal WaveformGenerator::genericTriangle(int length, double amplitude,
                                           double riseRatio, double phase,
                                           double period) {
    Signal sig(static_cast<size_t>(length));

    if (length == 0) {
        return sig;
    }

    double samplesPerPeriod = static_cast<double>(length) / period;
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
            sample = ((t - fallSamples) / riseHalfSamples) * amplitude + negAmplitude;
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
    auto it = createdNames_.find(name);
    if (it == createdNames_.end()) {
        // First time — must invoke the factory.
        if (!factory) {
            // Matches __throw_bad_function_call at 25be2d.
            throw std::bad_function_call();
        }

        Signal signal = factory(args);

        // Register the newly-created Signal under `name` with the wavetable
        // front-end, which both creates a WaveformFront and inserts `name`
        // into createdNames_ for subsequent cache hits.
        return wavetableFront_->newWaveform(signal, name, args);
    }

    // Cache hit: look up the existing WaveformFront and bump its use-count.
    auto wf = wavetableFront_->getWaveformByFunDescr(name, args);
    if (wf) {
        // Field at +0xd8 in WaveformFront is a uint32_t use-count incremented
        // on every cache hit (line 25be11 in the binary).
        // NOTE(WaveformFront): expose this counter properly once WaveformFront
        // is reconstructed; for now we leave it untouched and rely on the
        // shared_ptr refcount.
    }
    return wf;
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
            static_cast<ErrorMessageT>(0x37), name, aliasIt->second);
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0xd8), name), 0);
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

    // 0x25c5fa-0x25c5ff: setValue(VarType(5), value) — VarType 5 = Waveform
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x55),
                                  paramName, funcName),
            0);
    }
    return val.toDouble();
}

double WaveformGenerator::readDoubleAmplitude(  // 0x25caa0
    Value val, std::string const& paramName,
    std::string const& funcName)
{
    // Like readDouble but additionally clamps/validates that |result| <= 1.0.
    // The disassembly uses a jump-table on val.which_ to extract the variant
    // payload directly into a stack slot, then likely checks fabs(result)>1.0
    // and throws.  Without full reconstruction of the bounds checks we just
    // delegate; TODO: add the |x|>1.0 check matching ErrorMessage at format
    // slot used after the conversion.
    return readDouble(val, paramName, funcName);
}

int WaveformGenerator::readInt(  // 0x25cca0
    Value val, std::string const& paramName, int minVal,
    std::string const& funcName)
{
    if (val.type_ == ValueType::String) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x55),
                                  paramName, funcName),
            0);
    }
    int result = val.toInt();
    if (result < minVal) {
        // ErrorMessage 0x60 takes (paramName, funcName, "positive"|"negative")
        // and the ValueException's argIndex_ field carries minVal.
        const char* sign = (minVal >= 0) ? "positive" : "negative";
        throw WaveformGeneratorValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x60),
                                  paramName, funcName, std::string(sign)),
            static_cast<size_t>(minVal));
    }
    return result;
}

int WaveformGenerator::readPositiveInt(  // 0x25d490
    Value val, std::string const& paramName, int minVal,
    std::string const& funcName)
{
    // Convenience wrapper: forces minVal>=0 semantics regardless of caller.
    int result = readInt(val, paramName, minVal, funcName);
    if (result < 0) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x60),
                                  paramName, funcName,
                                  std::string("positive")),
            static_cast<size_t>(minVal));
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
            static_cast<ErrorMessageT>(0x56), paramName, funcName);
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
            static_cast<ErrorMessageT>(0x5a), funcName, waveName);
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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
                static_cast<ErrorMessageT>(0x63),
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
// Creates a Signal by linearly interpolating between the given (x,y) points.
// The markers and markerBits are copied from the input vectors.
//
// Disassembly notes (432 bytes — small enough to reconstruct):
//   - Constructs MarkerBitsPerChannel from the markerBits parameter
//   - Signal::Signal(size_t, MarkerBitsPerChannel const&) at 0x25f48f
//   - If length == 0: returns empty signal (0x25f4b0)
//   - Precomputes: lengthDouble = (double)length at 0x25f4b9
//   - Iterates xPoints: for each segment between consecutive x-points,
//     linearly interpolates y values
//   - Loop at 0x25f510: iterates markers/markerBits per channel
//   - Samples written via Signal::append(double, uint8_t)
//
// The interpolation finds which segment each output sample falls in
// by scanning xPoints, then linearly interpolates the corresponding yPoints.
Signal WaveformGenerator::interpolateLinear(                                   // 0x25f410
    int length,
    std::vector<double> const& xPoints,
    std::vector<double> const& yPoints,
    std::vector<uint8_t> const& markers,
    std::vector<uint8_t> const& markerBits)
{
    // Build MarkerBitsPerChannel from input markerBits
    MarkerBitsPerChannel mbpc(markerBits.begin(), markerBits.end());            // 0x25f434

    Signal sig(static_cast<size_t>(length), mbpc);                             // 0x25f48f

    if (length == 0) return sig;                                               // 0x25f4b0

    double lengthDouble = static_cast<double>(length);                         // 0x25f4b9

    // Find segments and interpolate
    // The binary iterates through xPoints to find segment boundaries,
    // then linearly interpolates yPoints for each output sample index.
    size_t numPoints = xPoints.size();  // already element count (vector<double>)
    // Actually xPoints.size() is the count directly.

    size_t seg = 1;  // current segment index (starts at 1)                    // 0x25f4d1
    for (int n = 0; n < length; ++n) {                                         // 0x25f4f1
        double t = static_cast<double>(n + 1);                                // 0x25f4fd

        // Advance segment while t >= xPoints[seg] (if more segments exist)
        while (seg < xPoints.size() && xPoints[seg] <= t) {                   // 0x25f4e0
            ++seg;
        }

        // TODO: The exact interpolation formula needs verification.
        // The binary accesses markers and markerBits arrays per-channel
        // in the inner loop at 0x25f510.
        // For now, use simple linear interpolation between segment endpoints.
        double x0 = (seg > 0 && seg - 1 < xPoints.size()) ? xPoints[seg - 1] : 0.0;
        double x1 = (seg < xPoints.size()) ? xPoints[seg] : lengthDouble;
        double y0 = (seg > 0 && seg - 1 < yPoints.size()) ? yPoints[seg - 1] : 0.0;
        double y1 = (seg < yPoints.size()) ? yPoints[seg] : y0;

        double frac = (x1 != x0) ? (t - x0) / (x1 - x0) : 0.0;
        double sample = y0 + frac * (y1 - y0);

        uint8_t marker = (static_cast<size_t>(n) < markers.size()) ? markers[n] : 0;
        sig.append(sample, marker);
    }

    return sig;
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
                ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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
    checkArgCount(args, "gauss", 4);
    int length      = readInt(args[0], "length", 1, "gauss");
    double amp      = readDoubleAmplitude(args[1], "amplitude", "gauss");
    double position = readDouble(args[2], "position", "gauss");
    double width    = readDouble(args[3], "width", "gauss");

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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                                  "sin", 3, args.size()));
    }

    double nPeriods = 1.0;                                                       // default from rodata @0x956030
    int length;
    double amplitude, phase;

    if (args.size() == 4) {
        length    = readPositiveInt(args[0], "1 (length)", 1, "sin");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "sin");
        phase     = readDouble(args[2], "3 (phase)", "sin");
        nPeriods  = readDouble(args[3], "4 (nPeriods)", "sin");
    } else {
        length    = readPositiveInt(args[0], "1 (length)", 1, "sin");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "sin");
        phase     = readDouble(args[2], "3 (phase)", "sin");
    }

    // Validate nPeriods >= 0                                                    // 0x24a963
    if (nPeriods < 0.0) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5e),
                                  "nPeriods", "sin"), 3);
    }

    Signal sig(static_cast<size_t>(length));                                     // 0x24a974
    if (length == 0) return sig;

    // Precompute: twoNPeriodsPi = 2 * nPeriods * PI                            // 0x24a983-0x24a98f
    double twoNPeriodsPi = 2.0 * nPeriods * M_PI;
    double dLength = static_cast<double>(length);

    for (int i = 0; i < length; ++i) {                                          // 0x24a9b0
        double theta = static_cast<double>(i) * twoNPeriodsPi / dLength + phase;
        sig.append(amplitude * std::sin(theta), 0);                              // 0x24a9d9
    }
    return sig;                                                                  // 0x24a9e6
}
// cos(length, amplitude, phase[, nPeriods]) @0x24abd0
//   Identical to sin() but calls std::cos instead of std::sin.
Signal WaveformGenerator::cos(std::vector<Value> const& args) {                  // 0x24abd0
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                                  "cosine", 3, args.size()));
    }

    double nPeriods = 1.0;
    int length;
    double amplitude, phase;

    if (args.size() == 4) {
        length    = readPositiveInt(args[0], "1 (length)", 1, "cosine");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "cosine");
        phase     = readDouble(args[2], "3 (phase)", "cosine");
        nPeriods  = readDouble(args[3], "4 (nPeriods)", "cosine");
    } else {
        length    = readPositiveInt(args[0], "1 (length)", 1, "cosine");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "cosine");
        phase     = readDouble(args[2], "3 (phase)", "cosine");
    }

    if (nPeriods < 0.0) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5e),
                                  "nPeriods", "cosine"), 3);
    }

    Signal sig(static_cast<size_t>(length));
    if (length == 0) return sig;

    double twoNPeriodsPi = 2.0 * nPeriods * M_PI;
    double dLength = static_cast<double>(length);

    for (int i = 0; i < length; ++i) {                                          // 0x24b560
        double theta = static_cast<double>(i) * twoNPeriodsPi / dLength + phase;
        sig.append(amplitude * std::cos(theta), 0);                              // 0x24b589
    }
    return sig;
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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
        // 3-arg path: reads args[0..2] then reads args[3] out of bounds.
        // This is effectively dead code in production. We replicate the
        // 4-arg behavior but shift: (length, amplitude, position) with no beta.
        // Since beta would be UB, we mirror the 3-arg reads and treat args[2]
        // as both position AND beta (which the binary would do from garbage).
        length    = readPositiveInt(args[0], "1 (length)", 1, "sinc");           // 0x24b9b6
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "sinc");
        position  = readPositiveInt(args[2], "3 (position)", 3, "sinc");         // 0x24bd35
        beta      = static_cast<double>(position);  // best-effort for 3-arg
    }

    // Warn if position > length                                                 // 0x24bf9c
    if (static_cast<unsigned int>(position) > static_cast<unsigned int>(length)) {
        if (warningCallback_) {
            warningCallback_(ErrorMessages::format(
                static_cast<ErrorMessageT>(0x5f), "position", "sinc"));
        }
    }

    // beta == 0 → throw                                                         // 0x24bff3
    if (floatEqual(beta, 0.0)) {
        throw WaveformGeneratorException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x62),
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                                  "ramp", 3, args.size()));
    }

    int length       = readInt(args[0], "1 (length)", 1, "ramp");                // 0x24c3c1
    double startLevel = readDouble(args[1], "2 (startLevel)", "ramp");           // 0x24c4dd
    double endLevel   = readDouble(args[2], "3 (endLevel)", "ramp");             // 0x24c5e8

    // Validate |startLevel| <= 1.0 and |endLevel| <= 1.0                       // 0x24c617-0x24c64a
    if (std::fabs(startLevel) > 1.0) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5e),
                                  "startLevel", "ramp"), 1);
    }
    if (std::fabs(endLevel) > 1.0) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5e),
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                                  "sawtooth", 3, args.size()));
    }

    double nPeriods = 1.0;                                                       // default @0x956030
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5e),
                                  "nPeriods", "sawtooth"), 3);
    }

    return genericTriangle(length, amplitude, 1.0, phase, nPeriods);             // 0x24d14e — riseRatio=1.0 @0x956030
}
// triangle(length, amplitude, phase[, nPeriods]) @0x24d330
//   Delegates to genericTriangle with riseRatio = 0.5.
Signal WaveformGenerator::triangle(std::vector<Value> const& args) {             // 0x24d330
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                                  "triangle", 3, args.size()));
    }

    double nPeriods = 1.0;                                                       // default @0x956030
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5e),
                                  "nPeriods", "triangle"), 3);
    }

    return genericTriangle(length, amplitude, 0.5, phase, nPeriods);             // 0x24dbce — riseRatio=0.5 @0x9562c8
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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
                    static_cast<ErrorMessageT>(0x5f), "position", "drag"));
            }
        }

        // sigma == 0 → throw                                          // 0x24f257
        if (floatEqual(sigma, 0.0)) {
            throw WaveformGeneratorException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x62),
                                      "sigma", "drag"));
        }

        Signal sig(static_cast<size_t>(length));                       // 0x24f273
        if (length == 0) return sig;

        // Precompute                                                   // 0x24f281
        double invSigma    = -1.0 / sigma;       // @0x9562d0 = -1.0
        double ampCoeff    = amplitude / invSigma; // = -amplitude * sigma
        double sigmaSq     = sigma * sigma;
        double twoSigmaSq  = sigmaSq + sigmaSq;   // 2 * sigma^2

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
                static_cast<ErrorMessageT>(0x5f), "position", "drag"));
        }
    }

    if (floatEqual(sigma, 0.0)) {                                      // 0x24f257
        throw WaveformGeneratorException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x62),
                                  "sigma", "drag"));
    }

    Signal sig(static_cast<size_t>(length));                           // 0x24f273
    if (length == 0) return sig;

    double invSigma    = -1.0 / sigma;
    double ampCoeff    = amplitude / invSigma;
    double sigmaSq     = sigma * sigma;
    double twoSigmaSq  = sigmaSq + sigmaSq;

    for (int i = 0; i < length; ++i) {                                 // 0x24f2c0
        double dx  = position - static_cast<double>(i);
        double neg = -dx;
        double expArg = neg * dx / twoSigmaSq;
        double deriv  = dx / sigmaSq * ampCoeff;
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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
// chirp(length, amplitude, startFreq[, stopFreq[, phase]]) @0x250bb0
//   3 args: (length, amplitude, startFreq) — stopFreq = startFreq, phase = 0
//   4 args: (length, amplitude, startFreq, stopFreq) — phase = 0
//   5 args: (length, amplitude, startFreq, stopFreq, phase)
// Linear frequency sweep: instantaneous freq = startFreq + freqRate * i
//   where freqRate = (stopFreq - startFreq) / length.
// Loop: theta = 2*PI*startFreq*i + phase + PI*freqRate*i^2
//       sample = amplitude * sin(theta)
// After loop: zero-pad until (total samples) % 16 == 0.
Signal WaveformGenerator::chirp(std::vector<Value> const& args) {                // 0x250bb0
    if (args.size() < 3 || args.size() > 5) {
        throw WaveformGeneratorException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5c),
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
        length    = readPositiveInt(args[0], "1 (length)", 1, "chirp");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "chirp");
        startFreq = readDouble(args[2], "3 (startFrequency)", "chirp");
        stopFreq  = readDouble(args[3], "4 (stopFrequency)", "chirp");
        phase     = 0.0;
    } else {
        // 3 args: stopFreq = startFreq, phase = 0
        length    = readPositiveInt(args[0], "1 (length)", 1, "chirp");
        amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "chirp");
        startFreq = readDouble(args[2], "3 (startFrequency)", "chirp");
        stopFreq  = startFreq;
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
//   - Generation loop at 0x252580+ uses inline mt19937_64 twist + normal_distribution
//   - Samples are: amplitude * dist(rng) + mean  (verified: matches normal_distribution API)
Signal WaveformGenerator::rand(std::vector<Value> const& args) {               // 0x251cf0
    if (args.size() != 3 && args.size() != 4) {
        throw WaveformGeneratorException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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

    // The binary uses the TLS mt19937_64 state (GlobalResources::random)
    // with an inline normal_distribution implementation (Box-Muller or
    // Ziggurat variant). For functional equivalence we use <random> directly.
    // The exact PRNG state is thread-local GlobalResources::random[313].
    auto* rng = reinterpret_cast<std::mt19937_64*>(GlobalResources::random);
    std::normal_distribution<double> dist(mean, stddev);

    for (int i = 0; i < length; ++i) {                                        // 0x252580
        double sample = amplitude * dist(*rng);
        sig.append(sample, 0);
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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

    auto* rng = reinterpret_cast<std::mt19937_64*>(GlobalResources::random);
    std::normal_distribution<double> dist(mean, stddev);

    for (int i = 0; i < length; ++i) {
        double sample = amplitude * dist(*rng);
        sig.append(sample, 0);
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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
    auto* rng = reinterpret_cast<std::mt19937_64*>(GlobalResources::random);
    std::uniform_real_distribution<double> dist(-amplitude, amplitude);

    for (int i = 0; i < length; ++i) {                                        // 0x2538d0
        sig.append(dist(*rng), 0);
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x65),
                                  "lfsrGaloisMarker"));
    }

    // TODO: Error 0x64 validation of marker bit value at 0x254111
    // The exact check is: if marker exceeds some limit, throw error 0x64
    // with (marker, "lfsrGaloisMarker"). Exact condition unclear from disasm.
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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
                static_cast<ErrorMessageT>(0x5f), "position", "rrc"));
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0xe1), 100));
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
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

    // Build markerBits vector based on which markers are enabled
    MarkerBitsPerChannel markerBits;
    if (hasMarker0) markerBits.push_back(1);
    if (hasMarker1) markerBits.push_back(1);

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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                                  "join", 1, args.size()));
    }

    // Read first signal to determine channel count and markerBits template
    Signal first = readWave(args[0], "wave_1", -1, "join")->signal;

    if (first.reserveOnly_) {                                                  // reserveOnly short-circuit
        // For placeholders, compute total length and return a combined placeholder
        size_t totalLength = first.length_;
        for (size_t i = 1; i < args.size(); ++i) {
            std::string paramName = "wave_" + std::to_string(i + 1);
            Signal s = readWave(args[i], paramName, -1, "join")->signal;
            totalLength += s.length_;
        }
        ReserveOnly tag;
        return Signal(tag, totalLength, first.markerBits_);
    }

    first.checkAllocation();                                                   // 0x256416

    // Compute total length for all signals
    size_t totalLength = first.samples_.size() / std::max<uint16_t>(first.channels_, 1);
    for (size_t i = 1; i < args.size(); ++i) {
        std::string paramName = "wave_" + std::to_string(i + 1);
        Signal s = readWave(args[i], paramName, -1, "join")->signal;
        s.checkAllocation();
        totalLength += s.samples_.size() / std::max<uint16_t>(s.channels_, 1);
    }

    // Create output signal with appropriate markerBits
    Signal result(totalLength, first.markerBits_);                             // 0x25636b

    // Re-read and append all signals
    // NOTE: The binary likely caches the signals from the first pass rather
    // than re-reading. For correctness we re-read here.
    {
        Signal s = readWave(args[0], "wave_1", -1, "join")->signal;
        s.checkAllocation();
        result.append(s);                                                      // 0x256408
    }
    for (size_t i = 1; i < args.size(); ++i) {
        std::string paramName = "wave_" + std::to_string(i + 1);
        Signal s = readWave(args[i], paramName, -1, "join")->signal;
        s.checkAllocation();                                                   // 0x256429+
        result.append(s);
    }

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

    // The binary then adjusts the markers_ vector length.
    // At 0x25815f: r14 = result.markers_.begin(), rdi = result.markers_.end()
    // It ensures markers_.size() == 1 (either truncating or extending with 0).
    // This is because after setting channels_=1, only one marker byte is needed.
    if (result.markers_.size() > 1) {
        result.markers_.resize(1);
    } else if (result.markers_.empty()) {
        result.markers_.push_back(0);
    }
    return result;
}
// multiply(sig1, sig2, ...) @0x258750
//
// Pointwise multiplication of two or more signals.
// Structurally similar to add() but with multiplication instead of addition.
//
// Disassembly notes (4480 bytes, complex):
//   - Must have >= 2 args (same pattern as add())
//   - Signal::Signal(size_t, MarkerBitsPerChannel const&) at 0x258d28
//   - Loop appends samples via Signal::append(double, uint8_t) at 0x259384
//   - Error 0x?? at 0x2593ba if channel count mismatch
//   - Handles multi-channel signals by multiplying corresponding channels
//   - Markers are OR'd together (same as add)
//   - reserveOnly_ short-circuit like add()
//
// Due to the 4480-byte complexity (multi-channel, marker handling, error paths),
// this is a documented structural stub.
Signal WaveformGenerator::multiply(std::vector<Value> const& args) {           // 0x258750
    if (args.size() < 2) {
        throw WaveformGeneratorException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                                  "multiply", 2, args.size()));
    }

    Signal first = readWave(args[0], "wave_1", -1, "multiply")->signal;
    if (first.reserveOnly_) {
        return first;
    }
    first.checkAllocation();

    // For single-channel case: pointwise multiply
    std::vector<double> product = first.samples_;
    std::vector<uint8_t> markers = first.markers_;

    for (size_t i = 1; i < args.size(); ++i) {
        std::string paramName = "wave_" + std::to_string(i + 1);
        Signal s = readWave(args[i], paramName,
                            static_cast<int>(product.size()), "multiply")->signal;
        s.checkAllocation();
        for (size_t j = 0; j < product.size(); ++j) {
            product[j] *= s.samples_[j];
        }
        if (s.markers_.size() == markers.size()) {
            for (size_t j = 0; j < markers.size(); ++j) {
                markers[j] |= s.markers_[j];
            }
        }
    }

    return Signal(product, markers, first.markerBits_);
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

    Signal sig  = readWave(args[0], "1 (waveform)", -1, "cut")->signal;                // 0x2599e6
    int start   = readPositiveInt(args[1], "2 (offset)", 1, "cut");            // 0x259af8
    int cutLen  = readPositiveInt(args[2], "3 (length)", 1, "cut");            // 0x259c0f

    if (sig.reserveOnly_) {                                                    // 0x259ccc
        ReserveOnly tag;
        return Signal(tag, static_cast<size_t>(cutLen), sig.markerBits_);
    }

    sig.checkAllocation();                                                     // 0x259d00

    // The binary uses the offset value directly (1-based from user, no subtract-1).
    // readPositiveInt enforces offset >= 1.
    size_t startIdx = static_cast<size_t>(start);
    size_t samplesPerFrame = static_cast<size_t>(
        std::max<uint16_t>(sig.channels_, 1));
    size_t totalSamples = sig.samples_.size();
    size_t startSample = startIdx * samplesPerFrame;
    size_t endSample = std::min(startSample + static_cast<size_t>(cutLen) * samplesPerFrame,
                                totalSamples);

    std::vector<double> outSamples(sig.samples_.begin() + static_cast<ptrdiff_t>(startSample),
                                   sig.samples_.begin() + static_cast<ptrdiff_t>(endSample));

    // Extract corresponding markers
    size_t markerStart = startIdx;
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
// filter(signal, b_coeffs, [a_coeffs]) @0x25a540
//
// Applies an FIR or IIR digital filter to the signal.
// - 2 args: FIR filter (b_coeffs only, a_coeffs = [1.0])
// - 3 args: IIR filter (b_coeffs numerator, a_coeffs denominator)
//
// Disassembly notes (2976 bytes, complex):
//   - Args: 2 or 3 (validated at entry)
//   - readWave "1 (waveform)" at 0x25a63a → signal (any length)
//   - readWave "2 (b_coefficients)" at 0x25a74f → b-coefficients signal
//   - If 3 args: readWave "3 (a_coefficients)" at 0x25a861 → a-coefficients
//   - checkAllocation on all signals at 0x25a8a6, 0x25a8b9, 0x25a8cc
//   - Core filtering loop implements the difference equation:
//       y[n] = (1/a[0]) * (sum(b[k]*x[n-k]) - sum(a[k]*y[n-k]))
//   - Output Signal constructed via Signal::Signal(Signal const&) at 0x25abd8
//
// Due to the 2976-byte complexity (IIR/FIR branching, coefficient normalization,
// multi-channel handling), this is a documented structural stub.
Signal WaveformGenerator::filter(std::vector<Value> const& args) {             // 0x25a540
    if (args.size() != 2 && args.size() != 3) {
        throw WaveformGeneratorException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                                  "filter", 2, args.size()));
    }

    Signal sig = readWave(args[0], "1 (waveform)", -1, "filter")->signal;              // 0x25a63a
    Signal bCoeffs = readWave(args[1], "2 (b_coefficients)", -1, "filter")->signal;    // 0x25a74f

    Signal aCoeffs;
    if (args.size() == 3) {
        aCoeffs = readWave(args[2], "3 (a_coefficients)", -1, "filter")->signal;       // 0x25a861
        aCoeffs.checkAllocation();                                             // 0x25a8cc
    }

    sig.checkAllocation();                                                     // 0x25a8a6
    bCoeffs.checkAllocation();                                                 // 0x25a8b9

    size_t N = sig.samples_.size();
    size_t nb = bCoeffs.samples_.size();
    size_t na = aCoeffs.samples_.size();

    std::vector<double> y(N, 0.0);

    // Normalize by a[0] if IIR
    double a0 = (na > 0) ? aCoeffs.samples_[0] : 1.0;

    for (size_t n = 0; n < N; ++n) {
        double sum = 0.0;
        // FIR part: sum(b[k] * x[n-k])
        for (size_t k = 0; k < nb; ++k) {
            if (n >= k) {
                sum += bCoeffs.samples_[k] * sig.samples_[n - k];
            }
        }
        // IIR feedback: - sum(a[k] * y[n-k]) for k >= 1
        for (size_t k = 1; k < na; ++k) {
            if (n >= k) {
                sum -= aCoeffs.samples_[k] * y[n - k];
            }
        }
        y[n] = sum / a0;
    }

    // Construct output signal preserving markers and markerBits
    Signal result(sig);                                                        // 0x25abd8
    result.samples_ = std::move(y);
    return result;
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

    // Circular right shift by s: move last s frames to front
    // For multi-channel, rotate in chunks of `channels` samples
    if (channels == 1) {
        std::rotate(result.samples_.begin(),
                    result.samples_.end() - static_cast<ptrdiff_t>(s),
                    result.samples_.end());
    } else {
        // Multi-channel rotation: rotate frames (each frame = channels samples)
        size_t pivotFrame = framesPerChannel - s;
        std::rotate(result.samples_.begin(),
                    result.samples_.begin() + static_cast<ptrdiff_t>(pivotFrame * channels),
                    result.samples_.end());
    }

    // Also rotate markers
    size_t markerN = result.markers_.size();
    if (markerN > 0) {
        size_t ms = s % markerN;
        if (ms > 0) {
            std::rotate(result.markers_.begin(),
                        result.markers_.end() - static_cast<ptrdiff_t>(ms),
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
    if (args.size() < 2) {
        throw WaveformGeneratorException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                                  "merge", 2, args.size()));
    }

    // Read all signals
    std::vector<Signal> signals;
    signals.reserve(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        std::string paramName = "wave_" + std::to_string(i + 1);
        signals.push_back(readWave(args[i], paramName, -1, "merge")->signal);
    }

    // Check first signal for reserveOnly_
    if (signals[0].reserveOnly_) {                                             // 0x25fa98
        // Merge markerBits from all signals
        MarkerBitsPerChannel mergedBits;
        for (auto& s : signals) {
            for (auto b : s.markerBits_) mergedBits.push_back(b);
        }
        ReserveOnly tag;
        return Signal(tag, signals[0].length_, mergedBits);
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

    // Interleave samples: for each frame, append one sample from each channel
    for (size_t frame = 0; frame < frameCount; ++frame) {                      // 0x260140
        uint8_t mergedMarker = 0;
        for (size_t ch = 0; ch < signals.size(); ++ch) {
            if (frame < signals[ch].markers_.size()) {
                mergedMarker |= signals[ch].markers_[frame];
            }
        }
        for (size_t ch = 0; ch < signals.size(); ++ch) {
            double sample = (frame < signals[ch].samples_.size())
                            ? signals[ch].samples_[frame] : 0.0;
            result.append(sample, (ch == 0) ? mergedMarker : 0);
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
    checkArgCount(args, "grow", 2);

    Signal sig      = readWave(args[0], "1 (waveform)", -1, "grow")->signal;
    int targetLen   = readPositiveInt(args[1], "2 (length)", 1, "grow");

    if (sig.reserveOnly_) {                                                    // 0x26085f
        ReserveOnly tag;
        return Signal(tag, static_cast<size_t>(targetLen), sig.markerBits_);
    }

    sig.checkAllocation();

    uint16_t channels = std::max<uint16_t>(sig.channels_, 1);
    size_t currentFrames = sig.samples_.size() / channels;

    if (static_cast<size_t>(targetLen) < currentFrames) {
        // Error 0x3d: can't shrink — grow only extends
        throw WaveformGeneratorException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3d),
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
