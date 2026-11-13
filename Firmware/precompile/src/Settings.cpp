#include "Settings.h"
#include <Preferences.h>
#include "Config.h"

namespace {
const uint8_t BRIGHT[] = {20, 40, 60, 80, 100};
const uint16_t TIMEOUT[] = {0, 15, 30, 60, 120};
constexpr uint8_t NB = sizeof(BRIGHT) / sizeof(BRIGHT[0]);
constexpr uint8_t NT = sizeof(TIMEOUT) / sizeof(TIMEOUT[0]);
}  // namespace

void Settings::load() {
  Preferences p;
  // Read-only open fails when the namespace does not exist yet (first boot) or NVS
  // is unusable. Either way: run with defaults; save() creates/repairs it later.
  if (!p.begin("mr3", true)) {
    Serial.println("[settings] nothing stored (first boot?) - using defaults");
    return;
  }
  volume = p.getUChar("vol", 12);
  brightnessIdx = p.getUChar("bri", 3);
  timeoutIdx = p.getUChar("tmo", 2);
  shuffle = p.getBool("shuf", false);
  repeat = p.getUChar("rep", 0);
  p.end();

  if (volume > mr3::VOLUME_STEPS) volume = 12;
  if (brightnessIdx >= NB) brightnessIdx = 3;
  if (timeoutIdx >= NT) timeoutIdx = 2;
  if (repeat > 2) repeat = 0;

  snap_ = {volume, brightnessIdx, timeoutIdx, repeat, shuffle};
  snapValid_ = true;
}

bool Settings::writeAll() {
  Preferences p;
  if (!p.begin("mr3", false)) {
    Serial.println("[settings] NVS open failed");
    return false;
  }
  // put*() return the number of bytes written; 0 means the write failed.
  bool ok = true;
  ok &= p.putUChar("vol", volume) == 1;
  ok &= p.putUChar("bri", brightnessIdx) == 1;
  ok &= p.putUChar("tmo", timeoutIdx) == 1;
  ok &= p.putBool("shuf", shuffle) == 1;
  ok &= p.putUChar("rep", repeat) == 1;
  p.end();
  return ok;
}

bool Settings::save() {
  if (snapValid_ && snap_.volume == volume && snap_.brightnessIdx == brightnessIdx &&
      snap_.timeoutIdx == timeoutIdx && snap_.repeat == repeat && snap_.shuffle == shuffle)
    return true;  // nothing changed: no flash wear

  bool ok = writeAll();
  if (!ok) {
    delay(20);
    ok = writeAll();  // one retry
  }
  if (!ok) {
    saveFailures_++;
    Serial.printf("[settings] SAVE FAILED (%u failures)\n", (unsigned)saveFailures_);
    return false;
  }
  snap_ = {volume, brightnessIdx, timeoutIdx, repeat, shuffle};
  snapValid_ = true;
  return true;
}

uint8_t Settings::brightnessPct() const { return BRIGHT[brightnessIdx % NB]; }
uint16_t Settings::timeoutSec() const { return TIMEOUT[timeoutIdx % NT]; }
void Settings::cycleBrightness() { brightnessIdx = (brightnessIdx + 1) % NB; }
void Settings::cycleTimeout() { timeoutIdx = (timeoutIdx + 1) % NT; }
