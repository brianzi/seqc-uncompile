// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Class: zhinst::CsvParser
//
// The binary contains TWO non-template csvFileToWaveform implementations:
//   void CsvParser::csvFileToWaveform<WaveformFront>(shared_ptr<WaveformFront>, AwgDeviceType)  @0x2ba8b0
//   void CsvParser::csvFileToWaveform<WaveformIR>   (shared_ptr<WaveformIR>,    AwgDeviceType)  @0x2be830
//
// Supporting functions (all in the same TU):
//   CsvException::CsvException(string const&)                          @0x2b8b80
//   CsvException::~CsvException()                                      @0x2b8be0
//   CsvParser::getLineVector<WaveformFront>(shared_ptr<WaveformFront>) @0x2b8c20
//   CsvParser::getLineVector<WaveformIR>(shared_ptr<WaveformIR>)       @0x2bc200 (nearly identical)
//   CsvParser::setSampleFromString<WaveformFront>(...)                 @0x2b85c0
//   CsvParser::setSampleFromString<WaveformIR>(...)                    @0x2b8420 (nearly identical)
//   (anonymous namespace)::isCsvSeparator(char)                        @0x2ba7d0
//   ErrorMessages::format<string,size_t>(ErrorMessageT, string, size_t) @0x2b8a00
// ============================================================================

#include "zhinst/types.hpp"
#include "zhinst/waveform.hpp"
#include "zhinst/waveform_front.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/signal.hpp"
#include "zhinst/cached_parser.hpp"
#include "zhinst/error_messages.hpp"

#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/tokenizer.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace zhinst {

// ============================================================================
// CsvException — lightweight exception for CSV parse errors
// Layout: vptr (8) + std::string (24) = 0x20 bytes
// RTTI at 0xb078a0, vtable at 0xb089e0
// Inherits from std::bad_exception (for RTTI chain); source-level we just
// use std::exception since MI with bad_exception is not semantically important.
// ============================================================================
class CsvException : public std::exception {           // @0x2b8b80 ctor, @0x2b8be0 dtor
public:
    explicit CsvException(std::string const& msg)      // @0x2b8b80
        : message_(msg) {}

    ~CsvException() override = default;                // @0x2b8be0

    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;  // +0x08
};

// ============================================================================
// Forward declarations for wave conversion utilities (defined in awg_compiler.cpp)
// These live in zhinst::util::wave in the binary.
// ============================================================================
namespace util { namespace wave {
    double awg2double(uint16_t raw);     // @0x2996d0
    double awg2double16(uint32_t raw);   // @0x299740
    uint8_t awg2marker(uint16_t raw);    // @0x2996f0
}}

// ============================================================================
// Anonymous namespace helpers
// ============================================================================
namespace {

// isCsvSeparator — @0x2ba7d0
// Uses a function-local static string ",; \t" initialized from .rodata 0x90a7f5.
// Returns true if the character is found in the separator set.
bool isCsvSeparator(char c) {                          // @0x2ba7d0
    static const std::string sep(",; \t");
    if (sep.empty()) return false;
    return sep.find(c) != std::string::npos;
}

} // anonymous namespace

// ============================================================================
// CsvParser class — declared inline in wavetable TUs but defined here
// ============================================================================
class CsvParser {
public:
    template <typename WfT>
    static void csvFileToWaveform(CachedParser& cache, std::shared_ptr<WfT> wf, AwgDeviceType deviceType);

private:
    // getLineVector: opens CSV file, reads lines, trims, skips comments/empty,
    // auto-detects format, returns vector of data line strings (stripped of
    // comment-only prefix content, tokenized by separators).
    // @0x2b8c20 (WaveformFront), @0x2bc200 (WaveformIR)
    template <typename WfT>
    static std::vector<std::string> getLineVector(
        CachedParser& cachedParser,
        std::shared_ptr<WfT> const& wf);

    // setSampleFromString: parses a single cell string into a sample value
    // and marker, stores into the waveform's signal buffers.
    // @0x2b85c0 (WaveformFront), @0x2b8420 (WaveformIR)
    template <typename WfT>
    static void setSampleFromString(
        std::string const& cellStr,
        std::shared_ptr<WfT> const& wf,
        AwgDeviceType deviceType,
        size_t row,
        size_t col);
};

