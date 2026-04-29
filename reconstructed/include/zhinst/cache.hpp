// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Cache class — waveform cache memory management
//
// Confirmed from:
//   - Cache::Cache()               0x282920
//   - Cache::getSize()             0x282940
//   - Cache::getPageSize()         0x282950
//   - Cache::getFreeMemory()       0x282960
//   - Cache::getScope()            0x2829a0
//   - Cache::allocate(5-arg)       0x282a30
//   - Cache::allocate(CacheType)   0x282be0
//   - Cache::getBestPosition()     0x282cf0
//   - Cache::memoryWrite()         0x283020
//   - Cache::stillInMemory()       0x2832e0
//   - Cache::reuse()               0x2833d0
//   - Cache::play()                0x2834c0
//   - Cache::resetPlay()           0x283640
//   - Cache::free()                0x283690
//   - Cache::print()               0x283b50
//   - Cache::Pointer::str()        0x283c30
//   - CacheException               0x2835a0
//   - CacheException::what()       0x284200
// ============================================================================
#pragma once

#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "address_impl.hpp"

namespace zhinst {

struct WaveformIR;
struct DeviceConstants;

// ============================================================================
// CacheException — inherits std::exception, has string message_
//
// Layout (0x20 bytes):
//   +0x00: vtable*
//   +0x08: std::string message_ (24 bytes, libc++ SSO)
// ============================================================================
class CacheException : public std::exception {
public:
    explicit CacheException(std::string const& msg);  // 0x2835a0
    ~CacheException() override;                       // 0x283600
    const char* what() const noexcept override;       // 0x284200

private:
    std::string message_;  // +0x08
};

// ============================================================================
// Cache class — 0x28 bytes (40 bytes)
//
// Layout:
//   +0x00: uint32_t           size_        (AddressImpl<uint>, total cache size)
//   +0x04: int32_t            pageSize_    (page/alignment size)
//   +0x08: bool               isHirzel_    (if true, always append at end)
//   +0x09: 7 bytes padding
//   +0x10: vector<shared_ptr<Pointer>>  pointers_  (24 bytes: begin, end, cap)
//   +0x28: END
// ============================================================================
class Cache {
public:
    // PointerState enum (int32_t)
    //   0 = Ready      ("ready")
    //   1 = LastPlayed ("last played")
    //   2 = Playing    ("playing")
    //   3 = Free       ("free")
    enum class PointerState : int {
        Ready      = 0,
        LastPlayed = 1,
        Playing    = 2,
        Free       = 3,
    };

    // CacheType enum (int32_t, passed to allocate)
    //   0 = Normal     (no alignment doubling)
    //   1 = Aligned    (page size doubled for alignment)
    enum class CacheType : int {
        Normal  = 0,
        Aligned = 1,
    };

    // ========================================================================
    // Cache::Pointer — 0x24 bytes (36 bytes)
    //
    // Layout:
    //   +0x00: uint32_t          position_    (AddressImpl<uint>, start addr)
    //   +0x04: uint32_t          size_        (num samples allocated)
    //   +0x08: uint32_t          hash_        (computed: ~(position ^ (position + size/2)))
    //   +0x0C: uint32_t          numRepeats_  (computed: numSamples/pageSize + 1 style)
    //   +0x10: shared_ptr<WaveformIR> waveform_  (16 bytes; ptr at +0x10, ctrl at +0x18)
    //   +0x20: PointerState      state_       (int32_t)
    //   +0x24: END
    // ========================================================================
    struct Pointer {
        uint32_t position_;                     // +0x00
        uint32_t size_;                         // +0x04
        uint32_t hash_;                         // +0x08
        uint32_t numRepeats_;                   // +0x0C
        std::shared_ptr<WaveformIR> waveform_;  // +0x10
        PointerState state_;                    // +0x20

        // No additional fields: Pointer is exactly 0x24 bytes.
        // Verified — no Cache::Pointer::pageCount symbol exists in binary,
        // and no consumer in reconstructed src ever accesses such a field.
        // (Removed hallucinated `int pageCount` member.)

        std::string str() const;                // 0x283c30
    };
    // Note: sizeof(Pointer) == 0x24 in libc++ ABI, 0x28 in libstdc++ (extra tail padding).

    // Constructor
    Cache(detail::AddressImpl<uint32_t> size, int pageSize, bool isHirzel);  // 0x282920

    // Accessors
    detail::AddressImpl<uint32_t> getSize() const;       // 0x282940
    int getPageSize() const;                             // 0x282950
    uint32_t getFreeMemory() const;                      // 0x282960
    std::shared_ptr<Cache> getScope() const;             // 0x2829a0

    // Allocation
    std::shared_ptr<Pointer> allocate(
        std::shared_ptr<WaveformIR> waveform,
        detail::AddressImpl<uint32_t> numSamples,
        std::unordered_map<std::string, bool> const& nameMap,
        int pageSize,
        bool split);                                     // 0x282a30

    std::shared_ptr<Pointer> allocate(
        std::shared_ptr<WaveformIR> waveform,
        detail::AddressImpl<uint32_t> numSamples,
        std::unordered_map<std::string, bool> const& nameMap,
        CacheType cacheType);                            // 0x282be0

    // Internal
    std::shared_ptr<Pointer> getBestPosition(
        detail::AddressImpl<uint32_t> numSamples,
        std::unordered_map<std::string, bool> const& nameMap,
        bool gapScan);                                // 0x282cf0

    void memoryWrite(std::shared_ptr<Pointer> ptr);      // 0x283020
    bool stillInMemory(std::shared_ptr<Pointer> ptr) const;  // 0x2832e0
    void reuse(std::shared_ptr<Pointer> ptr);            // 0x2833d0
    void play(std::shared_ptr<Pointer> ptr, PointerState state);  // 0x2834c0
    void resetPlay();                                    // 0x283640
    void free(std::shared_ptr<Pointer> ptr);             // 0x283690
    void print() const;                                  // 0x283b50

private:
    uint32_t size_;                                      // +0x00
    int32_t pageSize_;                                   // +0x04
    bool isHirzel_;                                    // +0x08
    std::vector<std::shared_ptr<Pointer>> pointers_;     // +0x10
};

// Constants
// unusedCacheLine at 0x95deac = 0xFFFFFFFF (sentinel value for unallocated)
static constexpr uint32_t unusedCacheLine = 0xFFFFFFFF;

// kCacheFormat at 0x95e8f8 = "3" (SSO string encoding: 0x02, '3', 0x00)
// (This appears to be a static std::string, likely defined elsewhere)

} // namespace zhinst
