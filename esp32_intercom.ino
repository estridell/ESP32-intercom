#include <Arduino.h>
#include <string.h>

#include "state_machine.h"

extern "C" {
#include "driver/adc.h"
#if __has_include("driver/dac.h")
#include "driver/dac.h"
#define HAVE_ANALOG_DAC 1
#else
#define HAVE_ANALOG_DAC 0
#endif
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#if __has_include("esp_hf_client_api.h")
#include "esp_hf_client_api.h"
#define HAVE_HFP_CLIENT 1
#else
#define HAVE_HFP_CLIENT 0
#endif
#include "esp_err.h"
#include "nvs_flash.h"
}

// -------------------------- Build-time configuration --------------------------
static constexpr const char *kDeviceName = "ESP32-Intercom-v1";
static constexpr uint8_t kSafeMaxVolumeAbs = 72;      // 0..127 AVRCP scale (0.5W speaker safety clamp)
static constexpr uint8_t kDefaultVolumeAbs = 64;      // startup target (clamped)
static constexpr uint32_t kHeartbeatMs = 10000;
static constexpr uint32_t kDiscoverableRefreshMs = 15000;
static constexpr uint32_t kBootBondClearHoldMs = 3000;
static constexpr uint32_t kStartupMuteMs = 300;
static constexpr int32_t kRampStepQ15 = 256;          // gain ramp step per loop

// Practical default wiring pins for ESP32-WROOM-32 DevKit.
static constexpr int PIN_SPKR_DAC = 25;               // DAC1 / GPIO25 -> PAM8403 IN
static constexpr int PIN_MIC_ADC = 34;                // ADC1_CH6
static constexpr int PIN_BAT_ADC = -1;                // set to ADC pin if divider/jumper is wired
static constexpr int PIN_BOND_CLEAR_BUTTON = 33;      // active LOW
static constexpr int PIN_LED_RED = 18;                // active HIGH
static constexpr int PIN_LED_GREEN = 19;              // active HIGH

static constexpr uint32_t kMusicSampleRate = 44100;
static constexpr uint32_t kCallSampleRate = 16000;
static constexpr uint8_t kMusicDownsampleFactor = 4;  // effective write pacing for software DAC path
static constexpr uint8_t kCallDownsampleFactor = 2;
static constexpr uint16_t kLowBatteryMillivolts = 3400;

enum class FatalCode {
  NVS_INIT = 1,
  AUDIO_INIT = 2,
  BT_CTRL_INIT = 3,
  BT_CTRL_ENABLE = 4,
  BLUEDROID_INIT = 5,
  BLUEDROID_ENABLE = 6,
  A2DP_INIT = 7,
  AVRCP_INIT = 8,
  HFP_INIT = 9,
};

// -------------------------- Runtime globals --------------------------
static portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
static ModeArbiter gModeArbiter;

static volatile bool gSourceConnected = false;
static volatile bool gA2dpLinkConnected = false;
static volatile bool gHfpLinkConnected = false;
static volatile bool gMusicActive = false;
static volatile bool gCallActive = false;
static volatile AudioMode gActiveMode = AudioMode::IDLE;

static volatile uint8_t gVolumeAbs = 0;
static volatile int32_t gTargetGainQ15 = 0;
static volatile int32_t gCurrentGainQ15 = 0;
static volatile bool gStartupMuteReleased = false;
static volatile bool gAvrcpVolumeRnRegistered = false;

static bool gShouldClearBonds = false;
static uint32_t gBootMs = 0;
static uint32_t gLastHeartbeat = 0;
static uint32_t gLastDiscoverableRefresh = 0;
static uint32_t gCurrentOutputRate = kMusicSampleRate;
static bool gOutputPathReady = false;
static bool gOutputPathBlocked = false;
static bool gHfpPathBlocked = false;
static bool gLowBatteryFeatureBlocked = false;
static bool gLowBatteryWarningLatched = false;
static uint32_t gLastLowBatteryLogMs = 0;

// -------------------------- Utilities --------------------------
static void fatalHalt(FatalCode code, const char *detail) {
  Serial.printf("FATAL-%03d %s\n", static_cast<int>(code), detail);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  while (true) {
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_LED_GREEN, LOW);
    delay(250);
    digitalWrite(PIN_LED_RED, LOW);
    delay(250);
  }
}

static void logRecoverable(const char *category, esp_err_t err) {
  Serial.printf("WARN %s err=0x%X (%s)\n", category, static_cast<unsigned>(err), esp_err_to_name(err));
}

