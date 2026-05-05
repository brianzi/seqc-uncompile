#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

#include "zhinst/asm/asm_parser_context.hpp"

namespace zhinst {

struct DeviceConstants;
class Assembler;
class AsmExpression;

// LabelBimap type alias — bimap<string, multiset_of<int>>
using LabelBimap = boost::bimaps::bimap<
    std::string,
    boost::bimaps::multiset_of<int>>;

// ============================================================================
// AWGAssemblerImpl
//
// sizeof(AWGAssemblerImpl) = 0x170 (verified at 0x285050: mov edi,0x170)
//
// Layout (verified against ctor at 0x285180 and dtor at 0x2853c0):
//
//   Offset  Size  Type                                    Field
//   ------  ----  ----                                    -----
//   0x000   0x08  DeviceConstants const*                  deviceConstants_
//   0x008   0x18  std::string                             filename_
//   0x020   0x18  std::string                             asmSource_
//   0x038   0x18  std::string                             unusedStr038_ (no observed reader/writer in reconstructed methods)
//   0x050   0x18  std::vector<uint32_t>                   opcodes_
//                                                         (verified by getOpcode()
//                                                          at 0x289060: lea rax,[rdi+0x50];
//                                                          and by writeToFile() at
//                                                          0x2885da passing &opcodes_
//                                                          directly to ElfWriter::addCode
//                                                          which takes vector<uint32_t>)
//   0x068   0x04  uint32_t                                memoryOffset_
//                                                         (verified by setMemoryOffset()
//                                                          at 0x288560: mov [rdi+0x68],esi)
//   0x06c   0x04  (padding)
//   0x070   0x04  int32_t                                 currentLine_
//                                                         (verified by errorMessage at
//                                                          0x289080 reading [rdi+0x70] as
//                                                          the int prefix of an emitted
//                                                          Message; reset to 0 at
//                                                          assembleString+0x16c)
//   0x074   0x04  (padding)
//   0x078   0x18  std::vector<std::string>                sourceLines_
//   0x090   0x18  std::vector<Message>                    messages_
//                                                         (Message stride = 0x20)
//   0x0a8   0x48  LabelBimap                              labelBimap_
//                                                         (sentinel at 0xc8, header
//                                                          ptr at 0xc0 → 0x50-byte heap
//                                                          allocation; tree pointers
//                                                          at 0xb0/0xd8/0xe0)
//   0x0f0   0x80  AsmParserContext                        parserCtx_
//                                                         (verified: errorMessage adds
//                                                          0xf0 to this then calls
//                                                          AsmParserContext::setSyntaxError
//                                                          which writes byte at +0x03 of
//                                                          parser ctx → AWG offset 0xf3)
//
// Total: 0x170.
// ============================================================================

class AWGAssemblerImpl {
public:
    // ---- Message struct — stored in messages_ vector ----
    //
    // Layout (verified from errorMessage at 0x289070 and parserMessage at
    // 0x289190; emplace stride = 0x20):
    //
    //   +0x00  int        code           — line number (when written by
    //                                       errorMessage from currentLine_)
    //                                       OR severity level (when written
    //                                       by parserMessage from `level`
    //                                       arg). The same field; the binary
    //                                       does not distinguish them. There
    //                                       is NO separate `lineNumber` slot.
    //   +0x04  (4 bytes padding)
    //   +0x08  std::string text
    //
    // Total: 0x20.
    struct Message {
        int code = 0;          // +0x00 — see note above
        std::string text;      // +0x08
    };

    // ---- Lifecycle ----
    AWGAssemblerImpl(DeviceConstants const& dc);            // 0x285180
    ~AWGAssemblerImpl();                                    // 0x2853c0

    // ---- Public API ----
    void assembleFile(std::string const& path);             // 0x285ec0
    void assembleString(std::string const& src);            // 0x286490
    void assembleAsmList(std::vector<Assembler> const& asmList);
    std::vector<std::shared_ptr<AsmExpression>>
        assembleStringToExpressionsVec(std::string const& src);   // 0x286e40
    void setMemoryOffset(unsigned int offset);              // 0x288560
    void writeToFile(std::string const& path);              // 0x288570
    std::vector<uint32_t> const& getOpcode() const;         // 0x289060
    std::string getReport() const;                          // 0x285bc0
    void printOpcode(int startIndex) const;                     // 0x288b50

