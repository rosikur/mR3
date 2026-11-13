#include "Library.h"
#include <SD_MMC.h>
#include <algorithm>
#include <ctype.h>
#include <string.h>
#include <strings.h>

namespace {

void copyStr(char* dst, size_t n, const char* src) {
  strncpy(dst, src, n - 1);
  dst[n - 1] = 0;
}

bool isAudioFile(const char* name) {
  const char* dot = strrchr(name, '.');
  if (!dot) return false;
  static const char* const EXT[] = {".mp3", ".flac", ".wav", ".aac", ".m4a", ".ogg", ".opus"};
  for (const char* e : EXT)
    if (strcasecmp(dot, e) == 0) return true;
  return false;
}

bool isSep(char c) { return c == ' ' || c == '.' || c == '-' || c == '_'; }

// Fill title/artist/album from "/Artist/Album/01 - Title.ext"
void metaFromPath(const char* path, Track& t) {
  char buf[128];
  copyStr(buf, sizeof(buf), path);

  const char* parts[12];
  int n = 0;
  char* p = buf;
  if (*p == '/') p++;
  parts[n++] = p;
  for (char* q = p; *q; q++) {
    if (*q == '/') {
      *q = 0;
      if (n < 12) parts[n++] = q + 1;
    }
  }

  // title: file name without extension and without leading track number
  char title[64];
  copyStr(title, sizeof(title), parts[n - 1]);
  char* dot = strrchr(title, '.');
  if (dot) *dot = 0;
  const char* s = title;
  const char* d = s;
  while (isdigit((unsigned char)*d)) d++;
  if (d > s && isSep(*d)) {
    while (isSep(*d)) d++;
    if (*d) s = d;
  }
  copyStr(t.title, sizeof(t.title), s);

  copyStr(t.album, sizeof(t.album), n >= 2 ? parts[n - 2] : "Unknown Album");
  copyStr(t.artist, sizeof(t.artist), n >= 3 ? parts[n - 3] : "Unknown Artist");
}

// Empty results returned while the scan task owns the data.
const Track kNoTrack = {};
const std::vector<int16_t> kNoSongs;
const Library::PsramShorts kNoArtists;
const Library::PsramAlbums kNoAlbums;

constexpr size_t INTERNAL_RESERVE = 96 * 1024;  // never let the library eat the last internal RAM

}  // namespace

// ---------------------------------------------------------------- accessors

int Library::count() const { return scanning() ? 0 : (int)tracks_.size(); }

const Track& Library::track(int i) const {
  if (scanning() || i < 0 || i >= (int)tracks_.size()) return kNoTrack;
  return tracks_[i];
}

const std::vector<int16_t>& Library::songs() const { return scanning() ? kNoSongs : songs_; }
const Library::PsramShorts& Library::artists() const { return scanning() ? kNoArtists : artists_; }
const Library::PsramAlbums& Library::albums() const { return scanning() ? kNoAlbums : albums_; }

// ---------------------------------------------------------------- scanning

bool Library::startScan() {
  bool expected = false;
  if (!busy_.compare_exchange_strong(expected, true)) return false;  // already running
  progress_.store(0);
  // Priority 1 on core 1 (same as the Arduino loop task, which sleeps between
  // iterations); audio decoding runs on core 0 at priority 4 and is not affected.
  if (xTaskCreatePinnedToCore(scanTaskEntry, "libscan", 8192, this, 1, nullptr, 1) != pdPASS) {
    Serial.println("[library] could not start scan task");
    busy_.store(false);
    return false;
  }
  return true;
}

void Library::scanTaskEntry(void* arg) {
  static_cast<Library*>(arg)->doScan();  // clears busy_ itself
  vTaskDelete(nullptr);
}

// How many tracks fit in the largest free block (PSRAM preferred) without
// starving the rest of the firmware. 0 = not even a small library fits.
size_t Library::pickCapacity() const {
  size_t blk = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (blk == 0) {  // no PSRAM: use internal RAM, keep a reserve
    const size_t in = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    blk = in > INTERNAL_RESERVE ? in - INTERNAL_RESERVE : 0;
  }
  size_t cap = blk / sizeof(Track);
  if (cap > (size_t)MAX_TRACKS) cap = MAX_TRACKS;
  return cap;
}

void Library::doScan() {
  tracks_.clear();
  songs_.clear();
  artists_.clear();
  albums_.clear();
  truncated_ = false;

  maxTracks_ = pickCapacity();
  if (maxTracks_ < 16) {
    Serial.println("[library] not enough free memory for a library");
    busy_.store(false, std::memory_order_release);
    return;
  }
  // One allocation, never grows: no reallocation, no heap fragmentation.
  tracks_.reserve(maxTracks_);

  walk("/", 0);
  buildIndexes();

  Serial.printf("[library] %d tracks, %d artists, %d albums%s\n", (int)tracks_.size(),
                (int)artists_.size(), (int)albums_.size(), truncated_ ? " (TRUNCATED)" : "");
  busy_.store(false, std::memory_order_release);  // publishes all results
}