static void setStatusLeds(AudioMode mode) {
  switch (mode) {
    case AudioMode::IDLE:
      digitalWrite(PIN_LED_RED, LOW);
      digitalWrite(PIN_LED_GREEN, LOW);
      break;
    case AudioMode::MUSIC:
      digitalWrite(PIN_LED_RED, LOW);
      digitalWrite(PIN_LED_GREEN, HIGH);
      break;
    case AudioMode::CALL:
      digitalWrite(PIN_LED_RED, HIGH);
      digitalWrite(PIN_LED_GREEN, LOW);
      break;
    default:
      digitalWrite(PIN_LED_RED, HIGH);
      digitalWrite(PIN_LED_GREEN, HIGH);
      break;
  }
}

static void setDiscoverableConnectable() {
  const esp_err_t err = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
  if (err != ESP_OK) {
    logRecoverable("gap_set_scan_mode", err);
  } else {
    Serial.println("BT discoverable/connectable enabled");
  }
}

static uint8_t clampVolumeAbs(uint8_t req) {
  return req > kSafeMaxVolumeAbs ? kSafeMaxVolumeAbs : req;
}

static uint8_t getVolumeAbs() {
  portENTER_CRITICAL(&gMux);
  const uint8_t v = gVolumeAbs;
  portEXIT_CRITICAL(&gMux);
  return v;
}

static int32_t volumeToQ15(uint8_t volAbs) {
  if (kSafeMaxVolumeAbs == 0) return 0;
  return (static_cast<int32_t>(volAbs) * 32767) / static_cast<int32_t>(kSafeMaxVolumeAbs);
}

#if defined(ESP_AVRC_RN_VOLUME_CHANGE) && defined(ESP_AVRC_RN_RSP_INTERIM) && \
    defined(ESP_AVRC_RN_RSP_CHANGED) && defined(ESP_AVRC_BIT_MASK_OP_SET)
#define HAVE_AVRCP_VOLUME_NOTIFICATION 1
#else
#define HAVE_AVRCP_VOLUME_NOTIFICATION 0
#endif

static void setAvrcpVolumeNotificationRegistered(bool value) {
  portENTER_CRITICAL(&gMux);
  gAvrcpVolumeRnRegistered = value;
  portEXIT_CRITICAL(&gMux);
}

static bool isAvrcpVolumeNotificationRegistered() {
  portENTER_CRITICAL(&gMux);
  const bool value = gAvrcpVolumeRnRegistered;
  portEXIT_CRITICAL(&gMux);
  return value;
}

static void setTargetVolume(uint8_t req, const char *reason, bool notifyAvrcpChanged = false) {
  const uint8_t clamped = clampVolumeAbs(req);
  portENTER_CRITICAL(&gMux);
  gVolumeAbs = clamped;
  gTargetGainQ15 = volumeToQ15(clamped);
  portEXIT_CRITICAL(&gMux);

  if (clamped != req) {
    Serial.printf("INFO volume %u clamped to safe max %u (%s)\n", req, clamped, reason);
  } else {
    Serial.printf("INFO volume set %u (%s)\n", clamped, reason);
  }

#if HAVE_AVRCP_VOLUME_NOTIFICATION
  if (notifyAvrcpChanged && isAvrcpVolumeNotificationRegistered()) {
    esp_avrc_rn_param_t rn_param{};
    rn_param.volume = getVolumeAbs();
    const esp_err_t err = esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_CHANGED, &rn_param);
    if (err != ESP_OK) {
      logRecoverable("avrc_tg_send_rn_rsp_changed", err);
    }
  }
#else
  (void)notifyAvrcpChanged;
#endif
}

static AudioMode getActiveMode() {
  portENTER_CRITICAL(&gMux);
  const AudioMode mode = gActiveMode;
  portEXIT_CRITICAL(&gMux);
  return mode;
}

static void setActiveMode(AudioMode mode) {
  portENTER_CRITICAL(&gMux);
  gActiveMode = mode;
  portEXIT_CRITICAL(&gMux);
}

static ModeInputs snapshotInputs() {
  ModeInputs in{};
  portENTER_CRITICAL(&gMux);
  in.source_connected = gSourceConnected;
  in.music_active = gMusicActive;
  in.call_active = gCallActive;
  portEXIT_CRITICAL(&gMux);
  return in;
}

