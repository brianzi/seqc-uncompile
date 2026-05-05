// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// PlayConfig method implementations
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/waveform/play_config.hpp"

#include <algorithm>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

namespace zhinst {

// 0x1dc500 — Pack all fields into a 32-bit CWVF play word.
//
// Bit layout (32-bit):
//   [1:0]   channels     — channelMask, but forced to 1 if hold or dummy
//   [5:2]   rate         — max(effectiveRate, 0); effectiveRate = rate≥0 ? rate : defaultRate
//   [19:6]  suppress     — suppress, but holdSuppressExceptSigouts (636) if hold
//   [21:20] fourChannel  — channelMask if is4Channel, else 0
//   [22]    defaultRate  — sign bit of this->rate (1 if rate < 0)
//   [23]    dummy        — 1 if hold or dummy
//   [27:24] markerBits   — direct
//   [29:28] trigger      — direct
//   [31:30] precompFlag  — direct
//
// Key: hold and dummy share the same encoding — both force channels=1 and
// set the dummy bit. The suppress substitution only happens for hold.
uint32_t PlayConfig::encodeCwvf(int defaultRate) const {
    // Determine if "dummy mode" is active (hold OR dummy)
    bool dummyFlag = hold || dummy;

    // Channels: forced to 1 in dummy mode, otherwise use channelMask
    uint32_t channels = dummyFlag ? 1u : channelMask.value;

    // Rate: use field rate if >= 0, else use the provided defaultRate
    int effectiveRate = (rate >= 0) ? rate : defaultRate;
    uint32_t rateBits = static_cast<uint32_t>(std::max(effectiveRate, 0));

    // Suppress: substitute holdSuppressExceptSigouts when hold is active
    uint32_t suppressVal = hold ? holdSuppressExceptSigouts : suppress.value;

    // FourChannel: encode channelMask into fourChannel field when is4Channel
    uint32_t fourChannelVal = is4Channel ? channelMask.value : 0u;

    // DefaultRate bit: sign bit of effectiveRate (NOT this->rate).
    // Binary 0x1dc54f: shr r8d,0x1f — r8d is effectiveRate after cmovs at 0x1dc50e.
    uint32_t defaultRateBit = (static_cast<uint32_t>(effectiveRate) >> 31) & 1u;

    // Dummy bit: same as dummyFlag
    uint32_t dummyBit = dummyFlag ? 1u : 0u;

    // Pack everything
    uint32_t word = 0;
    word |= (channels      & 0x3u)  << channelsShift;      // [1:0]
    word |= (rateBits      & 0xFu)  << rateShift;           // [5:2]
    word |= (suppressVal   & 0x3FFFu) << suppressShift;     // [19:6]
    word |= (fourChannelVal & 0x3u) << fourChannelShift;     // [21:20]
    word |= (defaultRateBit & 0x1u) << defaultRateShift;     // [22]
    word |= (dummyBit      & 0x1u)  << dummyShift;          // [23]
    word |= (markerBits.value & 0xFu) << markerBitsShift;   // [27:24]
    word |= (trigger.value  & 0x3u) << triggerShift;         // [29:28]
    word |= (precompFlags.value & 0x3u) << precompFlagShift; // [31:30]

    return word;
}

// 0x1d5770 — Field-wise inequality comparison.
//
// Compares all fields EXCEPT:
//   - `now` is NOT compared (deliberately excluded — transient flag)
//   - `hold` is only compared when rate > 0 (hold is irrelevant for
//     default-rate or stopped playback)
bool PlayConfig::operator!=(const PlayConfig& other) const {
    if (channelMask.value != other.channelMask.value)
        return true;
    if (rate != other.rate)
        return true;
    if (suppress.value != other.suppress.value)
        return true;
    if (is4Channel != other.is4Channel)
        return true;
    if (markerBits.value != other.markerBits.value)
        return true;
    if (trigger.value != other.trigger.value)
        return true;
    if (precompFlags.value != other.precompFlags.value)
        return true;
    if (dummy != other.dummy)
        return true;
    // hold is only compared when rate > 0
    if (rate > 0 && hold != other.hold)
        return true;
    return false;
}

// 0x269d60 — Serialize to boost::json::value.
// Produces an object with 10 keys:
//   "channelMask", "rate", "suppress", "is4Channel", "markerBits",
//   "trigger", "precompFlags", "now", "hold", "dummy"
boost::json::value PlayConfig::toJson() const {
    return boost::json::value({
        {"channelMask",  static_cast<uint64_t>(channelMask.value)},
        {"rate",         rate},
        {"suppress",     static_cast<uint64_t>(suppress.value)},
        {"is4Channel",   is4Channel},
        {"markerBits",   static_cast<uint64_t>(markerBits.value)},
        {"trigger",      static_cast<uint64_t>(trigger.value)},
        {"precompFlags", static_cast<uint64_t>(precompFlags.value)},
        {"now",          now},
        {"hold",         hold},
        {"dummy",        dummy},
    });
}

// 0x26b440 — Static: deserialize from boost::json::value.
// Reads the same 10 keys as toJson produces.
// Numeric uint fields use 3-way type dispatch (int64→uint64→double)
// with range checking.
PlayConfig PlayConfig::fromJson(const boost::json::value& jv) {
    PlayConfig pc;
    const auto& obj = jv.as_object();

    // Bool fields
    pc.now  = obj.at("now").as_bool();
    pc.hold = obj.at("hold").as_bool();
    pc.dummy = obj.at("dummy").as_bool();

    // Uint fields (via to_number with range check)
    pc.channelMask  = detail::AddressImpl<uint32_t>{
        static_cast<uint32_t>(obj.at("channelMask").to_number<uint64_t>())};
    pc.suppress     = detail::AddressImpl<uint32_t>{
        static_cast<uint32_t>(obj.at("suppress").to_number<uint64_t>())};
    pc.markerBits   = detail::AddressImpl<uint32_t>{
        static_cast<uint32_t>(obj.at("markerBits").to_number<uint64_t>())};
    pc.trigger      = detail::AddressImpl<uint32_t>{
        static_cast<uint32_t>(obj.at("trigger").to_number<uint64_t>())};
    pc.precompFlags = detail::AddressImpl<uint32_t>{
        static_cast<uint32_t>(obj.at("precompFlags").to_number<uint64_t>())};

    // Int field
    pc.rate = static_cast<int32_t>(obj.at("rate").to_number<int64_t>());

    // Bool field
    pc.is4Channel = obj.at("is4Channel").as_bool();

    return pc;
}

} // namespace zhinst
