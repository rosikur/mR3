#pragma once
#include <stdint.h>

// mR3 hardware configuration. Source: mp3.kicad_sch (ESP32-S3-WROOM-1).
// Change pins here only.

namespace mr3 {

// Firmware identity (used by the About screen)
constexpr const char* FW_NAME     = "mR3 OS";
constexpr const char* FW_VERSION  = "0.2.0";
constexpr const char* FW_CODENAME = "Snickerdoodle";

// Audio volume steps used by ESP32-audioI2S (0..VOLUME_STEPS)
constexpr int VOLUME_STEPS = 21;

// BQ25895 charger I2C address (7-bit)
constexpr uint8_t BQ_ADDR = 0x6A;

namespace pins {

// Display (J3, 4-wire SPI, no MISO, no touch). Controller assumed ILI9341 240x320.
// TFT_eSPI takes its pins from the -DTFT_* build flags in platformio.ini. Those are
// preprocessor macros, so the constants here must NOT be called TFT_SCLK etc. (the macro
// would turn "constexpr int8_t TFT_SCLK = 42;" into "constexpr int8_t 42 = 42;").
// The static_asserts at the end of this file keep both copies in sync.
constexpr int8_t DISP_SCLK = 42;
constexpr int8_t DISP_MOSI = 41;
constexpr int8_t DISP_DC   = 39;
constexpr int8_t DISP_CS   = 38;
constexpr int8_t DISP_RST  = 40;
constexpr int8_t TFT_BL    = 48;  // backlight PWM is driven by the firmware, not by TFT_eSPI

// microSD (J1), SDMMC 1-bit. DAT3 is only pulled up, DAT1/DAT2 unconnected.
constexpr int8_t SD_CLK = 18;
constexpr int8_t SD_CMD = 6;
constexpr int8_t SD_D0  = 5;

// PCM5102A DAC (U3). Note: schematic net labels say "GPIO 14/15/16", real pins below.
constexpr int8_t I2S_BCK  = 17;  // U3.13 BCK
constexpr int8_t I2S_WS   = 15;  // U3.15 LRCK
constexpr int8_t I2S_DOUT = 16;  // U3.14 DIN

// BQ25895 charger (U5), I2C address 0x6A. INT/QON not connected.
constexpr int8_t BQ_SDA = 14;
constexpr int8_t BQ_SCL = 13;

// Buttons: active low, external 10k pull-ups to 3V3.
constexpr int8_t BTN_MENU    = 10;
constexpr int8_t BTN_SEL     = 11;
constexpr int8_t BTN_PLAY    = 12;
constexpr int8_t BTN_SKIP    = 9;   // next
constexpr int8_t BTN_UNSKIP  = 8;   // previous
constexpr int8_t BTN_LOUDER  = 2;
constexpr int8_t BTN_QUIETER = 1;
constexpr int8_t BTN_ONOFF   = 4;
constexpr int8_t BTN_BOOT    = 0;

} // namespace pins
} // namespace mr3

// Fail the build if platformio.ini and this file ever disagree about the display wiring.
#if defined(TFT_SCLK)
static_assert(TFT_SCLK == mr3::pins::DISP_SCLK, "TFT_SCLK in platformio.ini differs from Config.h");
#endif
#if defined(TFT_MOSI)
static_assert(TFT_MOSI == mr3::pins::DISP_MOSI, "TFT_MOSI in platformio.ini differs from Config.h");
#endif
#if defined(TFT_DC)
static_assert(TFT_DC == mr3::pins::DISP_DC, "TFT_DC in platformio.ini differs from Config.h");
#endif
#if defined(TFT_CS)
static_assert(TFT_CS == mr3::pins::DISP_CS, "TFT_CS in platformio.ini differs from Config.h");
#endif
#if defined(TFT_RST)
static_assert(TFT_RST == mr3::pins::DISP_RST, "TFT_RST in platformio.ini differs from Config.h");
#endif