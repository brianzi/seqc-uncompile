// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// PlayConfig — packed waveform play configuration
// ============================================================================
#pragma once

#include <cstdint>
#include <boost/json/value.hpp>
#include "address_impl.hpp"

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
struct PlayConfig {
    detail::AddressImpl<uint32_t> channelMask = {};   // +0x00  JSON: "channelMask"
    int32_t  rate          = 0;       // +0x04  JSON: "rate"
    detail::AddressImpl<uint32_t> suppress = {};      // +0x08  JSON: "suppress"
    bool     is4Channel    = false;   // +0x0C  JSON: "is4Channel"
    // 3 bytes padding
    detail::AddressImpl<uint32_t> markerBits = {};    // +0x10  JSON: "markerBits"
    detail::AddressImpl<uint32_t> trigger    = {};    // +0x14  JSON: "trigger"
    detail::AddressImpl<uint32_t> precompFlags = {};  // +0x18  JSON: "precompFlags"
    bool     now           = false;   // +0x1C  JSON: "now"
    bool     hold          = false;   // +0x1D  JSON: "hold"
    bool     dummy         = false;   // +0x1E  JSON: "dummy"
    // 1 byte padding

    // --- Static shift constants (bit positions) ---
    static inline constexpr uint32_t channelsShift     = 0;
    static inline constexpr uint32_t rateShift          = 2;
    static inline constexpr uint32_t suppressShift      = 6;
    static inline constexpr uint32_t fourChannelShift   = 20;   // 0x14
    static inline constexpr uint32_t defaultRateShift   = 22;   // 0x16
    static inline constexpr uint32_t dummyShift         = 23;   // 0x17
    static inline constexpr uint32_t markerBitsShift    = 24;   // 0x18
    static inline constexpr uint32_t triggerShift       = 28;   // 0x1c
    static inline constexpr uint32_t precompFlagShift   = 30;   // 0x1e

    // --- Static mask constants ---
    static inline constexpr uint32_t channelsMask       = 0x00000003;
    static inline constexpr uint32_t rateMask           = 0x0000003C;
    static inline constexpr uint32_t suppressMask       = 0x000FFFC0;
    static inline constexpr uint32_t fourChannelMask    = 0x00300000;
    static inline constexpr uint32_t defaultRateMask    = 0x00400000;
    static inline constexpr uint32_t dummyMask          = 0x00800000;
    static inline constexpr uint32_t markerBitsMask     = 0x0F000000;
    static inline constexpr uint32_t triggerMask        = 0x30000000;
    static inline constexpr uint32_t precompFlagMask    = 0xC0000000;

    // Special constant: suppress value used for "hold suppress except sigouts"
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
    uint32_t encodeCwvf(int defaultRate) const;

    // Field-wise inequality. Special: hold field only compared when rate > 0.
    // Reconstructed from operator!= at 0x1d5770.
    bool operator!=(const PlayConfig& other) const;

    // Serialize to JSON object with keys:
    //   channelMask, rate, suppress, is4Channel, markerBits,
    //   trigger, precompFlags, now, hold, dummy
    boost::json::value toJson() const;

    // Static: deserialize from JSON object. 0x26b440
    static PlayConfig fromJson(const boost::json::value& jv);

    // Deserialize from a boost::json::value (object).
    // Reads the same 10 keys as toJson.
    // Numeric fields (channelMask, suppress, markerBits, trigger, precompFlags)
    // are read via to_number<uint64_t>() which handles int/uint/double JSON types.
    // rate is read via as_int64(). Booleans via as_bool().
    // Reconstructed from fromJson() at 0x26b440.
    static PlayConfig fromJson(const boost::json::value& json);
};

} // namespace zhinst
