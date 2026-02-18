/*------------------------------------------------------------------------------
 * ESP32 Intercom
 * Single-sketch implementation for A2DP sink + HFP client with dynamic I2S mode
 * switching (44.1 kHz stereo media, 8/16 kHz mono call audio).
 *----------------------------------------------------------------------------*/

#include <string.h>

#include "BluetoothSerial.h"
#include "driver/i2s.h"
#include "esp_a2dp_api.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "nvs_flash.h"

namespace {

constexpr char kDeviceName[] = "esp_intercom";
constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr int kI2sBclkPin = 26;
constexpr int kI2sWsPin = 25;
constexpr int kI2sDataOutPin = 22;
constexpr int kI2sDataInPin = 35;

constexpr uint32_t kMediaSampleRateHz = 44100;
constexpr uint32_t kVoiceNarrowbandSampleRateHz = 8000;
constexpr uint32_t kVoiceWidebandSampleRateHz = 16000;

constexpr TickType_t kI2sIoTimeoutTicks = 0;
constexpr unsigned long kHeartbeatPeriodMs = 5000;

// Numeric values below follow Arduino ESP32 Core 2.0.17 callbacks.
constexpr int kHfEventConnectionState = 0;
constexpr int kHfEventAudioState = 2;
constexpr int kHfEventCallIndicator = 3;
constexpr int kHfConnStateDisconnected = 0;
constexpr int kHfConnStateConnectedMin = 1;
constexpr int kHfAudioStateDisconnected = 0;
constexpr int kHfAudioStateConnectedCvSd = 2;
constexpr int kHfAudioStateConnectedMsbc = 3;

constexpr int kA2dpEventConnectionState = 0;
constexpr int kA2dpEventAudioState = 1;
constexpr int kA2dpConnStateDisconnected = 0;
constexpr int kA2dpConnStateConnected = 2;
constexpr int kA2dpAudioStateRemoteSuspend = 0;
constexpr int kA2dpAudioStateStopped = 1;
constexpr int kA2dpAudioStateStarted = 2;

enum AudioMode : uint8_t {
  AUDIO_MODE_IDLE = 0,
  AUDIO_MODE_MEDIA,
  AUDIO_MODE_VOICE_NB,
  AUDIO_MODE_VOICE_WB,
};

BluetoothSerial serialBtBootstrap;

volatile AudioMode gAudioMode = AUDIO_MODE_IDLE;
volatile bool gA2dpStreaming = false;
volatile bool gCallAudioConnected = false;
volatile unsigned long gTransitionStartMs = 0;
volatile uint32_t gA2dpDropBytes = 0;
volatile uint32_t gHfpIncomingDropBytes = 0;
volatile uint32_t gHfpOutgoingPadBytes = 0;

const char *audioModeToString(AudioMode mode) {
  switch (mode) {
    case AUDIO_MODE_MEDIA:
      return "MEDIA_STREAM";
    case AUDIO_MODE_VOICE_NB:
      return "VOICE_NB_8K";
    case AUDIO_MODE_VOICE_WB:
      return "VOICE_WB_16K";
    case AUDIO_MODE_IDLE:
    default:
      return "IDLE";
  }
}

bool checkErr(const char *label, esp_err_t err) {
  if (err == ESP_OK) {
    return true;
  }

  Serial.print("[ERROR] ");
  Serial.print(label);
  Serial.print(" failed: ");
  Serial.print(esp_err_to_name(err));
  Serial.print(" (");
  Serial.print(static_cast<int>(err));
  Serial.println(")");
  return false;
}

bool setI2sForMode(AudioMode mode) {
  uint32_t sampleRate = kMediaSampleRateHz;
  i2s_channel_t channels = I2S_CHANNEL_STEREO;

  if (mode == AUDIO_MODE_VOICE_NB) {
    sampleRate = kVoiceNarrowbandSampleRateHz;
    channels = I2S_CHANNEL_MONO;
  } else if (mode == AUDIO_MODE_VOICE_WB) {
    sampleRate = kVoiceWidebandSampleRateHz;
    channels = I2S_CHANNEL_MONO;
  }

  return checkErr("i2s_set_clk", i2s_set_clk(kI2sPort, sampleRate,
                                              I2S_BITS_PER_SAMPLE_16BIT, channels));
}

bool initNvs() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    if (!checkErr("nvs_flash_erase", nvs_flash_erase())) {
      return false;
    }
    err = nvs_flash_init();
  }
  return checkErr("nvs_flash_init", err);
}

