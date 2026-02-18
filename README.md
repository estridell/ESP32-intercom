# ESP32-Intercom: Bluetooth A2DP & HFP for iPhone

A high-fidelity Bluetooth audio system using Dual-Profile handshaking (Music & Calls).

📖 Overview

This project implements a dual-profile Bluetooth system on the ESP32, specifically optimized for iPhone integration. It enables high-quality music streaming via A2DP and seamless switching to duplex voice communication via HFP (Hands-Free Profile).

Technical Implementation Summary
"The system utilizes a state-driven event handler to manage the transition between Bluetooth A2DP and HFP profiles. By intercepting low-level protocol events (SCO/ACL), the firmware dynamically reconfigures the I2S hardware clock frequency in real-time. This ensures synchronization between the MCU's internal DAC/ADC and the iPhone's variable sample rates (switching from 44.1kHz stereo media to 16kHz mono wideband speech), effectively eliminating digital pitch-shift and latency jitter during profile handovers."

By utilizing digital I2S communication, it bypasses the internal 8-bit DAC of the ESP32 to provide professional-grade audio fidelity for DIY helmet intercoms or hands-free systems.
✨ Key Features

    Dual-Profile Handshaking: Automatic switching between A2DP (Media) and HFP (Voice/Calls).

    Dynamic I2S Clock Scaling: Real-time hardware re-syncing between 44.1kHz (Music) and 16kHz (Wideband Speech).

    Full-Duplex Audio: Simultaneous high-quality speaker output and digital microphone input.

    Persistence: Bluetooth Link Keys stored in NVS (Non-Volatile Storage) for reliable auto-reconnection.

    Latency Debugging: Built-in real-time monitoring of profile transition speeds.

🛠 Hardware Setup

To ensure digital audio quality, the following I2S components are recommended:
Component	Purpose	Recommended Model
MCU	Main Controller	ESP32-WROOM-32
DAC / Amp	Audio Output	MAX98357A
Microphone	Audio Input	INMP441 (Digital I2S)
Pin Mapping

    BCLK (Bit Clock): GPIO 26

    LRCK / WS (Word Select): GPIO 25

    DIN (Data Out to Speaker): GPIO 22

    DOUT (Data In from Mic): GPIO 35

💻 Software Requirements

    Arduino ESP32 Core: v2.0.17 (Stable)

    Partition Scheme: "Huge APP" or similar (to accommodate the Bluetooth stack)

Dependencies

    ESP32-A2DP by pschatzmann

    Arduino-Audio-Tools

🚀 Installation

    Set up your Arduino IDE for ESP32 (version 2.0.17).

    Install the required libraries listed under Dependencies.

    Flash the esp_intercom.ino sketch.

    Open the Serial Monitor at 115200 baud.

    Search for esp_intercom on your iPhone and pair.

📡 System Status Logs

The firmware provides real-time feedback via the Serial Monitor:

    [STATUS]: Connection handshakes and profile readiness.

    [EVENT]: Profile transitions (e.g., from Media to Voice).

    [LATENCY]: Timing data for profile switching (measured in ms).

    [INFO]: Heartbeat monitor showing current active mode.

📄 License

Distributed under the MIT License. See LICENSE for more information.
