/**
 * @file: SSTV_BEACON_PD120.ino
 * @brief: **ESP32-CAM Slow-Scan Television (SSTV) Transmitter.**
 * * This program captures an image, processes it with overlay text, 
 * encodes it using the **PD120 SSTV mode**, and transmits the resulting audio signal 
 * via a designated speaker output, controlling the **Push-To-Talk (PTT)** pin. 
 * The system is designed for **deep sleep** functionality to conserve power 
 * between transmissions.
 * * @author: IU5HKU Marco Campinoti
 * @date: November 3, 2025
 * @license: Free for all users
 */

// --- Required Library Includes ---
#include <driver/ledc.h>       //- Used for Audio PWM generation (Pulse Width Modulation)
#include <esp_task_wdt.h>     //- Watchdog Timer (needed for some ESP32 configurations)
#include <driver/rtc_io.h>      //- For managing GPIO pins during Deep Sleep mode (RTC - Real Time Clock)
#include "camera.h"       //- Custom Camera driver

// --- Deep Sleep Configuration (Power Saving) ---
#define uS_TO_S_FACTOR 1000000   /* Conversion factor for micro seconds (uS) to seconds (S) */
#define TIME_TO_SLEEP  60     /* Time in seconds (60s = 1 minute) the ESP32 will stay in Deep Sleep */  

