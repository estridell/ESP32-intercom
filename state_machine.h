#ifndef ESP32_INTERCOM_STATE_MACHINE_H
#define ESP32_INTERCOM_STATE_MACHINE_H

#include <stdint.h>

enum class AudioMode : uint8_t {
  IDLE = 0,
  MUSIC = 1,
  CALL = 2,
};

struct ModeInputs {
  bool source_connected;
  bool music_active;
  bool call_active;
};

struct ModeTransition {
  AudioMode from;
  AudioMode to;
  bool changed;
};

inline const char *modeToString(AudioMode mode) {
  switch (mode) {
    case AudioMode::IDLE:
      return "IDLE";
    case AudioMode::MUSIC:
      return "MUSIC";
    case AudioMode::CALL:
      return "CALL";
    default:
      return "UNKNOWN";
  }
}

inline const char *transitionDirection(AudioMode from, AudioMode to) {
  if (from == AudioMode::IDLE && to == AudioMode::MUSIC) return "IDLE->MUSIC";
  if (from == AudioMode::IDLE && to == AudioMode::CALL) return "IDLE->CALL";
  if (from == AudioMode::MUSIC && to == AudioMode::IDLE) return "MUSIC->IDLE";
  if (from == AudioMode::MUSIC && to == AudioMode::CALL) return "MUSIC->CALL";
  if (from == AudioMode::CALL && to == AudioMode::IDLE) return "CALL->IDLE";
  if (from == AudioMode::CALL && to == AudioMode::MUSIC) return "CALL->MUSIC";
  if (from == to) return "NO-CHANGE";
  return "UNSPECIFIED";
}

inline ModeInputs sanitizeInputs(const ModeInputs &raw) {
  ModeInputs s = raw;
  if (!s.source_connected) {
    s.music_active = false;
    s.call_active = false;
  }
  return s;
}

inline AudioMode resolveMode(const ModeInputs &raw) {
  const ModeInputs inputs = sanitizeInputs(raw);
  if (inputs.call_active) {
    return AudioMode::CALL;
  }
  if (inputs.music_active) {
    return AudioMode::MUSIC;
  }
  return AudioMode::IDLE;
}

class ModeArbiter {
 public:
  ModeArbiter() : current_(AudioMode::IDLE), inputs_({false, false, false}) {}

  AudioMode currentMode() const { return current_; }
  ModeInputs inputs() const { return inputs_; }

  ModeTransition applyInputs(const ModeInputs &next_inputs) {
    inputs_ = sanitizeInputs(next_inputs);
    return transitionTo(resolveMode(inputs_));
  }

  ModeTransition setSourceConnected(bool connected) {
    ModeInputs next = inputs_;
    next.source_connected = connected;
    return applyInputs(next);
  }

  ModeTransition setMusicActive(bool active) {
    ModeInputs next = inputs_;
    next.music_active = active;
    return applyInputs(next);
  }

  ModeTransition setCallActive(bool active) {
    ModeInputs next = inputs_;
    next.call_active = active;
    return applyInputs(next);
  }

 private:
  ModeTransition transitionTo(AudioMode next_mode) {
    ModeTransition t = {current_, next_mode, current_ != next_mode};
    current_ = next_mode;
    return t;
  }

  AudioMode current_;
  ModeInputs inputs_;
};

#endif
