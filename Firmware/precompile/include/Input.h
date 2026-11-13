#pragma once
#include <Arduino.h>

enum class Btn : uint8_t { Menu, Sel, Play, Skip, Unskip, Louder, Quieter, OnOff, Boot, Count };

// Press   : button went down (immediately)
// Click   : released after a short press (no long/repeat/chord happened)
// Long    : held ~0.7 s (OnOff: 1.5 s), fired once
// Repeat  : Skip/Unskip/Louder/Quieter held, fires every ~130 ms after 450 ms
// Release : button went up (always)
// Chord   : Menu+Sel+Play held together for 1.2 s
enum class EvType : uint8_t { Press, Click, Long, Repeat, Release, Chord };

struct InputEvent {
  Btn btn;
  EvType type;
};

class Input {
 public:
  void begin();
  bool poll(InputEvent& out);  // call often; returns queued events one at a time
  bool isDown(Btn b) const { return s_[(uint8_t)b].down; }

  // Overflow detection: events are never lost silently.
  uint32_t droppedEvents() const { return dropped_; }   // total events lost since boot
  bool takeOverflow() { bool o = overflow_; overflow_ = false; return o; }  // true once per overflow burst

 private:
  struct State {
    bool down = false;
    bool longFired = false;
    bool repeating = false;
    bool consumed = false;
    uint32_t changedAt = 0;
    uint32_t pressedAt = 0;
    uint32_t lastRepeat = 0;
  };
  static constexpr uint8_t QN = 24;

  void scan();
  void push(Btn b, EvType t);
  uint8_t size() const { return (uint8_t)((head_ + QN - tail_) % QN); }
  void removeAt(uint8_t offsetFromTail);
  static bool lossy(EvType t) { return t == EvType::Repeat || t == EvType::Press; }
  void noteDrop(const char* what);

  State s_[(uint8_t)Btn::Count];
  InputEvent q_[QN];
  uint8_t head_ = 0, tail_ = 0;
  uint32_t dropped_ = 0;
  uint32_t lastDropLog_ = 0;
  bool overflow_ = false;
  bool chordArmed_ = false, chordFired_ = false;
  uint32_t chordAt_ = 0;
};
