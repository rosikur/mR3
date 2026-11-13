// mR3 OS - main entry. Boot sequence only; all logic lives in the modules.
#include <Arduino.h>
#include <SD_MMC.h>
#include "AudioEngine.h"
#include "Config.h"
#include "Input.h"
#include "Library.h"
#include "Power.h"
#include "Settings.h"
#include "UI.h"

using namespace mr3;

Library library;
AudioEngine audioEngine;
Settings settings;
Battery battery;
Input input;
UI ui(library, audioEngine, settings, battery, input);

static void halt(const char* l1, const char* l2) {
  Serial.printf("FATAL: %s %s\n", l1, l2);
  ui.message(l1, l2);
  while (true) delay(1000);
}

void setup() {
  Serial.begin(115200);
  settings.load();
  input.begin();
  battery.begin();

  ui.begin();
  if (!ui.ok()) {
    Serial.println("FATAL: could not allocate display buffer");
    while (true) delay(1000);
  }
  ui.splash();

  // The audio library needs PSRAM for its buffers.
  if (!psramFound()) halt("PSRAM not found", "Set memory_type in platformio.ini");

  // microSD, SDMMC 1-bit. Retry until a card shows up.
  // maxOpenFiles 8: the audio file, the scan's directory handle and headroom.
  SD_MMC.setPins(pins::SD_CLK, pins::SD_CMD, pins::SD_D0);
  while (!SD_MMC.begin("/sdcard", true, false, 40000, 8)) {
    ui.message("No SD card", "Insert a FAT32 card");
    delay(1000);
  }

  if (!audioEngine.begin(settings.volume)) halt("Audio init failed", "Could not start task");
  const uint32_t t0 = millis();
  while (!audioEngine.ready() && !audioEngine.failed() && millis() - t0 < 3000) delay(10);
  if (!audioEngine.ready())
    halt("Audio init failed", audioEngine.failed() ? "I2S or memory error" : "Audio task timeout");

  // Returns immediately. loop() shows the progress and opens the menu when done.
  ui.startScan();
}

void loop() {
  ui.update();
  delay(2);
}
