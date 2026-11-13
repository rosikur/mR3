#pragma once
#include <Arduino.h>

// Persistent settings (stored in flash via NVS, independent of the SD card).
struct Settings {
  uint8_t volume = 12;        // 0..21
  uint8_t brightnessIdx = 3;  // index into brightness levels
  uint8_t timeoutIdx = 2;     // index into screen-off timeouts
  bool shuffle = false;
  uint8_t repeat = 0;         // 0 off, 1 all, 2 one

  void load();
  // Writes only what changed. Returns false if flash could not be written
  // (after one retry); the values stay valid in RAM either way.
  bool save();

  uint32_t saveFailures() const { return saveFailures_; }

  uint8_t brightnessPct() const;
  uint16_t timeoutSec() const;  // 0 = never
  void cycleBrightness();
  void cycleTimeout();

 private:
  struct Snapshot {  // what is known to be in flash
    uint8_t volume, brightnessIdx, timeoutIdx, repeat;
    bool shuffle;
  };
  bool writeAll();
  Snapshot snap_{};
  bool snapValid_ = false;
  uint32_t saveFailures_ = 0;
};
