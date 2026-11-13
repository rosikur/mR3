#pragma once
#include <Arduino.h>
#include <atomic>

enum class PlayState : uint8_t { Stopped, Playing, Paused };

// Why the last track stopped. Lets the UI tell a normal end from a failure.
enum class EndReason : uint8_t {
  None = 0,
  Finished,      // decoder reported a normal end of file
  OpenFailed,    // file could not be opened / format not recognised
  DecoderError,  // opened, but no audio was ever produced (corrupt / unsupported data)
  StoppedEarly,  // decoder stopped without an EOF event mid-track (corrupt data, SD error)
};

// Wraps ESP32-audioI2S.
//
// Threading model:
//  * All calls into the Audio object happen inside ONE FreeRTOS task (core 0).
//  * The UI task (core 1) only talks to it through
//      - a FreeRTOS command queue (play / pause / stop: must not be lost), and
//      - two coalescing atomics (volume, seek: only the latest value matters,
//        so they can never be lost to a full queue).
//  * Status flows back through std::atomic members (no volatile, no locks).
class AudioEngine {
 public:
  bool begin(uint8_t initialVolume);
  bool ready() const { return ready_.load(std::memory_order_acquire); }   // Audio object created
  bool failed() const { return initFailed_.load(std::memory_order_acquire); }

  // Return false if the command could not be handed to the audio task.
  bool play(const char* path);
  bool togglePause();
  bool stop();
  void setVolume(uint8_t v);       // 0..21, coalesced
  void seekRelative(int seconds);  // accumulated until the task consumes it

  PlayState state() const { return state_.load(std::memory_order_relaxed); }
  uint32_t position() const { return pos_.load(std::memory_order_relaxed); }
  uint32_t duration() const { return dur_.load(std::memory_order_relaxed); }
  uint8_t volume() const { return vol_.load(std::memory_order_relaxed); }

  // Track end reporting: endedCount() increases once per finished/failed track.
  // Read endedCount() first, then lastEndReason() (release/acquire ordered).
  uint32_t endedCount() const { return endSeq_.load(std::memory_order_acquire); }
  EndReason lastEndReason() const { return endReason_.load(std::memory_order_relaxed); }
  bool lastEndFailed() const {
    const EndReason r = lastEndReason();
    return r != EndReason::None && r != EndReason::Finished;
  }
  static const char* reasonText(EndReason r);

  uint32_t droppedCommands() const { return dropped_.load(std::memory_order_relaxed); }
  uint32_t decoderErrorLogs() const { return errLogs_.load(std::memory_order_relaxed); }

 private:
  enum class Op : uint8_t { Play, Toggle, Stop };
  struct Cmd {
    Op op;
    char path[160];
  };

  static void taskEntry(void* arg);
  void run();
  bool post(const Cmd& c, bool supersedesQueue);
  void finishTrack(EndReason r);

  QueueHandle_t queue_ = nullptr;

  std::atomic<PlayState> state_{PlayState::Stopped};
  std::atomic<uint32_t> pos_{0};
  std::atomic<uint32_t> dur_{0};
  std::atomic<uint8_t> vol_{12};
  std::atomic<int16_t> targetVol_{-1};   // -1 = nothing pending
  std::atomic<int32_t> seekAccum_{0};

  std::atomic<uint32_t> endSeq_{0};
  std::atomic<EndReason> endReason_{EndReason::None};
  std::atomic<uint32_t> dropped_{0};
  std::atomic<uint32_t> errLogs_{0};
  std::atomic<bool> eof_{false};
  std::atomic<bool> ready_{false};
  std::atomic<bool> initFailed_{false};
};
