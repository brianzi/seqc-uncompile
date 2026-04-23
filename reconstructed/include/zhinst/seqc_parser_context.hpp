// ============================================================================
// SeqcParserContext — minimal stub for Expression parser actions
//
// Full reconstruction deferred (Phase 11+ or later).
// Only the methods called by expression.cpp are declared here.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>

namespace zhinst {

class SeqcParserContext {
public:
    int32_t currentLineNumber() const;   // 0x247c80
    void raiseError(const std::string& msg);  // 0x247ae0
    void setSyntaxError();               // 0x247cb0
};

} // namespace zhinst
