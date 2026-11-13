#pragma once
#include <Arduino.h>

// Reads battery voltage / charge state from the BQ25895 over I2C.
// There is no fuel gauge on the board, so the percentage is only a rough
// estimate from the cell voltage. UNTESTED on real hardware.
class Battery {
 public:
  void begin();
  void update();  // call often; polls the chip every 5 s

  bool present() const { return ok_; }
  int percent() const { return pct_; }
  bool charging() const { return charging_; }
  uint16_t millivolts() const { return mv_; }

 private:
  bool readReg(uint8_t reg, uint8_t& v);
  bool writeReg(uint8_t reg, uint8_t v);
  void poll();

  bool ok_ = false;
  bool charging_ = false;
  int pct_ = 0;
  uint16_t mv_ = 0;
  uint32_t lastPoll_ = 0;
};
