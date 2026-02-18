/* * -------------------------------------------------------------------------------
 * PROJECT: Open-Source Pro-Intercom MCU System
 * VERSION: 2.3.0 (Latency Debug Build)
 * DESCRIPTION: Dual-profile (A2DP & HFP) with Real-time Latency Tracking
 * -------------------------------------------------------------------------------
 */

#include "BluetoothSerial.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_hf_client_api.h"
#include "driver/i2s.h"

BluetoothSerial SerialBT;

// --- I2S PIN CONFIGURATION ---
#define I2S_NUM           I2S_NUM_0
#define I2S_BCK_IO        26
#define I2S_WS_IO         25
#define I2S_DO_IO         22 
#define I2S_DI_IO         35 

// --- STATE & LATENCY TRACKING ---
bool isCallActive = false;
bool isMediaStreaming = false;
unsigned long transitionStartTime = 0;

// --- AUDIO CALLBACKS ---

void hf_incoming_data_cb(const uint8_t *data, uint32_t len) {
    size_t bytes_written;
    i2s_write(I2S_NUM, data, len, &bytes_written, portMAX_DELAY);
}

uint32_t hf_outgoing_data_cb(uint8_t *data, uint32_t len) {
    size_t bytes_read = 0;
    i2s_read(I2S_NUM, data, len, &bytes_read, portMAX_DELAY);
    return (uint32_t)bytes_read;
}

void a2dp_sink_data_cb(const uint8_t *data, uint32_t len) {
    size_t bytes_written;
    i2s_write(I2S_NUM, data, len, &bytes_written, portMAX_DELAY);
}

// --- EVENT HANDLERS WITH LATENCY MEASUREMENT ---

void hf_client_event_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param) {
    // Event 3: Call Indicators (Battery, Signal, Call Status)
    if (event == 3) {
        // Status 1 = Call is officially active on the iPhone
        if (param->call.status == 1) {
            if (!isCallActive) {
                transitionStartTime = millis(); // Start latency timer
                Serial.println("[HFP] Call Detected! Waiting for audio channel (SCO)...");
            }
            isCallActive = true; 
        } else {
            isCallActive = false;
            Serial.println("[HFP] Call Terminated.");
        }
    }
    
    // Event 2: Audio State (The actual "pipe" for your voice)
    else if (event == 2) { 
        if (param->audio_stat.state == 2) { // 2 = Audio Connected (SCO)
            unsigned long latency = millis() - transitionStartTime;
            Serial.print("[LATENCY] Call Signaling to Audio Link: ");
            Serial.print(latency);
            Serial.println(" ms");
            Serial.println("[EVENT] MODE: VOICE_CALL_AUDIO_READY");
        } else if (param->audio_stat.state == 0) {
            Serial.println("[EVENT] MODE: VOICE_CALL_AUDIO_DISCONNECTED");
        }
    }
}

void a2dp_sink_event_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    // Event 1 = Audio State Change
    if (event == 1) {
        if (param->audio_stat.state == 2) {
            unsigned long latency = millis() - transitionStartTime;
            isMediaStreaming = true;
            Serial.print("[LATENCY] A2DP Media Stream Setup: ");
            Serial.print(latency);
            Serial.println(" ms");
            Serial.println("[EVENT] MODE: MEDIA_STREAMING_STARTED");
        } else {
            isMediaStreaming = false;
            transitionStartTime = millis(); // Start timer for next play command
            Serial.println("[EVENT] MODE: MEDIA_STREAMING_PAUSED");
        }
    }
}

void setup() {
    Serial.begin(115200);
    while(!Serial && millis() < 3000);
    delay(500); 

    Serial.println("\n###########################################");
    Serial.println("#     INTERCOM SYSTEM - LATENCY DEBUG     #");
    Serial.println("###########################################");

    // 1. Setup I2S
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };
    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_IO, .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO, .data_in_num = I2S_DI_IO
    };
    i2s_set_pin(I2S_NUM, &pin_config);

    // 2. Start Bluetooth
    if (!SerialBT.begin("esp_intercom")) {
        Serial.println("[FATAL] BT Hardware Error");
        while (1);
    }

    // 3. Set Identity
    esp_bt_cod_t cod;
    cod.major = 0x04; cod.minor = 0x08; 
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);

    // 4. Initialize Profiles
    esp_a2d_register_callback(a2dp_sink_event_cb);
    esp_a2d_sink_init();
    esp_a2d_sink_register_data_callback(a2dp_sink_data_cb);

    esp_hf_client_register_callback(hf_client_event_cb);
    esp_hf_client_init();
    esp_hf_client_register_data_callback(hf_incoming_data_cb, hf_outgoing_data_cb);

    // 5. Visibility
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    Serial.println("[SUCCESS] Pro-Intercom Online.");
}

void loop() {
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 5000) {
        lastMsg = millis();
        Serial.print("[STATUS] Ready | Active Profile: ");
        if (isCallActive) Serial.println("CALL");
        else if (isMediaStreaming) Serial.println("MEDIA");
        else Serial.println("IDLE");
    }
}