void Library::walk(const char* dirPath, int depth) {
  if (depth > MAX_DEPTH) return;
  File dir = SD_MMC.open(dirPath);
  if (!dir) return;
  if (!dir.isDirectory()) {
    dir.close();
    return;
  }

  // Sub-directory paths are collected first (NUL-separated in one block), then this
  // handle is closed before recursing: the FAT driver only allows a few open files.
  std::vector<char, PsramAllocator<char>> subs;

  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const char* name = f.name();
    const char* path = f.path();
    if (!name || !path || name[0] == '.') continue;
    const size_t plen = strlen(path);
    if (plen >= sizeof(Track::path)) continue;  // path too long, skip

    if (f.isDirectory()) {
      if (strcasecmp(name, "System Volume Information") == 0) continue;
      subs.insert(subs.end(), path, path + plen + 1);
      continue;
    }
    if (!isAudioFile(name)) continue;
    if (tracks_.size() >= maxTracks_) {
      truncated_ = true;
      break;
    }

    Track t;
    memset(&t, 0, sizeof(t));
    copyStr(t.path, sizeof(t.path), path);
    metaFromPath(t.path, t);
    tracks_.push_back(t);  // capacity reserved, cannot reallocate
    progress_.store((int)tracks_.size(), std::memory_order_relaxed);
    if ((tracks_.size() & 15) == 0) vTaskDelay(1);  // let other tasks breathe
  }
  dir.close();

  for (size_t off = 0; off < subs.size();) {
    const char* p = &subs[off];
    off += strlen(p) + 1;
    if (tracks_.size() >= maxTracks_) {
      truncated_ = true;
      break;
    }
    walk(p, depth + 1);
  }
}

// Sorting instead of nested linear searches: O(n log n), and the artist/album
// vectors are reserved to their exact size (no growth reallocations).
void Library::buildIndexes() {
  const int n = (int)tracks_.size();

  songs_.reserve(n);
  for (int i = 0; i < n; i++) songs_.push_back((int16_t)i);
  std::sort(songs_.begin(), songs_.end(), [this](int16_t a, int16_t b) {
    return strcasecmp(tracks_[a].title, tracks_[b].title) < 0;
  });

  std::vector<int16_t> order(songs_);  // same ids, re-sorted by artist, then album
  std::sort(order.begin(), order.end(), [this](int16_t a, int16_t b) {
    int c = strcasecmp(tracks_[a].artist, tracks_[b].artist);
    if (c != 0) return c < 0;
    return strcasecmp(tracks_[a].album, tracks_[b].album) < 0;
  });

  size_t nArtists = 0, nAlbums = 0;
  const Track* prev = nullptr;
  for (int16_t id : order) {
    const Track& t = tracks_[id];
    const bool newArtist = !prev || strcasecmp(prev->artist, t.artist) != 0;
    const bool newAlbum = newArtist || strcasecmp(prev->album, t.album) != 0;
    if (newArtist) nArtists++;
    if (newAlbum) nAlbums++;
    prev = &t;
  }
  artists_.reserve(nArtists);
  albums_.reserve(nAlbums);

  prev = nullptr;
  for (int16_t id : order) {
    const Track& t = tracks_[id];
    const bool newArtist = !prev || strcasecmp(prev->artist, t.artist) != 0;
    const bool newAlbum = newArtist || strcasecmp(prev->album, t.album) != 0;
    if (newArtist) {
      ShortStr s;
      copyStr(s.s, sizeof(s.s), t.artist);
      artists_.push_back(s);
    }
    if (newAlbum) {
      AlbumInfo a;
      copyStr(a.artist.s, sizeof(a.artist.s), t.artist);
      copyStr(a.name.s, sizeof(a.name.s), t.album);
      albums_.push_back(a);
    }
    prev = &t;
  }
  // artists_ is already sorted (order is sorted by artist); albums by name:
  std::sort(albums_.begin(), albums_.end(), [](const AlbumInfo& a, const AlbumInfo& b) {
    return strcasecmp(a.name.s, b.name.s) < 0;
  });
}

void Library::albumsOfArtist(int artistIdx, std::vector<int16_t>& out) const {
  out.clear();
  if (scanning() || artistIdx < 0 || artistIdx >= (int)artists_.size()) return;
  for (int i = 0; i < (int)albums_.size(); i++)
    if (strcasecmp(albums_[i].artist.s, artists_[artistIdx].s) == 0) out.push_back((int16_t)i);
}

void Library::tracksOfAlbum(int albumIdx, std::vector<int16_t>& out) const {
  out.clear();
  if (scanning() || albumIdx < 0 || albumIdx >= (int)albums_.size()) return;
  const AlbumInfo& a = albums_[albumIdx];
  for (int i = 0; i < (int)tracks_.size(); i++)
    if (strcasecmp(a.artist.s, tracks_[i].artist) == 0 && strcasecmp(a.name.s, tracks_[i].album) == 0)
      out.push_back((int16_t)i);
  std::sort(out.begin(), out.end(), [this](int16_t x, int16_t y) {
    return strcasecmp(tracks_[x].path, tracks_[y].path) < 0;  // "01 ..." sorts naturally
  });
}
