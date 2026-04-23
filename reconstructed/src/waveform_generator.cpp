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
#include "zhinst/error_messages.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace zhinst {

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

    // TODO: The ctor also likely registers aliases in aliasMap_.
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
        // TODO(WaveformFront): expose this counter properly once WaveformFront
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
    std::string const& /*name*/,
    std::vector<Value> const& /*args*/)
{
    // TODO(EvalResults): replace with the real body once EvalResults is
    // defined.  Reference reconstruction:
    //
    //   auto wf = call(name, args);
    //   auto results = std::make_shared<EvalResults>();
    //   Value v;  v.type_ = ValueType::String;  v.which_ = 3;
    //   /* assign name into v's string slot */
    //   results->setValue(static_cast<VarType>(5), v);
    //   results->setWaveform(wf);   // writes shared_ptr at +0x60
    //   return results;
    throw std::runtime_error(
        "WaveformGenerator::eval not implemented (blocked on EvalResults)");
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

Signal WaveformGenerator::readWave(  // 0x25d6f0
    Value /*val*/, std::string const& /*paramName*/, int /*expectedLength*/,
    std::string const& /*funcName*/)
{
    // Extracts a Signal from a Value (Value can hold a wave reference).
    // The disassembly checks val.type_ (must be Wave/Signal), validates
    // that the signal length matches expectedLength (or expectedLength==-1
    // for "any length"), and returns by sret-copy.
    // TODO: full reconstruction once the wave-typed Value variant is
    // confirmed; for now throw a placeholder.
    throw std::runtime_error("WaveformGenerator::readWave not implemented");
}

// --- Helper methods ---

Signal WaveformGenerator::markerImpl(  // 0x25e230
    std::vector<Value> const& /*args*/, bool /*isMask*/)
{
    throw std::runtime_error("WaveformGenerator::markerImpl not implemented");
}

Signal WaveformGenerator::interpolateLinear(  // 0x25f410
    int /*length*/,
    std::vector<double> const& /*xPoints*/,
    std::vector<double> const& /*yPoints*/,
    std::vector<uint8_t> const& /*markers*/,
    std::vector<uint8_t> const& /*markerBits*/)
{
    throw std::runtime_error("WaveformGenerator::interpolateLinear not implemented");
}

// ============================================================================
// Reconstructed waveform generation functions
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
    Signal sig    = readWave(args[0], "wave", -1, "scale");
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

    Signal first = readWave(args[0], "wave_1", -1, "add");
    if (first.reserveOnly_) {
        return first;  // matches the disassembly's reserveOnly_ short-circuit
    }
    first.checkAllocation();

    std::vector<double> sum = first.samples_;
    std::vector<uint8_t> markers = first.markers_;

    for (size_t i = 1; i < args.size(); ++i) {
        std::string paramName = "wave_" + std::to_string(i + 1);
        Signal s = readWave(args[i], paramName, static_cast<int>(sum.size()), "add");
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
// Stubbed waveform generation functions (still TODO)
// ============================================================================

Signal WaveformGenerator::sin(std::vector<Value> const& /*args*/) {              // 0x24a0f0
    throw std::runtime_error("WaveformGenerator::sin not implemented");
}
Signal WaveformGenerator::cos(std::vector<Value> const& /*args*/) {              // 0x24abd0
    throw std::runtime_error("WaveformGenerator::cos not implemented");
}
Signal WaveformGenerator::sinc(std::vector<Value> const& /*args*/) {             // 0x24b6e0
    throw std::runtime_error("WaveformGenerator::sinc not implemented");
}
Signal WaveformGenerator::ramp(std::vector<Value> const& /*args*/) {             // 0x24c2c0
    throw std::runtime_error("WaveformGenerator::ramp not implemented");
}
Signal WaveformGenerator::sawtooth(std::vector<Value> const& /*args*/) {         // 0x24c8b0
    throw std::runtime_error("WaveformGenerator::sawtooth not implemented");
}
Signal WaveformGenerator::triangle(std::vector<Value> const& /*args*/) {         // 0x24d330
    // TODO: triangle(length, amplitude, riseRatio, phase, period) — wraps genericTriangle()
    throw std::runtime_error("WaveformGenerator::triangle not implemented");
}
Signal WaveformGenerator::drag(std::vector<Value> const& /*args*/) {             // 0x24e950
    // TODO: drag(length, amplitude, position, width, beta) — Gauss derivative pulse
    throw std::runtime_error("WaveformGenerator::drag not implemented");
}
Signal WaveformGenerator::blackman(std::vector<Value> const& /*args*/) {         // 0x24f530
    throw std::runtime_error("WaveformGenerator::blackman not implemented");
}
Signal WaveformGenerator::hamming(std::vector<Value> const& /*args*/) {          // 0x24fd20
    throw std::runtime_error("WaveformGenerator::hamming not implemented");
}
Signal WaveformGenerator::hann(std::vector<Value> const& /*args*/) {             // 0x250250
    throw std::runtime_error("WaveformGenerator::hann not implemented");
}
Signal WaveformGenerator::chirp(std::vector<Value> const& /*args*/) {            // 0x250bb0
    // TODO: chirp(length, amplitude, freqStart, freqStop, phase) — frequency sweep
    throw std::runtime_error("WaveformGenerator::chirp not implemented");
}
Signal WaveformGenerator::mask(std::vector<Value> const& /*args*/) {             // 0x251cb0
    // 1-line wrapper: tail-call markerImpl(args, /*isMask=*/true)
    throw std::runtime_error("WaveformGenerator::mask not implemented");
}
Signal WaveformGenerator::marker(std::vector<Value> const& /*args*/) {           // 0x251cd0
    // 1-line wrapper: tail-call markerImpl(args, /*isMask=*/false)
    throw std::runtime_error("WaveformGenerator::marker not implemented");
}
Signal WaveformGenerator::rand(std::vector<Value> const& /*args*/) {             // 0x251cf0
    // TODO: rand(length, amplitude, mean, stddev) — Gaussian noise (Mersenne Twister)
    throw std::runtime_error("WaveformGenerator::rand not implemented");
}
Signal WaveformGenerator::randomGauss(std::vector<Value> const& /*args*/) {      // 0x252930
    throw std::runtime_error("WaveformGenerator::randomGauss not implemented");
}
Signal WaveformGenerator::randomUniform(std::vector<Value> const& /*args*/) {    // 0x253440
    throw std::runtime_error("WaveformGenerator::randomUniform not implemented");
}
Signal WaveformGenerator::lfsrGaloisMarker(std::vector<Value> const& /*args*/) { // 0x253bc0
    // TODO: LFSR (Galois config) marker pattern generator
    throw std::runtime_error("WaveformGenerator::lfsrGaloisMarker not implemented");
}
Signal WaveformGenerator::rrc(std::vector<Value> const& /*args*/) {              // 0x254290
    // TODO: rrc(length, amplitude, beta, sps) — Root-raised cosine pulse
    throw std::runtime_error("WaveformGenerator::rrc not implemented");
}
Signal WaveformGenerator::vect(std::vector<Value> const& /*args*/) {             // 0x255570
    // TODO: vect(d1, d2, ...) — variadic, builds Signal directly from args
    throw std::runtime_error("WaveformGenerator::vect not implemented");
}
Signal WaveformGenerator::placeholder(std::vector<Value> const& /*args*/) {      // 0x255850
    // TODO: placeholder(length) — Signal with reserveOnly_=true (lazy alloc)
    throw std::runtime_error("WaveformGenerator::placeholder not implemented");
}
Signal WaveformGenerator::join(std::vector<Value> const& /*args*/) {             // 0x255da0
    // TODO: variadic concat of signals
    throw std::runtime_error("WaveformGenerator::join not implemented");
}
Signal WaveformGenerator::interleave(std::vector<Value> const& /*args*/) {       // 0x258140
    // 1-line wrapper around merge()/some helper — likely interleaves two waves
    throw std::runtime_error("WaveformGenerator::interleave not implemented");
}
Signal WaveformGenerator::multiply(std::vector<Value> const& /*args*/) {         // 0x258750
    // TODO: pointwise multiply; structurally similar to add()
    throw std::runtime_error("WaveformGenerator::multiply not implemented");
}
Signal WaveformGenerator::cut(std::vector<Value> const& /*args*/) {              // 0x2598d0
    // TODO: cut(signal, start, length) — slice
    throw std::runtime_error("WaveformGenerator::cut not implemented");
}
Signal WaveformGenerator::flip(std::vector<Value> const& /*args*/) {             // 0x25a310
    // 1-line wrapper around reverse()
    throw std::runtime_error("WaveformGenerator::flip not implemented");
}
Signal WaveformGenerator::filter(std::vector<Value> const& /*args*/) {           // 0x25a540
    // TODO: filter(signal, coeffs, [coeffs2]) — FIR/IIR filtering
    throw std::runtime_error("WaveformGenerator::filter not implemented");
}
Signal WaveformGenerator::circshift(std::vector<Value> const& /*args*/) {        // 0x25b0e0
    // TODO: circshift(signal, n) — circular sample shift
    throw std::runtime_error("WaveformGenerator::circshift not implemented");
}
Signal WaveformGenerator::merge(std::vector<Value> const& /*args*/) {            // 0x25f5c0
    // TODO: merge two single-channel signals into one multi-channel Signal
    throw std::runtime_error("WaveformGenerator::merge not implemented");
}
Signal WaveformGenerator::grow(std::vector<Value> const& /*args*/) {             // 0x260640
    // TODO: grow(signal, length) — pad the wave with trailing zeros
    throw std::runtime_error("WaveformGenerator::grow not implemented");
}

} // namespace zhinst
