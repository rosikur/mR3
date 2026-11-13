// mR3 OS - hardware diagnostics (env: hwtest).
// Checks, in order: display + backlight, PSRAM/heap, I2C bus (scan + BQ25895),
// SD card (mount, size, write/read-back, read speed), I2S/DAC init, buttons.
// PLAY = test tone (left, then right channel). All results also go to Serial.
// Charger register decoding is UNTESTED on real hardware - compare with the datasheet.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <math.h>
#include "Config.h"

using namespace mr3;

TFT_eSPI tft;
I2SClass i2s;

constexpr int W = 320;
constexpr int H = 240;
constexpr int LINE_H = 15;
constexpr int DIAG_W = 190;  // left: diagnostics, right: buttons
constexpr uint8_t BQ = BQ_ADDR;

enum class R : uint8_t { Pass, Warn, Fail, Info };
int diagY = 20;
int failCount = 0;
bool i2sOk = false;

void report(R r, const String& text) {
  static const uint16_t col[] = {TFT_GREEN, TFT_YELLOW, TFT_RED, TFT_WHITE};
  static const char* tag[] = {"PASS", "WARN", "FAIL", "INFO"};
  if (r == R::Fail) failCount++;
  Serial.printf("[%s] %s\n", tag[(int)r], text.c_str());
  if (diagY > H - 2 * LINE_H) return;  // out of screen, still in the serial log
  tft.setTextColor(col[(int)r], TFT_BLACK);
  tft.fillRect(0, diagY, DIAG_W, LINE_H, TFT_BLACK);
  tft.drawString(text, 2, diagY, 2);
  diagY += LINE_H;
}

// ------------------------------------------------------------------ display

void testDisplay() {
  const uint16_t bars[] = {TFT_RED, TFT_GREEN, 0x001F, TFT_WHITE, TFT_BLACK};
  for (int i = 0; i < 5; i++) tft.fillRect(i * (W / 5), 0, W / 5, H, bars[i]);
  for (int duty = 0; duty <= 255; duty += 15) {  // backlight ramp
    ledcWrite(pins::TFT_BL, duty);
    delay(25);
  }
  delay(700);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("mR3 OS hardware test", 2, 2, 2);
  report(R::Info, "Display " + String(tft.width()) + "x" + String(tft.height()) +
                      " (colour bars + backlight ramp shown)");
}

// ------------------------------------------------------------------ memory

void testMemory() {
  if (psramFound()) report(R::Pass, "PSRAM " + String(ESP.getPsramSize() / 1024) + " KB");
  else report(R::Fail, "PSRAM not found (memory_type?)");
  report(R::Info, "Heap " + String(ESP.getFreeHeap() / 1024) + "/" + String(ESP.getHeapSize() / 1024) + " KB, flash " +
                      String(ESP.getFlashChipSize() / (1024 * 1024)) + " MB");
}

// ------------------------------------------------------------------ I2C

bool bqRead(uint8_t reg, uint8_t& v) {
  Wire.beginTransmission(BQ);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)BQ, (uint8_t)1) != 1) return false;
  v = Wire.read();
  return true;
}

void testI2C() {
  if (!Wire.begin(pins::BQ_SDA, pins::BQ_SCL)) {
    report(R::Fail, "I2C: Wire.begin failed");
    return;
  }
  Wire.setClock(100000);
  String found;
  int n = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      char b[8];
      snprintf(b, sizeof(b), "%s0x%02X", n ? " " : "", a);
      found += b;
      n++;
    }
  }
  if (n == 0) {
    report(R::Fail, "I2C: no device (check SDA/SCL pull-ups)");
    return;
  }
  report(R::Info, "I2C devices: " + found);

  uint8_t r0b = 0, r0e = 0, r14 = 0, r02 = 0;
  if (!bqRead(0x14, r14)) {
    report(R::Fail, "BQ25895 (0x6A) not answering");
    return;
  }
  report(R::Pass, "BQ25895 answers, REG14=0x" + String(r14, HEX));
  if (bqRead(0x02, r02)) {  // enable continuous ADC, wait for the first conversion
    Wire.beginTransmission(BQ);
    Wire.write((uint8_t)0x02);
    Wire.write((uint8_t)(r02 | 0x40));
    Wire.endTransmission();
    delay(1100);
  }
  if (bqRead(0x0E, r0e) && bqRead(0x0B, r0b)) {
    const unsigned mv = 2304 + (r0e & 0x7F) * 20;
    static const char* chg[] = {"not charging", "pre-charge", "fast charge", "charge done"};
    const bool plausible = mv > 2800 && mv < 4500;
    report(plausible ? R::Pass : R::Warn,
           "Battery " + String(mv) + " mV, " + chg[(r0b >> 3) & 3] + (plausible ? "" : " (implausible!)"));
  } else {
    report(R::Warn, "BQ25895: could not read battery registers");
  }
}

