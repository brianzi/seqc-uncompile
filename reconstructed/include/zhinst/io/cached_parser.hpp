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
// Binary: no default ctor exists.
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

//! On-disk LRU cache of parsed waveform sample data, keyed by a
//! content hash of the source waveform file.
//!
//! Parsing waveforms from CSV / WAV / arbitrary text inputs is
//! expensive; the same file is often reused across runs. This class
//! stores parsed sample buffers (plus marker bits / markers) inside a
//! cache directory and an `index` file mapping content hash to entry
//! metadata. On a subsequent run, `getCachedFile()` returns the
//! parsed result directly, skipping re-parsing entirely.
//!
//! The cache is byte-budgeted: `cacheSize` is the maximum total size
//! of cached data. When inserting would exceed the budget,
//! `removeOldFiles()` evicts oldest-first until room is freed; pinned
//! entries (marked during a cache hit in the current run) are
//! protected from eviction. Constructing with `cacheSize == 0`
//! disables the cache entirely; subsequent calls become no-ops or
//! return empty results.
//!
//! Thread-safety is the caller's responsibility — this class does
//! not lock the on-disk index.
class CachedParser {
public:
    // ----- Nested types -----

    // CacheEntry — 0x60 bytes on libc++. One per cached waveform file in index_.
    //! Index entry describing one cached waveform file: its display
    //! name, on-disk path, byte size, cache-insertion timestamp, and
    //! content hash. Serialised verbatim to the cache index file via
    //! Boost.Serialization.
    //!
    //! \note The `pinned_` flag is intentionally not serialised:
    //! pinning is a per-run concept (set when the entry is hit in the
    //! current run to protect it from eviction). On load it always
    //! starts `false`.
    struct CacheEntry {
        //! \brief Default-construct an empty entry with zeroed scalars
        //! and `pinned_ = false`.
        //! \details Used by Boost.Serialization when materialising
        //! entries from the on-disk index; the named fields are filled
        //! in by the serialiser before the entry is observable.
        CacheEntry() : byteSize_(0), timestamp_(0), pinned_(false) {} // default ctor for boost deserialization

        //! \brief Construct a fresh index entry, stamping `timestamp_`
        //! with the current wall-clock time.
        //! \param name     Display name of the cached source (typically
        //!                 the user-visible file name).
        //! \param filePath On-disk path of the cached `.wave` artifact
        //!                 inside the cache directory.
        //! \param fileSize Cached payload size in bytes (used for the
        //!                 cache-byte budget).
        //! \param hash     Content hash that keys this entry inside
        //!                 the parser's `index_` map.
        //! \param valid    Initial value of `pinned_`; normally `true`
        //!                 for newly written entries (so they survive
        //!                 the immediate eviction pass of the same run).
        CacheEntry(const std::string& name, const std::string& filePath,
                   std::size_t fileSize, std::vector<unsigned int> hash,
                   bool valid);                                    // 0x2b10b0

        //! \brief Destroy the entry and release its strings and hash
        //! vector.
        ~CacheEntry();                                             // 0x2b1320

        //! \brief Copy-assign every field, including `pinned_`.
        //! \param other Source entry; self-assignment is a no-op.
        //! \return Reference to `*this`.
        CacheEntry& operator=(const CacheEntry& other);            // 0x2b1210

        //! \brief Boost.Serialization hook for the on-disk index file.
        //! \details Persists exactly five fields: `name_`, `filePath_`,
        //! `byteSize_`, `timestamp_`, `hash_`. `pinned_` is deliberately
        //! omitted because it is a per-run flag (see the class-level
        //! note).
        //! \tparam Archive Boost.Archive instantiation (text input or
        //!                 text output).
        //! \param ar Archive to read from or write to.
        template <class Archive>
        void serialize(Archive& ar, unsigned int /*version*/)
        {
            ar & name_;
            ar & filePath_;
            ar & byteSize_;
            ar & timestamp_;
            ar & hash_;
        }

        std::string               name_;       //!< Display name of the cached source. +0x00
        std::string               filePath_;   //!< On-disk path of the cached `.wave` artifact. +0x18
        std::size_t               byteSize_;   //!< Cached payload size in bytes; charged against `cacheSize_`. +0x30
        std::time_t               timestamp_;  //!< Wall-clock time at insertion or last hit; oldest entries are evicted first. +0x38
        std::vector<unsigned int> hash_;       //!< Content hash that keys this entry inside the cache index. +0x40
        bool                      pinned_;     //!< Per-run pin: `true` protects the entry from eviction in `removeOldFiles()`; reset to `false` on load. +0x58
        // padded to 0x60
    };