static void recomputeSourceConnectivityLocked() {
  gSourceConnected = gA2dpLinkConnected || gHfpLinkConnected;
  if (!gSourceConnected) {
    gMusicActive = false;
    gCallActive = false;
  }
}

static void setA2dpLinkConnected(bool value) {
  portENTER_CRITICAL(&gMux);
  gA2dpLinkConnected = value;
  recomputeSourceConnectivityLocked();
  portEXIT_CRITICAL(&gMux);
}

static void setHfpLinkConnected(bool value) {
  portENTER_CRITICAL(&gMux);
  gHfpLinkConnected = value;
  recomputeSourceConnectivityLocked();
  portEXIT_CRITICAL(&gMux);
}

static void setMusicActive(bool value) {
  portENTER_CRITICAL(&gMux);
  gMusicActive = value;
  portEXIT_CRITICAL(&gMux);
}

static void setCallActive(bool value) {
  portENTER_CRITICAL(&gMux);
  gCallActive = value;
  portEXIT_CRITICAL(&gMux);
}

static bool okOrAlreadyInitialized(const char *label, esp_err_t err) {
  if (err == ESP_OK) {
    return true;
  }
  if (err == ESP_ERR_INVALID_STATE) {
    Serial.printf("INFO %s already initialized (ESP_ERR_INVALID_STATE)\n", label);
    return true;
  }
  return false;
}

static void setHandsFreeClassOfDevice() {
#if defined(ESP_BT_SET_COD_MAJOR_MINOR) && defined(ESP_BT_COD_MAJOR_DEV_AUDIO_VIDEO)
  esp_bt_cod_t cod{};
  cod.major = ESP_BT_COD_MAJOR_DEV_AUDIO_VIDEO;
#if defined(ESP_BT_COD_MINOR_DEV_AV_HANDSFREE)
  cod.minor = ESP_BT_COD_MINOR_DEV_AV_HANDSFREE;
#elif defined(ESP_BT_COD_MINOR_DEV_AV_HEADPHONES)
  cod.minor = ESP_BT_COD_MINOR_DEV_AV_HEADPHONES;
#elif defined(ESP_BT_COD_MINOR_DEV_AV_LOUDSPEAKER)
  cod.minor = ESP_BT_COD_MINOR_DEV_AV_LOUDSPEAKER;
#endif
#if defined(ESP_BT_COD_SRVC_RENDERING)
  cod.service |= ESP_BT_COD_SRVC_RENDERING;
#endif
#if defined(ESP_BT_COD_SRVC_AUDIO)
  cod.service |= ESP_BT_COD_SRVC_AUDIO;
#endif
#if defined(ESP_BT_COD_SRVC_TELEPHONY)
  cod.service |= ESP_BT_COD_SRVC_TELEPHONY;
#endif
  const esp_err_t err = esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);
  if (err != ESP_OK) {
    logRecoverable("gap_set_cod", err);
  } else {
    Serial.println("BT class-of-device set to audio/video hands-free");
  }
#else
  Serial.println("INFO CoD APIs/macros unavailable; using stack default class-of-device");
#endif
}

static void configureAvrcpVolumeNotificationCapability() {
#if HAVE_AVRCP_VOLUME_NOTIFICATION
  esp_avrc_rn_evt_cap_mask_t evt_cap{};
  esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_cap, ESP_AVRC_RN_VOLUME_CHANGE);
  const esp_err_t err = esp_avrc_tg_set_rn_evt_cap(&evt_cap);
  if (err != ESP_OK) {
    logRecoverable("avrc_tg_set_rn_evt_cap", err);
  } else {
    Serial.println("BT AVRCP RN capability enabled for absolute volume");
  }
#else
  Serial.println("INFO AVRCP RN volume capability macros unavailable; using basic absolute-volume support");
#endif
}

// -------------------------- Audio path --------------------------
static bool initAudioOutputPath() {
#if HAVE_ANALOG_DAC
  const esp_err_t err = dac_output_enable(DAC_CHANNEL_1);
  if (err != ESP_OK) {
    logRecoverable("dac_output_enable", err);
    gOutputPathBlocked = true;
    return false;
  }
  dac_output_voltage(DAC_CHANNEL_1, 128);
  gCurrentOutputRate = kMusicSampleRate;
  gOutputPathReady = true;
  Serial.printf("AUDIO output=analog_dac gpio=%d target=PAM8403_in\n", PIN_SPKR_DAC);
  return true;
#else
  Serial.println("BLOCKED OUTPUT-PATH analog DAC API unavailable in this core build");
  gOutputPathBlocked = true;
  return false;
#endif
}

