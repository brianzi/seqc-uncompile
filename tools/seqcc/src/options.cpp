// =============================================================================
// seqcc — option-model implementation
// =============================================================================

#include "options.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace seqcc {

namespace {

//! Keys that `-mtune=KEY=VALUE` rejects outright.  These keys either
//! have a dedicated flag now (use that instead) or are silently
//! ignored by compileSeqc() (use the right flag instead).
//!
//! Currently:
//!   - `index` — was silently a no-op in T2 (see IF-296 in
//!     reconstructed/notes/incidental_findings.md).  Use `--index=N`.
bool isReservedTuneKey(std::string const& k) {
    static const std::unordered_set<std::string> kReserved{"index"};
    return kReserved.count(k) != 0;
}

//! Keys recognised by compileSeqc() as JSON numbers (not strings).
//! Currently only `samplerate`; the binding's kwarg merge tries
//! `cast<double>` before `cast<string>`, so any numeric value the user
//! supplies via the escape hatch should land as a JSON number.
bool isNumericTuneKey(std::string const& k) {
    return k == "samplerate";
}

//! Minimal JSON string escaper.  compileSeqc() runs the input through
//! boost::json::parse, so anything that breaks JSON well-formedness
//! must be escaped.  Covers the characters that can legally appear in
//! a Windows or POSIX file path (`\`, `"`, control chars).
void appendJsonString(std::string& out, std::string const& s) {
    out.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

//! Append a JSON number in a form that round-trips through
//! `boost::json::parse` losslessly.  Uses max-digits10 precision so
//! `samplerate=2.4e9` doesn't drift to `2399999999.something` via the
//! default `<<` precision.
void appendJsonNumber(std::string& out, double d) {
    std::ostringstream os;
    os.precision(17);
    os << d;
    out += os.str();
}

//! Append a `"key":value` pair to a JSON-object-in-progress.
//! Sets `first` to false; caller initialises it to true.
void appendKey(std::string& out, bool& first, std::string const& key) {
    if (!first) out.push_back(',');
    first = false;
    appendJsonString(out, key);
    out.push_back(':');
}

}  // namespace

std::string buildJsonConfig(Options const& opts) {
    std::string out;
    out.push_back('{');
    bool first = true;

    // Dedicated fields first.  Track which keys we've emitted so the
    // escape-hatch loop below doesn't re-emit them (it's harmless for
    // boost::json::parse — last-wins — but tidy output makes
    // `--dump=compile-config` readable in T3b).
    std::unordered_set<std::string> emitted;
    auto emitStringField = [&](char const* key, std::string const& val) {
        if (val.empty()) return;
        appendKey(out, first, key);
        appendJsonString(out, val);
        emitted.insert(key);
    };
    emitStringField("sequencer", opts.sequencer);
    emitStringField("filename",  opts.filename);
    emitStringField("wavepath",  opts.wavePath);
    emitStringField("waveforms", opts.waveforms);

    if (opts.samplerate.has_value()) {
        appendKey(out, first, "samplerate");
        appendJsonNumber(out, *opts.samplerate);
        emitted.insert("samplerate");
    }

    // Escape-hatch kwargs.
    for (auto const& [k, v] : opts.tune) {
        if (emitted.count(k)) continue;  // dedicated flag wins
        appendKey(out, first, k);
        if (isNumericTuneKey(k)) {
            std::size_t consumed = 0;
            double d;
            try {
                d = std::stod(v, &consumed);
            } catch (std::exception const& e) {
                throw std::invalid_argument(
                    "-mtune=" + k + "=" + v +
                    ": not a valid number (" + e.what() + ")");
            }
            if (consumed != v.size()) {
                throw std::invalid_argument(
                    "-mtune=" + k + "=" + v +
                    ": trailing characters after numeric value");
            }
            appendJsonNumber(out, d);
        } else {
            appendJsonString(out, v);
        }
    }

    out.push_back('}');
    return out;
}

std::string buildDevoptsString(Options const& opts) {
    std::string out;
    bool first = true;
    for (auto const& d : opts.devopts) {
        if (d.empty()) continue;  // tolerate empty entries from -mdevopts= splits
        if (!first) out.push_back('\n');
        first = false;
        out += d;
    }
    return out;
}

// Exposed for main.cpp's -mtune= validation; defined here so the
// reserved-key list lives in one place.
bool tuneKeyIsReserved(std::string const& k) {
    return isReservedTuneKey(k);
}

}  // namespace seqcc