// ============================================================================
// setSampleFromString<WaveformFront>  @0x2b85c0
//
// If wf->file->isIntegerFormat != 0 (AWG integer format):
//   stol(cellStr) → range check → dispatch on deviceType:
//     types 1,4: awg2double(uint16), else: awg2double16(uint32) + awg2marker(uint16)
// If wf->file->isIntegerFormat == 0 (float format):
//   stod(cellStr) → sample
//
// Stores sample in wf->signal.samples_[row*channels + col],
//        marker in wf->signal.markers_[row*channels + col],
//        OR's marker into wf->signal.markerBits_[index % markerBits.size()]
// ============================================================================
template <>
void CsvParser::setSampleFromString<WaveformFront>(    // @0x2b85c0
        std::string const& cellStr,
        std::shared_ptr<WaveformFront> const& wf,
        AwgDeviceType deviceType,
        size_t row,
        size_t col)
{
    auto* filePtr = wf->file.get();
    uint8_t marker = 0;
    double sample;

    if (filePtr->isIntegerFormat != 0) {
        // AWG integer format path
        long val = std::stol(cellStr, nullptr, 0);

        // Range check: determine max based on device type
        uint32_t maxVal;
        if (deviceType == UHFLI || deviceType == UHFQA) {
            maxVal = 0xFEFFFF;  // 16-bit AWG: ~16M
        } else {
            maxVal = 0xFBFFFF;  // 32-bit AWG: ~16M (different marker layout)
        }

        if (val < 0 || static_cast<uint32_t>(val) > maxVal) {
            // Throw CsvException with ErrorMessage 0x25 (CsvWrongValue)
            // format<string, size_t>(0x25, file->name, row)
            throw CsvException(
                ErrorMessages::format(
                    static_cast<ErrorMessageT>(0x25),
                    filePtr->name, row));
        }

        uint16_t raw16 = static_cast<uint16_t>(val);
        if (deviceType == UHFLI || deviceType == UHFQA) {
            sample = util::wave::awg2double(raw16);
        } else {
            sample = util::wave::awg2double16(static_cast<uint32_t>(val));
            marker = util::wave::awg2marker(raw16);
        }
    } else {
        // Float format path
        sample = std::stod(cellStr, nullptr);
    }

    // Store sample and marker
    uint16_t channels = wf->signal.channels_;
    size_t idx = row * channels + col;
    wf->signal.samples_[idx] = sample;
    wf->signal.markers_[idx] = marker;

    // OR marker into markerBits at index % markerBits.size()
    size_t mbSize = wf->signal.markerBits_.size();
    if (mbSize > 0) {
        wf->signal.markerBits_[idx % mbSize] |= marker;
    }
}

// ============================================================================
// setSampleFromString<WaveformIR>  @0x2b8420
// Identical logic to WaveformFront specialization.
// ============================================================================
template <>
void CsvParser::setSampleFromString<WaveformIR>(       // @0x2b8420
        std::string const& cellStr,
        std::shared_ptr<WaveformIR> const& wf,
        AwgDeviceType deviceType,
        size_t row,
        size_t col)
{
    auto* filePtr = wf->file.get();
    uint8_t marker = 0;
    double sample;

    if (filePtr->isIntegerFormat != 0) {
        long val = std::stol(cellStr, nullptr, 0);

        uint32_t maxVal;
        if (deviceType == UHFLI || deviceType == UHFQA) {
            maxVal = 0xFEFFFF;
        } else {
            maxVal = 0xFBFFFF;
        }

        if (val < 0 || static_cast<uint32_t>(val) > maxVal) {
            throw CsvException(
                ErrorMessages::format(
                    static_cast<ErrorMessageT>(0x25),
                    filePtr->name, row));
        }

        uint16_t raw16 = static_cast<uint16_t>(val);
        if (deviceType == UHFLI || deviceType == UHFQA) {
            sample = util::wave::awg2double(raw16);
        } else {
            sample = util::wave::awg2double16(static_cast<uint32_t>(val));
            marker = util::wave::awg2marker(raw16);
        }
    } else {
        sample = std::stod(cellStr, nullptr);
    }

    uint16_t channels = wf->signal.channels_;
    size_t idx = row * channels + col;
    wf->signal.samples_[idx] = sample;
    wf->signal.markers_[idx] = marker;

    size_t mbSize = wf->signal.markerBits_.size();
    if (mbSize > 0) {
        wf->signal.markerBits_[idx % mbSize] |= marker;
    }
}