/*******************************************************
 * FUNCTION: print_wakeup_reason
 * DESCRIPTION: Prints the cause of the ESP32 waking up
 * from Deep Sleep (Timer, external signal, etc.) to the serial monitor.
 * INPUT: None
 * OUTPUT: None
 *******************************************************/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause(); // Retrieves the wake-up cause

  switch(wakeup_reason)
  {
    // Wakeup cause: RTC_IO pin (external, e.g., button)
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    // Wakeup cause: Multiple RTC_CNTL pins (external)
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    // Wakeup cause: Timer (the most common for a periodic beacon)
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    // Other less common causes...
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    // Wakeup not caused by Deep Sleep (e.g., power-on reset)
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

/*******************************************************
 * MACRO: RGB565_CONV
 * DESCRIPTION: Converts 8-bit R, G, B components (0-255) into a single 
 * 16-bit RGB565 value. This is the standard color format for camera data.
 *******************************************************/
#define RGB565_CONV(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// --- Overlay Text Configuration (Text on Image) ---

#define TEXT_TOP  "IU5HKU JN53HB"   // Content for the top-left text (Callsign and Locator)
// COLOR, POSITION, and SIZE for the TOP TEXT
#define OVERLAY_COLOR_TOP RGB565_CONV(255, 0, 255) // MAGENTA
#define OUTLINE_TOP RGB565_CONV(0 ,0, 0)    // Text outline color (BLACK)
#define TEXT_TOP_X  5             // X coordinate (Horizontal)
#define TEXT_TOP_Y  20              // Y coordinate (Vertical)
#define TEXT_TOP_SIZE 1             // Text scaling factor

#define TEXT_BOTTOM "SSTV TEST"   // Content for the bottom-right text
// COLOR, POSITION, and SIZE for the BOTTOM TEXT
#define OVERLAY_COLOR_BTM RGB565_CONV(200, 200, 200) // GRAY
#define OUTLINE_BTM RGB565_CONV(0 , 0, 255)     // Text outline color (BLUE)
#define TEXT_BTM_X  500             // X coordinate (high value for right placement)
#define TEXT_BTM_Y  475             // Y coordinate (high value for bottom placement)
#define TEXT_BTM_SIZE 1             // Text scaling factor

// --- Hardware Pin Configuration ---

#define USE_FLASH           // Macro to enable/disable the use of the flash/LED
#define LED_FLASH    4    // Pin for the Flash LED. Activated by HIGH level.
#define PTT      15   // Push-To-Talk Pin. Activates transmission (HIGH active).
#define LED_RED    33   // Red status LED Pin (Debug/Indication). Activated by LOW level.
#define SPEAKER_OUTPUT 14   // GPIO Pin used as the PWM audio output. 

// Declaration of the timer handle pointer. 
// Used to manage the timing between sending each SSTV audio pixel.
esp_timer_handle_t pixelTimerHandle = NULL;

#include "sstv_pd120.h" // Inclusion of the specific implementation file for PD120 SSTV mode

/*******************************************************
 * FUNCTION: setup
 * DESCRIPTION: Arduino setup function. Initializes serial,
 * camera, audio/timer configuration, and starts the SSTV transmission cycle.
 * INPUT: None
 * OUTPUT: None
 *******************************************************/
void setup() {

  Serial.begin(115200);
  delay(1000);
  
  // --- Wakeup Management ---
  print_wakeup_reason(); // Prints the reason for waking up
  
  /*
   * Configuration of the wake-up source: the Timer.
   * Sets the ESP32 to wake up after TIME_TO_SLEEP seconds.
   */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.println("Start..");
  delay(500);
  
  // --- Hardware Initialization ---
  setupCamera(); // Function from camera.h driver to initialize the sensor
  
  // Configuration of control pins
  pinMode(LED_FLASH,OUTPUT);
  pinMode(LED_RED,OUTPUT);
  pinMode(PTT,OUTPUT);
  
  // Set pins to their initial resting state
  digitalWrite(LED_FLASH,LOW);  // Flash OFF
  digitalWrite(LED_RED,HIGH);   // Red LED OFF (if 'low level' active)
  
  // Disable Hold on the PTT pin so its state can be changed
  rtc_gpio_hold_dis((gpio_num_t)PTT); 
  digitalWrite(PTT,LOW);      // PTT inactive (Transceiver in receive mode)
  
  // --- PWM Audio Configuration (LEDC) ---
  
  // LEDC Timer Configuration (defines the carrier frequency)
  ledc_timer_config_t ledc_timer;
  ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_timer.duty_resolution = LEDC_TIMER_12_BIT; // Duty cycle resolution (0-4095)
  ledc_timer.timer_num = LEDC_TIMER_0;
  ledc_timer.freq_hz = 2200; // Initial audio carrier frequency (will change during SSTV transmission)
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&ledc_timer);

  // LEDC Channel Configuration (connects the timer to the output pin)
  ledc_channel_config_t ledc_channel;
  ledc_channel.channel = LEDC_CHANNEL_0;
  ledc_channel.duty = 2048; // Initial duty cycle (50% for a symmetrical square wave, 4096/2)
  ledc_channel.intr_type = LEDC_INTR_DISABLE;
  ledc_channel.gpio_num = SPEAKER_OUTPUT; // Pin connected to the speaker
  ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_channel.hpoint = 0;
  ledc_channel.timer_sel = LEDC_TIMER_0;
  ledc_channel_config(&ledc_channel);

  // Initialize audio output to 0 (silence)
  ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);

  // --- ESP-Timer Configuration for SSTV ---
  // Create a periodic timer that will be started/stopped during transmission
  // and will call the `pixelTimerCallback` function to send each audio sample.
  esp_timer_create_args_t timer_args = {
    .callback = &pixelTimerCallback, // Function called on every timer tick
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "pixel_timer"
  };
  esp_timer_create(&timer_args, &pixelTimerHandle);
  
  // --- Main Operating Cycle ---
  // Captures the image, processes it, and transmits it via SSTV
  takeAndTransmitImageViaSSTV();

  // --- Preparation for Deep Sleep ---
  Serial.println("Going to sleep now");
  Serial.flush(); 
  // Enable Hold on the PTT pin to ensure the LOW state (inactive)
  // is maintained during Deep Sleep, preventing accidental transmissions.
  rtc_gpio_hold_en((gpio_num_t)PTT); 
  esp_deep_sleep_start();
  
  // This code will never be reached, the ESP32 will reboot from Deep Sleep.
  Serial.println("This will never be printed"); 
}

void loop() {
  // Since the transmission cycle ends with deep sleep and reboot,
  // the loop() function is not needed and remains empty.
}