bool initI2s() {
  const i2s_config_t i2sConfig = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
      .sample_rate = static_cast<int>(kMediaSampleRateHz),
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false,
  };

  if (!checkErr("i2s_driver_install", i2s_driver_install(kI2sPort, &i2sConfig, 0, nullptr))) {
    return false;
  }

  const i2s_pin_config_t pinConfig = {
      .bck_io_num = kI2sBclkPin,
      .ws_io_num = kI2sWsPin,
      .data_out_num = kI2sDataOutPin,
      .data_in_num = kI2sDataInPin,
  };
  if (!checkErr("i2s_set_pin", i2s_set_pin(kI2sPort, &pinConfig))) {
    return false;
  }

  return setI2sForMode(AUDIO_MODE_MEDIA);
}

void noteTransitionStart() {
  gTransitionStartMs = millis();
}

void hfIncomingDataCallback(const uint8_t *data, uint32_t len) {
  size_t bytesWritten = 0;
  const esp_err_t err =
      i2s_write(kI2sPort, data, len, &bytesWritten, kI2sIoTimeoutTicks);
  if (err != ESP_OK || bytesWritten < len) {
    gHfpIncomingDropBytes += (len - static_cast<uint32_t>(bytesWritten));
  }
}

uint32_t hfOutgoingDataCallback(uint8_t *data, uint32_t len) {
  memset(data, 0, len);

  size_t bytesRead = 0;
  const esp_err_t err =
      i2s_read(kI2sPort, data, len, &bytesRead, kI2sIoTimeoutTicks);

  if (err != ESP_OK) {
    gHfpOutgoingPadBytes += len;
    return len;
  }

  if (bytesRead < len) {
    gHfpOutgoingPadBytes += (len - static_cast<uint32_t>(bytesRead));
    return len;
  }
  return static_cast<uint32_t>(bytesRead);
}

void a2dpSinkDataCallback(const uint8_t *data, uint32_t len) {
  size_t bytesWritten = 0;
  const esp_err_t err =
      i2s_write(kI2sPort, data, len, &bytesWritten, kI2sIoTimeoutTicks);
  if (err != ESP_OK || bytesWritten < len) {
    gA2dpDropBytes += (len - static_cast<uint32_t>(bytesWritten));
  }
}

void hfClientEventCallback(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param) {
  switch (static_cast<int>(event)) {
    case kHfEventConnectionState: {
      const int connState = static_cast<int>(param->conn_stat.state);
      if (connState == kHfConnStateDisconnected) {
        gCallAudioConnected = false;
        if (!gA2dpStreaming) {
          gAudioMode = AUDIO_MODE_IDLE;
        }
        noteTransitionStart();
        Serial.println("[STATUS] HFP: disconnected");
      } else if (connState >= kHfConnStateConnectedMin) {
        Serial.print("[STATUS] HFP: connected (state=");
        Serial.print(connState);
        Serial.println(")");
      }
      break;
    }

    case kHfEventAudioState: {
      const int audioState = static_cast<int>(param->audio_stat.state);
      const bool connectedNarrowband = (audioState == kHfAudioStateConnectedCvSd);
      const bool connectedWideband = (audioState == kHfAudioStateConnectedMsbc);

      if (connectedNarrowband || connectedWideband) {
        const AudioMode mode = connectedWideband ? AUDIO_MODE_VOICE_WB : AUDIO_MODE_VOICE_NB;
        if (setI2sForMode(mode)) {
          gCallAudioConnected = true;
          gAudioMode = mode;
        }
        const unsigned long latency = millis() - gTransitionStartMs;
        Serial.print("[LATENCY] HFP audio ready in ");
        Serial.print(latency);
        Serial.print(" ms (");
        Serial.print(connectedWideband ? "mSBC 16k" : "CVSD 8k");
        Serial.println(")");
        Serial.println("[EVENT] MODE: VOICE_CALL_ACTIVE");
      } else if (audioState == kHfAudioStateDisconnected) {
        gCallAudioConnected = false;
        noteTransitionStart();
        if (setI2sForMode(AUDIO_MODE_MEDIA)) {
          gAudioMode = gA2dpStreaming ? AUDIO_MODE_MEDIA : AUDIO_MODE_IDLE;
        }
        Serial.println("[EVENT] MODE: VOICE_CALL_IDLE");
      }
      break;
    }

    case kHfEventCallIndicator:
      if (param->call.status == 1) {
        noteTransitionStart();
        Serial.println("[DEBUG] HFP: call signaled, waiting for SCO audio");
      }
      break;

    default:
      break;
  }
}

