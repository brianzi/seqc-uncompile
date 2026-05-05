// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CachedParser — on-disk LRU cache of parsed waveform data
//
// Total size: 0x60 bytes (96 bytes), verified by:
//   - ctor at 0x2afa70: writes fields up through +0x58 (end of indexFilePath_)
//   - dtor at 0x29aac0: destroys fields at +0x48, +0x30, +0x08
//   - WavetableFront layout: cachedParser_ at +0x118, next member at +0x178
//
// The first 0x30 bytes are an embedded libc++ std::map header (the cache):
//     std::map<std::vector<unsigned int>, CacheEntry>
// confirmed by:
//   - dtor tail-calls __tree<__value_type<vector<uint>, CacheEntry>, ...>::destroy
//   - boost serialization symbols (loadCacheIndex @ 0x2afec0) reference exactly
//     this map type via boost::archive::detail::iserializer<text_iarchive,
//     std::map<vector<uint>, CachedParser::CacheEntry>>.
//
// Layout (verified offsets from ctor 0x2afa70 / dtor 0x29aac0):
//   +0x00  8   __begin_node_   = &__pair1_  (initialized to rdi+0x8 at 2afa92)
//   +0x08  16  __pair1_        (parent + size; xmm0-zeroed at 2afa8e)
//   +0x18  1   enabled_        bool, true iff cacheSize != 0 (setne 2afa98)
//   +0x19  7   <padding>
//   +0x20  8   cacheSize_      cache capacity in bytes (rsi stored at 2afa9c)
//   +0x28  8   currentSize_    current total cached size in bytes (zeroed at 2afaa0)
//   +0x30  24  cachePath_      boost::filesystem::path (= std::string),
//                              copy-constructed from ctor arg (r14=rdi+0x30)
//   +0x48  24  indexFilePath_  boost::filesystem::path = cachePath_/"index"
//                              built by append_v3 at 2afbba then stored to
//                              [rbx+0x48..0x58] at 2afbf4
//   = 0x60 total
//
// Methods identified in binary:
//   CachedParser(size_t cacheSize, path const&)   @ 0x2afa70
//   ~CachedParser()                                @ 0x29aac0
//   loadCacheIndex()                               @ 0x2afec0  (private)
//   cleanCache()                                   @ 0x2b0140  (private)
//   removeOldFiles()                               @ 0x2b01a0  (private)
//
// NOTE: no default ctor exists in the binary — the previously-declared
// CachedParser() was fabricated and is removed.
//
// CacheEntry (0x60 = 96 bytes), verified by:
//   - ctor @0x2b10b0: CacheEntry(string const&, string const&, size_t, vector<uint>, bool)
//   - dtor @0x2b1320: destroys vector<uint> at +0x40, string at +0x18, string at +0x00
//   - operator= @0x2b1210: copies all fields including +0x58 bool
//   - relocate @0x2b2b90: stride = 0x60 confirmed
//   Layout:
//     +0x00  string (24B libc++)  name_         first ctor arg
//     +0x18  string (24B libc++)  filePath_     second ctor arg
//     +0x30  size_t               byteSize_     third ctor arg
//     +0x38  time_t               timestamp_    time(nullptr) in ctor
//     +0x40  vector<uint> (24B)   hash_         fourth ctor arg (move)
//     +0x58  bool                 pinned_        fifth ctor arg
//     = 0x60 total (padded)
//
// CachedFile (0x50 = 80 bytes):
//   - dtor @0x2b1f70 destroys vectors at +0x38, +0x20, +0x08
//   - getCachedFile @0x2b1900 zero-inits +0x08..+0x4F at entry, then:
//       * Read .channels section, take first int32, store low 16 bits at +0x00
//       * Read .marker_bits section → vector<uint8_t> at +0x08
//       * Read .data section          → vector<double>  at +0x20  (8-byte stride)
//       * Read .marker section        → vector<uint8_t> at +0x38
//   - cacheFile @0x2b05b0 writes the same set of sections from caller args.
//   - "found" is implied: a default-constructed CachedFile has channel_=0 and
//     all three vectors empty. Callers test by checking samples_.empty().
//   Layout:
//     +0x00  uint16_t             channel_      first 16 bits of .channels section
//     +0x02  6 bytes padding to +0x08
//     +0x08  vector<uint8_t>(24B) markerBits_   from .marker_bits section
//     +0x20  vector<double> (24B) samples_      from .data section
//     +0x38  vector<uint8_t>(24B) markers_      from .marker section
//     +0x48  padding to 0x50
//
// Boost serialization: CacheEntry has a serialize() template instantiated for
// text_iarchive and text_oarchive. We declare it but don't reconstruct the
// boost template machinery.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <ctime>
#include <map>
#include <string>
#include <vector>
#include <boost/filesystem/path.hpp>

namespace zhinst {

class CachedParser {
public:
    // ----- Nested types -----

    // CacheEntry — 0x60 bytes on libc++. One per cached waveform file in index_.
    struct CacheEntry {
        CacheEntry() : byteSize_(0), timestamp_(0), pinned_(false) {} // default ctor for boost deserialization
        CacheEntry(const std::string& name, const std::string& filePath,
                   std::size_t fileSize, std::vector<unsigned int> hash,
                   bool valid);                                    // 0x2b10b0
        ~CacheEntry();                                             // 0x2b1320
        CacheEntry& operator=(const CacheEntry& other);            // 0x2b1210