// ============================================================================
// getLineVector — @0x2b8c20 (WaveformFront), @0x2bc200 (WaveformIR)
//
// Opens the CSV file via boost::filesystem::basic_ifstream.
// Reads line by line with std::getline.
// Trims each line with boost::algorithm::trim.
// Skips empty lines.
// Lines starting with '%' are comment lines:
//   - If wf->file->formatType == 1 (AWG format), skip all comment lines.
//   - Otherwise, check for "% Time (s)" pattern; if found and it's the
//     ONLY occurrence (exact 10-char match), skip that line.
//   - In comment lines (non-"% Time (s)"), scan for separator chars using
//     bitmask 0x800100000000200 (matches tab=9, comma=44, semicolon=59).
//     If found, use boost::tokenizer with ",;\t" separators to extract
//     columns. If >2 columns: set formatType=2 (multi-column float).
//     If <=2 columns: set formatType=1 (single-column AWG integer).
//
// For non-comment data lines when formatType == 0 (first data line, auto-detect):
//   - Count separators using isCsvSeparator(), set field count in local var.
//   - Check if value starts with "0x" and has no '.' or 'e': if so, leave
//     isIntegerFormat as-is (AWG integer); otherwise set isIntegerFormat=0 (float format).
//
// For formatType == 1 (AWG integer format): scan for separator in data line
//   using bitmask 0x800100000000200; if no separator found, treat as
//   single-column (field count = -1).
//
// Data lines are stripped of trailing comment (everything after first '%'
// that isn't part of a "% Time (s)" header), then appended to result vector.
//
// For format==0 (auto-detected float): each line is tokenized with
// ",; \t" separator, individual tokens are trimmed and parsed with stod().
// For format==2 (multi-column float): same tokenization.
// For format==1 (AWG integer): lines are returned as-is (single string per line).
// ============================================================================
template <typename WfT>
std::vector<std::string> CsvParser::getLineVector(
    CachedParser& /*cachedParser*/,
    std::shared_ptr<WfT> const& wf)
{
    std::vector<std::string> result;

    // Open CSV file
    auto& fileName = wf->file->name;
    boost::filesystem::ifstream ifs(fileName, std::ios::in);

    if (!ifs.is_open()) {
        // File failed to open — return empty
        return result;
    }

    std::string line;
    size_t lineNum = 1;
    bool firstDataLine = true;

    while (std::getline(ifs, line)) {
        boost::algorithm::trim(line);

        // Skip empty lines
        if (line.empty()) {
            ++lineNum;
            continue;
        }

        // Comment line handling (lines starting with '%')
        if (line[0] == '%') {
            // If AWG integer format (formatType == 1), skip all comment lines
            if (wf->file->formatType == 1) {
                ++lineNum;
                continue;
            }

            // Check for "% Time (s)" pattern (10-char match)
            // Binary checks: 8-byte match at offset 0 = 0x2820656d69542025 ("% Time (")
            //                 2-byte match at offset 8 = 0x2973 ("s)")
            if (line.size() >= 10) {
                // Search for "% Time (s)" substring in the comment line
                auto pos = line.find("% Time (s)");
                if (pos != std::string::npos) {
                    // Found "% Time (s)" — skip this comment line
                    ++lineNum;
                    continue;
                }
            }

            // For other comment lines, scan for separator characters to
            // auto-detect format. Bitmask 0x800100000000200 matches:
            //   bit 9 = tab, bit 32 = space, bit 44 = comma, bit 59 = semicolon
            // (but bit 32 is actually beyond 64-bit for space... the bitmask
            //  0x800100000000200 matches tab(9), comma(44), semicolon(59))
            bool hasSep = false;
            for (size_t i = 0; i < line.size(); ++i) {
                unsigned char ch = static_cast<unsigned char>(line[i]);
                if (ch <= 59) {
                    uint64_t mask = 0x800100000000200ULL;
                    if ((mask >> ch) & 1) {
                        hasSep = true;
                        break;
                    }
                }
            }

            if (hasSep) {
                // Tokenize with ",;\t" to count columns
                boost::char_separator<char> sep(",;\t", "", boost::drop_empty_tokens);
                boost::tokenizer<boost::char_separator<char>> tok(line, sep);
                size_t colCount = 0;
                for (auto it = tok.begin(); it != tok.end(); ++it) {
                    ++colCount;
                }
                if (colCount > 2) {
                    wf->file->formatType = 2;  // multi-column float
                } else {
                    wf->file->formatType = 1;  // AWG integer
                }
            }

            ++lineNum;
            continue;
        }

        // Data line handling

        // Strip trailing comment: find first '%' (that isn't "% Time (s)")
        // Actually, from the binary: the data line is truncated at the first
        // '%' character, unless no '%' is found.
        {
            // Search for '%' in the data portion
            auto pos = line.find('%');
            if (pos != std::string::npos) {
                line = line.substr(0, pos);
            }
            // Clamp to actual string length (npos means no truncation)
        }

        // Append to result vector
        result.push_back(line);

        // Format auto-detection on first data line when formatType == 0
        if (wf->file->formatType == 0) {
            if (firstDataLine) {
                firstDataLine = false;

                // Count number of separator-delimited columns
                size_t numFields = 1;
                bool prevWasSep = false;
                for (size_t i = 0; i < line.size(); ++i) {
                    bool isSep = isCsvSeparator(line[i]);
                    if (isSep && !prevWasSep) {
                        ++numFields;
                    }
                    prevWasSep = isSep;
                }

                // Check if this looks like "0x..." hex format (AWG integer)
                // by checking for "0x" prefix and absence of '.' and 'e'
                if (line.size() >= 2 &&
                    line[0] == '0' && line[1] == 'x') {
                    // It starts with "0x" — check no '.' or 'e' present
                    bool hasDot = (std::memchr(line.data(), '.', line.size()) != nullptr);
                    bool hasE   = (std::memchr(line.data(), 'e', line.size()) != nullptr);
                    if (!hasDot && !hasE) {
                        // AWG integer format — isIntegerFormat stays as-is (non-zero)
                        // (already set by caller or default)
                    } else {
                        wf->file->isIntegerFormat = 0;  // float format
                    }
                } else {
                    wf->file->isIntegerFormat = 0;  // float format
                }
            } else if (wf->file->isIntegerFormat != 0) {
                // Subsequent lines in AWG format: scan for separator
                // If no separator found, single-column
            }
        }

        // For formatType == 1 (AWG integer format): scan data line for separator
        // using bitmask to detect column count per line
        if (wf->file->formatType == 1) {
            bool hasSep = false;
            for (size_t i = 0; i < line.size(); ++i) {
                unsigned char ch = static_cast<unsigned char>(line[i]);
                if (ch <= 59) {
                    uint64_t mask = 0x800100000000200ULL;
                    if ((mask >> ch) & 1) {
                        hasSep = true;
                        break;
                    }
                }
            }
            (void)hasSep;  // result used downstream but handling is in csvFileToWaveform
        }

        ++lineNum;
    }

    return result;
}

