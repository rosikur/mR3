#pragma once
#include <Arduino.h>
#include <atomic>
#include <vector>
#include "PsramAlloc.h"

struct Track {
  char path[128];   // full path on the SD card, e.g. /Artist/Album/01 Song.flac
  char title[48];
  char artist[32];
  char album[32];
};

// Fixed-size names: no per-string heap allocations (the old String members
// fragmented the internal heap on every rescan).
struct ShortStr {
  char s[32];
  const char* c_str() const { return s; }
};

struct AlbumInfo {
  ShortStr artist;
  ShortStr name;
};

// Scans the SD card recursively in a background task. Metadata comes from the
// folder layout (Artist/Album/Track.ext) and the file name. ID3/Vorbis/MP4 tag
// reading is a planned next step.
//
// Threading: startScan() returns immediately. While scanning() is true the
// scan task owns all data; the accessors then return empty results, so a UI
// that forgets to check can never read half-built data. scanProgress() is safe
// to poll at any time.
class Library {
 public:
  using PsramShorts = std::vector<ShortStr, PsramAllocator<ShortStr>>;
  using PsramAlbums = std::vector<AlbumInfo, PsramAllocator<AlbumInfo>>;

  bool startScan();                       // false: already running or task could not start
  bool scanning() const { return busy_.load(std::memory_order_acquire); }
  int scanProgress() const { return progress_.load(std::memory_order_relaxed); }
  bool truncated() const { return !scanning() && truncated_; }  // more files than fit in memory
  bool foundAny() const { return !scanning() && !tracks_.empty(); }

  int count() const;
  const Track& track(int i) const;
  const std::vector<int16_t>& songs() const;     // sorted by title
  const PsramShorts& artists() const;            // sorted, unique
  const PsramAlbums& albums() const;             // sorted by name, unique

  void albumsOfArtist(int artistIdx, std::vector<int16_t>& out) const;
  void tracksOfAlbum(int albumIdx, std::vector<int16_t>& out) const;

 private:
  static constexpr int MAX_TRACKS = 1500;
  static constexpr int MAX_DEPTH = 8;

  static void scanTaskEntry(void* arg);
  void doScan();
  size_t pickCapacity() const;
  void walk(const char* dirPath, int depth);
  void buildIndexes();

  std::vector<Track, PsramAllocator<Track>> tracks_;
  std::vector<int16_t> songs_;
  PsramShorts artists_;
  PsramAlbums albums_;

  std::atomic<bool> busy_{false};
  std::atomic<int> progress_{0};
  size_t maxTracks_ = 0;
  bool truncated_ = false;  // written by the scan task before busy_ is released
};