    // CachedFile — 0x50 bytes. Returned by getCachedFile().
    // No explicit "found" flag: callers test samples_.empty() (or all-zero
    // channel_ + empty vectors) to detect a cache miss.
    //! Result of a cache lookup: the channel index, marker-bit
    //! sample stream, sample buffer, and marker stream of one cached
    //! waveform file.
    //!
    //! \binarynote There is no explicit `found` flag. A cache miss is
    //! signalled by returning a default-constructed value: `channel_`
    //! is zero and all three vectors are empty. Callers detect this
    //! by testing `samples_.empty()`.
    struct CachedFile {
        //! \brief Destroy the result and release its sample / marker
        //! buffers.
        ~CachedFile();                                             // 0x2b1f70

        std::uint16_t              channel_;     //!< Channel / oscillator index recovered from the `.channels` ELF section (low 16 bits). +0x00
        // 6 bytes padding to +0x08
        std::vector<std::uint8_t>  markerBits_;  //!< Marker-bit stream from the `.marker_bits` section. +0x08
        std::vector<double>        samples_;     //!< Decoded sample data from the `.data` section; emptiness signals a cache miss. +0x20
        std::vector<std::uint8_t>  markers_;     //!< Marker stream from the `.marker` section. +0x38
        // padded to 0x50
    };

    // ----- Construction -----

    //! \brief Open or create the on-disk waveform cache rooted at
    //! `cachePath`.
    //! \details When `cacheSize == 0` the cache is left disabled
    //! (`enabled_` is `false`) and no filesystem I/O is performed;
    //! every public method becomes a no-op or returns an empty result.
    //! When `cacheSize != 0` the directory is created (existing
    //! directories are tolerated), `indexFilePath_` is set to
    //! `cachePath / "index"`, and the persisted index is loaded. If
    //! directory creation fails for any other reason the cache is
    //! disabled and a warning is logged.
    //! \param cacheSize Maximum total size of cached payloads in bytes;
    //!                  `0` disables the cache.
    //! \param cachePath Directory in which cached `.wave` artifacts and
    //!                  the `index` file are stored.
    // Only ctor present in the binary.
    CachedParser(std::size_t cacheSize, const boost::filesystem::path& cachePath);

    //! \brief Destroy the parser; the on-disk index is not flushed
    //! here (it is rewritten on every mutating operation).
    ~CachedParser();

    //! \brief Deleted: cache is non-copyable (it owns an on-disk
    //! resource).
    CachedParser(const CachedParser&) = delete;
    //! \brief Deleted: cache is non-assignable.
    CachedParser& operator=(const CachedParser&) = delete;

    // ----- Public methods -----

    //! \brief Persist a parsed waveform to disk and register it in
    //! the index under `hash`.
    //! \details Charges the cache budget, evicting oldest entries via
    //! `removeOldFiles()` if the new entry would push `currentSize_`
    //! past `cacheSize_`; if eviction is blocked by a pinned entry the
    //! charge is rolled back and the call returns without writing.
    //! Otherwise the payload is serialised as a small ELF file
    //! (`csv<hash2str>.wave` inside `cachePath_`) carrying the
    //! `.format`, `.file_name`, `.channels`, `.marker_bits`, `.data`,
    //! `.marker`, and `.config` sections, the index entry is
    //! inserted (or replaced), and the index is flushed.
    //! \note The byte budget charged is
    //! `samples.size() * sizeof(double) / 4`, not the actual on-disk
    //! file size — the cache accounting is intentionally an estimate.
    //! \param name           Display name of the original source file.
    //! \param hash           Content hash used as the index key.
    //! \param sampleFormat   Channel / oscillator descriptor written
    //!                       to the `.channels` section as a 4-byte int.
    //! \param markers        Bytes written to the `.marker_bits` section.
    //! \param markerBits     Marker-bit count, serialised as decimal
    //!                       text in the `.config` section.
    //! \param samples        Sample data written to the `.data` section.
    //! \param markerBitsVec  Bytes written to the `.marker` section.
    void cacheFile(const std::string& name,
                   std::vector<unsigned int> hash,
                   int sampleFormat,
                   const std::vector<std::uint8_t>& markers,
                   int markerBits,
                   const std::vector<double>& samples,
                   const std::vector<std::uint8_t>& markerBitsVec);  // 0x2b05b0

    //! \brief Decide whether a previously cached `.wave` artifact is
    //! out-of-date relative to its source file.
    //! \details Opens the cached ELF, verifies the `.format` section
    //! holds the expected version byte (`'3'`), and reads the
    //! `.file_name` section to recover the original source path. The
    //! cached file is considered outdated when the source's
    //! `last_write_time` is newer than the cached ELF's, or when any
    //! step throws (missing file, missing section, ELF parse error,
    //! etc.).
    //! \binarynote Despite the parameter name, this takes the path of
    //! the cached `.wave` artifact, not the original source — the
    //! source path is recovered from inside the artifact.
    //! \param name Path of the cached `.wave` file.
    //! \return `true` if the cached file is missing, malformed, or
    //!         older than its source.
    bool cacheFileOutdated(const std::string& name) const;  // 0x2b14d0

