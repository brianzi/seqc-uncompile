#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

#include "asm_parser_context.hpp"

namespace zhinst {

struct DeviceConstants;
struct AssemblerInstr;
class AsmExpression;
class AsmParserContext;

// LabelBimap type alias — bimap<string, multiset_of<int>>
using LabelBimap = boost::bimaps::bimap<
    std::string,
    boost::bimaps::multiset_of<int>>;

// sizeof(AWGAssemblerImpl) = 0x170
//
// Layout (from ctor at 0x285180 and dtor at 0x2853c0):
//
// Offset  Size  Type                              Field
// ------  ----  ----                              -----
// 0x00    0x08  DeviceConstants const*            deviceConstants_
// 0x08    0x18  std::string                       filename_
// 0x20    0x18  std::string                       asmSource_
// 0x38    0x18  std::string                       str2_
// 0x50    0x18  std::vector<uint64_t>             opcodes_
// 0x68    0x04  uint32_t                          memoryOffset_
// 0x6c    0x04  (padding)
// 0x70    0x08  (ptr, part of next field)
// 0x78    0x18  std::vector<std::string>          sourceLines_
// 0x90    0x18  std::vector<Message>              messages_
// 0xa8-0xe8     LabelBimap                        labelBimap_
// 0xf0    0x04  uint32_t                          flags_
// 0xf4    0x01  bool                              initialized_
// 0xf5-0x170    remaining fields (parser context, hash table, etc.)

class AWGAssemblerImpl {
public:
    // Message struct — stored in messages_ vector
    struct Message {
        int level = 0;         // 0=info, 1=warning, 2=error
        int lineNumber = 0;    // source line number
        std::string text;
    };

    AWGAssemblerImpl(DeviceConstants const& dc);
    ~AWGAssemblerImpl();

    void assembleFile(std::string const& path);
    void assembleString(std::string const& src);
    void assembleAsmList(std::vector<AssemblerInstr> const& asmList);
    std::vector<std::shared_ptr<AsmExpression>> assembleStringToExpressionsVec(
        std::string const& src);
    void setMemoryOffset(unsigned int offset);
    void writeToFile(std::string const& path);
    std::vector<uint64_t> const& getOpcode() const;
    std::string getReport() const;
    void printOpcode(int format) const;

    // --- Opcode encoding methods (awg_assembler_opcodes.cpp) ---
    unsigned int getReg(std::shared_ptr<AsmExpression> const& expr);     // 0x2892b0
    unsigned int getVal(std::shared_ptr<AsmExpression> const& expr,
                        int bits);                                       // 0x289370
    uint64_t opcode0(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x2895c0
    uint64_t opcode1(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x289860
    uint64_t opcode2(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x289a10
    uint64_t opcode3(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x289c90
    uint64_t opcode4(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x28a010
    uint64_t opcode5(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x28a610

    // --- Pipeline methods (awg_assembler_impl_pipeline.cpp) ---
    void parseLine(std::string const& line);
    void parseString(std::string const& src,
                     std::vector<std::shared_ptr<AsmExpression>>& exprs);
    void encodeExpressions(std::vector<std::shared_ptr<AsmExpression>> const& exprs);
    std::shared_ptr<AsmExpression> getAST(std::string const& src);
    void assembleExpressions(std::vector<std::shared_ptr<AsmExpression>> const& exprs,
                             std::vector<uint64_t> const& offsets);
    int evaluate(std::shared_ptr<AsmExpression> const& expr);
    std::string extractComment(std::string const& line);

    // --- Error reporting helper ---
    // Stores error/warning message; used throughout opcode/pipeline methods.
    void errorMessage(std::string const& msg);
    void parserMessage(int level, std::string const& msg);

    // --- Label access ---
    LabelBimap const& getLabelBimap() const { return labelBimap_; }

    DeviceConstants const* deviceConstants_;   // 0x00
    std::string filename_;                     // 0x08
    std::string asmSource_;                    // 0x20
    std::string str2_;                         // 0x38
    std::vector<uint64_t> opcodes_;            // 0x50
    uint32_t memoryOffset_ = 0;                // 0x68
    uint32_t pad0_ = 0;                        // 0x6c

    // 0x70: TODO — exact type at 0x70 unclear (ptr field)
    void* field_70_ = nullptr;                 // 0x70

    std::vector<std::string> sourceLines_;     // 0x78
    std::vector<Message> messages_;              // 0x90

    LabelBimap labelBimap_;                    // 0xa8 (approx 0x48 bytes)

    // Remaining fields stored as opaque storage
    // TODO: reconstruct individual fields as needed
    char remaining_fields_[0x80]{};            // fills to 0x170

    int lineNumber_ = 0;                       // used in pipeline
    AsmParserContext parserCtx_;               // by-value parser context (offset ~0xF0)

    // Raw pointer fields for opcode/source buffer management (used in pipeline)
    uint64_t* opcodes_begin_ = nullptr;        // offset TBD
    uint64_t* opcodes_end_ = nullptr;          // offset TBD
    std::string* sourceLines_begin_ = nullptr; // offset TBD
    std::string* sourceLines_end_ = nullptr;   // offset TBD
};

} // namespace zhinst
