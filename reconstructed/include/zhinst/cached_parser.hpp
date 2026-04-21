// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CachedParser — caches parsed waveform data
//
// Size: 0x60 bytes (96 bytes)
// Used inline in WavetableIR and WavetableFront.
// TODO: internal layout not yet fully reconstructed.
// ============================================================================
#pragma once

#include <cstdint>
#include <boost/filesystem/path.hpp>

namespace zhinst {

class CachedParser {
public:
    CachedParser();
    CachedParser(size_t cacheSize, const boost::filesystem::path& cachePath);
    ~CachedParser();

    // TODO: methods not yet reconstructed

private:
    char data_[0x60];  // opaque storage — exact fields TBD
};
static_assert(sizeof(CachedParser) == 0x60, "CachedParser must be 0x60 bytes");

} // namespace zhinst
