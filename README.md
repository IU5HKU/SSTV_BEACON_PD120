# 📡 ESP32-CAM SSTV PD120 Beacon (IU5HKU)

This repository contains the source code for an **ESP32-CAM based Slow-Scan Television (SSTV) Beacon** designed to periodically capture an image, encode it using the **PD120 mode**, and transmit the resulting audio signal over a connected radio.

The system utilizes the ESP32's **Deep Sleep** functionality to significantly reduce power consumption between transmissions, making it ideal for battery-powered or remote beacon applications.

## ✨ Features

* **SSTV Mode:** PD120 (Standard for amateur radio image transmission).
* **Imaging:** Uses the integrated camera module (e.g., OV2640 on ESP32-CAM).
* **Overlay:** Adds configurable callsign and identifier text directly onto the image data.
* **Power Saving:** Implements Deep Sleep for scheduled, periodic transmissions.
* **PTT Control:** Dedicated Push-To-Talk pin for interfacing with a radio transmitter.

## 🛠️ Hardware Requirements

* **Microcontroller:** ESP32-CAM module (e.g., AI-Thinker).
* **Speaker Output:** A connection on GPIO 14 (configured as `SPEAKER_OUTPUT`) to an audio amplifier or directly to the radio's audio input.
* **PTT Interface:** A connection on GPIO 15 (configured as `PTT`) to control the radio's transmit function (PTT line).
* **Flash LED:** Connected to GPIO 4 (used for illumination during capture).

### Pin Configuration Summary

| Function | GPIO Pin | Code Macro | Activation Level |
| :--- | :--- | :--- | :--- |
| Audio Output | 14 | `SPEAKER_OUTPUT` | PWM Signal |
| Push-To-Talk (PTT) | 15 | `PTT` | HIGH |
| Flash LED | 4 | `LED_FLASH` | HIGH |
| Status LED | 33 | `LED_RED` | LOW |

## ⚙️ Software Setup

### Prerequisites

1.  **ESP32 Board Support Package** installed in your Arduino IDE or PlatformIO environment.
2.  **Required Libraries/Files:**
    * **`camera.h`**: The specific camera driver implementation for the ESP32-CAM.
    * **`sstv_pd120.h` / `sstv_pd120.c` (or .cpp)**: The core implementation for the PD120 SSTV encoding logic.

### Configuration

Before uploading, review the main settings in `SSTV_BEACON_PD120.ino`:

1.  **Sleep Interval:** Adjust the transmission period:
    ```c
    #define TIME_TO_SLEEP  60 		/* Time in seconds (60s = 1 minute) */
    ```
2.  **Overlay Text:** Customize your callsign and messages:
    ```c
    #define TEXT_TOP  "IU5HKU JN53HB" 
    #define OVERLAY_COLOR_TOP RGB565_CONV(255, 0, 255) // MAGENTA
    #define OUTLINE_TOP RGB565_CONV(0 ,0, 0)     // Text outline color (BLACK)
    #define TEXT_TOP_X  5                        // X coordinate (Horizontal)
    #define TEXT_TOP_Y  20                       // Y coordinate (Vertical)
    #define TEXT_TOP_SIZE 1                      // Text scaling factor

    #define TEXT_BOTTOM "SSTV TEST"
    ...the same for the bottom text
    ```
3.  **Pinout:** Verify the GPIO pins match your specific ESP32-CAM module or wiring setup.

## 🚀 Usage

1.  **Compile and Upload** the sketch to your ESP32-CAM board.
2.  **Monitor the Serial Output** (115200 baud) to see the device's boot count, wake-up reason, and transmission status.
3.  The device will:
    * Wake up from Deep Sleep.
    * Configure the camera and pins.
    * Capture an image (with Flash if enabled).
    * Activate the PTT pin (goes HIGH).
    * Transmit the PD120 audio signal.
    * Deactivate the PTT pin (goes LOW).
    * Enter Deep Sleep for the defined `TIME_TO_SLEEP` period.

---

This project is licensed under the Free for all users license, as specified in the source code.
