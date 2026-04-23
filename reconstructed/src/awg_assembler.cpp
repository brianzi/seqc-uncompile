#include "zhinst/awg_assembler.hpp"
#include "zhinst/awg_assembler_impl.hpp"

namespace zhinst {

// 0x285040
AWGAssembler::AWGAssembler(DeviceConstants const& dc)
    : pImpl_(new AWGAssemblerImpl(dc))  // operator new(0x170)
{
}

// 0x285090
AWGAssembler::~AWGAssembler()
{
    if (pImpl_) {
        pImpl_->~AWGAssemblerImpl();
        operator delete(pImpl_, std::size_t(0x170));
        pImpl_ = nullptr;
    }
}

// 0x2850d0
void AWGAssembler::assembleFile(std::string const& path)
{
    pImpl_->assembleFile(path);
}

// 0x2850e0
void AWGAssembler::assembleString(std::string const& src)
{
    pImpl_->assembleString(src);
}

// 0x2850f0
void AWGAssembler::assembleAsmList(std::vector<AssemblerInstr> const& asmList)
{
    pImpl_->assembleAsmList(asmList);
}

// 0x285100 — sret: rdi=retval, rsi=this, rdx=string
// Wrapper forwards to pImpl_->assembleStringToExpressionsVec(src).
// Returns vector<shared_ptr<AsmExpression>> by value (sret via rdi).
std::vector<std::shared_ptr<AsmExpression>>
AWGAssembler::assembleStringToExpressionsVec(std::string const& src)
{
    return pImpl_->assembleStringToExpressionsVec(src);
}

// 0x285120
void AWGAssembler::setMemoryOffset(unsigned int offset)
{
    pImpl_->setMemoryOffset(offset);
}

// 0x285130
void AWGAssembler::writeToFile(std::string const& path)
{
    pImpl_->writeToFile(path);
}

// 0x285140
std::vector<uint32_t> const& AWGAssembler::getOpcode() const
{
    return pImpl_->getOpcode();
}

// 0x285150 — sret: rdi=retval, rsi=this
// Returns string by value
// std::string AWGAssembler::getReport() const {
//     return pImpl_->getReport();
// }

// 0x285170
void AWGAssembler::printOpcode(int format) const
{
    pImpl_->printOpcode(format);
}

} // namespace zhinst
