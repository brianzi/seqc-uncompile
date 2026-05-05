#include "zhinst/codegen/awg_assembler_impl.hpp"

namespace zhinst {

// 0x285180
AWGAssemblerImpl::AWGAssemblerImpl(DeviceConstants const& dc)
    : deviceConstants_(&dc),
      filename_(),                  // 0x08-0x1f zeroed (16 bytes xmm0 store — SSO buffer)
      asmSource_(),
      unusedStr038_(),
      opcodes_(),                  // 0x50-0x67: begin/end/cap zeroed
      memoryOffset_(0),
      pad_memOffset_(0)
      // currentLine_ = 0 by in-class initializer (0x70)
      // pad_currentLine_ = 0 by in-class initializer (0x74)
{
    // 0x70-0x9f zeroed (movaps xmm0 x3 = 48 bytes)
    // 0xa0 = 0 (movq)
    // These are: vec<string> at 0x78, vec<Message> at 0x90, and a size field at 0xa0

    // 0xb0 = lea 0xc8(%this) → self-pointer (bimap sentinel)
    // Allocate bimap core: operator new(0x50) → stored at 0xc0
    // Initialize bimap node at allocated memory:
    //   +0x20: nullptr (left child of ordered index 0)
    //   +0x28: ptr to self+0x20 (parent of ordered index 0)
    //   +0x30: ptr to self+0x20
    //   +0x38: nullptr (left child of ordered index 1)
    //   +0x40: ptr to self+0x38 (parent of ordered index 1)
    //   +0x48: ptr to self+0x38

    // 0xd0 = 0 (node count)
    // 0xd8 = &this->0xc8 (right index header left)
    // 0xe0 = &this->0xc8 (right index header right)

    // 0xf0 = 0 (uint32)
    // 0xf4 = 1 (bool, true)
    // 0xf8 = 0 (qword)

    // 0x120 = 0 (ptr/qword)
    // 0x130-0x15f zeroed (48 bytes = 3 xmm stores) — vector + hash table fields
    // 0x160 = 0
    // 0x168 = 0x3f800000 (float 1.0f) — sampleRate
}

// 0x2853c0
AWGAssemblerImpl::~AWGAssemblerImpl()
{
    // Destroys in reverse order:
    // 1. Linked list at 0x158 (nodes of size 0x28 with string at +0x10)
    // 2. Hash table at 0x148 (freed with capacity * 8)
    // 3. Vector at 0x130
    // 4. Object at 0x120 (virtual dtor if != &this+0x100; else offset-based dtor)
    // 5. Bimap at 0xb0/0xc0 (delete_all_nodes then free core 0x50)
    // 6. Vector<Message> at 0x90 (elements have string at -0x18 offset pattern)
    // 7. Vector<string> at 0x78
    // 8. Vector<uint64_t> at 0x50
    // 9. Strings at 0x38, 0x20, 0x08
}

// 0x288560
void AWGAssemblerImpl::setMemoryOffset(unsigned int offset)
{
    memoryOffset_ = offset;
}

// 0x289060
std::vector<uint32_t> const& AWGAssemblerImpl::getOpcode() const
{
    return opcodes_;
}

} // namespace zhinst
