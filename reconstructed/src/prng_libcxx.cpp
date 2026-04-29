// ============================================================================
// PRNG shim — matches the original binary's runtime PRNG behavior.
//
// The original binary uses TWO different PRNGs for waveform generation:
//
//   1. `WaveformGenerator::rand`        — Park-Miller MINSTD LCG
//                                         (multiplier 48271, modulus 2^31-1)
//                                         + Marsaglia polar Box-Muller.
//                                         State is reset to 1 at the start
//                                         of each call (NOT shared across
//                                         calls or with MT19937_64).
//
//   2. `WaveformGenerator::randomGauss`,
//      `WaveformGenerator::randomUniform`  — libc++ MT19937_64 with
//                                            normal_distribution and
//                                            uniform_real_distribution.
//                                            State persists across calls in
//                                            `GlobalResources::random[]`
//                                            (TLS, 313 * 8 bytes).
//                                            Re-seeded by `randomSeed()`.
//
// libc++ vs libstdc++ disagree on Box-Muller pair order in
// `normal_distribution`: libstdc++ returns first then second of each
// pair, libc++ returns second then first. To match the binary's
// output byte-for-byte, the MT19937_64 path is compiled with
// `clang++ -stdlib=libc++` so the distribution code comes from libc++
// headers and inlines as weak symbols (no libc++.so runtime dep).
//
// MINSTD path is portable C++ (no stdlib PRNG), so it builds with the
// same toolchain regardless.
//
// State layout convention (libc++ mt19937_64):
//   uint64_t state[313]:
//     state[0..311] = MT state
//     state[312]    = read index (0 = next read at position 0)
//
// The original binary's GlobalResources::seedRandom uses this same
// convention (random[312] = 0 after init), so we share state with the
// rest of the recon transparently.
// ============================================================================

#include <random>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>