static void setOutputRate(uint32_t sampleRate) {
  if (sampleRate == gCurrentOutputRate) return;
  gCurrentOutputRate = sampleRate;
  Serial.printf("AUDIO analog target sample rate -> %lu\n", static_cast<unsigned long>(sampleRate));
}

static inline int16_t applyGainQ15(int16_t sample, int32_t gainQ15) {
  const int32_t scaled = (static_cast<int32_t>(sample) * gainQ15) / 32767;
  if (scaled > 32767) return 32767;
  if (scaled < -32768) return -32768;
  return static_cast<int16_t>(scaled);
}

static uint8_t pcm16ToDac8(int16_t sample) {
  const int32_t shifted = (static_cast<int32_t>(sample) + 32768) >> 8;
  if (shifted < 0) return 0;
  if (shifted > 255) return 255;
  return static_cast<uint8_t>(shifted);
}

static inline void writeAnalogSample(int16_t sample) {
  if (!gOutputPathReady) return;
#if HAVE_ANALOG_DAC
  dac_output_voltage(DAC_CHANNEL_1, pcm16ToDac8(sample));
#else
  (void)sample;
#endif
}

static void writeAnalogStereoPcm(const int16_t *samples, size_t sampleCount, int32_t gainQ15) {
  if (!samples || sampleCount < 2 || !gOutputPathReady) return;

  static uint8_t decim = 0;
  for (size_t i = 0; i + 1 < sampleCount; i += 2) {
    const int16_t l = applyGainQ15(samples[i], gainQ15);
    const int16_t r = applyGainQ15(samples[i + 1], gainQ15);
    const int16_t mono = static_cast<int16_t>((static_cast<int32_t>(l) + static_cast<int32_t>(r)) / 2);
    decim++;
    if (decim >= kMusicDownsampleFactor) {
      decim = 0;
      writeAnalogSample(mono);
    }
  }
}

static void writeAnalogMonoPcm(const int16_t *samples, size_t sampleCount, int32_t gainQ15) {
  if (!samples || sampleCount == 0 || !gOutputPathReady) return;

  static uint8_t decim = 0;
  for (size_t i = 0; i < sampleCount; ++i) {
    const int16_t s = applyGainQ15(samples[i], gainQ15);
    decim++;
    if (decim >= kCallDownsampleFactor) {
      decim = 0;
      writeAnalogSample(s);
    }
  }
}

static void silenceAudioOutput() {
#if HAVE_ANALOG_DAC
  if (gOutputPathReady) {
    dac_output_voltage(DAC_CHANNEL_1, 128);
  }
#endif
}

static int readBatteryMillivolts() {
  if (PIN_BAT_ADC < 0) return -1;
  const int raw = analogRead(PIN_BAT_ADC);
  // Placeholder conversion for a future resistor-divider wiring; tune from bench measurements.
  return (raw * 4200) / 4095;
}

static void updateLowBatteryProtection() {
  const int batteryMv = readBatteryMillivolts();
  if (batteryMv < 0) {
    if (!gLowBatteryFeatureBlocked) {
      gLowBatteryFeatureBlocked = true;
      Serial.println("BLOCKED LOW-BATTERY no VBAT ADC wiring configured; protection pending-device");
    }
    return;
  }

  if (batteryMv <= static_cast<int>(kLowBatteryMillivolts)) {
    if (!gLowBatteryWarningLatched) {
      gLowBatteryWarningLatched = true;
      setCallActive(false);
      setMusicActive(false);
      setTargetVolume(24, "low_battery_guard");
    }
    const uint32_t now = millis();
    if (now - gLastLowBatteryLogMs >= 5000) {
      gLastLowBatteryLogMs = now;
      Serial.printf("WARN LOW-BATTERY %dmV <= %umV, forcing IDLE\n",
                    batteryMv,
                    static_cast<unsigned>(kLowBatteryMillivolts));
    }
  } else if (batteryMv > static_cast<int>(kLowBatteryMillivolts + 150)) {
    if (gLowBatteryWarningLatched) {
      Serial.printf("INFO LOW-BATTERY cleared %dmV\n", batteryMv);
    }
    gLowBatteryWarningLatched = false;
  }
}

// -------------------------- Bond policy --------------------------
static bool sameBdAddr(const esp_bd_addr_t a, const esp_bd_addr_t b) {
  return memcmp(a, b, ESP_BD_ADDR_LEN) == 0;
}