void a2dpSinkEventCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  switch (static_cast<int>(event)) {
    case kA2dpEventConnectionState:
      if (param->conn_stat.state == kA2dpConnStateConnected) {
        Serial.println("[STATUS] A2DP: connected");
      } else if (param->conn_stat.state == kA2dpConnStateDisconnected) {
        gA2dpStreaming = false;
        if (!gCallAudioConnected) {
          gAudioMode = AUDIO_MODE_IDLE;
        }
        Serial.println("[STATUS] A2DP: disconnected");
      }
      break;

    case kA2dpEventAudioState:
      if (param->audio_stat.state == kA2dpAudioStateStarted) {
        gA2dpStreaming = true;
        if (!gCallAudioConnected) {
          gAudioMode = AUDIO_MODE_MEDIA;
          setI2sForMode(AUDIO_MODE_MEDIA);
        }
        const unsigned long latency = millis() - gTransitionStartMs;
        Serial.print("[LATENCY] A2DP audio ready in ");
        Serial.print(latency);
        Serial.println(" ms");
        Serial.println("[EVENT] MODE: MEDIA_STREAMING_STARTED");
      } else if (param->audio_stat.state == kA2dpAudioStateStopped ||
                 param->audio_stat.state == kA2dpAudioStateRemoteSuspend) {
        gA2dpStreaming = false;
        noteTransitionStart();
        if (!gCallAudioConnected) {
          gAudioMode = AUDIO_MODE_IDLE;
        }
        Serial.println("[EVENT] MODE: MEDIA_STREAMING_PAUSED");
      }
      break;

    default:
      break;
  }
}

bool initBluetoothProfiles() {
  if (!serialBtBootstrap.begin(kDeviceName)) {
    Serial.println("[ERROR] Bluetooth stack bootstrap failed");
    return false;
  }

  esp_bt_cod_t cod = {};
  cod.major = 0x04;  // Audio/Video
  cod.minor = 0x08;  // Hands-free
  if (!checkErr("esp_bt_gap_set_cod", esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL))) {
    return false;
  }

  if (!checkErr("esp_a2d_register_callback", esp_a2d_register_callback(a2dpSinkEventCallback))) {
    return false;
  }
  if (!checkErr("esp_a2d_sink_init", esp_a2d_sink_init())) {
    return false;
  }
  if (!checkErr("esp_a2d_sink_register_data_callback",
                esp_a2d_sink_register_data_callback(a2dpSinkDataCallback))) {
    return false;
  }

  if (!checkErr("esp_hf_client_register_callback",
                esp_hf_client_register_callback(hfClientEventCallback))) {
    return false;
  }
  if (!checkErr("esp_hf_client_init", esp_hf_client_init())) {
    return false;
  }
  if (!checkErr("esp_hf_client_register_data_callback",
                esp_hf_client_register_data_callback(hfIncomingDataCallback,
                                                     hfOutgoingDataCallback))) {
    return false;
  }

  return checkErr(
      "esp_bt_gap_set_scan_mode",
      esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }
  delay(200);

  Serial.println();
  Serial.println("###########################################");
  Serial.println("#         ESP32 INTERCOM STARTUP          #");
  Serial.println("###########################################");

  if (!initNvs() || !initI2s() || !initBluetoothProfiles()) {
    Serial.println("[FATAL] Initialization failed. Halting.");
    while (true) {
      delay(1000);
    }
  }

  noteTransitionStart();
  Serial.println("[SUCCESS] Ready. Pair with 'esp_intercom'.");
}

void loop() {
  static unsigned long lastHeartbeatMs = 0;
  const unsigned long nowMs = millis();
  if (nowMs - lastHeartbeatMs < kHeartbeatPeriodMs) {
    return;
  }
  lastHeartbeatMs = nowMs;

  const AudioMode mode = gAudioMode;
  const bool media = gA2dpStreaming;
  const bool call = gCallAudioConnected;
  const uint32_t a2dpDrops = gA2dpDropBytes;
  const uint32_t hfpInDrops = gHfpIncomingDropBytes;
  const uint32_t hfpOutPads = gHfpOutgoingPadBytes;

  Serial.print("[INFO] mode=");
  Serial.print(audioModeToString(mode));
  Serial.print(" media=");
  Serial.print(media ? "on" : "off");
  Serial.print(" callAudio=");
  Serial.print(call ? "on" : "off");
  Serial.print(" drop_bytes[a2dp=");
  Serial.print(a2dpDrops);
  Serial.print(",hfp_in=");
  Serial.print(hfpInDrops);
  Serial.print(",hfp_out_pad=");
  Serial.print(hfpOutPads);
  Serial.println("]");
}