// ------------------------------------------------------------------ SD card

void testSD() {
  SD_MMC.setPins(pins::SD_CLK, pins::SD_CMD, pins::SD_D0);
  if (!SD_MMC.begin("/sdcard", true, false, 40000, 8)) {
    report(R::Fail, "SD: mount failed (card? FAT32?)");
    return;
  }
  static const char* type[] = {"none", "MMC", "SD", "SDHC", "unknown"};
  const uint8_t t = SD_MMC.cardType();
  report(R::Pass, String("SD ") + type[t > 4 ? 4 : t] + " " + String((uint32_t)(SD_MMC.cardSize() / (1024ULL * 1024ULL))) + " MB");

  // write / read-back (needs a working CMD + DAT0 path in both directions)
  const char* tmp = "/mr3_test.tmp";
  uint8_t buf[512], back[512];
  for (int i = 0; i < 512; i++) buf[i] = (uint8_t)(i * 7 + 3);
  bool wr = false;
  File f = SD_MMC.open(tmp, FILE_WRITE);
  if (f) {
    wr = f.write(buf, sizeof(buf)) == sizeof(buf);
    f.close();
  }
  bool rd = false;
  f = SD_MMC.open(tmp, FILE_READ);
  if (f) {
    rd = f.read(back, sizeof(back)) == sizeof(back) && memcmp(buf, back, sizeof(buf)) == 0;
    f.close();
  }
  SD_MMC.remove(tmp);
  report(wr && rd ? R::Pass : R::Fail, wr ? (rd ? "SD write/read-back ok" : "SD read-back MISMATCH") : "SD write failed");

  // read speed + audio file count in the root folder
  int audio = 0, files = 0;
  String bigFile;
  File root = SD_MMC.open("/");
  if (root) {
    for (File e = root.openNextFile(); e; e = root.openNextFile()) {
      if (e.isDirectory()) continue;
      files++;
      String n = e.name();
      n.toLowerCase();
      if (n.endsWith(".mp3") || n.endsWith(".flac") || n.endsWith(".wav") || n.endsWith(".m4a") || n.endsWith(".aac") ||
          n.endsWith(".ogg") || n.endsWith(".opus"))
        audio++;
      if (bigFile.length() == 0 && e.size() > 1024 * 1024) bigFile = e.path();
    }
    root.close();
  }
  report(R::Info, "Root: " + String(files) + " files, " + String(audio) + " audio");
  if (bigFile.length()) {
    File g = SD_MMC.open(bigFile.c_str(), FILE_READ);
    if (g) {
      static uint8_t chunk[4096];
      const uint32_t t0 = millis();
      size_t total = 0;
      while (total < 1024 * 1024) {
        const size_t r = g.read(chunk, sizeof(chunk));
        if (r == 0) break;
        total += r;
      }
      const uint32_t ms = millis() - t0;
      g.close();
      if (total > 0 && ms > 0) {
        const float mbps = (total / 1048576.0f) / (ms / 1000.0f);
        report(mbps >= 1.0f ? R::Pass : R::Warn, "SD read " + String(mbps, 1) + " MB/s" + (mbps < 1.0f ? " (slow)" : ""));
      }
    }
  } else {
    report(R::Info, "SD speed: no file >1 MB in root");
  }
}

// ------------------------------------------------------------------ audio