// Explicit instantiations
template std::vector<std::string> CsvParser::getLineVector<WaveformFront>(
    CachedParser&, std::shared_ptr<WaveformFront> const&);
template std::vector<std::string> CsvParser::getLineVector<WaveformIR>(
    CachedParser&, std::shared_ptr<WaveformIR> const&);

// ============================================================================
// csvFileToWaveform<WaveformFront>  @0x2ba8b0 (~7KB, stack frame 0x528)
//
// Flow:
//   1. Check wf and wf->file are non-null, else throw CsvException(errMsg[0xe9])
//   2. If wf->file->data (hash at +0x28) is empty, compute hash via
//      CachedParser::getHash(file->name) and store into file->data.
//   3. CachedParser::getCachedFile(hash) — returns CachedFile
//   4. If cache hit (samples_ non-empty): copy samples/markers/markerBits
//      directly into wf->signal, set channels_ and length_, done.
//   5. If cache miss: call getLineVector<WfT>() to parse the CSV.
//   6. Dispatch on wf->file->formatType (waveformType):
//      - 1 (AWG integer): allocate samples/markers/markerBits arrays sized
//        by lineCount * channels, iterate lines calling setSampleFromString
//      - 2 (multi-column float): same but tokenize with ",; \t" first,
//        each token → stod → sample
//      - 0 (simple float / auto-detected): tokenize each line with ",; \t",
//        each token → trim → stod → sample
//   7. After parsing: assign arrays to wf->signal, set channels/length,
//      call Signal::checkAllocation().
//   8. CachedParser::cacheFile(...) to store parsed data.
//   9. Clean up temporary vectors and getLineVector result.
// ============================================================================
template <>
void CsvParser::csvFileToWaveform<WaveformFront>(      // @0x2ba8b0
        CachedParser& cache,
        std::shared_ptr<WaveformFront> wf,
        AwgDeviceType deviceType)
{
    // Step 1: null checks
    if (!wf || !wf->file) {
        // errMsg[0xe9] = ErrorMessageT(233) = WaveformNotFound
        throw CsvException(errMsg[static_cast<ErrorMessageT>(0xe9)]);
    }

    auto& fileRef = *wf->file;

    // Step 2: compute hash if not already present                              // @0x2ba910
    if (fileRef.data.begin() == fileRef.data.end()) {
        fileRef.data = cache.getHash(fileRef.name);                            // @0x2ba938
    }

    // Step 3: check cache                                                     // @0x2ba960
    CachedParser::CachedFile cached = cache.getCachedFile(fileRef.data);       // @0x2ba990

    // Step 4: cache hit — copy directly into wf->signal                       // @0x2ba9a0
    if (!cached.samples_.empty()) {
        wf->signal.samples_ = std::move(cached.samples_);
        wf->signal.markers_ = std::move(cached.markers_);
        wf->signal.channels_ = cached.channel_;
        if (!wf->signal.samples_.empty() && wf->signal.channels_ > 0) {
            wf->signal.length_ = wf->signal.samples_.size() / wf->signal.channels_;
        }
        // Copy markerBits                                                     // @0x2baa20
        for (size_t i = 0; i < cached.markerBits_.size() && i < wf->signal.markerBits_.size(); ++i) {
            wf->signal.markerBits_[i] = cached.markerBits_[i];
        }
        return;
    }

    // Step 5: cache miss — parse CSV

    // Open CSV file
    boost::filesystem::ifstream ifs(fileRef.name, std::ios::in);
    if (!ifs.is_open()) {
        throw CsvException(
            ErrorMessages::format(
                static_cast<ErrorMessageT>(0x23),  // CsvInconsistentChannels
                fileRef.name));
    }

    std::string rawLine;
    std::vector<std::string> dataLines;
    size_t lineNum = 1;

    while (std::getline(ifs, rawLine)) {
        boost::algorithm::trim(rawLine);

        if (rawLine.empty()) {
            ++lineNum;
            continue;
        }

        if (rawLine[0] == '%') {
            // Comment line
            if (fileRef.formatType == 1) {
                // AWG format: skip all comment lines
                ++lineNum;
                continue;
            }

            // Check for "% Time (s)" pattern. If found, skip the
            // separator-scan below (binary: jne to skip_comment label).
            bool hasTimeColumn = false;
            if (rawLine.size() >= 10) {
                size_t pos = 0;
                while (pos + 10 <= rawLine.size()) {
                    auto found = rawLine.find('%', pos);
                    if (found == std::string::npos) break;
                    if (found + 10 <= rawLine.size() &&
                        std::memcmp(&rawLine[found], "% Time (s)", 10) == 0) {
                        hasTimeColumn = true;
                        break;
                    }
                    pos = found + 1;
                }
            }

            if (!hasTimeColumn) {
                // Scan for separator characters (bitmask 0x800100000000200)
                const char* p = rawLine.data();
                size_t len = rawLine.size();
                for (size_t i = 0; i < len; ++i) {
                    unsigned char ch = static_cast<unsigned char>(p[i]);
                    if (ch <= 59) {
                        uint64_t mask = 0x800100000000200ULL;
                        if ((mask >> ch) & 1) {
                            // Found separator in comment — auto-detect format
                            // Tokenize with ",;\t"
                            boost::char_separator<char> sep(",;\t", "",
                                boost::drop_empty_tokens);
                            boost::tokenizer<boost::char_separator<char>> tok(
                                rawLine, sep);
                            size_t colCount = 0;
                            for (auto it = tok.begin(); it != tok.end(); ++it)
                                ++colCount;
                            if (colCount > 2)
                                fileRef.formatType = 2;
                            else
                                fileRef.formatType = 1;
                            break;
                        }
                    }
                }
            }

            ++lineNum;
            continue;
        }

        // Data line: strip comment suffix (first '%')
        {
            auto pos = rawLine.find('%');
            if (pos != std::string::npos)
                rawLine.resize(pos);
        }

        // Append trimmed data line
        dataLines.push_back(rawLine);

        // Auto-detect format on first data line (formatType == 0)
        if (fileRef.formatType == 0 && dataLines.size() == 1) {
            // Count separators
            size_t numFields = 1;
            bool prevWasSep = false;
            for (size_t i = 0; i < rawLine.size(); ++i) {
                bool s = isCsvSeparator(rawLine[i]);
                if (s && !prevWasSep) ++numFields;
                prevWasSep = s;
            }

            // Check if "0x" prefix with no '.' or 'e' → AWG integer
            if (rawLine.size() >= 2 &&
                rawLine[0] == '0' && rawLine[1] == 'x') {
                bool hasDot = (std::memchr(rawLine.data(), '.', rawLine.size()) != nullptr);
                bool hasE   = (std::memchr(rawLine.data(), 'e', rawLine.size()) != nullptr);
                if (hasDot || hasE) {
                    fileRef.isIntegerFormat = 0;
                }
                // else: leave isIntegerFormat as-is (AWG integer)
            } else {
                fileRef.isIntegerFormat = 0;
            }
        }

        ++lineNum;
    }

    ifs.close();

    // Step 6: Dispatch on wf->file->formatType to parse data
    size_t numLines = dataLines.size();

    if (fileRef.formatType == 2) {
        // Multi-column float format
        // First pass: determine column count from first line
        uint16_t channels = wf->signal.channels_;
        size_t totalSamples = numLines * channels;

        std::vector<double> samples(totalSamples, 0.0);
        std::vector<uint8_t> markers(totalSamples, 0);
        std::vector<uint8_t> markerBits;
        if (channels > 0) {
            markerBits.resize(channels, 0);
        }

        for (size_t row = 0; row < numLines; ++row) {
            boost::char_separator<char> sep(",; \t", "",
                boost::drop_empty_tokens);
            boost::tokenizer<boost::char_separator<char>> tok(
                dataLines[row], sep);

            auto it = tok.begin();
            auto end = tok.end();

            // Verify column count matches channels
            std::vector<std::string> tokens(it, end);
            if (tokens.size() != static_cast<size_t>(channels)) {
                throw CsvException(
                    ErrorMessages::format(
                        static_cast<ErrorMessageT>(0x23),
                        fileRef.name));
            }

            for (size_t col = 0; col < tokens.size(); ++col) {
                std::string cell = tokens[col];
                boost::algorithm::trim(cell);
                double val = std::stod(cell, nullptr);
                size_t idx = row * channels + col;
                samples[idx] = val;
            }
        }

        // Assign to waveform
        wf->signal.samples_ = std::move(samples);
        wf->signal.markers_ = std::move(markers);
        wf->signal.markerBits_ = std::move(markerBits);
        wf->signal.reserveOnly_ = false;
        wf->signal.channels_ = channels;
        if (channels > 0)
            wf->signal.length_ = wf->signal.samples_.size() / channels;

    } else if (fileRef.formatType == 1) {
        // AWG integer format (or single-column)
        size_t numTokens = numLines;  // one token per line for AWG format

        // But we need to check: the binary actually iterates dataLines
        // and calls setSampleFromString for each line
        if (numTokens == 0) {
            // Empty CSV
            throw CsvException(
                ErrorMessages::format(
                    static_cast<ErrorMessageT>(0x23),
                    fileRef.name));
        }

        uint16_t channels = wf->signal.channels_;
        size_t totalSamples = numTokens * channels;

        // Allocate buffers
        wf->signal.samples_.assign(totalSamples, 0.0);
        wf->signal.markers_.assign(totalSamples, 0);
        if (channels > 0) {
            wf->signal.markerBits_.assign(channels, 0);
        }
        wf->signal.reserveOnly_ = false;
        wf->signal.channels_ = channels;
        if (channels > 0)
            wf->signal.length_ = totalSamples / channels;

        // Parse each line
        for (size_t i = 0; i < numTokens; ++i) {
            setSampleFromString<WaveformFront>(
                dataLines[i], wf, deviceType, i, 0);
        }

    } else {
        // Format 0: simple float, auto-detected
        // columnMode determines if single or multi value per line
        if (dataLines.empty()) {
            // Nothing to do (empty CSV allowed for format 0?)
            return;
        }

        // Tokenize each line with ",; \t" separator
        // First pass: count total tokens to determine total samples
        std::vector<std::vector<std::string>> allTokens;
        allTokens.reserve(numLines);

        for (size_t row = 0; row < numLines; ++row) {
            boost::char_separator<char> sep(",; \t", "",
                boost::drop_empty_tokens);
            boost::tokenizer<boost::char_separator<char>> tok(
                dataLines[row], sep);

            std::vector<std::string> lineTokens(tok.begin(), tok.end());
            allTokens.push_back(std::move(lineTokens));
        }

        // Determine channels from first line token count if not already set
        uint16_t channels = wf->signal.channels_;

        // For format 0, the binary checks wf->signal.length_ (at +0xD0).
        // If length_ == 0, use token count from first line as channels count.
        if (wf->signal.length_ == 0 && !allTokens.empty()) {
            channels = static_cast<uint16_t>(allTokens[0].size());
        }

        size_t totalSamples = numLines * channels;
        std::vector<double> samples(totalSamples, 0.0);
        std::vector<uint8_t> markers(totalSamples, 0);
        std::vector<uint8_t> markerBits;
        if (channels > 0) {
            markerBits.resize(channels, 0);
        }

        // Parse all tokens
        // The binary uses a flat iteration with bitmask separator scanning,
        // extracting substrings between separators, trimming, and stod().
        for (size_t row = 0; row < numLines; ++row) {
            auto& toks = allTokens[row];
            // Check column count consistency
            if (toks.size() != static_cast<size_t>(channels)) {
                throw CsvException(
                    ErrorMessages::format(
                        static_cast<ErrorMessageT>(0x23),
                        fileRef.name));
            }

            for (size_t col = 0; col < toks.size(); ++col) {
                std::string cell = toks[col];
                boost::algorithm::trim(cell);
                double val = std::stod(cell, nullptr);
                size_t idx = row * channels + col;
                samples[idx] = val;
            }
        }

        wf->signal.samples_ = std::move(samples);
        wf->signal.markers_ = std::move(markers);
        wf->signal.markerBits_ = std::move(markerBits);
        wf->signal.reserveOnly_ = false;
        wf->signal.channels_ = channels;
        if (channels > 0)
            wf->signal.length_ = wf->signal.samples_.size() / channels;
    }

    // Step 7: checkAllocation
    wf->signal.checkAllocation();
}

