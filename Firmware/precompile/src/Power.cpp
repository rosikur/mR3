#include "Power.h"
#include <Wire.h>
#include "Config.h"

using namespace mr3;

namespace {
constexpr uint8_t REG_ADC_CTRL = 0x02;  // bit6 CONV_RATE: continuous ADC
constexpr uint8_t REG_STATUS = 0x0B;    // bits 4:3 CHRG_STAT
constexpr uint8_t REG_BATV = 0x0E;      // bits 6:0, 2304 mV + 20 mV * value
}  // namespace

bool Battery::readReg(uint8_t reg, uint8_t& v) {
  Wire.beginTransmission(BQ_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)BQ_ADDR, (uint8_t)1) != 1) return false;
  v = Wire.read();
  return true;
}

bool Battery::writeReg(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(BQ_ADDR);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

void Battery::begin() {
  Wire.begin(pins::BQ_SDA, pins::BQ_SCL);
  Wire.setClock(100000);
  poll();
}

void Battery::update() {
  if (millis() - lastPoll_ >= 5000) poll();
}

void Battery::poll() {
  lastPoll_ = millis();
  uint8_t r02, r0e, r0b;
  if (!readReg(REG_ADC_CTRL, r02)) {
    ok_ = false;
    return;
  }
  if (!(r02 & 0x40)) writeReg(REG_ADC_CTRL, r02 | 0x40);  // continuous conversion
  if (!readReg(REG_BATV, r0e) || !readReg(REG_STATUS, r0b)) {
    ok_ = false;
    return;
  }
  ok_ = true;
  mv_ = 2304 + (r0e & 0x7F) * 20;
  const uint8_t chg = (r0b >> 3) & 0x03;  // 0 idle, 1 pre-charge, 2 fast charge, 3 done
  charging_ = (chg == 1 || chg == 2);
  int p = ((int)mv_ - 3300) * 100 / (4150 - 3300);
  pct_ = p < 0 ? 0 : (p > 100 ? 100 : p);
}
