/* * -------------------------------------------------------------------------------
 * PROJECT: Open-Source Pro-Intercom MCU System
 * VERSION: 2.7.0 (Stable Release)
 * DESCRIPTION: Dual-profile (A2DP Sink & HFP Client) with Full-Duplex I2S Audio
 * HARDWARE: ESP32-WROOM Series (Arduino Core 2.0.17)
 * -------------------------------------------------------------------------------
 */

#include "BluetoothSerial.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_hf_client_api.h"
#include "driver/i2s.h"
#include "nvs_flash.h"

BluetoothSerial SerialBT;

// --- I2S PIN CONFIGURATION ---
#define I2S_NUM           I2S_NUM_0
#define I2S_BCK_IO        26
#define I2S_WS_IO         25
#define I2S_DO_IO         22  // Output to Speaker/DAC (MAX98357A)
#define I2S_DI_IO         35  // Input from Microphone (INMP441)

// --- STATE & LATENCY TRACKING ---
bool isCallActive = false;
bool isMediaStreaming = false;
unsigned long transitionStartTime = 0;

// --- AUDIO DATA CALLBACKS ---

// Handles voice from the iPhone to the ESP32 (Incoming Call)
void hf_incoming_data_cb(const uint8_t *data, uint32_t len) {
    size_t bytes_written;
    i2s_write(I2S_NUM, data, len, &bytes_written, portMAX_DELAY);
}

// Handles voice from the ESP32 Microphone to the iPhone (Outgoing Call)
uint32_t hf_outgoing_data_cb(uint8_t *data, uint32_t len) {
    size_t bytes_read = 0;
    i2s_read(I2S_NUM, data, len, &bytes_read, portMAX_DELAY);
    return (uint32_t)bytes_read;
}

// Handles music from the iPhone to the ESP32 (A2DP Sink)
void a2dp_sink_data_cb(const uint8_t *data, uint32_t len) {
    size_t bytes_written;
    i2s_write(I2S_NUM, data, len, &bytes_written, portMAX_DELAY);
}

// --- EVENT HANDLERS ---

void hf_client_event_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param) {
    // Event 0 = Connection Status
    if (event == 0) {
        if (param->conn_stat.state == 1) Serial.println("[STATUS] HFP: Profile Connected");
    }
    // Event 2 = Audio State Change (The SCO link)
    else if (event == 2) { 
        if (param->audio_stat.state == 2) { // Audio Connected
            unsigned long latency = millis() - transitionStartTime;
            isCallActive = true;
            
            // Switch I2S to Voice Mode (16kHz Mono)
            i2s_set_clk(I2S_NUM, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
            
            Serial.print("[LATENCY] HFP Audio Setup: ");
            Serial.print(latency);
            Serial.println(" ms");
            Serial.println("[EVENT] MODE: VOICE_CALL_ACTIVE");
        } else {
            isCallActive = false;
            transitionStartTime = millis(); // Reset timer for next audio request
            
            // Switch I2S back to Music Mode (44.1kHz Stereo)
            i2s_set_clk(I2S_NUM, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
            
            Serial.println("[EVENT] MODE: VOICE_CALL_IDLE");
        }
    }
    // Event 3 = Call Indicators (Network/Signal)
    else if (event == 3) {
        // status 1 = call is active on the iPhone
        if (param->call.status == 1) {
            transitionStartTime = millis(); // Start tracking time until audio opens
            Serial.println("[DEBUG] HFP: Call signal detected. Preparing audio channel...");
        }
    }
}

void a2dp_sink_event_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    // Event 0 = Connection Status
    if (event == 0) {
        if (param->conn_stat.state == 2) Serial.println("[STATUS] A2DP: Profile Connected");
    }
    // Event 1 = Audio State Change
    else if (event == 1) {
        if (param->audio_stat.state == 2) { // State Started
            unsigned long latency = millis() - transitionStartTime;
            isMediaStreaming = true;
            
            //Serial.print("[LATENCY] Media Playback Setup: ");
            //Serial.print(latency);
            //Serial.println(" ms");
            Serial.println("[EVENT] MODE: MEDIA_STREAMING_STARTED");
        } else {
            isMediaStreaming = false;
            transitionStartTime = millis(); // Reset timer for accurate Play-trigger measurement
            Serial.println("[EVENT] MODE: MEDIA_STREAMING_PAUSED");
        }
    }
}

void setup() {
    // Initialize Serial with a delay to ensure Monitor is ready
    Serial.begin(115200);
    while(!Serial && millis() < 3000);
    delay(500);

    Serial.println("\n###########################################");
    Serial.println("#     PRO-INTERCOM SYSTEM INITIALIZING    #");
    Serial.println("###########################################");

    // Initialize NVS for Bluetooth Link Key persistence
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // 1. Setup I2S Hardware for Full-Duplex
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
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO,
        .data_in_num = I2S_DI_IO
    };
    i2s_set_pin(I2S_NUM, &pin_config);

    // 2. Start Bluetooth Controller
    if (!SerialBT.begin("esp_intercom")) {
        Serial.println("[FATAL] Bluetooth hardware failure.");
        while (1);
    }

    // 3. Set Device Identity (Audio Headset)
    esp_bt_cod_t cod;
    cod.major = 0x04; cod.minor = 0x08; 
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);

    // 4. Initialize A2DP (Media)
    esp_a2d_register_callback(a2dp_sink_event_cb);
    esp_a2d_sink_init();
    esp_a2d_sink_register_data_callback(a2dp_sink_data_cb);

    // 5. Initialize HFP (Calls/Intercom)
    esp_hf_client_register_callback(hf_client_event_cb);
    esp_hf_client_init();
    esp_hf_client_register_data_callback(hf_incoming_data_cb, hf_outgoing_data_cb);

    // 6. Finalize Discoverability
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    Serial.println("[SUCCESS] System ready. Scan for 'esp_intercom' on iPhone.");
    
    transitionStartTime = millis(); // Initialize timer
}

void loop() {
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 5000) {
        lastLog = millis();
        Serial.print("[STATUS] System State: ");
        if (isCallActive) Serial.println("IN_VOICE_CALL");
        else if (isMediaStreaming) Serial.println("STREAMING_MEDIA");
        else Serial.println("STANDBY_IDLE");
    }
}
