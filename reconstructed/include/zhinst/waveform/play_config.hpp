// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// PlayConfig — packed waveform play configuration
// ============================================================================
#pragma once

#include <cstdint>
#include <boost/json/value.hpp>
#include "zhinst/asm/address_impl.hpp"

namespace zhinst {

// PlayConfig is embedded in Node objects for Play/Table node types.
// The fields are packed into a single uint32_t "play word" (CWVF register)
// using static shift/mask constants.
//
// Struct size: 0x20 (32 bytes), alignment 4.
//
// Play-word bit layout (32-bit):
//   Bits  0-1  : channels       (2 bits)
//   Bits  2-5  : rate           (4 bits)
//   Bits  6-19 : suppress       (14 bits)
//   Bits 20-21 : fourChannel    (2 bits)
//   Bit  22    : defaultRate    (1 bit)
//   Bit  23    : dummy          (1 bit)
//   Bits 24-27 : markerBits     (4 bits)
//   Bits 28-29 : trigger        (2 bits)
//   Bits 30-31 : precompFlag    (2 bits)
//! \brief Per-`playWave` configuration packed into the 32-bit CWVF
//! sequencer register: channel mask, sample rate, suppress mask,
//! marker bits, trigger setup, precompensation flags, and assorted
//! single-bit modifiers (`now`, `hold`, `dummy`, `is4Channel`).
//!
//! \details Carried alongside each Play / Table node in the waveform
//! intermediate representation. `encodeCwvf()` packs the fields into
//! the wire-format word; `toJson()` / `fromJson()` round-trip the
//! struct through JSON for the public AWG metadata section.
struct PlayConfig {
    detail::AddressImpl<uint32_t> channelMask = {};   //!< Channel-enable mask (2 bits in encoded word; bits 0-1). Selects which physical channels receive the waveform; forced to `1` in the encoded word when `hold` or `dummy` is set. JSON key `"channelMask"`. +0x00
    int32_t  rate          = 0;       //!< Sample-rate divider for this playback. A non-negative value is encoded directly; a negative value triggers the `defaultRate` bit and substitutes the default-rate argument passed to `encodeCwvf()`. JSON key `"rate"`. +0x04
    detail::AddressImpl<uint32_t> suppress = {};      //!< 14-bit suppress mask gating which output paths are suppressed during playback. Replaced by the constant `holdSuppressExceptSigouts` in the encoded word when `hold` is set. JSON key `"suppress"`. +0x08
    bool     is4Channel    = false;   //!< When `true`, the encoded word stores `channelMask` in the 4-channel field (bits 20-21) so that 4-channel devices can address all channels distinctly. JSON key `"is4Channel"`. +0x0C
    detail::AddressImpl<uint32_t> markerBits = {};    //!< Marker bits to drive on the output during this play (4 bits in the encoded word; bits 24-27). JSON key `"markerBits"`. +0x10
    detail::AddressImpl<uint32_t> trigger    = {};    //!< Trigger source / mode selector (2 bits in the encoded word; bits 28-29). JSON key `"trigger"`. +0x14
    detail::AddressImpl<uint32_t> precompFlags = {};  //!< Pre-compensation filter flags (2 bits in the encoded word; bits 30-31). JSON key `"precompFlags"`. +0x18
    bool     now           = false;   //!< If `true`, the play executes immediately without waiting for the dispatch trigger. \binarynote `now` is deliberately excluded from `operator!=` because it is a transient dispatch-time flag. JSON key `"now"`. +0x1C
    bool     hold          = false;   //!< If `true`, the play sustains the previous output (no new samples consumed); causes `channels` to be encoded as `1`, the dummy bit to be set, and `suppress` to be replaced by `holdSuppressExceptSigouts`. JSON key `"hold"`. +0x1D
    bool     dummy         = false;   //!< If `true`, encodes a dummy play whose `channels` is forced to `1` and dummy bit set, used for placeholder slots without sample output. JSON key `"dummy"`. +0x1E

    //! \name Encoded-word bit positions
    //! \brief Static shift constants giving the low-bit position of each
    //! field inside the 32-bit CWVF play word produced by `encodeCwvf()`.
    //! @{
    static inline constexpr uint32_t channelsShift     = 0;   //!< Shift for the 2-bit channel field (bits 0-1).
    static inline constexpr uint32_t rateShift          = 2;  //!< Shift for the 4-bit rate field (bits 2-5).
    static inline constexpr uint32_t suppressShift      = 6;  //!< Shift for the 14-bit suppress field (bits 6-19).
    static inline constexpr uint32_t fourChannelShift   = 20; //!< Shift for the 2-bit four-channel field (bits 20-21). 0x14.
    static inline constexpr uint32_t defaultRateShift   = 22; //!< Shift for the 1-bit default-rate flag (bit 22). 0x16.
    static inline constexpr uint32_t dummyShift         = 23; //!< Shift for the 1-bit dummy flag (bit 23). 0x17.
    static inline constexpr uint32_t markerBitsShift    = 24; //!< Shift for the 4-bit marker-bits field (bits 24-27). 0x18.
    static inline constexpr uint32_t triggerShift       = 28; //!< Shift for the 2-bit trigger field (bits 28-29). 0x1c.
    static inline constexpr uint32_t precompFlagShift   = 30; //!< Shift for the 2-bit precomp-flag field (bits 30-31). 0x1e.
    //! @}