extern "C" {

// Re-seed with the binary's default seed (0x1571).
// Equivalent to `std::mt19937_64 rng(0x1571);` but writes into caller's
// 313*8-byte state array.
void seqc_libcxx_mt19937_seed_default(uint64_t* state) {
    std::mt19937_64 rng(0x1571);
    static_assert(sizeof(std::mt19937_64) == 313 * sizeof(uint64_t),
                  "mt19937_64 must be 313*8 bytes");
    __builtin_memcpy(state, &rng, sizeof(std::mt19937_64));
}

// Re-seed from /dev/urandom (matches the binary's randomSeed()).
// Reads 4 bytes (matching binary's std::random_device::operator() which
// returns unsigned int) and seeds the mt19937_64 with that.
void seqc_libcxx_mt19937_seed_urandom(uint64_t* state) {
    unsigned int seed = 0;
    if (FILE* f = std::fopen("/dev/urandom", "rb")) {
        (void)std::fread(&seed, sizeof(seed), 1, f);
        std::fclose(f);
    }
    std::mt19937_64 rng(seed);
    __builtin_memcpy(state, &rng, sizeof(std::mt19937_64));
}

// Generate `n` normal-distributed samples with the given (mean, stddev),
// scaled by `amplitude` (i.e. sample = amplitude * dist(rng)).
// Updates state in place.
void seqc_libcxx_normal_amplitude(uint64_t* state, double amplitude,
                                   double mean, double stddev,
                                   double* out, size_t n) {
    auto* rng = reinterpret_cast<std::mt19937_64*>(state);
    std::normal_distribution<double> dist(mean, stddev);
    for (size_t i = 0; i < n; ++i) {
        out[i] = amplitude * dist(*rng);
    }
}

// Generate `n` uniform-distributed samples in [min, max].
// Updates state in place.
void seqc_libcxx_uniform(uint64_t* state, double min, double max,
                          double* out, size_t n) {
    auto* rng = reinterpret_cast<std::mt19937_64*>(state);
    std::uniform_real_distribution<double> dist(min, max);
    for (size_t i = 0; i < n; ++i) {
        out[i] = dist(*rng);
    }
}

// ----------------------------------------------------------------------------
// MINSTD + Marsaglia polar Box-Muller — matches binary's WaveformGenerator::rand
// ----------------------------------------------------------------------------
//
// Reverse-engineered from disassembly at 0x251cf0..0x252800 in
// _seqc_compiler.so. Verified bit-exact against original ELF samples
// (12 of 12 first samples match for rand(1024, 1.0, 0, 0.1)).
//
// Algorithm:
//   - Park-Miller MINSTD LCG: state = (state * 48271) mod (2^31 - 1)
//   - State is reset to 1 at the START of each call to rand() (verified
//     by `mov $0x1, %edx` at 0x25255c, before the generation loop).
//   - Per Box-Muller trial: 4 LCG steps -> 2 uniforms in [-1, 1)
//     using libstdc++/libc++ uniform_real_distribution-style combine:
//       u = 2*(low + high*M)/M^2 - 1   where M = 2^31 - 2
//   - Marsaglia polar rejection: while s = u^2 + v^2 >= 1 or s == 0,
//     regenerate. Else factor = sqrt(-2 ln(s) / s); emit (v*factor)
//     and cache (u*factor) for next sample.
//   - Output is alternately v*f then cached u*f, then a fresh trial.
//   - Final sample = (z * stddev + mean) * amplitude
//     (stddev/mean/amplitude order verified at 0x252795..0x25279f).
//
// Notes on lane ordering (verified by sample comparison):
//   xmm0  packs [low_of_lcg1, low_of_lcg3]  (from r8, rsi after paddq -1)
//   xmm12 packs [high_of_lcg4, high_of_lcg2] (from rdx, rcx after paddq -1)
//   After convert/combine:
//     u = pair_to_uniform(low=lcg1, high=lcg4)   -> lane0
//     v = pair_to_uniform(low=lcg3, high=lcg2)   -> lane1
//   Output emit order: v*f first (unpckhpd extracts lane1), then u*f.
void seqc_minstd_normal_amplitude(double amplitude, double mean, double stddev,
                                   double* out, size_t n) {
    constexpr uint64_t kMul = 48271ULL;
    constexpr uint64_t kMod = 2147483647ULL;   // 2^31 - 1
    constexpr double   kRangeM = 2147483646.0; // 2^31 - 2
    const double kRangeM2 = kRangeM * kRangeM; // (2^31 - 2)^2

    uint64_t state = 1;
    auto step = [&]() -> uint64_t {
        state = (state * kMul) % kMod;
        return state;  // in [1, 2^31-1]
    };
    auto pair_to_uniform = [&](uint64_t low, uint64_t high) -> double {
        // low,high in [1, 2^31-1]; subtract 1 -> [0, 2^31-2].
        double dl = static_cast<double>(low - 1);
        double dh = static_cast<double>(high - 1);
        return 2.0 * (dl + dh * kRangeM) / kRangeM2 - 1.0;
    };

    double cached = 0.0;
    bool have_cached = false;

    for (size_t i = 0; i < n; ++i) {
        double z;
        if (have_cached) {
            z = cached;
            have_cached = false;
        } else {
            double u, v, s;
            do {
                uint64_t a = step();
                uint64_t b = step();
                uint64_t c = step();
                uint64_t d = step();
                u = pair_to_uniform(/*low=*/a, /*high=*/d);
                v = pair_to_uniform(/*low=*/c, /*high=*/b);
                s = u * u + v * v;
            } while (s >= 1.0 || s == 0.0);
            double factor = std::sqrt(-2.0 * std::log(s) / s);
            cached = u * factor;       // emitted on next call
            z      = v * factor;       // emit now (lane1)
            have_cached = true;
        }
        out[i] = (z * stddev + mean) * amplitude;
    }
}

}  // extern "C"
