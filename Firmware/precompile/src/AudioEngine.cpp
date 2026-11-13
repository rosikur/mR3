#include "AudioEngine.h"
#include <Audio.h>     // ESP32-audioI2S
#include <SD_MMC.h>
#include <new>
#include <string.h>
#include "Config.h"

using namespace mr3;

namespace {
constexpr uint32_t START_GRACE_MS = 1500;  // decoder may legitimately report !running right after start
constexpr TickType_t POST_WAIT = pdMS_TO_TICKS(25);
}  // namespace

const char* AudioEngine::reasonText(EndReason r) {
  switch (r) {
    case EndReason::OpenFailed: return "Can't open file";
    case EndReason::DecoderError: return "Unsupported or damaged file";
    case EndReason::StoppedEarly: return "Playback stopped unexpectedly";
    default: return "";
  }
}

bool AudioEngine::begin(uint8_t initialVolume) {
  vol_.store(initialVolume);
  queue_ = xQueueCreate(8, sizeof(Cmd));
  if (!queue_) return false;
  // priority 4 > Arduino loop task (1); core 0 so UI rendering on core 1 never blocks it
  return xTaskCreatePinnedToCore(taskEntry, "audio", 12288, this, 4, nullptr, 0) == pdPASS;
}

// play/stop supersede everything queued before them (the user has moved on),
// so if the queue is full we flush it and retry instead of losing the command.
// Toggle is relative to the current state, so it is never flushed: it fails
// loudly (returns false, counted) instead.
bool AudioEngine::post(const Cmd& c, bool supersedesQueue) {
  if (!queue_) return false;
  if (xQueueSend(queue_, &c, POST_WAIT) == pdPASS) return true;
  if (supersedesQueue) {
    xQueueReset(queue_);
    if (xQueueSend(queue_, &c, 0) == pdPASS) return true;
  }
  dropped_.fetch_add(1, std::memory_order_relaxed);
  Serial.printf("[audio] command %u dropped (queue full)\n", (unsigned)c.op);
  return false;
}

bool AudioEngine::play(const char* path) {
  if (!path || strlen(path) >= sizeof(Cmd::path)) {
    Serial.println("[audio] play(): path missing or too long");
    return false;
  }
  Cmd c{};
  c.op = Op::Play;
  strcpy(c.path, path);  // length checked above
  seekAccum_.store(0);   // a seek meant for the previous track must not hit this one
  return post(c, true);
}

bool AudioEngine::togglePause() {
  Cmd c{};
  c.op = Op::Toggle;
  return post(c, false);
}

bool AudioEngine::stop() {
  Cmd c{};
  c.op = Op::Stop;
  return post(c, true);
}

void AudioEngine::setVolume(uint8_t v) {
  vol_.store(v);
  targetVol_.store(v);
}

void AudioEngine::seekRelative(int seconds) {
  seekAccum_.fetch_add(seconds);
}

void AudioEngine::taskEntry(void* arg) {
  static_cast<AudioEngine*>(arg)->run();
}

// Called from the audio task only.
void AudioEngine::finishTrack(EndReason r) {
  state_.store(PlayState::Stopped);
  endReason_.store(r, std::memory_order_relaxed);
  endSeq_.fetch_add(1, std::memory_order_release);  // publishes endReason_ to the UI
}

void AudioEngine::run() {
  // The callback runs in the library's own task context: keep it tiny and lock-free.
  // With a callback installed the library no longer prints its log, so errors are
  // counted here and forwarded to Serial.
  Audio::audio_info_callback = [this](Audio::msg_t m) {
    if (m.e == Audio::evt_eof) {
      eof_.store(true);
    } else if (m.e == Audio::evt_log && m.s && m.msg) {
      if (strcmp(m.s, "LOGE") == 0) {
        errLogs_.fetch_add(1, std::memory_order_relaxed);
        Serial.printf("[audio] %s\n", m.msg);
      } else if (strcmp(m.s, "LOGW") == 0) {
        Serial.printf("[audio] %s\n", m.msg);
      }
    }
  };

  Audio* audio = new (std::nothrow) Audio();
  if (!audio) {
    Serial.println("[audio] FATAL: out of memory creating Audio object");
    initFailed_.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
    return;
  }
  if (!audio->setPinout(pins::I2S_BCK, pins::I2S_WS, pins::I2S_DOUT)) {
    Serial.println("[audio] FATAL: I2S pin setup failed");
    initFailed_.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
    return;
  }
  audio->setVolume(vol_.load());
  ready_.store(true, std::memory_order_release);

  uint32_t startedAt = 0;
  uint32_t errBase = 0;

  for (;;) {
    Cmd c;
    while (xQueueReceive(queue_, &c, 0) == pdTRUE) {
      switch (c.op) {
        case Op::Play:
          audio->stopSong();
          eof_.store(false);
          pos_.store(0);
          dur_.store(0);
          errBase = errLogs_.load(std::memory_order_relaxed);
          if (audio->connecttoFS(SD_MMC, c.path)) {
            state_.store(PlayState::Playing);
            startedAt = millis();
          } else {
            Serial.printf("[audio] cannot open %s\n", c.path);
            finishTrack(EndReason::OpenFailed);
          }
          break;
        case Op::Toggle:
          if (state_.load() == PlayState::Playing) {
            if (audio->pauseResume()) state_.store(PlayState::Paused);
          } else if (state_.load() == PlayState::Paused) {
            if (audio->pauseResume()) state_.store(PlayState::Playing);
          }
          break;
        case Op::Stop:
          audio->stopSong();
          state_.store(PlayState::Stopped);
          pos_.store(0);
          dur_.store(0);
          break;
      }
    }

    const int16_t tv = targetVol_.exchange(-1);
    if (tv >= 0) audio->setVolume((uint8_t)tv);

    const int32_t seek = seekAccum_.exchange(0);
    if (seek != 0 && state_.load() != PlayState::Stopped) audio->setTimeOffset((int)seek);

    audio->loop();

    if (state_.load() == PlayState::Playing) {
      const uint32_t pos = audio->getAudioCurrentTime();
      const uint32_t dur = audio->getAudioFileDuration();
      pos_.store(pos);
      dur_.store(dur);

      const bool eof = eof_.exchange(false);
      const bool dead = !audio->isRunning() && millis() - startedAt > START_GRACE_MS;
      if (eof || dead) {
        const bool producedAudio = pos >= 1 || (dur > 0 && dur <= 1);
        EndReason r;
        if (eof) r = producedAudio ? EndReason::Finished : EndReason::DecoderError;
        else r = producedAudio ? EndReason::StoppedEarly : EndReason::DecoderError;
        if (r != EndReason::Finished) {
          audio->stopSong();  // make sure the decoder is really idle before the next track
          Serial.printf("[audio] track ended abnormally: %s (pos %us, %u decoder errors)\n",
                        reasonText(r), (unsigned)pos,
                        (unsigned)(errLogs_.load(std::memory_order_relaxed) - errBase));
        }
        finishTrack(r);
      }
    }
    vTaskDelay(1);
  }
}