    //! \name Encoded-word bit masks
    //! \brief Static masks (unshifted, position-aligned) covering the
    //! corresponding fields in the CWVF play word.
    //! @{
    static inline constexpr uint32_t channelsMask       = 0x00000003; //!< Mask for the channel field (bits 0-1).
    static inline constexpr uint32_t rateMask           = 0x0000003C; //!< Mask for the rate field (bits 2-5).
    static inline constexpr uint32_t suppressMask       = 0x000FFFC0; //!< Mask for the suppress field (bits 6-19).
    static inline constexpr uint32_t fourChannelMask    = 0x00300000; //!< Mask for the four-channel field (bits 20-21).
    static inline constexpr uint32_t defaultRateMask    = 0x00400000; //!< Mask for the default-rate flag (bit 22).
    static inline constexpr uint32_t dummyMask          = 0x00800000; //!< Mask for the dummy flag (bit 23).
    static inline constexpr uint32_t markerBitsMask     = 0x0F000000; //!< Mask for the marker-bits field (bits 24-27).
    static inline constexpr uint32_t triggerMask        = 0x30000000; //!< Mask for the trigger field (bits 28-29).
    static inline constexpr uint32_t precompFlagMask    = 0xC0000000; //!< Mask for the precomp-flag field (bits 30-31).
    //! @}

    //! \brief Suppress-mask sentinel substituted in the encoded word when
    //! `hold` is set, suppressing every path except the signal outputs
    //! (decimal `636`).
    static inline constexpr uint32_t holdSuppressExceptSigouts = 0x27C;  // 636

    // Pack all fields into a single uint32_t play word (CWVF register).
    // Reconstructed from encodeCwvf() at 0x1dc500.
    //
    // uint32_t encodeCwvf(int defaultRate) const:
    //   - channels = (dummy || hold) ? 1 : channelMask
    //     but if hold: suppress source is holdSuppressExceptSigouts instead of this->suppress
    //   - rate = (rate > 0) ? rate : 0;  (clamped, negative means use default)
    //   - is4Channel field encodes channelMask when is4Channel is true
    //   - defaultRate bit = (rate < 0) ? 1 : 0  (sign bit of rate)
    //   - dummy bit from the dummy || hold flags
    //   - All fields shifted and masked into a 32-bit word
    //! \brief Pack all fields into the 32-bit CWVF play word for the
    //! sequencer register.
    //! \details `channels` is forced to `1` when either `hold` or
    //! `dummy` is set; `rate` is replaced by `defaultRate` when this
    //! object's `rate` is negative (and the `defaultRate` bit is set
    //! to record that substitution); `suppress` is replaced by
    //! `holdSuppressExceptSigouts` when `hold` is set; the dummy bit
    //! is `hold || dummy`. The remaining fields are shifted into the
    //! masks documented above.
    //! \param defaultRate Sample-rate divider used when this object's
    //! `rate` is negative.
    //! \return The encoded 32-bit CWVF play word.
    uint32_t encodeCwvf(int defaultRate) const;

    //! \brief Static-call form of `encodeCwvf` accepting the `PlayConfig`
    //! as its first argument.
    //! \param config Configuration to encode.
    //! \param defaultRate Sample-rate divider used when `config.rate` is negative.
    //! \return The encoded 32-bit CWVF play word.
    static uint32_t encodeCwvf(const PlayConfig& config, int defaultRate) {
        return config.encodeCwvf(defaultRate);
    }

    //! \brief Field-wise inequality comparison used for play-word
    //! deduplication in the IR.
    //! \details Compares every field except `now` (deliberately
    //! excluded as a transient dispatch flag). `hold` is only
    //! compared when `rate > 0` because `hold` is irrelevant for a
    //! default-rate or stopped playback.
    //! \binarynote The asymmetry around `now` and `hold` means two
    //! configurations may compare equal despite differing `now` /
    //! `hold` values; do not use this operator as a substitute for
    //! exact memberwise equality.
    //! \param other Configuration to compare against.
    //! \return `true` if any compared field differs, `false` otherwise.
    bool operator!=(const PlayConfig& other) const;

    //! \brief Serialise this configuration to a JSON object.
    //! \details The produced object has exactly ten keys:
    //! `channelMask`, `rate`, `suppress`, `is4Channel`, `markerBits`,
    //! `trigger`, `precompFlags`, `now`, `hold`, `dummy`. Numeric
    //! address-typed fields are written as unsigned integers; `rate`
    //! is signed; the three single-bit flags are JSON booleans.
    //! \return A JSON value carrying the serialised representation.
    boost::json::value toJson() const;

    //! \brief Deserialise a `PlayConfig` from its JSON object form.
    //! \details Reads the same ten keys produced by `toJson`. The
    //! unsigned numeric fields are parsed via
    //! `boost::json::value::to_number<uint64_t>()` (which accepts
    //! integer, unsigned and double JSON values, range-checked);
    //! `rate` is parsed via `as_int64()`; flags via `as_bool()`. 0x26b440.
    //! \param jv JSON value expected to be an object with the ten keys above.
    //! \return The deserialised configuration.
    static PlayConfig fromJson(const boost::json::value& jv);
};

static_assert(sizeof(PlayConfig) == 0x20, "PlayConfig must be 0x20 bytes (pure POD, ABI-safe)");

} // namespace zhinst