        // Boost.Serialization support (instantiated for text_iarchive/text_oarchive).
        //
        // Resolved #114: Binary serializes exactly 5 fields (NOT 6).
        // text_iarchive @0x2b7700: load(name_), load(filePath_),
        //   istream>>byteSize_, istream>>timestamp_, load_object(hash_).
        // text_oarchive @0x2b8440: save(name_), save(filePath_),
        //   ostream<<byteSize_, ostream<<timestamp_, save_object(hash_).
        // pinned_ is NOT serialized — it is reconstructed at load time
        // (set to false by default, set to true on cache hit in getCachedFile).
        template <class Archive>
        void serialize(Archive& ar, unsigned int /*version*/)
        {
            ar & name_;
            ar & filePath_;
            ar & byteSize_;
            ar & timestamp_;
            ar & hash_;
        }

        std::string               name_;       // +0x00
        std::string               filePath_;   // +0x18
        std::size_t               byteSize_;   // +0x30
        std::time_t               timestamp_;  // +0x38
        std::vector<unsigned int> hash_;       // +0x40
        bool                      pinned_;      // +0x58
        // padded to 0x60
    };

    // CachedFile — 0x50 bytes. Returned by getCachedFile().
    // No explicit "found" flag: callers test samples_.empty() (or all-zero
    // channel_ + empty vectors) to detect a cache miss.
    struct CachedFile {
        ~CachedFile();                                             // 0x2b1f70

        std::uint16_t              channel_;     // +0x00 (from .channels)
        // 6 bytes padding to +0x08
        std::vector<std::uint8_t>  markerBits_;  // +0x08 (from .marker_bits)
        std::vector<double>        samples_;     // +0x20 (from .data)
        std::vector<std::uint8_t>  markers_;     // +0x38 (from .marker)
        // padded to 0x50
    };

    // ----- Construction -----

    // The only ctor present in the binary.
    // - cacheSize == 0 leaves the cache disabled (enabled_ = false) and
    //   does not touch the filesystem or call loadCacheIndex.
    // - cacheSize != 0 creates the cache directory, appends "index" to
    //   build indexFilePath_, then calls loadCacheIndex().
    CachedParser(std::size_t cacheSize, const boost::filesystem::path& cachePath);

    ~CachedParser();

    // Non-copyable, non-movable (no such symbols in the binary).
    CachedParser(const CachedParser&) = delete;
    CachedParser& operator=(const CachedParser&) = delete;

    // ----- Public methods -----

    // Cache a parsed waveform file. Inserts/updates a CacheEntry in index_,
    // writes the actual sample/marker data to cachePath_/<hash>, then
    // calls saveCacheIndex().                                       // 0x2b05b0
    void cacheFile(const std::string& name,
                   std::vector<unsigned int> hash,
                   int sampleFormat,
                   const std::vector<std::uint8_t>& markers,
                   int markerBits,
                   const std::vector<double>& samples,
                   const std::vector<std::uint8_t>& markerBitsVec);

    // Check whether a cached file is outdated (file modified since caching).
    // Returns true if outdated or not found.                        // 0x2b14d0
    bool cacheFileOutdated(const std::string& name) const;

    // Retrieve a cached file by its hash. Returns a default-constructed
    // CachedFile (channel_=0, all vectors empty) if not in cache or outdated.
    // No explicit "found" flag — callers test samples_.empty().      // 0x2b1900
    CachedFile getCachedFile(const std::vector<unsigned int>& hash);

    // Compute hash for a file path. Returns empty vector if cache disabled.
    // Delegates to zhinst::util::wave::hash().                      // 0x2b1fe0
    std::vector<unsigned int> getHash(const std::string& filePath) const;

private:
    // Implementation methods present in the binary.
    void loadCacheIndex();   // 0x2afec0 — deserialize index_ from indexFilePath_
    void saveCacheIndex();   // 0x2b03c0 — serialize index_ to indexFilePath_
    void cleanCache();       // 0x2b0140 — clear map, remove+recreate cachePath_
    bool removeOldFiles();   // 0x2b01a0 — evict oldest entries past cacheSize_
                             // Returns true if a pinned entry stopped eviction early.

    // ----- Layout (must match binary; see header comment) -----

    // +0x00 .. +0x30 : embedded std::map (libc++ std::__1::__tree head)
    //     std::map<std::vector<unsigned int>, CacheEntry>
    // We declare the field with the host's std::map for correctness in
    // host-side analysis, but note that on the binary's libc++ this
    // occupies exactly 0x30 bytes.
    std::map<std::vector<unsigned int>, CacheEntry> index_;  // +0x00 (0x30 bytes on libc++)

    bool        enabled_;          // +0x18
    // 7 bytes padding to +0x20
    std::size_t cacheSize_;        // +0x20  cache capacity in BYTES (0 = disabled)
    std::size_t currentSize_;      // +0x28  current total size of cached files in BYTES
    boost::filesystem::path cachePath_;      // +0x30 (24 bytes on libc++)
    boost::filesystem::path indexFilePath_;  // +0x48 (24 bytes on libc++) = cachePath_ / "index"
};

// Cannot static_assert sizeof(CachedParser) == 0x60 from the host:
// the binary uses libc++ where std::__1::basic_string is 24 bytes and
// std::__1::__tree is 0x30 bytes; the build host uses libstdc++ where
// std::string is 32 bytes and std::map is 48 bytes. Layout matches the
// binary only when compiled against libc++.

} // namespace zhinst
