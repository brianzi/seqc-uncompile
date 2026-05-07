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

#include "zhinst/waveform/waveform_generator.hpp"
#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/waveform/waveform_front.hpp"
#include "zhinst/ast/eval_results.hpp"
#include "zhinst/ast/eval_result_value.hpp"
#include "zhinst/core/error_messages.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numeric>
#include <random>
#include <stdexcept>

#include "zhinst/runtime/resources.hpp"

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
// out-of-line definition (was forward-decl only). Defined in
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
//, replace the stub body with the make_shared-based code below.
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
    // IF-177: detect var arguments (ValueType::Unspecified == 0) before
    // toInt() — orig issues "<func> can't be called with var arguments"
    // (template id 103 / CantCallWithVar) instead of the internal
    // "unspecified value type detected in toInt conversion" exception.
    if (val.type_ == ValueType::Unspecified) {
        throw WaveformGeneratorValueException(
            ErrorMessages::format(CantCallWithVar, funcName),
            static_cast<size_t>(argIndex));
    }
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
                markerValue, markerValue & 3, funcName);
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

} // namespace zhinst
