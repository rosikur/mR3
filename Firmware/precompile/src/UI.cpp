#include "UI.h"
#include <SD_MMC.h>
#include <driver/gpio.h>
#include <esp_random.h>
#include <esp_sleep.h>
#include <utility>
#include "Config.h"

using namespace mr3;

namespace {
constexpr uint8_t ROTATION = 1;  // landscape 320x240 (use 3 if the picture is upside down)
constexpr int W = 320;
constexpr int H = 240;
constexpr int HEADER_H = 30;
constexpr int ROW_H = 26;
constexpr int ROWS = 8;

const char* const BOOT_MSGS[] = {
  "Cookies: baked.", "Still not an iPod.", "Bass: loading...",
  "No wheel? No problem.", "It just plays.", "Warming up the DAC.",
};
constexpr int NBOOT = sizeof(BOOT_MSGS) / sizeof(BOOT_MSGS[0]);

const Btn KONAMI[10] = {Btn::Louder, Btn::Louder, Btn::Quieter, Btn::Quieter, Btn::Unskip,
                        Btn::Skip,   Btn::Unskip, Btn::Skip,    Btn::Menu,    Btn::Sel};

const char* const REPEAT_NAMES[] = {"Off", "All", "One"};

String fmtTime(uint32_t s) {
  char b[12];
  snprintf(b, sizeof(b), "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
  return String(b);
}
}  // namespace

UI::UI(Library& lib, AudioEngine& audio, Settings& set, Battery& bat, Input& in)
    : lib_(lib), audio_(audio), set_(set), bat_(bat), in_(in), tft_(), g_(&tft_) {}

// ---------------------------------------------------------------- setup

void UI::begin() {
  ledcAttach(pins::TFT_BL, 5000, 8);
  setBacklight(0);
  tft_.init();
  tft_.setRotation(ROTATION);  // landscape 320x240
  tft_.fillScreen(TFT_WHITE);

  g_.setColorDepth(16);
  spriteOk_ = (g_.createSprite(W, H) != nullptr);
  if (!spriteOk_) {
    tft_.fillScreen(TFT_RED);
    tft_.setTextColor(TFT_WHITE);
    tft_.drawString("Sprite alloc failed", 10, 10, 2);
    setBacklight(80);
    return;
  }
  setBacklight(set_.brightnessPct());
  lastActivity_ = millis();
}

uint16_t UI::grey(uint8_t v) { return tft_.color565(v, v, v); }

void UI::setBacklight(uint8_t pct) {
  ledcWrite(pins::TFT_BL, (uint32_t)pct * 255 / 100);
}

String UI::clean(const char* s) {
  // The built-in fonts are ASCII only: replace each UTF-8 character by '?'.
  String out;
  for (const uint8_t* p = (const uint8_t*)s; *p; ++p) {
    if (*p < 0x80) out += (char)*p;
    else if (*p >= 0xC0) out += '?';
  }
  return out;
}

String UI::fit(const String& s, int maxW, int font) {
  if (g_.textWidth(s, font) <= maxW) return s;
  String t = s;
  while (t.length() > 1 && g_.textWidth(t + "...", font) > maxW) t.remove(t.length() - 1);
  return t + "...";
}

void UI::splash() {
  g_.fillSprite(TFT_WHITE);
  g_.setTextColor(TFT_BLACK);
  g_.setTextDatum(MC_DATUM);
  g_.setTextSize(3);
  g_.drawString("mR3", W / 2, 85, 4);
  g_.setTextSize(1);
  g_.drawString("OS", W / 2, 140, 4);
  const char* msg = (esp_random() % 3 == 0) ? BOOT_MSGS[esp_random() % NBOOT] : "Welcome";
  g_.setTextColor(grey(120));
  g_.drawString(msg, W / 2, 205, 2);
  g_.pushSprite(0, 0);
  delay(1200);
}

void UI::message(const char* l1, const char* l2, const char* l3) {
  if (!spriteOk_) return;
  g_.fillSprite(TFT_WHITE);
  g_.setTextDatum(MC_DATUM);
  g_.setTextColor(TFT_BLACK);
  g_.drawString(l1, W / 2, H / 2 - 30, 4);
  g_.setTextColor(grey(110));
  if (l2) g_.drawString(l2, W / 2, H / 2 + 10, 2);
  if (l3) g_.drawString(l3, W / 2, H / 2 + 32, 2);
  g_.pushSprite(0, 0);
}

void UI::openMain() {
  stack_.clear();
  stack_.push_back(buildMain());
  screen_ = Screen::Menu;
  seenEnded_ = audio_.endedCount();
  dirty_ = true;
}

// ---------------------------------------------------------------- menus

UI::Menu UI::buildMain() {
  Menu m;
  m.kind = Kind::Main;
  m.title = "mR3";
  m.chevron = true;
  const char* names[] = {"Now Playing", "Songs", "Artists", "Albums", "Shuffle Songs", "Settings", "About mR3"};
  for (int i = 0; i < 7; i++) {
    m.items.push_back(names[i]);
    m.ids.push_back(i);
  }
  return m;
}

UI::Menu UI::buildSongs() {
  Menu m;
  m.kind = Kind::TrackList;
  m.title = "Songs";
  for (int16_t i : lib_.songs()) {
    m.items.push_back(clean(lib_.track(i).title));
    m.ids.push_back(i);
  }
  if (m.items.empty()) {
    m.items.push_back("No music found");
    m.ids.push_back(-1);
  }
  return m;
}

UI::Menu UI::buildArtists() {
  Menu m;
  m.kind = Kind::Artists;
  m.title = "Artists";
  m.chevron = true;
  for (int i = 0; i < (int)lib_.artists().size(); i++) {
    m.items.push_back(clean(lib_.artists()[i].c_str()));
    m.ids.push_back(i);
  }
  if (m.items.empty()) {
    m.items.push_back("No music found");
    m.ids.push_back(-1);
  }
  return m;
}

UI::Menu UI::buildArtistAlbums(int artistIdx) {
  Menu m;
  m.kind = Kind::ArtistAlbums;
  m.title = clean(lib_.artists()[artistIdx].c_str());
  m.chevron = true;
  std::vector<int16_t> al;
  lib_.albumsOfArtist(artistIdx, al);
  for (int16_t a : al) {
    m.items.push_back(clean(lib_.albums()[a].name.c_str()));
    m.ids.push_back(a);
  }
  if (m.items.empty()) {
    m.items.push_back("No albums");
    m.ids.push_back(-1);
  }
  return m;
}

UI::Menu UI::buildAlbums() {
  Menu m;
  m.kind = Kind::Albums;
  m.title = "Albums";
  m.chevron = true;
  for (int i = 0; i < (int)lib_.albums().size(); i++) {
    m.items.push_back(clean(lib_.albums()[i].name.c_str()));
    m.ids.push_back(i);
  }
  if (m.items.empty()) {
    m.items.push_back("No music found");
    m.ids.push_back(-1);
  }
  return m;
}

UI::Menu UI::buildAlbumTracks(int albumIdx) {
  Menu m;
  m.kind = Kind::TrackList;
  m.title = clean(lib_.albums()[albumIdx].name.c_str());
  std::vector<int16_t> tr;
  lib_.tracksOfAlbum(albumIdx, tr);
  for (int16_t t : tr) {
    m.items.push_back(clean(lib_.track(t).title));
    m.ids.push_back(t);
  }
  if (m.items.empty()) {
    m.items.push_back("No songs");
    m.ids.push_back(-1);
  }
  return m;
}

UI::Menu UI::buildSettings() {
  Menu m;
  m.kind = Kind::Settings;
  m.title = "Settings";
  auto add = [&](const String& s, int id) {
    m.items.push_back(s);
    m.ids.push_back((int16_t)id);
  };
  add("Brightness|" + String(set_.brightnessPct()) + "%", 0);
  add(String("Shuffle|") + (set_.shuffle ? "On" : "Off"), 1);
  add(String("Repeat|") + REPEAT_NAMES[set_.repeat], 2);
  add("Screen Off|" + (set_.timeoutSec() ? String(set_.timeoutSec()) + "s" : String("Never")), 3);
  add("Rescan Library", 4);
  add("Tracks|" + String(lib_.count()), 5);
  add("SD Card|" + String((uint32_t)(SD_MMC.cardSize() / (1024ULL * 1024ULL))) + " MB", 6);
  add(String("Version|") + FW_VERSION, 7);
  return m;
}

void UI::refreshSettings() {
  Menu nm = buildSettings();
  nm.sel = cur().sel;
  nm.top = cur().top;
  cur() = nm;
}

void UI::push(Menu m) {
  stack_.push_back(std::move(m));
  dirty_ = true;
}

void UI::back() {
  if (stack_.size() > 1) stack_.pop_back();
  dirty_ = true;
}

void UI::home() {
  while (stack_.size() > 1) stack_.pop_back();
  screen_ = Screen::Menu;
  dirty_ = true;
}

void UI::move(int delta) {
  Menu& m = cur();
  int n = (int)m.items.size();
  if (n == 0) return;
  m.sel += delta;
  if (m.sel < 0) m.sel = 0;
  if (m.sel >= n) m.sel = n - 1;
  if (m.sel < m.top) m.top = m.sel;
  if (m.sel >= m.top + ROWS) m.top = m.sel - ROWS + 1;
  dirty_ = true;
}

void UI::activate() {
  Menu& m = cur();
  if (m.items.empty()) return;
  const int id = m.ids[m.sel];

  switch (m.kind) {
    case Kind::Main:
      switch (id) {
        case 0: screen_ = Screen::NowPlaying; break;
        case 1: push(buildSongs()); break;
        case 2: push(buildArtists()); break;
        case 3: push(buildAlbums()); break;
        case 4: shuffleAll(); break;
        case 5: push(buildSettings()); break;
        case 6: aboutTaps_ = 0; screen_ = Screen::About; break;
      }
      break;
    case Kind::TrackList:
      if (id >= 0) {
        std::vector<int16_t> q(m.ids.begin(), m.ids.end());
        startQueue(q, m.sel);
      }
      break;
    case Kind::Artists:
      if (id >= 0) push(buildArtistAlbums(id));
      break;
    case Kind::ArtistAlbums:
    case Kind::Albums:
      if (id >= 0) push(buildAlbumTracks(id));
      break;
    case Kind::Settings:
      changeSetting(id);
      break;
  }
  dirty_ = true;
}

void UI::changeSetting(int id) {
  switch (id) {
    case 0:
      set_.cycleBrightness();
      setBacklight(set_.brightnessPct());
      break;
    case 1: set_.shuffle = !set_.shuffle; break;
    case 2: set_.repeat = (set_.repeat + 1) % 3; break;
    case 3: set_.cycleTimeout(); break;
    case 4:
      startScan();  // runs in a background task; update() shows the progress
      return;
    default: return;  // read-only rows
  }
  saveSettings();
  refreshSettings();
}

// ---------------------------------------------------------------- playback

const Track* UI::currentTrack() const {
  if (qpos_ < 0 || qpos_ >= (int)queue_.size()) return nullptr;
  int idx = queue_[qpos_];
  if (idx < 0 || idx >= lib_.count()) return nullptr;
  return &lib_.track(idx);
}

void UI::startQueue(const std::vector<int16_t>& ids, int pos) {
  queue_ = ids;
  qpos_ = pos;
  fails_ = 0;
  playCurrent();
  screen_ = Screen::NowPlaying;
}

void UI::shuffleAll() {
  const auto& s = lib_.songs();
  if (s.empty()) return;
  queue_.assign(s.begin(), s.end());
  set_.shuffle = true;
  saveSettings();
  qpos_ = esp_random() % queue_.size();
  fails_ = 0;
  playCurrent();
  screen_ = Screen::NowPlaying;
}

void UI::playCurrent() {
  const Track* t = currentTrack();
  if (!t) return;
  if (!audio_.play(t->path)) notify("Audio busy", "Command not accepted - try again");
  lastPos_ = 0xFFFFFFFF;
  dirty_ = true;
}

void UI::next(bool fromEnd) {
  if (queue_.empty()) return;
  const int n = (int)queue_.size();
  if (set_.repeat == 2 && fromEnd) {
    playCurrent();
    return;
  }
  if (set_.shuffle && n > 1) {
    int r;
    do { r = esp_random() % n; } while (r == qpos_);
    qpos_ = r;
  } else if (qpos_ + 1 < n) {
    qpos_++;
  } else if (set_.repeat == 1 || !fromEnd) {
    qpos_ = 0;
  } else {
    audio_.stop();
    dirty_ = true;
    return;
  }
  playCurrent();
}

void UI::prev() {
  if (queue_.empty()) return;
  if (audio_.position() > 3 || qpos_ <= 0) {
    playCurrent();  // restart current track
    return;
  }
  qpos_--;
  playCurrent();
}

void UI::togglePlay() {
  if (audio_.state() == PlayState::Stopped) {
    if (currentTrack()) playCurrent();
  } else {
    audio_.togglePause();
  }
  dirty_ = true;
}

void UI::onTrackEnded() {
  if (queue_.empty()) return;
  if (audio_.lastEndFailed()) {
    const EndReason r = audio_.lastEndReason();
    const Track* t = currentTrack();
    Serial.printf("[ui] playback failure: %s (%s)\n", AudioEngine::reasonText(r), t ? t->path : "?");
    notify(AudioEngine::reasonText(r), t ? t->title : nullptr);
    fails_++;
    if (fails_ >= (int)queue_.size()) {  // every track failed -> stop instead of looping forever
      fails_ = 0;
      notify("No playable tracks", "Check the files on the card", 4000);
      return;
    }
    // skip the broken file, even in "repeat one"
    const uint8_t savedRepeat = set_.repeat;
    if (set_.repeat == 2) set_.repeat = 0;
    next(true);
    set_.repeat = savedRepeat;
    return;
  }
  next(true);
}

// ---------------------------------------------------------------- input

bool UI::konami(const InputEvent& e) {
  const bool vol = (e.btn == Btn::Louder || e.btn == Btn::Quieter);
  if (e.type != (vol ? EvType::Press : EvType::Click)) return false;
  if (e.btn == KONAMI[konamiIdx_]) {
    konamiIdx_++;
    if (konamiIdx_ >= 10) {
      konamiIdx_ = 0;
      screen_ = Screen::Konami;
      dirty_ = true;
      return true;
    }
  } else {
    konamiIdx_ = (e.btn == KONAMI[0]) ? 1 : 0;
  }
  return false;
}

void UI::adjustVolume(int delta) {
  int v = (int)set_.volume + delta;
  if (v < 0) v = 0;
  if (v > VOLUME_STEPS) v = VOLUME_STEPS;
  if (v == set_.volume) {
    toastUntil_ = millis() + 1200;
    dirty_ = true;
    return;
  }
  set_.volume = (uint8_t)v;
  audio_.setVolume(set_.volume);
  markSettingsDirty();
  toastUntil_ = millis() + 1200;
  dirty_ = true;
}

void UI::screenOffNow() {
  screenOff_ = true;
  setBacklight(0);
}

void UI::wake() {
  screenOff_ = false;
  setBacklight(set_.brightnessPct());
  dirty_ = true;
}

void UI::powerOff() {
  audio_.stop();
  saveSettings();
  message("Goodbye");
  delay(700);
  setBacklight(0);
  while (digitalRead(pins::BTN_ONOFF) == LOW) delay(10);  // wait for release, else we wake instantly
  tft_.writecommand(0x10);                                // display sleep
  esp_sleep_enable_ext0_wakeup((gpio_num_t)pins::BTN_ONOFF, 0);
  esp_deep_sleep_start();
}

void UI::handle(const InputEvent& e) {
  if (konami(e)) return;

  if (e.type == EvType::Chord) {
    screen_ = (screen_ == Screen::Dev) ? Screen::Menu : Screen::Dev;
    dirty_ = true;
    return;
  }
  if (e.btn == Btn::OnOff) {
    if (e.type == EvType::Click) screenOffNow();
    else if (e.type == EvType::Long) powerOff();
    return;
  }
  if (e.btn == Btn::Louder || e.btn == Btn::Quieter) {
    if (e.type == EvType::Press || e.type == EvType::Repeat)
      adjustVolume(e.btn == Btn::Louder ? 1 : -1);
    return;
  }
  if (e.btn == Btn::Play) {
    if (e.type == EvType::Click) togglePlay();
    return;
  }
  if (e.type == EvType::Press || e.type == EvType::Release) return;

  switch (screen_) {
    case Screen::Menu:
      if (e.btn == Btn::Unskip && (e.type == EvType::Click || e.type == EvType::Repeat)) move(-1);
      else if (e.btn == Btn::Skip && (e.type == EvType::Click || e.type == EvType::Repeat)) move(1);
      else if (e.btn == Btn::Sel && e.type == EvType::Click) activate();
      else if (e.btn == Btn::Menu && e.type == EvType::Click) back();
      else if (e.btn == Btn::Menu && e.type == EvType::Long) home();
      break;

    case Screen::NowPlaying:
      if (e.btn == Btn::Skip && e.type == EvType::Click) next(false);
      else if (e.btn == Btn::Unskip && e.type == EvType::Click) prev();
      else if (e.btn == Btn::Skip && e.type == EvType::Repeat) audio_.seekRelative(4);
      else if (e.btn == Btn::Unskip && e.type == EvType::Repeat) audio_.seekRelative(-4);
      else if (e.btn == Btn::Sel && e.type == EvType::Click) {
        set_.repeat = (set_.repeat + 1) % 3;
        saveSettings();
      } else if (e.btn == Btn::Sel && e.type == EvType::Long) {
        set_.shuffle = !set_.shuffle;
        saveSettings();
      } else if (e.btn == Btn::Menu && e.type == EvType::Click) screen_ = Screen::Menu;
      else if (e.btn == Btn::Menu && e.type == EvType::Long) home();
      dirty_ = true;
      break;

    case Screen::About:
      if (e.btn == Btn::Sel && e.type == EvType::Click) {
        if (++aboutTaps_ >= 5) {  // hidden: developer mode
          aboutTaps_ = 0;
          screen_ = Screen::Dev;
        }
      } else if (e.btn == Btn::Menu && e.type == EvType::Click) screen_ = Screen::Menu;
      dirty_ = true;
      break;

    case Screen::Dev:
    case Screen::Konami:
      if (e.btn == Btn::Menu && e.type == EvType::Click) screen_ = Screen::Menu;
      dirty_ = true;
      break;
  }
}

// ---------------------------------------------------------------- update loop

void UI::update() {
  const uint32_t now = millis();
  bat_.update();

  InputEvent e;
  while (in_.poll(e)) {
    lastActivity_ = now;
    if (scanActive_) {  // the scan task owns the library: ignore input except power-off
      if (e.btn == Btn::OnOff && e.type == EvType::Long) powerOff();
      continue;
    }
    if (screenOff_) {
      if (e.type == EvType::Press) {
        wake();
        swallowing_ = true;
        swallowBtn_ = e.btn;
      }
      continue;
    }
    if (swallowing_ && e.btn == swallowBtn_) {
      if (e.type == EvType::Release) swallowing_ = false;
      continue;
    }
    handle(e);
  }

  // library scan running in the background: show progress, touch nothing else
  if (scanActive_) {
    if (lib_.scanning()) {
      if (!screenOff_ && now - lastScanDraw_ >= 200) {
        lastScanDraw_ = now;
        char b[24];
        snprintf(b, sizeof(b), "%d tracks", lib_.scanProgress());
        message("Scanning music...", b);
      }
      return;
    }
    finishScan();
  }

  // audio task could not accept a command
  const uint32_t dropped = audio_.droppedCommands();
  if (dropped != seenDropped_) {
    seenDropped_ = dropped;
    notify("Audio busy", "A command was dropped");
  }

  // track finished or failed -> advance
  const uint32_t ended = audio_.endedCount();
  if (ended != seenEnded_) {
    seenEnded_ = ended;
    onTrackEnded();
  }
  if (audio_.state() == PlayState::Playing && audio_.position() >= 1) fails_ = 0;

  // screen off after inactivity
  const uint16_t tmo = set_.timeoutSec();
  if (!screenOff_ && tmo > 0 && now - lastActivity_ > (uint32_t)tmo * 1000UL) screenOffNow();

  // debounced flash writes (volume changes); failed writes are retried every 10 s
  if (settingsDirty_ && (int32_t)(now - nextSaveTry_) >= 0) saveSettings();

  if (screenOff_ || !spriteOk_) return;

  // decide whether a redraw is needed
  if (toastUntil_ && now > toastUntil_) {
    toastUntil_ = 0;
    dirty_ = true;
  }
  if (noteUntil_ && now > noteUntil_) {
    noteUntil_ = 0;
    dirty_ = true;
  }
  if (audio_.position() != lastPos_ || audio_.duration() != lastDur_ || audio_.state() != lastState_)
    dirty_ = true;
  if (bat_.percent() != lastBat_) dirty_ = true;
  if (screen_ == Screen::Dev && now - lastDraw_ >= 500) dirty_ = true;
  if (screen_ == Screen::Konami && now - lastDraw_ >= 40) dirty_ = true;

  if (dirty_) render();
}

// ---------------------------------------------------------------- drawing

void UI::render() {
  switch (screen_) {
    case Screen::Menu: drawMenu(); break;
    case Screen::NowPlaying: drawNowPlaying(); break;
    case Screen::About: drawAbout(); break;
    case Screen::Dev: drawDev(); break;
    case Screen::Konami: drawKonami(); break;
  }
  if (toastUntil_ && screen_ != Screen::NowPlaying && screen_ != Screen::Konami) drawToast();
  if (noteUntil_ && screen_ != Screen::Konami) drawNote();
  g_.pushSprite(0, 0);

  dirty_ = false;
  lastDraw_ = millis();
  lastPos_ = audio_.position();
  lastDur_ = audio_.duration();
  lastState_ = audio_.state();
  lastBat_ = bat_.percent();
}

void UI::drawBattery(int x, int y) {
  g_.drawRect(x, y, 22, 11, TFT_BLACK);
  g_.fillRect(x + 22, y + 3, 2, 5, TFT_BLACK);
  if (!bat_.present()) {
    g_.setTextColor(TFT_BLACK);
    g_.setTextDatum(MC_DATUM);
    g_.drawString("?", x + 11, y + 6, 1);
    return;
  }
  int w = bat_.percent() * 18 / 100;
  if (w > 0) g_.fillRect(x + 2, y + 2, w, 7, TFT_BLACK);
  if (bat_.charging()) {  // small lightning bolt, drawn in the contrasting colour
    uint16_t c = (w > 9) ? TFT_WHITE : TFT_BLACK;
    g_.drawLine(x + 12, y + 2, x + 9, y + 6, c);
    g_.drawLine(x + 9, y + 6, x + 13, y + 6, c);
    g_.drawLine(x + 13, y + 6, x + 10, y + 9, c);
  }
}

void UI::drawHeader(const String& title) {
  for (int y = 0; y < HEADER_H; y++) {
    uint8_t v = 248 - (y * 48) / HEADER_H;
    g_.drawFastHLine(0, y, W, grey(v));
  }
  g_.drawFastHLine(0, HEADER_H - 1, W, grey(150));

  g_.setTextDatum(MC_DATUM);
  g_.setTextColor(TFT_BLACK);
  String t = fit(title, 210, 2);
  g_.drawString(t, W / 2, HEADER_H / 2 - 1, 2);
  g_.drawString(t, W / 2 + 1, HEADER_H / 2 - 1, 2);  // fake bold

  // play / pause indicator
  if (audio_.state() == PlayState::Playing) {
    g_.fillTriangle(10, 8, 10, 21, 21, 14, TFT_BLACK);
  } else if (audio_.state() == PlayState::Paused) {
    g_.fillRect(10, 8, 4, 13, TFT_BLACK);
    g_.fillRect(17, 8, 4, 13, TFT_BLACK);
  }
  drawBattery(W - 34, 9);
}

void UI::drawMenu() {
  Menu& m = cur();
  g_.fillSprite(TFT_WHITE);
  drawHeader(m.title);

  for (int r = 0; r < ROWS; r++) {
    const int idx = m.top + r;
    if (idx >= (int)m.items.size()) break;
    const int y = HEADER_H + r * ROW_H;
    const bool sel = (idx == m.sel);

    if (sel) g_.fillRect(0, y, W, ROW_H, TFT_BLACK);
    else g_.drawFastHLine(0, y + ROW_H - 1, W, grey(228));

    const String& raw = m.items[idx];
    const int bar = raw.indexOf('|');
    const String label = (bar >= 0) ? raw.substring(0, bar) : raw;
    const String value = (bar >= 0) ? raw.substring(bar + 1) : String("");

    g_.setTextColor(sel ? TFT_WHITE : TFT_BLACK);
    g_.setTextDatum(TL_DATUM);
    const int maxW = (bar >= 0) ? 190 : (m.chevron ? W - 40 : W - 28);
    g_.drawString(fit(label, maxW, 2), 10, y + 6, 2);

    if (bar >= 0) {
      g_.setTextDatum(TR_DATUM);
      g_.setTextColor(sel ? grey(200) : grey(110));
      g_.drawString(value, W - 12, y + 6, 2);
    }
    if (m.chevron) {  // ">" chevron
      const uint16_t c = sel ? TFT_WHITE : grey(150);
      const int cx = W - 18;
      const int cy = y + ROW_H / 2;
      g_.drawLine(cx, cy - 5, cx + 5, cy, c);
      g_.drawLine(cx + 5, cy, cx, cy + 5, c);
      g_.drawLine(cx + 1, cy - 5, cx + 6, cy, c);
      g_.drawLine(cx + 6, cy, cx + 1, cy + 5, c);
    }
  }

  const int n = (int)m.items.size();
  if (n > ROWS) {  // scroll bar
    const int trackH = ROWS * ROW_H;
    const int thumbH = max(14, trackH * ROWS / n);
    const int thumbY = HEADER_H + (trackH - thumbH) * m.top / (n - ROWS);
    g_.fillRoundRect(W - 5, thumbY, 3, thumbH, 1, grey(120));
  }
}

void UI::drawNowPlaying() {
  g_.fillSprite(TFT_WHITE);
  drawHeader("Now Playing");

  // album art placeholder (artwork support comes with tag reading)
  g_.fillRoundRect(12, 42, 112, 112, 8, grey(205));
  const uint16_t nc = grey(85);
  g_.fillCircle(52, 126, 11, nc);
  g_.fillRect(60, 74, 5, 52, nc);
  g_.fillTriangle(65, 74, 89, 90, 65, 98, nc);

  const int tx = 136;
  const int tw = W - tx - 12;
  const Track* t = currentTrack();
  g_.setTextDatum(TL_DATUM);
  if (t) {
    String title = fit(clean(t->title), tw, 2);
    g_.setTextColor(TFT_BLACK);
    g_.drawString(title, tx, 46, 2);
    g_.drawString(title, tx + 1, 46, 2);  // fake bold
    g_.setTextColor(grey(60));
    g_.drawString(fit(clean(t->artist), tw, 2), tx, 70, 2);
    g_.setTextColor(grey(120));
    g_.drawString(fit(clean(t->album), tw, 2), tx, 90, 2);

    g_.setTextColor(grey(150));
    g_.drawString(String(qpos_ + 1) + " of " + String((int)queue_.size()), tx, 118, 2);
    String modes;
    if (set_.shuffle) modes += "Shuffle";
    if (set_.repeat == 1) modes += (modes.length() ? "  Repeat All" : "Repeat All");
    if (set_.repeat == 2) modes += (modes.length() ? "  Repeat One" : "Repeat One");
    if (modes.length()) g_.drawString(modes, tx, 136, 2);
  } else {
    g_.setTextColor(grey(120));
    g_.drawString("Nothing playing", tx, 70, 2);
  }

  // progress bar
  const uint32_t pos = audio_.position();
  const uint32_t dur = audio_.duration();
  const int bx = 14;
  const int bw = W - 28;
  g_.fillRoundRect(bx, 172, bw, 6, 3, grey(210));
  if (dur > 0) {
    int fw = (int)((uint64_t)pos * bw / dur);
    if (fw > bw) fw = bw;
    if (fw > 0) g_.fillRoundRect(bx, 172, fw < 6 ? 6 : fw, 6, 3, TFT_BLACK);
  }
  g_.setTextColor(grey(90));
  g_.setTextDatum(TL_DATUM);
  g_.drawString(fmtTime(pos), bx, 182, 2);
  g_.setTextDatum(TR_DATUM);
  if (dur > 0) g_.drawString("-" + fmtTime(dur > pos ? dur - pos : 0), W - 14, 182, 2);
  else g_.drawString("--:--", W - 14, 182, 2);

  // volume
  g_.fillRect(16, 212, 5, 8, TFT_BLACK);
  g_.fillTriangle(21, 212, 28, 207, 28, 225, TFT_BLACK);
  g_.fillTriangle(21, 212, 21, 220, 28, 225, TFT_BLACK);
  const int vx = 40;
  const int vwMax = W - 14 - vx;
  g_.fillRoundRect(vx, 213, vwMax, 6, 3, grey(210));
  const int vw = set_.volume * vwMax / VOLUME_STEPS;
  if (vw > 0) g_.fillRoundRect(vx, 213, vw < 6 ? 6 : vw, 6, 3, TFT_BLACK);
}

void UI::drawToast() {
  const int bx = (W - 200) / 2;
  const int by = H - 40;
  g_.fillRoundRect(bx, by, 200, 30, 8, TFT_BLACK);
  g_.fillRect(bx + 12, by + 12, 4, 7, TFT_WHITE);
  g_.fillTriangle(bx + 16, by + 12, bx + 22, by + 8, bx + 22, by + 23, TFT_WHITE);
  g_.fillTriangle(bx + 16, by + 12, bx + 16, by + 19, bx + 22, by + 23, TFT_WHITE);
  g_.drawRoundRect(bx + 34, by + 12, 150, 6, 3, TFT_WHITE);
  const int vw = set_.volume * 150 / VOLUME_STEPS;
  if (vw > 0) g_.fillRoundRect(bx + 34, by + 12, vw < 6 ? 6 : vw, 6, 3, TFT_WHITE);
}

void UI::drawAbout() {
  g_.fillSprite(TFT_WHITE);
  drawHeader("About mR3");
  g_.setTextDatum(TC_DATUM);
  g_.setTextColor(TFT_BLACK);
  g_.setTextSize(2);
  g_.drawString("mR3", W / 2, 36, 4);
  g_.setTextSize(1);
  g_.setTextColor(grey(80));
  g_.drawString(String(FW_NAME) + " " + FW_VERSION, W / 2, 98, 2);
  g_.drawString(String("Codename: ") + FW_CODENAME, W / 2, 116, 2);
  g_.drawString("Built with too much coffee", W / 2, 134, 2);
  g_.drawString("and a tiny bit of cookie.", W / 2, 150, 2);
  g_.setTextColor(grey(130));
  g_.drawString(String(lib_.count()) + " songs on the card", W / 2, 190, 2);
  g_.drawString("ESP32-S3 + PCM5102A", W / 2, 210, 2);
}

void UI::drawDev() {
  g_.fillSprite(TFT_BLACK);
  g_.setTextColor(TFT_GREEN);
  g_.setTextDatum(TL_DATUM);
  char b[48];
  int y = 6;
  auto line = [&](const char* s) {
    g_.drawString(s, 6, y, 2);
    y += 18;
  };
  snprintf(b, sizeof(b), "%s v%s (%s)", FW_NAME, FW_VERSION, FW_CODENAME);
  line(b);
  const uint32_t up = millis() / 1000;
  snprintf(b, sizeof(b), "Uptime: %uh %02um %02us", (unsigned)(up / 3600), (unsigned)(up / 60 % 60), (unsigned)(up % 60));
  line(b);
  snprintf(b, sizeof(b), "Heap: %u / %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getHeapSize() / 1024));
  line(b);
  snprintf(b, sizeof(b), "PSRAM: %u / %u KB", (unsigned)(ESP.getFreePsram() / 1024), (unsigned)(ESP.getPsramSize() / 1024));
  line(b);
  snprintf(b, sizeof(b), "CPU: %s @ %u MHz", ESP.getChipModel(), (unsigned)ESP.getCpuFreqMHz());
  line(b);
  snprintf(b, sizeof(b), "Flash: %u MB", (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));
  line(b);
  snprintf(b, sizeof(b), "SD: %u MB, %d tracks", (unsigned)(SD_MMC.cardSize() / (1024ULL * 1024ULL)), lib_.count());
  line(b);
  const char* st = audio_.state() == PlayState::Playing ? "playing" : (audio_.state() == PlayState::Paused ? "paused" : "stopped");
  snprintf(b, sizeof(b), "Audio: %s, vol %d/%d", st, set_.volume, VOLUME_STEPS);
  line(b);
  line("Dec: mp3 aac m4a flac wav ogg opus");
  snprintf(b, sizeof(b), "Drops: cmd %u in %u, dec err %u", (unsigned)audio_.droppedCommands(),
           (unsigned)in_.droppedEvents(), (unsigned)audio_.decoderErrorLogs());
  line(b);
  if (bat_.present()) snprintf(b, sizeof(b), "Battery: %u mV %s", bat_.millivolts(), bat_.charging() ? "(charging)" : "");
  else snprintf(b, sizeof(b), "Battery: no BQ25895 reply");
  line(b);
  g_.setTextColor(grey(120));
  g_.drawString("MENU = back", 6, H - 20, 2);
}

void UI::drawKonami() {
  static int x = 40, y = 100, dx = 3, dy = 2;
  static const uint16_t cols[6] = {TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN, TFT_CYAN, TFT_MAGENTA};
  g_.fillSprite(TFT_BLACK);
  x += dx;
  y += dy;
  if (x < 0 || x > W - 90) dx = -dx;
  if (y < 50 || y > H - 110) dy = -dy;
  g_.setTextSize(2);
  g_.setTextColor(cols[(millis() / 200) % 6]);
  g_.setTextDatum(TL_DATUM);
  g_.drawString("mR3", x, y, 4);
  g_.setTextSize(1);
  g_.setTextColor(TFT_WHITE);
  g_.setTextDatum(TC_DATUM);
  g_.drawString("KONAMI CODE ACCEPTED", W / 2, 10, 2);
  g_.drawString("+30 lives", W / 2, 30, 2);
  g_.setTextColor(grey(120));
  g_.drawString("MENU = back", W / 2, H - 22, 2);
}

// ---------------------------------------------------------------- scan / notices / settings

void UI::startScan() {
  message("Scanning music...");
  audio_.stop();
  queue_.clear();
  qpos_ = -1;
  stack_.clear();  // menus hold indices into the old library
  if (!lib_.startScan()) {
    Serial.println("[ui] library scan could not start");
    openMain();  // the old library data is still valid
    notify("Scan failed", "Could not start the scan task");
    return;
  }
  scanActive_ = true;
  lastScanDraw_ = millis();
}

void UI::finishScan() {
  scanActive_ = false;
  openMain();
  if (lib_.count() == 0) notify("No music found", "Copy audio files to the SD card", 5000);
  else if (lib_.truncated()) notify("Library truncated", "Too many tracks for free memory", 5000);
  lastActivity_ = millis();
}

void UI::notify(const char* l1, const char* l2, uint32_t ms) {
  strncpy(note1_, l1 ? l1 : "", sizeof(note1_) - 1);
  note1_[sizeof(note1_) - 1] = 0;
  strncpy(note2_, l2 ? l2 : "", sizeof(note2_) - 1);
  note2_[sizeof(note2_) - 1] = 0;
  noteUntil_ = millis() + ms;
  dirty_ = true;
}

void UI::drawNote() {
  const bool two = note2_[0] != 0;
  const int bh = two ? 46 : 28;
  const int bw = W - 24;
  const int bx = 12;
  const int by = H - bh - 8;
  g_.fillRoundRect(bx, by, bw, bh, 8, TFT_BLACK);
  g_.setTextDatum(MC_DATUM);
  g_.setTextColor(TFT_WHITE);
  g_.drawString(fit(clean(note1_), bw - 16, 2), W / 2, two ? by + 14 : by + bh / 2, 2);
  if (two) {
    g_.setTextColor(grey(190));
    g_.drawString(fit(clean(note2_), bw - 16, 2), W / 2, by + 32, 2);
  }
}

void UI::markSettingsDirty() {
  settingsDirty_ = true;
  nextSaveTry_ = millis() + 1500;
}

bool UI::saveSettings() {
  if (set_.save()) {
    settingsDirty_ = false;
    saveFailShown_ = false;
    return true;
  }
  settingsDirty_ = true;
  nextSaveTry_ = millis() + 10000;  // try again later
  if (!saveFailShown_) {
    saveFailShown_ = true;
    notify("Settings not saved", "Flash write failed");
  }
  return false;
}