// ============================================================================
// csvFileToWaveform<WaveformIR>  @0x2be830
// Nearly identical to WaveformFront. The binary is a separate ~7KB function
// with the same structure but operating on WaveformIR.
// ============================================================================
template <>
void CsvParser::csvFileToWaveform<WaveformIR>(         // @0x2be830
        CachedParser& cache,
        std::shared_ptr<WaveformIR> wf,
        AwgDeviceType deviceType)
{
    if (!wf || !wf->file) {
        throw CsvException(errMsg[static_cast<ErrorMessageT>(0xe9)]);
    }

    auto& fileRef = *wf->file;

    // Cache lookup (same pattern as WaveformFront version)                    // @0x2be880
    if (fileRef.data.begin() == fileRef.data.end()) {
        fileRef.data = cache.getHash(fileRef.name);
    }
    CachedParser::CachedFile cached = cache.getCachedFile(fileRef.data);
    if (!cached.samples_.empty()) {
        wf->signal.samples_ = std::move(cached.samples_);
        wf->signal.markers_ = std::move(cached.markers_);
        wf->signal.channels_ = cached.channel_;
        if (!wf->signal.samples_.empty() && wf->signal.channels_ > 0) {
            wf->signal.length_ = wf->signal.samples_.size() / wf->signal.channels_;
        }
        for (size_t i = 0; i < cached.markerBits_.size() && i < wf->signal.markerBits_.size(); ++i) {
            wf->signal.markerBits_[i] = cached.markerBits_[i];
        }
        return;
    }

    // Cache miss — parse CSV
    // Open CSV file
    boost::filesystem::ifstream ifs(fileRef.name, std::ios::in);
    if (!ifs.is_open()) {
        throw CsvException(
            ErrorMessages::format(
                static_cast<ErrorMessageT>(0x23),
                fileRef.name));
    }

    std::string rawLine;
    std::vector<std::string> dataLines;
    size_t lineNum = 1;

    while (std::getline(ifs, rawLine)) {
        boost::algorithm::trim(rawLine);

        if (rawLine.empty()) {
            ++lineNum;
            continue;
        }

        if (rawLine[0] == '%') {
            if (fileRef.formatType == 1) {
                ++lineNum;
                continue;
            }

            // Check for "% Time (s)" pattern. If found, skip the
            // separator-scan below (binary: jne to skip_comment_ir label).
            bool hasTimeColumn = false;
            if (rawLine.size() >= 10) {
                size_t pos = 0;
                while (pos + 10 <= rawLine.size()) {
                    auto found = rawLine.find('%', pos);
                    if (found == std::string::npos) break;
                    if (found + 10 <= rawLine.size() &&
                        std::memcmp(&rawLine[found], "% Time (s)", 10) == 0) {
                        hasTimeColumn = true;
                        break;
                    }
                    pos = found + 1;
                }
            }

            if (!hasTimeColumn) {
                const char* p = rawLine.data();
                size_t len = rawLine.size();
                for (size_t i = 0; i < len; ++i) {
                    unsigned char ch = static_cast<unsigned char>(p[i]);
                    if (ch <= 59) {
                        uint64_t mask = 0x800100000000200ULL;
                        if ((mask >> ch) & 1) {
                            boost::char_separator<char> sep(",;\t", "",
                                boost::drop_empty_tokens);
                            boost::tokenizer<boost::char_separator<char>> tok(
                                rawLine, sep);
                            size_t colCount = 0;
                            for (auto it = tok.begin(); it != tok.end(); ++it)
                                ++colCount;
                            if (colCount > 2)
                                fileRef.formatType = 2;
                            else
                                fileRef.formatType = 1;
                            break;
                        }
                    }
                }
            }

            ++lineNum;
            continue;
        }

        {
            auto pos = rawLine.find('%');
            if (pos != std::string::npos)
                rawLine.resize(pos);
        }

        dataLines.push_back(rawLine);

        if (fileRef.formatType == 0 && dataLines.size() == 1) {
            size_t numFields = 1;
            bool prevWasSep = false;
            for (size_t i = 0; i < rawLine.size(); ++i) {
                bool s = isCsvSeparator(rawLine[i]);
                if (s && !prevWasSep) ++numFields;
                prevWasSep = s;
            }

            if (rawLine.size() >= 2 &&
                rawLine[0] == '0' && rawLine[1] == 'x') {
                bool hasDot = (std::memchr(rawLine.data(), '.', rawLine.size()) != nullptr);
                bool hasE   = (std::memchr(rawLine.data(), 'e', rawLine.size()) != nullptr);
                if (hasDot || hasE) {
                    fileRef.isIntegerFormat = 0;
                }
            } else {
                fileRef.isIntegerFormat = 0;
            }
        }

        ++lineNum;
    }

    ifs.close();

    size_t numLines = dataLines.size();

    if (fileRef.formatType == 2) {
        uint16_t channels = wf->signal.channels_;
        size_t totalSamples = numLines * channels;

        std::vector<double> samples(totalSamples, 0.0);
        std::vector<uint8_t> markers(totalSamples, 0);
        std::vector<uint8_t> markerBits;
        if (channels > 0) markerBits.resize(channels, 0);

        for (size_t row = 0; row < numLines; ++row) {
            boost::char_separator<char> sep(",; \t", "",
                boost::drop_empty_tokens);
            boost::tokenizer<boost::char_separator<char>> tok(
                dataLines[row], sep);
            std::vector<std::string> tokens(tok.begin(), tok.end());

            if (tokens.size() != static_cast<size_t>(channels)) {
                throw CsvException(
                    ErrorMessages::format(
                        static_cast<ErrorMessageT>(0x23),
                        fileRef.name));
            }

            for (size_t col = 0; col < tokens.size(); ++col) {
                std::string cell = tokens[col];
                boost::algorithm::trim(cell);
                samples[row * channels + col] = std::stod(cell, nullptr);
            }
        }

        wf->signal.samples_ = std::move(samples);
        wf->signal.markers_ = std::move(markers);
        wf->signal.markerBits_ = std::move(markerBits);
        wf->signal.reserveOnly_ = false;
        wf->signal.channels_ = channels;
        if (channels > 0)
            wf->signal.length_ = wf->signal.samples_.size() / channels;

    } else if (fileRef.formatType == 1) {
        size_t numTokens = numLines;
        if (numTokens == 0) {
            throw CsvException(
                ErrorMessages::format(
                    static_cast<ErrorMessageT>(0x23),
                    fileRef.name));
        }

        uint16_t channels = wf->signal.channels_;
        size_t totalSamples = numTokens * channels;

        wf->signal.samples_.assign(totalSamples, 0.0);
        wf->signal.markers_.assign(totalSamples, 0);
        if (channels > 0) wf->signal.markerBits_.assign(channels, 0);
        wf->signal.reserveOnly_ = false;
        wf->signal.channels_ = channels;
        if (channels > 0)
            wf->signal.length_ = totalSamples / channels;

        for (size_t i = 0; i < numTokens; ++i) {
            setSampleFromString<WaveformIR>(
                dataLines[i], wf, deviceType, i, 0);
        }

    } else {
        if (dataLines.empty()) return;

        std::vector<std::vector<std::string>> allTokens;
        allTokens.reserve(numLines);

        for (size_t row = 0; row < numLines; ++row) {
            boost::char_separator<char> sep(",; \t", "",
                boost::drop_empty_tokens);
            boost::tokenizer<boost::char_separator<char>> tok(
                dataLines[row], sep);
            allTokens.emplace_back(tok.begin(), tok.end());
        }

        uint16_t channels = wf->signal.channels_;
        if (wf->signal.length_ == 0 && !allTokens.empty()) {
            channels = static_cast<uint16_t>(allTokens[0].size());
        }

        size_t totalSamples = numLines * channels;
        std::vector<double> samples(totalSamples, 0.0);
        std::vector<uint8_t> markers(totalSamples, 0);
        std::vector<uint8_t> markerBits;
        if (channels > 0) markerBits.resize(channels, 0);

        for (size_t row = 0; row < numLines; ++row) {
            auto& toks = allTokens[row];
            if (toks.size() != static_cast<size_t>(channels)) {
                throw CsvException(
                    ErrorMessages::format(
                        static_cast<ErrorMessageT>(0x23),
                        fileRef.name));
            }
            for (size_t col = 0; col < toks.size(); ++col) {
                std::string cell = toks[col];
                boost::algorithm::trim(cell);
                samples[row * channels + col] = std::stod(cell, nullptr);
            }
        }

        wf->signal.samples_ = std::move(samples);
        wf->signal.markers_ = std::move(markers);
        wf->signal.markerBits_ = std::move(markerBits);
        wf->signal.reserveOnly_ = false;
        wf->signal.channels_ = channels;
        if (channels > 0)
            wf->signal.length_ = wf->signal.samples_.size() / channels;
    }

    wf->signal.checkAllocation();
}

}  // namespace zhinst