static void clearAllBonds() {
  int count = esp_bt_gap_get_bond_device_num();
  if (count <= 0) {
    Serial.println("BOND no devices to clear");
    return;
  }
  if (count > 20) count = 20;
  esp_bd_addr_t devices[20];
  int listed = count;
  esp_err_t err = esp_bt_gap_get_bond_device_list(&listed, devices);
  if (err != ESP_OK) {
    logRecoverable("bond_list_clear", err);
    return;
  }
  for (int i = 0; i < listed; ++i) {
    err = esp_bt_gap_remove_bond_device(devices[i]);
    if (err != ESP_OK) {
      logRecoverable("bond_remove", err);
    }
  }
  Serial.printf("BOND cleared %d entries\n", listed);
}

static void enforceSingleBond(const esp_bd_addr_t keep) {
  int count = esp_bt_gap_get_bond_device_num();
  if (count <= 1) return;
  if (count > 20) count = 20;

  esp_bd_addr_t devices[20];
  int listed = count;
  esp_err_t err = esp_bt_gap_get_bond_device_list(&listed, devices);
  if (err != ESP_OK) {
    logRecoverable("bond_list_single", err);
    return;
  }
  for (int i = 0; i < listed; ++i) {
    if (!sameBdAddr(devices[i], keep)) {
      err = esp_bt_gap_remove_bond_device(devices[i]);
      if (err != ESP_OK) {
        logRecoverable("bond_remove_single", err);
      }
    }
  }
  Serial.println("BOND policy=replace-old keep-latest-authenticated");
}

static bool detectBondClearRequest() {
  if (digitalRead(PIN_BOND_CLEAR_BUTTON) != LOW) return false;
  const uint32_t start = millis();
  while ((millis() - start) < kBootBondClearHoldMs) {
    if (digitalRead(PIN_BOND_CLEAR_BUTTON) != LOW) return false;
    delay(10);
  }
  return true;
}

// -------------------------- Bluetooth callbacks --------------------------
static void a2dpDataCallback(const uint8_t *data, uint32_t len) {
  if (!data || len < 2) return;
  if (getActiveMode() != AudioMode::MUSIC) return;  // CALL and IDLE suppress media output

  int32_t gainQ15 = 0;
  portENTER_CRITICAL_ISR(&gMux);
  gainQ15 = gCurrentGainQ15;
  portEXIT_CRITICAL_ISR(&gMux);

  const int16_t *in = reinterpret_cast<const int16_t *>(data);
  const size_t totalSamples = len / sizeof(int16_t);
  writeAnalogStereoPcm(in, totalSamples, gainQ15);
}

static void a2dpEventCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT: {
      const int state = param->conn_stat.state;
      Serial.printf("BT A2DP conn_state=%d\n", state);
      if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        setA2dpLinkConnected(true);
      } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        setA2dpLinkConnected(false);
        setMusicActive(false);
        setDiscoverableConnectable();
        Serial.printf("REC reconnectable after A2DP disconnect reason=%d\n", param->conn_stat.disc_rsn);
      }
      break;
    }
    case ESP_A2D_AUDIO_STATE_EVT: {
      const int state = param->audio_stat.state;
      Serial.printf("BT A2DP audio_state=%d\n", state);
      if (state == ESP_A2D_AUDIO_STATE_STARTED) {
        setMusicActive(true);
      } else if (state == ESP_A2D_AUDIO_STATE_STOPPED || state == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND) {
        setMusicActive(false);
      }
      break;
    }
    default:
      break;
  }
}

static void avrcControllerCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
  (void)param;
  if (event == ESP_AVRC_CT_CONNECTION_STATE_EVT) {
    Serial.printf("BT AVRCP-CT conn connected=%d\n", param->conn_stat.connected);
    if (!param->conn_stat.connected) {
      setAvrcpVolumeNotificationRegistered(false);
    }
  }
}