void testAudioInit() {
  i2s.setPins(pins::I2S_BCK, pins::I2S_WS, pins::I2S_DOUT);
  i2sOk = i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  if (i2sOk) {
    i2s.end();
    report(R::Pass, "I2S init ok (BCK17 WS15 DOUT16)");
  } else {
    report(R::Fail, "I2S init FAILED");
  }
}

void playTone() {
  if (!i2sOk) return;
  i2s.setPins(pins::I2S_BCK, pins::I2S_WS, pins::I2S_DOUT);
  if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("[FAIL] I2S begin failed during tone");
    return;
  }
  static int16_t buf[256 * 2];
  float phase = 0;
  const float step = 2.0f * (float)M_PI * 440.0f / 44100.0f;
  for (int half = 0; half < 2; half++) {  // 0 = left channel, 1 = right channel
    for (int block = 0; block < 44100 / 256 / 2; block++) {
      for (int n = 0; n < 256; n++) {
        const int16_t v = (int16_t)(3000.0f * sinf(phase));
        phase += step;
        if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
        buf[n * 2] = half == 0 ? v : 0;
        buf[n * 2 + 1] = half == 1 ? v : 0;
      }
      i2s.write((uint8_t*)buf, sizeof(buf));
    }
  }
  i2s.end();
}

// ------------------------------------------------------------------ buttons

struct Btn {
  const char* name;
  int8_t pin;
};
constexpr Btn BTNS[] = {
    {"MENU", pins::BTN_MENU},       {"SEL", pins::BTN_SEL},      {"PLAY", pins::BTN_PLAY},
    {"SKIP", pins::BTN_SKIP},       {"UNSKIP", pins::BTN_UNSKIP}, {"LOUDER", pins::BTN_LOUDER},
    {"QUIETER", pins::BTN_QUIETER}, {"ONOFF", pins::BTN_ONOFF},  {"BOOT", pins::BTN_BOOT},
};
constexpr size_t NBTN = sizeof(BTNS) / sizeof(BTNS[0]);
int8_t lastState[NBTN];
bool everPressed[NBTN];

void drawButton(size_t i) {
  const int y = 20 + (int)i * 17;
  const bool down = lastState[i] == 1;
  const uint16_t bg = down ? TFT_GREEN : (everPressed[i] ? 0x03E0 : TFT_BLACK);
  tft.fillRect(DIAG_W + 4, y, W - DIAG_W - 4, 16, bg);
  tft.setTextColor(down || everPressed[i] ? TFT_BLACK : TFT_WHITE, bg);
  tft.drawString(String(BTNS[i].name) + (down ? " DOWN" : (everPressed[i] ? " ok" : "")), DIAG_W + 8, y, 2);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("mR3 OS hardware test");

  for (size_t i = 0; i < NBTN; i++) {
    pinMode(BTNS[i].pin, INPUT);  // external pull-ups on the PCB
    lastState[i] = -1;
    everPressed[i] = false;
  }

  ledcAttach(pins::TFT_BL, 5000, 8);
  ledcWrite(pins::TFT_BL, 0);
  tft.init();
  tft.setRotation(1);  // landscape 320x240, as on the PCB (use 3 if upside down)
  testDisplay();

  testMemory();
  testI2C();
  testSD();
  testAudioInit();

  report(failCount ? R::Fail : R::Pass, failCount ? String(failCount) + " check(s) FAILED" : String("all checks passed"));
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("PLAY = tone L then R", 2, H - 16, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Buttons:", DIAG_W + 8, 2, 2);
  for (size_t i = 0; i < NBTN; i++) drawButton(i);
}

void loop() {
  for (size_t i = 0; i < NBTN; i++) {
    const int8_t s = digitalRead(BTNS[i].pin) == LOW ? 1 : 0;
    if (s != lastState[i]) {
      lastState[i] = s;
      if (s) {
        everPressed[i] = true;
        Serial.printf("%s pressed\n", BTNS[i].name);
      }
      drawButton(i);
      if (s && BTNS[i].pin == pins::BTN_PLAY) playTone();
    }
  }
  delay(10);
}
