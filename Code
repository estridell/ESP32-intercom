#include "BluetoothSerial.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  delay(500);

  // 1. Starta BT via biblioteket (Detta sköter initieringen perfekt utan krascher)
  if(!SerialBT.begin("ESP32_INTERCOM")){
    Serial.println("Hårdvaran vägrar fortfarande. Testa en annan USB-port för mer ström.");
    return;
  }

  Serial.println("Hårdvara OK. Injicerar Headset-identitet...");

  // 2. Ändra identitet till Headset (Class of Device)
  // Utan detta ser iPhone bara en "Serial"-enhet och döljer den.
  esp_bt_cod_t cod;
  cod.major = 0x04; // Audio/Video
  cod.minor = 0x08; // Hands-free
  esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);

  // 3. Starta HFP-klienten manuellt
  // Det är detta som gör att den dyker upp i samtalsmenyn senare
  esp_hf_client_init();

  // 4. Gör enheten extra synlig för iPhone
  esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

  Serial.println("Klar! Sök nu i iPhone Bluetooth-inställningar.");
}