static void avrcTargetCallback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param) {
  switch (event) {
#ifdef ESP_AVRC_TG_CONNECTION_STATE_EVT
    case ESP_AVRC_TG_CONNECTION_STATE_EVT:
      Serial.printf("BT AVRCP-TG conn connected=%d\n", param->conn_stat.connected);
      if (!param->conn_stat.connected) {
        setAvrcpVolumeNotificationRegistered(false);
      }
      break;
#endif
#if HAVE_AVRCP_VOLUME_NOTIFICATION && defined(ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT)
    case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT:
      if (param->reg_ntf.event_id == ESP_AVRC_RN_VOLUME_CHANGE) {
        setAvrcpVolumeNotificationRegistered(true);
        esp_avrc_rn_param_t rn_param{};
        rn_param.volume = getVolumeAbs();
        const esp_err_t err = esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_INTERIM, &rn_param);
        if (err != ESP_OK) {
          logRecoverable("avrc_tg_send_rn_rsp_interim", err);
        }
      }
      break;
#endif
#ifdef ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT
    case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT:
      setTargetVolume(param->set_abs_vol.volume, "avrcp_set_absolute_volume", true);
      break;
#endif
    default:
      break;
  }
}

#if HAVE_HFP_CLIENT
static bool hfpConnectionStateIsLinked(int state) {
#ifdef ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED
  if (state == ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED) return true;
#endif
#ifdef ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED
  if (state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED) return true;
#endif
  return false;
}

static bool hfpAudioStateIsActive(int state) {
#ifdef ESP_HF_CLIENT_AUDIO_STATE_CONNECTED
  if (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED) return true;
#endif
#ifdef ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC
  if (state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) return true;
#endif
  return false;
}

static void hfpIncomingAudioCallback(const uint8_t *buf, uint32_t len) {
  if (!buf || len < 2) return;
  if (getActiveMode() != AudioMode::CALL) return;

  int32_t gainQ15 = 0;
  portENTER_CRITICAL_ISR(&gMux);
  gainQ15 = gCurrentGainQ15;
  portEXIT_CRITICAL_ISR(&gMux);

  // HFP payload is mono 16-bit PCM; routed to analog output path for PAM8403 input stage.
  const int16_t *inMono = reinterpret_cast<const int16_t *>(buf);
  const size_t monoSamples = len / sizeof(int16_t);
  writeAnalogMonoPcm(inMono, monoSamples, gainQ15);
}

static uint32_t hfpOutgoingAudioCallback(uint8_t *buf, uint32_t len) {
  if (!buf || len < 2) return 0;

  int16_t *out = reinterpret_cast<int16_t *>(buf);
  const uint32_t samples = len / sizeof(int16_t);
  if (getActiveMode() != AudioMode::CALL) {
    memset(buf, 0, len);
    return len;
  }

  for (uint32_t i = 0; i < samples; ++i) {
    const int raw = analogRead(PIN_MIC_ADC);     // 0..4095
    const int centered = raw - 2048;
    int32_t sample = centered * 12;              // conservative gain
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    out[i] = static_cast<int16_t>(sample);
  }
  return samples * sizeof(int16_t);
}

static void hfpClientCallback(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param) {
  switch (event) {
    case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
      Serial.printf("BT HFP conn_state=%d\n", param->conn_stat.state);
      {
        const bool linked = hfpConnectionStateIsLinked(param->conn_stat.state);
        setHfpLinkConnected(linked);
        if (!linked) {
          setCallActive(false);
        }
      }
      if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
        setDiscoverableConnectable();
        Serial.println("REC HFP disconnected, discoverable restored");
      }
      break;
    case ESP_HF_CLIENT_AUDIO_STATE_EVT: {
      const int state = param->audio_stat.state;
      Serial.printf("BT HFP audio_state=%d\n", state);
      setCallActive(hfpAudioStateIsActive(state));
      break;
    }
    default:
      // Keep compatibility if callback enum set differs by core/IDF version.
      Serial.printf("BT HFP evt=%d\n", static_cast<int>(event));
      break;
  }
}
#endif

static void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
        Serial.println("BT GAP auth complete");
        enforceSingleBond(param->auth_cmpl.bda);
      } else {
        Serial.printf("WARN auth_failed stat=%d\n", param->auth_cmpl.stat);
      }
      break;
    case ESP_BT_GAP_MODE_CHG_EVT:
      Serial.printf("BT GAP mode_change mode=%d\n", param->mode_chg.mode);
      break;
    default:
      break;
  }
}