    // ---- Opcode encoding (awg_assembler_opcodes.cpp) ----
    unsigned int getReg(std::shared_ptr<AsmExpression> const& expr);     // 0x2892b0
    unsigned int getVal(std::shared_ptr<AsmExpression> const& expr,
                        int bits);                                       // 0x289370
    uint64_t opcode0(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x2895c0
    uint64_t opcode1(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x289860
    uint64_t opcode2(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x289a10
    uint64_t opcode3(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x289c90
    uint64_t opcode4(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x28a010
    uint64_t opcode5(unsigned int base, std::shared_ptr<AsmExpression> const& expr); // 0x28a610

    // ---- Pipeline (awg_assembler_impl_pipeline.cpp) ----
    // Phase S.2 M5: removed three header-only declarations
    // (`parseLine`, `parseString`, `encodeExpressions`) that were
    // never defined in the recon and never referenced by any caller.
    // `nm` on `_seqc_compiler.so` confirms the binary has no
    // matching `AWGAssemblerImpl::parseLine/parseString/encodeExpressions`
    // symbols either, so the prior decls were spurious.
    std::shared_ptr<AsmExpression> getAST(std::string const& src);
    void assembleExpressions(std::vector<std::shared_ptr<AsmExpression>> const& exprs,
                             std::vector<uint64_t> const& offsets);          // 0x285620
    int evaluate(std::shared_ptr<AsmExpression> const& expr);                // 0x285b20
    std::string extractComment(std::string const& line);

    // ---- Error reporting helpers ----
    // Both append to messages_ at offset 0x90; the leading int of the
    // emitted Message is currentLine_ (errorMessage) or `level`
    // (parserMessage). errorMessage additionally calls
    // parserCtx_.setSyntaxError() afterwards.
    void errorMessage(std::string const& msg);                               // 0x289070
    void parserMessage(int level, std::string const& msg);                   // 0x289190

    // ---- Label access ----
    LabelBimap const& getLabelBimap() const { return labelBimap_; }

    // ---- Fields (verified offsets in comment block above) ----
    DeviceConstants const* deviceConstants_;     // 0x000
    std::string filename_;                       // 0x008
    std::string asmSource_;                      // 0x020
    std::string unusedStr038_;                    // 0x038 — no observed reader/writer
    std::vector<uint32_t> opcodes_;              // 0x050  // verified at 0x2885da: addCode(&[r14+0x50]) takes vector<unsigned int> directly
    uint32_t memoryOffset_ = 0;                  // 0x068
    uint32_t pad_memOffset_ = 0;                 // 0x06c (alignment)
    int32_t currentLine_ = 0;                    // 0x070
    uint32_t pad_currentLine_ = 0;               // 0x074 (alignment)
    std::vector<std::string> sourceLines_;       // 0x078
    std::vector<Message> messages_;              // 0x090
    LabelBimap labelBimap_;                      // 0x0a8 (0x48 bytes)
    AsmParserContext parserCtx_;                 // 0x0f0 (0x80 bytes)
};

// NOTE: Verifying sizeof(Message) == 0x20 requires the binary's libc++ ABI
// (std::__1::basic_string == 24 bytes). Build hosts using libstdc++ have
// std::string == 32 bytes, so a host-side static_assert(sizeof(Message) == 0x20)
// would falsely fail. Layout has been verified directly from the disassembly:
// Message stride is 0x20 (errorMessage emplace at 0x289070: add r14, 0x20).

// NOTE: A static_assert(sizeof(AWGAssemblerImpl) == 0x170) is desirable
// but depends on libc++/Boost ABI sizes for vector, string, bimap, and
// AsmParserContext matching the reconstructed offsets exactly. Add it
// once those component sizes are verified end-to-end.

} // namespace zhinst
