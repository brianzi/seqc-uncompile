#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward declarations for boost bimap used at offset 0xb0-0xc8
// The bimap is: bimap<string, multiset_of<int>>

namespace zhinst {

struct DeviceConstants;
class Assembler;

// sizeof(AWGAssemblerImpl) = 0x170
//
// Layout (from ctor at 0x285180 and dtor at 0x2853c0):
//
// Offset  Size  Type                              Field
// ------  ----  ----                              -----
// 0x00    0x08  DeviceConstants const*            deviceConstants_
// 0x08    0x18  std::string                       str0_ (sourceFile? inputPath?)
// 0x20    0x18  std::string                       str1_ (sourceString?)
// 0x38    0x18  std::string                       str2_ (another string)
// 0x50    0x18  std::vector<uint64_t>             opcodes_ (getOpcode returns &this+0x50)
// 0x68    0x04  uint32_t                          memoryOffset_
// 0x6c    0x04  (padding)
// 0x70    0x08  (ptr, part of next field)         \  
// 0x78    0x18  std::vector<std::string>          strings_ (messages/source lines)
// 0x90    0x18  std::vector<Message>              messages_ (AWGAssemblerImpl::Message)
// 0xa8    0x08  (zero-initialized)                count/size for bimap?
// 0xb0    0x08  ptr (self-ref to 0xc8)            bimap header ptr (left index sentinel)
// 0xb8    0x08  (bimap internals)
// 0xc0    0x08  ptr (heap alloc 0x50)             bimap core (allocated node storage)
// 0xc8    0x18  (bimap sentinel node area)        bimap sentinel/header
// 0xd0    0x08  size_t                            bimap node count
// 0xd8    0x08  ptr (init to &0xc8)              bimap right index header.left
// 0xe0    0x08  ptr (init to &0xc8)              bimap right index header.right
// 0xe8    0x08  (padding/reserved)
// 0xf0    0x04  uint32_t                          flags/state (init 0)
// 0xf4    0x01  bool                              initialized_ (init true)
// 0xf5    0x03  (padding)
// 0xf8    0x28  std::string                       str3_ (or could be other obj; 0x28 to 0x120)
//               NOTE: 0xf8-0x120 is 0x28 bytes. In dtor, 0x120 is checked
//               against &(this+0x100) suggesting embedded buffer → libc++ string
//               with inline buffer at 0x100. Actually this looks like an
//               std::stringstream or similar object with vtable at 0x120.
// 0x120   0x08  ptr (streambuf*/vtable obj)       streamObj_ (dtor checks if == &this+0x100)
// 0x128   0x08  (padding/reserved)
// 0x130   0x18  std::vector<?>                    vec3_ (some vector)
// 0x148   0x08  ptr                               hashTable_/ptr (freed with size*8)
// 0x150   0x08  size_t                            hashTableCapacity_
// 0x158   0x08  ptr                               linkedList_ (linked list head, nodes 0x28 each)
// 0x160   0x08  size_t                            count4_
// 0x168   0x04  float                             sampleRate_ (init 1.0f = 0x3f800000)
// 0x16c   0x04  (padding to 0x170)

class AWGAssemblerImpl {
public:
    AWGAssemblerImpl(DeviceConstants const& dc);
    ~AWGAssemblerImpl();

    void assembleFile(std::string const& path);
    void assembleString(std::string const& src);
    void assembleAsmList(std::vector<Assembler> const& asmList);
    // vector<...> assembleStringToExpressionsVec(std::string const& src);
    void setMemoryOffset(unsigned int offset);
    void writeToFile(std::string const& path);
    std::vector<uint64_t> const& getOpcode() const;
    // std::string getReport() const;
    void printOpcode(int format) const;

private:
    DeviceConstants const* deviceConstants_;   // 0x00
    std::string str0_;                         // 0x08
    std::string str1_;                         // 0x20
    std::string str2_;                         // 0x38
    std::vector<uint64_t> opcodes_;            // 0x50
    uint32_t memoryOffset_;                    // 0x68
    uint32_t pad0_;                            // 0x6c
    // 0x70-0xa8: vectors of strings and messages
    char fields_70_[0x38];                     // 0x70 (vec<string> + vec<Message>)
    // 0xa8-0xc8: bimap header area
    char bimap_[0x60];                         // 0xa8-0x108 (boost bimap internals)
    // Remaining fields...
    char fields_108_[0x68];                    // 0x108-0x170
};

} // namespace zhinst