// -------------------------- Bluetooth init --------------------------
static void initBluetoothOrHalt() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    err = nvs_flash_erase();
    if (err != ESP_OK) {
      fatalHalt(FatalCode::NVS_INIT, "nvs_flash_erase");
    }
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    fatalHalt(FatalCode::NVS_INIT, "nvs_flash_init");
  }

  err = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    logRecoverable("bt_mem_release_ble", err);
  }

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  err = esp_bt_controller_init(&bt_cfg);
  if (!okOrAlreadyInitialized("esp_bt_controller_init", err)) {
    fatalHalt(FatalCode::BT_CTRL_INIT, "esp_bt_controller_init");
  }

  err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  if (!okOrAlreadyInitialized("esp_bt_controller_enable", err)) {
    fatalHalt(FatalCode::BT_CTRL_ENABLE, "esp_bt_controller_enable");
  }

  err = esp_bluedroid_init();
  if (!okOrAlreadyInitialized("esp_bluedroid_init", err)) {
    fatalHalt(FatalCode::BLUEDROID_INIT, "esp_bluedroid_init");
  }
  err = esp_bluedroid_enable();
  if (!okOrAlreadyInitialized("esp_bluedroid_enable", err)) {
    fatalHalt(FatalCode::BLUEDROID_ENABLE, "esp_bluedroid_enable");
  }

  err = esp_bt_gap_register_callback(gapCallback);
  if (err != ESP_OK) {
    logRecoverable("gap_register_callback", err);
  }

  err = esp_bt_dev_set_device_name(kDeviceName);
  if (err != ESP_OK) {
    logRecoverable("set_device_name", err);
  }
  setHandsFreeClassOfDevice();

  const esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
  esp_bt_pin_code_t pin_code;
  pin_code[0] = '0';
  pin_code[1] = '0';
  pin_code[2] = '0';
  pin_code[3] = '0';
  err = esp_bt_gap_set_pin(pin_type, 4, pin_code);
  if (err != ESP_OK) {
    logRecoverable("gap_set_pin", err);
  }

  err = esp_a2d_register_callback(a2dpEventCallback);
  if (!okOrAlreadyInitialized("esp_a2d_register_callback", err)) {
    fatalHalt(FatalCode::A2DP_INIT, "a2d_register_callback");
  }
  err = esp_a2d_sink_register_data_callback(a2dpDataCallback);
  if (!okOrAlreadyInitialized("esp_a2d_sink_register_data_callback", err)) {
    fatalHalt(FatalCode::A2DP_INIT, "a2d_sink_register_data_callback");
  }
  err = esp_a2d_sink_init();
  if (!okOrAlreadyInitialized("esp_a2d_sink_init", err)) {
    fatalHalt(FatalCode::A2DP_INIT, "a2d_sink_init");
  }

  err = esp_avrc_ct_init();
  if (!okOrAlreadyInitialized("esp_avrc_ct_init", err)) {
    fatalHalt(FatalCode::AVRCP_INIT, "avrc_ct_init");
  }
  err = esp_avrc_ct_register_callback(avrcControllerCallback);
  if (!okOrAlreadyInitialized("esp_avrc_ct_register_callback", err)) {
    fatalHalt(FatalCode::AVRCP_INIT, "avrc_ct_register_callback");
  }
  err = esp_avrc_tg_init();
  if (!okOrAlreadyInitialized("esp_avrc_tg_init", err)) {
    fatalHalt(FatalCode::AVRCP_INIT, "avrc_tg_init");
  }
  err = esp_avrc_tg_register_callback(avrcTargetCallback);
  if (!okOrAlreadyInitialized("esp_avrc_tg_register_callback", err)) {
    fatalHalt(FatalCode::AVRCP_INIT, "avrc_tg_register_callback");
  }
  configureAvrcpVolumeNotificationCapability();

#if HAVE_HFP_CLIENT
  gHfpPathBlocked = false;
  err = esp_hf_client_register_callback(hfpClientCallback);
  if (!okOrAlreadyInitialized("esp_hf_client_register_callback", err)) {
    fatalHalt(FatalCode::HFP_INIT, "hfp_register_callback");
  }
  err = esp_hf_client_register_data_callback(hfpIncomingAudioCallback, hfpOutgoingAudioCallback);
  if (!okOrAlreadyInitialized("esp_hf_client_register_data_callback", err)) {
    fatalHalt(FatalCode::HFP_INIT, "hfp_register_data_callback");
  }
  err = esp_hf_client_init();
  if (!okOrAlreadyInitialized("esp_hf_client_init", err)) {
    fatalHalt(FatalCode::HFP_INIT, "hfp_init");
  }
#else
  gHfpPathBlocked = true;
  Serial.println("BLOCKED HFP-TOOLCHAIN missing esp_hf_client_api.h; call path disabled");
#endif

  setDiscoverableConnectable();
}

