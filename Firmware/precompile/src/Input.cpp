#include "Input.h"
#include "Config.h"

using namespace mr3;

namespace {
constexpr int8_t PIN[(uint8_t)Btn::Count] = {
  pins::BTN_MENU, pins::BTN_SEL,    pins::BTN_PLAY,    pins::BTN_SKIP, pins::BTN_UNSKIP,
  pins::BTN_LOUDER, pins::BTN_QUIETER, pins::BTN_ONOFF, pins::BTN_BOOT,
};
constexpr uint32_t DEBOUNCE_MS = 20;
constexpr uint32_t LONG_MS = 700;
constexpr uint32_t LONG_ONOFF_MS = 1500;
constexpr uint32_t REPEAT_START_MS = 450;
constexpr uint32_t REPEAT_EVERY_MS = 130;
constexpr uint32_t CHORD_MS = 1200;

bool isRepeater(Btn b) {
  return b == Btn::Skip || b == Btn::Unskip || b == Btn::Louder || b == Btn::Quieter;
}
}  // namespace

void Input::begin() {
  for (uint8_t i = 0; i < (uint8_t)Btn::Count; i++) {
    pinMode(PIN[i], INPUT);  // external pull-ups on the PCB
    s_[i] = State();
  }
  head_ = tail_ = 0;
}

void Input::noteDrop(const char* what) {
  dropped_++;
  overflow_ = true;
  const uint32_t now = millis();
  if (now - lastDropLog_ > 1000) {  // rate-limited log
    lastDropLog_ = now;
    Serial.printf("[input] queue overflow, dropped %s (total %u) - UI too slow?\n", what,
                  (unsigned)dropped_);
  }
}

void Input::removeAt(uint8_t k) {
  const uint8_t n = size();
  for (uint8_t i = k; i + 1 < n; i++) q_[(tail_ + i) % QN] = q_[(tail_ + i + 1) % QN];
  head_ = (head_ + QN - 1) % QN;
}

// When the queue is full the least valuable event goes first: an old auto-repeat,
// then an old Press. Click/Long/Release/Chord are only sacrificed as a last resort
// (a lost Release would leave the UI believing a button is stuck).
void Input::push(Btn b, EvType t) {
  if (size() >= QN - 1) {  // full (ring buffer keeps one slot free)
    bool freed = false;
    for (uint8_t pass = 0; pass < 2 && !freed; pass++) {
      const EvType victim = (pass == 0) ? EvType::Repeat : EvType::Press;
      for (uint8_t i = 0; i < size(); i++) {
        if (q_[(tail_ + i) % QN].type == victim) {
          removeAt(i);
          noteDrop(pass == 0 ? "repeat" : "press");
          freed = true;
          break;
        }
      }
    }
    if (!freed) {
      if (lossy(t)) {  // new event is expendable
        noteDrop("new event");
        return;
      }
      removeAt(0);  // everything queued is important and so is the new event: oldest loses
      noteDrop("oldest event");
    }
  }
  q_[head_] = {b, t};
  head_ = (head_ + 1) % QN;
}

bool Input::poll(InputEvent& out) {
  scan();
  if (head_ == tail_) return false;
  out = q_[tail_];
  tail_ = (tail_ + 1) % QN;
  return true;
}

void Input::scan() {
  const uint32_t now = millis();

  for (uint8_t i = 0; i < (uint8_t)Btn::Count; i++) {
    State& s = s_[i];
    const Btn b = (Btn)i;
    const bool raw = digitalRead(PIN[i]) == LOW;

    if (raw != s.down && now - s.changedAt >= DEBOUNCE_MS) {
      s.down = raw;
      s.changedAt = now;
      if (raw) {
        s.pressedAt = now;
        s.longFired = false;
        s.repeating = false;
        s.consumed = false;
        push(b, EvType::Press);
      } else {
        if (!s.consumed && !s.longFired && !s.repeating) push(b, EvType::Click);
        push(b, EvType::Release);
      }
    }
  }

  // Chord: Menu + Sel + Play
  State& m = s_[(uint8_t)Btn::Menu];
  State& se = s_[(uint8_t)Btn::Sel];
  State& pl = s_[(uint8_t)Btn::Play];
  if (m.down && se.down && pl.down) {
    if (!chordArmed_) {
      chordArmed_ = true;
      chordFired_ = false;
      chordAt_ = now;
      m.consumed = se.consumed = pl.consumed = true;
    } else if (!chordFired_ && now - chordAt_ >= CHORD_MS) {
      chordFired_ = true;
      push(Btn::Menu, EvType::Chord);
    }
  } else if (!m.down && !se.down && !pl.down) {
    chordArmed_ = false;
    chordFired_ = false;
  }

  // Long press / auto repeat
  for (uint8_t i = 0; i < (uint8_t)Btn::Count; i++) {
    State& s = s_[i];
    const Btn b = (Btn)i;
    if (!s.down || s.consumed) continue;
    const uint32_t held = now - s.pressedAt;
    if (isRepeater(b)) {
      if (held >= REPEAT_START_MS && (!s.repeating || now - s.lastRepeat >= REPEAT_EVERY_MS)) {
        s.repeating = true;
        s.lastRepeat = now;
        push(b, EvType::Repeat);
      }
    } else if (!s.longFired) {
      const uint32_t thr = (b == Btn::OnOff) ? LONG_ONOFF_MS : LONG_MS;
      if (held >= thr) {
        s.longFired = true;
        push(b, EvType::Long);
      }
    }
  }
}
