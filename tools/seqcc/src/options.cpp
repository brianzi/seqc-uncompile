// =============================================================================
// seqcc — option-model implementation (T2)
// =============================================================================

#include "options.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace seqcc {

namespace {

//! Keys whose values must be serialized as JSON numbers (double),
//! matching the binding's kwarg handling: `cast<double>` is tried
//! before `cast<string>`, so numeric kwargs land as JSON numbers in
//! the merged config object.
//!
//! Only `samplerate` is actually consumed as a number by compileSeqc()
//! today (see compile_seqc.cpp ~L196); the others are kept as a small
//! allowlist so that future numeric config keys "just work" without
//! having to extend seqcc again.
//!
//! TODO(IF-296): `index` is listed here but compileSeqc() ignores the
//! JSON `"index"` key entirely — it reads the AWG core only from its
//! positional `awgIndex` parameter.  Anything a user passes via
//! `-mtune=index=N` is silently dropped.  Remove from this allowlist
//! once appendTune() rejects or redirects the key in T3.
bool isNumericKey(std::string const& k) {
    static const std::unordered_set<std::string> kNumeric{
        "samplerate", "index"
    };
    return kNumeric.count(k) != 0;
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
                    // Rare in practice — emit \u00XX.
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

}  // namespace

std::string buildJsonConfig(Options const& opts) {
    std::string out;
    out.push_back('{');
    bool first = true;
    for (auto const& [k, v] : opts.tune) {
        if (!first) out.push_back(',');
        first = false;
        appendJsonString(out, k);
        out.push_back(':');
        if (isNumericKey(k)) {
            // Validate via std::stod; reuse the canonical decimal form
            // it parses so we never emit JSON-illegal tokens like "inf"
            // or thousands separators.
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
            // Round-trip through ostringstream for a stable JSON-safe
            // representation.  std::to_string truncates to 6 digits;
            // the original binding emits boost::json's default double
            // formatting which preserves enough precision for an exact
            // round-trip — ostringstream with max_digits10 matches.
            std::ostringstream os;
            os.precision(17);
            os << d;
            out += os.str();
        } else {
            appendJsonString(out, v);
        }
    }
    out.push_back('}');
    return out;
}

}  // namespace seqcc