// -------------------------- Mode and control loop --------------------------
static void handleModeTransition(const ModeTransition &t) {
  if (!t.changed) return;
  Serial.printf("MODE %s\n", transitionDirection(t.from, t.to));
  setActiveMode(t.to);
  setStatusLeds(t.to);

  switch (t.to) {
    case AudioMode::CALL:
      setOutputRate(kCallSampleRate);
      break;
    case AudioMode::MUSIC:
      setOutputRate(kMusicSampleRate);
      break;
    case AudioMode::IDLE:
    default:
      silenceAudioOutput();
      break;
  }
}

static void updateModeArbitration() {
  const ModeInputs in = snapshotInputs();
  const ModeTransition t = gModeArbiter.applyInputs(in);
  handleModeTransition(t);
}

static void updateGainRamp() {
  int32_t target = 0;
  int32_t current = 0;
  AudioMode mode = getActiveMode();

  portENTER_CRITICAL(&gMux);
  const bool unmuted = gStartupMuteReleased;
  const int32_t configuredTarget = gTargetGainQ15;
  current = gCurrentGainQ15;
  portEXIT_CRITICAL(&gMux);

  if (unmuted && mode != AudioMode::IDLE) {
    target = configuredTarget;
  }

  if (current < target) {
    current += kRampStepQ15;
    if (current > target) current = target;
  } else if (current > target) {
    current -= kRampStepQ15;
    if (current < target) current = target;
  }

  portENTER_CRITICAL(&gMux);
  gCurrentGainQ15 = current;
  portEXIT_CRITICAL(&gMux);
}

static void heartbeatLog() {
  const uint32_t now = millis();
  if (now - gLastHeartbeat < kHeartbeatMs) return;
  gLastHeartbeat = now;

  ModeInputs in = snapshotInputs();
  Serial.printf("HB mode=%s src=%d music=%d call=%d vol=%u gainQ15=%ld out=%s hfp=%s lowbat=%s\n",
                modeToString(getActiveMode()),
                static_cast<int>(in.source_connected),
                static_cast<int>(in.music_active),
                static_cast<int>(in.call_active),
                static_cast<unsigned>(gVolumeAbs),
                static_cast<long>(gCurrentGainQ15),
                gOutputPathReady ? "analog-ok" : (gOutputPathBlocked ? "blocked" : "init-pending"),
                gHfpPathBlocked ? "blocked" : "ok",
                gLowBatteryFeatureBlocked ? "blocked" : (gLowBatteryWarningLatched ? "active" : "ok"));
}

static void keepDiscoverableIfNeeded() {
  const uint32_t now = millis();
  if (now - gLastDiscoverableRefresh < kDiscoverableRefreshMs) return;
  gLastDiscoverableRefresh = now;

  const ModeInputs in = snapshotInputs();
  if (!in.source_connected) {
    setDiscoverableConnectable();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  gBootMs = millis();
  Serial.println("BOOT ESP32 Intercom v1");

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_BOND_CLEAR_BUTTON, INPUT_PULLUP);
  setStatusLeds(AudioMode::IDLE);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MIC_ADC, ADC_11db);
  if (PIN_BAT_ADC >= 0) {
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
  }

  gShouldClearBonds = detectBondClearRequest();
  if (gShouldClearBonds) {
    Serial.println("BOND clear requested by boot button hold");
  }

  if (!initAudioOutputPath()) {
    Serial.println("BLOCKED OUTPUT-PATH startup without analog output capability");
  }

  setTargetVolume(kDefaultVolumeAbs, "startup_default");
  portENTER_CRITICAL(&gMux);
  gCurrentGainQ15 = 0;         // start muted to avoid pops
  gStartupMuteReleased = false;
  portEXIT_CRITICAL(&gMux);

  initBluetoothOrHalt();

  if (gShouldClearBonds) {
    clearAllBonds();
    // Deterministic policy: clear only on explicit hold, otherwise keep persisted bonds.
  }

  setActiveMode(AudioMode::IDLE);
  gLastHeartbeat = millis();
  gLastDiscoverableRefresh = millis();
  Serial.println("BOOT complete");
}

void loop() {
  if (!gStartupMuteReleased && millis() - gBootMs > kStartupMuteMs) {
    portENTER_CRITICAL(&gMux);
    gStartupMuteReleased = true;
    portEXIT_CRITICAL(&gMux);
    Serial.println("AUDIO startup mute released, ramping gain");
  }

  updateModeArbitration();
  updateGainRamp();
  updateLowBatteryProtection();
  keepDiscoverableIfNeeded();
  heartbeatLog();

  delay(20);
}