    //! \brief Look up a cached parse result by content hash.
    //! \details On a hit, refreshes the entry's `timestamp_` and pins
    //! it so the eviction pass spares it for the rest of this run,
    //! then re-reads the cached ELF's `.channels`, `.marker_bits`,
    //! `.data`, and `.marker` sections into the returned `CachedFile`.
    //! On a miss, an outdated hit (deleted in passing), or any
    //! exception during ELF reading (which also triggers
    //! `cleanCache()`), a default-constructed `CachedFile` is returned.
    //! \param hash Content hash to look up.
    //! \return Loaded data on a cache hit; default-constructed value
    //!         on a miss.
    CachedFile getCachedFile(const std::vector<unsigned int>& hash);  // 0x2b1900

    //! \brief Compute the content hash of a source file.
    //! \details Returns an empty vector if the cache is disabled;
    //! otherwise delegates to `zhinst::util::wave::hash`.
    //! \param filePath Path of the source file to hash.
    //! \return Content hash, or an empty vector when the cache is
    //!         disabled.
    std::vector<unsigned int> getHash(const std::string& filePath) const;  // 0x2b1fe0

private:
    //! \brief Deserialise `index_` from `indexFilePath_` and
    //! recompute `currentSize_`.
    //! \details Silently leaves `index_` empty when the file does not
    //! exist; calls `cleanCache()` and starts fresh on any I/O or
    //! deserialisation error. Triggers `removeOldFiles()` if the
    //! restored payload total exceeds `cacheSize_`.
    void loadCacheIndex();   // 0x2afec0 — deserialize index_ from indexFilePath_

    //! \brief Serialise `index_` to `indexFilePath_`.
    //! \details Skipped when the cache is disabled or `index_` is
    //! empty. On any I/O or serialisation failure the cache is
    //! sticky-disabled (`enabled_ = false`) so subsequent operations
    //! become no-ops for the rest of the run.
    void saveCacheIndex();   // 0x2b03c0 — serialize index_ to indexFilePath_

    //! \brief Drop all cached state from memory and disk and recreate
    //! an empty cache directory.
    //! \details Clears `index_`, resets `currentSize_` to zero
    //! (without touching `cacheSize_`), removes `cachePath_`
    //! recursively, and re-creates it. Filesystem exceptions are
    //! swallowed.
    void cleanCache();       // 0x2b0140 — clear map, remove+recreate cachePath_

    //! \brief Evict oldest cached entries until `currentSize_ <=
    //! cacheSize_` or a pinned entry is hit, then flush the index.
    //! \details Sorts the index by `timestamp_` ascending, then for
    //! each entry deletes the on-disk file, subtracts its `byteSize_`
    //! from `currentSize_`, and removes it from `index_`. Iteration
    //! stops at the first `pinned_ == true` entry encountered.
    //! `saveCacheIndex()` is called unconditionally after the loop.
    //! \return `true` if iteration stopped because of a pinned entry
    //!         (i.e. the cache may still be over-budget after the
    //!         call).
    bool removeOldFiles();   // 0x2b01a0 — evict oldest entries past cacheSize_

    // ----- Layout (must match binary; see header comment) -----

    // +0x00 .. +0x30 : embedded std::map (libc++ std::__1::__tree head)
    //     std::map<std::vector<unsigned int>, CacheEntry>
    // We declare the field with the host's std::map for correctness in
    // host-side analysis, but note that on the binary's libc++ this
    // occupies exactly 0x30 bytes.
    std::map<std::vector<unsigned int>, CacheEntry> index_;  //!< Hash → entry map of every cached waveform; embedded `__tree` head is the first member of the class. +0x00 (0x30 bytes on libc++)

    bool        enabled_;          //!< `true` while the cache is usable; sticky-disabled to `false` after any I/O failure or when `cacheSize_ == 0`. +0x18
    // 7 bytes padding to +0x20
    std::size_t cacheSize_;        //!< Cache capacity in bytes; `0` disables the cache. +0x20  cache capacity in BYTES (0 = disabled)
    std::size_t currentSize_;      //!< Running total of `byteSize_` charged across all entries; compared against `cacheSize_` to drive eviction. +0x28  current total size of cached files in BYTES
    boost::filesystem::path cachePath_;      //!< Directory holding cached `.wave` artifacts and the `index` file. +0x30 (24 bytes on libc++)
    boost::filesystem::path indexFilePath_;  //!< `cachePath_ / "index"`; the on-disk persisted form of `index_`. +0x48 (24 bytes on libc++) = cachePath_ / "index"
};

// Cannot static_assert sizeof(CachedParser) == 0x60 from the host:
// the binary uses libc++ where std::__1::basic_string is 24 bytes and
// std::__1::__tree is 0x30 bytes; the build host uses libstdc++ where
// std::string is 32 bytes and std::map is 48 bytes. Layout matches the
// binary only when compiled against libc++.

} // namespace zhinst
