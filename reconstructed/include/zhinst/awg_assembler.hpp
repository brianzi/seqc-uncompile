#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zhinst {

class AWGAssemblerImpl;
struct AssemblerInstr;
struct DeviceConstants;

// sizeof(AWGAssembler) = 0x8
class AWGAssembler {
public:
    AWGAssembler(DeviceConstants const& dc);
    ~AWGAssembler();

    void assembleFile(std::string const& path);
    void assembleString(std::string const& src);
    void assembleAsmList(std::vector<AssemblerInstr> const& asmList);
    // Returns by value (sret); vector of parsed expression objects.
    std::vector<std::shared_ptr<struct AsmExpression>> assembleStringToExpressionsVec(std::string const& src);
    void setMemoryOffset(unsigned int offset);
    void writeToFile(std::string const& path);
    // Returns pointer to internal vector<uint64_t> at impl+0x50
    std::vector<uint32_t> const& getOpcode() const;
    // Returns by value (sret); likely std::string
    std::string getReport() const;
    void printOpcode(int format) const;

private:
    // unique_ptr semantics (ctor news 0x170, dtor deletes with size 0x170)
    // Implemented as raw owning pointer (no unique_ptr control block observed)
    AWGAssemblerImpl* pImpl_;  // offset 0x00
};

} // namespace zhinst
