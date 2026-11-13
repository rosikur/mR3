#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <vector>
#include "AudioEngine.h"
#include "Input.h"
#include "Library.h"
#include "Power.h"
#include "Settings.h"

// iPod-inspired UI: header bar, list menus with black selection bar,
// Now Playing screen. Everything is drawn into a full-screen sprite and
// pushed to the display in one go (no flicker).
class UI {
 public:
  UI(Library& lib, AudioEngine& audio, Settings& set, Battery& bat, Input& in);

  void begin();
  bool ok() const { return spriteOk_; }
  void splash();
  void message(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr);
  void openMain();
  void startScan();  // background library scan; update() shows progress, then opens the menu
  void update();     // call every loop iteration

 private:
  enum class Screen : uint8_t { Menu, NowPlaying, About, Dev, Konami };
  enum class Kind : uint8_t { Main, TrackList, Artists, ArtistAlbums, Albums, Settings };

  struct Menu {
    Kind kind = Kind::Main;
    String title;
    std::vector<String> items;   // "Label" or "Label|Value"
    std::vector<int16_t> ids;    // payload per item
    int sel = 0;
    int top = 0;
    bool chevron = false;
  };

  // menus
  Menu buildMain();
  Menu buildSongs();
  Menu buildArtists();
  Menu buildArtistAlbums(int artistIdx);
  Menu buildAlbums();
  Menu buildAlbumTracks(int albumIdx);
  Menu buildSettings();
  void refreshSettings();
  Menu& cur() { return stack_.back(); }
  void push(Menu m);
  void back();
  void home();
  void move(int delta);
  void activate();
  void changeSetting(int id);
  void finishScan();
  void notify(const char* l1, const char* l2 = nullptr, uint32_t ms = 3000);
  bool saveSettings();
  void markSettingsDirty();

  // input
  void handle(const InputEvent& e);
  bool konami(const InputEvent& e);
  void adjustVolume(int delta);
  void screenOffNow();
  void wake();
  void powerOff();

  // playback control
  const Track* currentTrack() const;
  void startQueue(const std::vector<int16_t>& ids, int pos);
  void shuffleAll();
  void playCurrent();
  void next(bool fromEnd);
  void prev();
  void togglePlay();
  void onTrackEnded();

  // drawing
  void render();
  void drawHeader(const String& title);
  void drawBattery(int x, int y);
  void drawMenu();
  void drawNowPlaying();
  void drawAbout();
  void drawDev();
  void drawKonami();
  void drawToast();
  void drawNote();
  void setBacklight(uint8_t pct);
  uint16_t grey(uint8_t v);
  String fit(const String& s, int maxW, int font);
  static String clean(const char* s);

  Library& lib_;
  AudioEngine& audio_;
  Settings& set_;
  Battery& bat_;
  Input& in_;

  TFT_eSPI tft_;
  TFT_eSprite g_;
  bool spriteOk_ = false;

  std::vector<Menu> stack_;
  Screen screen_ = Screen::Menu;

  std::vector<int16_t> queue_;
  int qpos_ = -1;
  int fails_ = 0;
  uint32_t seenEnded_ = 0;

  bool dirty_ = true;
  bool screenOff_ = false;
  bool swallowing_ = false;
  Btn swallowBtn_ = Btn::Menu;
  uint32_t lastActivity_ = 0;
  uint32_t lastDraw_ = 0;
  uint32_t toastUntil_ = 0;
  uint32_t nextSaveTry_ = 0;
  bool settingsDirty_ = false;
  bool saveFailShown_ = false;
  bool scanActive_ = false;
  uint32_t lastScanDraw_ = 0;
  uint32_t seenDropped_ = 0;
  char note1_[48] = {0};
  char note2_[48] = {0};
  uint32_t noteUntil_ = 0;
  uint8_t konamiIdx_ = 0;
  uint8_t aboutTaps_ = 0;

  uint32_t lastPos_ = 0xFFFFFFFF;
  uint32_t lastDur_ = 0xFFFFFFFF;
  PlayState lastState_ = PlayState::Stopped;
  int lastBat_ = -1;
};
