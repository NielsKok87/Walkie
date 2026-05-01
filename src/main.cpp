/*
 * ESP32 Walkie Talkie via ESP-NOW
 * DUAL CORE VERSIE
 *
 * Hardware:
 * - ESP32 (Dual-Core @ 240 MHz)
 * - INMP441 MEMS Microfoon (I2S Input)
 * - MAX98357A I2S Audio Versterker
 * - Push Button (Push-to-Talk)
 *
 * Bedrading:
 *
 * INMP441 (Microfoon) - I2S_NUM_0:
 *   SCK  -> GPIO 14 (I2S_MIC_BCLK)
 *   WS   -> GPIO 13 (I2S_MIC_WS)
 *   SD   -> GPIO 25 (I2S_SD_IN)
 *   L/R  -> GND     (Left channel)
 *   VDD  -> 3.3V
 *   GND  -> GND
 *
 * MAX98357A (Speaker) - I2S_NUM_1:
 *   BCLK -> GPIO 4  (I2S_SPK_BCLK)
 *   LRC  -> GPIO 17 (I2S_SPK_WS)
 *   DIN  -> GPIO 26 (I2S_SD_OUT)
 *   GAIN -> Float   (12dB, aanbevolen)
 *   VIN  -> 5V
 *   GND  -> GND
 *
 * Button:
 *   Een kant  -> GPIO 27
 *   Andere kant -> GND
 *
 * Core 0: ESP-NOW protocol & netwerk
 * Core 1: I2S audio lezen & afspelen
 * Communicatie via FreeRTOS queues
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <driver/i2s.h>
#include <math.h>
#include <esp_wifi_types.h>

// ============================================================================
// Pin Definitions
// ============================================================================

#define BUTTON_PIN 27 // Push-to-talk button (GPIO 0 is strapping pin, vermijd!)

// I2S Microphone pins (INMP441) - I2S_NUM_0
#define I2S_MIC_BCLK 14 // Bit Clock voor mic
#define I2S_MIC_WS 13   // Word Select voor mic
#define I2S_SD_IN 25    // Serial Data van mic

// I2S Speaker pins (MAX98357A) - I2S_NUM_1
#define I2S_SPK_BCLK 4 // Bit Clock voor speaker
#define I2S_SPK_WS 17  // Word Select voor speaker
#define I2S_SD_OUT 26  // Serial Data naar speaker

// ============================================================================
// Audio Configuration
// ============================================================================

#define SAMPLE_RATE 16000     // 16kHz sample rate (voice quality)
#define SAMPLE_BITS 16        // 16-bit samples
#define AUDIO_BUFFER_SIZE 120 // Samples per buffer (120 * 2 = 240 bytes, past in 1 ESP-NOW pakket)
#define ESP_NOW_MAX_DATA 250  // Maximum ESP-NOW packet size

// ============================================================================
// Queue Configuration
// ============================================================================

#define AUDIO_QUEUE_SIZE 8 // Number of audio buffers in queue
#define TX_QUEUE_SIZE 4    // Number of TX packets in queue

// ============================================================================
// I2S Port Numbers
// ============================================================================

#define I2S_PORT_MIC I2S_NUM_0 // Mic: eigen I2S poort
#define I2S_PORT_SPK I2S_NUM_1 // Speaker: eigen I2S poort

// ============================================================================
// Global Variables
// ============================================================================

// FreeRTOS Task Handles
TaskHandle_t audioTxTaskHandle = NULL;
TaskHandle_t audioRxTaskHandle = NULL;
TaskHandle_t networkTaskHandle = NULL;

// FreeRTOS Queues for inter-core communication
QueueHandle_t audioTxQueue = NULL; // Core 1 -> Core 0: Audio to transmit
QueueHandle_t audioRxQueue = NULL; // Core 0 -> Core 1: Received audio

// Synchronization
SemaphoreHandle_t buttonMutex = NULL;

// Button state (accessed by both cores)
volatile bool buttonPressed = false;

// Broadcast address (send to all)
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Audio buffer structure for queue
typedef struct
{
  int16_t data[AUDIO_BUFFER_SIZE];
  size_t size; // Number of samples
} AudioBuffer_t;

// Peer structure for ESP-NOW
esp_now_peer_info_t peerInfo;

// Statistics
unsigned long packetsReceived = 0;
unsigned long packetsSent = 0;
volatile int lastRSSI = -100; // RSSI van het laatste ontvangen pakket

// Promiscuous callback: pikt RSSI op van alle ontvangen WiFi frames
void promiscuousRSSI(void *buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  lastRSSI = pkt->rx_ctrl.rssi;
}

// ============================================================================
// I2S Configuration Functions
// ============================================================================

void setupI2S_Microphone()
{
  i2s_config_t cfg_mic = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = AUDIO_BUFFER_SIZE,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};
  i2s_pin_config_t pins_mic = {
      .bck_io_num = I2S_MIC_BCLK,
      .ws_io_num = I2S_MIC_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD_IN};
  i2s_driver_install(I2S_PORT_MIC, &cfg_mic, 0, NULL);
  i2s_set_pin(I2S_PORT_MIC, &pins_mic);
  i2s_zero_dma_buffer(I2S_PORT_MIC);
}

void setupI2S_Speaker()
{
  i2s_config_t cfg_spk = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = AUDIO_BUFFER_SIZE,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0};
  i2s_pin_config_t pins_spk = {
      .bck_io_num = I2S_SPK_BCLK,
      .ws_io_num = I2S_SPK_WS,
      .data_out_num = I2S_SD_OUT,
      .data_in_num = I2S_PIN_NO_CHANGE};
  i2s_driver_install(I2S_PORT_SPK, &cfg_spk, 0, NULL);
  i2s_set_pin(I2S_PORT_SPK, &pins_spk);
  i2s_zero_dma_buffer(I2S_PORT_SPK);
}

// ============================================================================
// ESP-NOW Callbacks (Run on Core 0)
// ============================================================================

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  if (status == ESP_NOW_SEND_SUCCESS)
    packetsSent++;
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  bool isReceiving = false;
  if (xSemaphoreTake(buttonMutex, 0) == pdTRUE)
  {
    isReceiving = !buttonPressed;
    xSemaphoreGive(buttonMutex);
  }

  if (isReceiving && len > 0)
  {
    // Create audio buffer and send to RX queue for Core 1 to process
    AudioBuffer_t rxBuffer;
    rxBuffer.size = len / 2; // Convert bytes to samples

    if (rxBuffer.size > AUDIO_BUFFER_SIZE)
    {
      rxBuffer.size = AUDIO_BUFFER_SIZE;
    }

    memcpy(rxBuffer.data, incomingData, rxBuffer.size * 2);

    xQueueSend(audioRxQueue, &rxBuffer, 0);
    packetsReceived++;
  }
}

// ============================================================================
// ESP-NOW Setup
// ============================================================================

void setupESPNow()
{
  // Set device as WiFi station
  WiFi.mode(WIFI_STA);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Set WiFi channel (all devices must be on same channel)
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callbacks
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // Promiscuous mode voor RSSI meting
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(promiscuousRSSI);

  // Register broadcast peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("ESP-NOW initialized");
}

// ============================================================================
// Core 1 Tasks - Audio Processing (High Priority)
// ============================================================================

// Task 1: Audio TX - Mic lezen en naar netwerk sturen
void audioTxTask(void *parameter)
{
  AudioBuffer_t txBuffer;
  size_t bytes_read;

  while (true)
  {
    bool shouldTransmit = false;
    if (xSemaphoreTake(buttonMutex, portMAX_DELAY) == pdTRUE)
    {
      shouldTransmit = buttonPressed;
      xSemaphoreGive(buttonMutex);
    }

    if (shouldTransmit)
    {
      i2s_read(I2S_PORT_MIC, txBuffer.data, AUDIO_BUFFER_SIZE * 2, &bytes_read, portMAX_DELAY);

      if (bytes_read > 0)
      {
        txBuffer.size = bytes_read / 2;
        xQueueSend(audioTxQueue, &txBuffer, portMAX_DELAY);
      }
    }
    else
    {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

// Task 2: Audio RX - Ontvangen audio afspelen via speaker
void audioRxTask(void *parameter)
{
  AudioBuffer_t rxBuffer;
  unsigned long spkWrites = 0;

  while (true)
  {
    if (xQueueReceive(audioRxQueue, &rxBuffer, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      size_t bytes_written = 0;
      esp_err_t err = i2s_write(I2S_PORT_SPK, rxBuffer.data, rxBuffer.size * 2, &bytes_written, portMAX_DELAY);
      spkWrites++;
      if (spkWrites % 200 == 0)
      {
        Serial.printf("[SPK] writes=%lu written=%d err=%d\n", spkWrites, bytes_written, err);
      }
    }
  }
}

// ============================================================================
// Core 0 Tasks - Network & Protocol Processing
// ============================================================================

// Task 3: Netwerk TX - Audio via ESP-NOW versturen
void networkTask(void *parameter)
{
  AudioBuffer_t txBuffer;

  while (true)
  {
    if (xQueueReceive(audioTxQueue, &txBuffer, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      // 120 samples * 2 bytes = 240 bytes, past in 1 ESP-NOW pakket (max 250)
      esp_now_send(broadcastAddress, (uint8_t *)txBuffer.data, txBuffer.size * 2);
    }
  }
}

// ============================================================================
// Setup Function
// ============================================================================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("ESP32-S3 Walkie Talkie");
  Serial.println("DUAL CORE OPTIMIZED");
  Serial.println("=================================");

  // Print core info
  Serial.printf("Setup running on Core: %d\n", xPortGetCoreID());
  Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());

  // Setup button with internal pullup
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Create mutexes and queues
  Serial.println("\nCreating FreeRTOS primitives...");
  buttonMutex = xSemaphoreCreateMutex();
  audioTxQueue = xQueueCreate(TX_QUEUE_SIZE, sizeof(AudioBuffer_t));
  audioRxQueue = xQueueCreate(AUDIO_QUEUE_SIZE, sizeof(AudioBuffer_t));

  if (buttonMutex == NULL || audioTxQueue == NULL || audioRxQueue == NULL)
  {
    Serial.println("ERROR: FreeRTOS init mislukt!");
    while (1)
      delay(1000);
  }
  Serial.println("FreeRTOS OK");

  // Initialize I2S
  Serial.println("\nInitializing I2S...");
  Serial.printf("  Mic BCLK: GPIO %d\n", I2S_MIC_BCLK);
  Serial.printf("  Mic WS:   GPIO %d\n", I2S_MIC_WS);
  Serial.printf("  Mic SD:   GPIO %d\n", I2S_SD_IN);
  Serial.printf("  Spk BCLK: GPIO %d\n", I2S_SPK_BCLK);
  Serial.printf("  Spk WS:   GPIO %d\n", I2S_SPK_WS);
  Serial.printf("  Spk SD:   GPIO %d\n", I2S_SD_OUT);

  setupI2S_Microphone();
  setupI2S_Speaker();
  Serial.println("I2S OK (2 aparte poorten)");

  // Initialize ESP-NOW
  Serial.println("\nInitializing ESP-NOW...");
  setupESPNow();

  // Create tasks on specific cores
  Serial.println("\nCreating tasks...");

  // Core 1 (APP_CPU) - Audio Processing (High Priority)
  xTaskCreatePinnedToCore(
      audioTxTask,        // Function
      "AudioTX",          // Name
      4096,               // Stack size
      NULL,               // Parameters
      2,                  // Priority (high)
      &audioTxTaskHandle, // Handle
      1                   // Core 1 (APP_CPU)
  );

  xTaskCreatePinnedToCore(
      audioRxTask,
      "AudioRX",
      4096,
      NULL,
      2, // Priority (high)
      &audioRxTaskHandle,
      1 // Core 1 (APP_CPU)
  );

  // Core 0 (PRO_CPU) - Network Processing (Medium Priority)
  xTaskCreatePinnedToCore(
      networkTask,
      "Network",
      4096,
      NULL,
      1, // Priority (medium, matches WiFi)
      &networkTaskHandle,
      0 // Core 0 (PRO_CPU, same as WiFi stack)
  );

  Serial.println("✓ Tasks created:");
  Serial.println("  - Core 0: Network/ESP-NOW");
  Serial.println("  - Core 1: Audio TX/RX");

  Serial.println("\n=================================");
  Serial.println("Walkie Talkie Ready!");
  Serial.println("Press and hold button to talk");
  Serial.println("=================================\n");
}

// ============================================================================
// Main Loop (Runs on Core 1)
// ============================================================================

void loop()
{
  // Button monitoring and statistics on Core 1
  static bool lastButtonState = false;
  bool currentButtonState = (digitalRead(BUTTON_PIN) == LOW);

  // Update button state with mutex protection
  if (currentButtonState != lastButtonState)
  {
    if (xSemaphoreTake(buttonMutex, portMAX_DELAY) == pdTRUE)
    {
      buttonPressed = currentButtonState;
      xSemaphoreGive(buttonMutex);
    }

    if (currentButtonState)
    {
      Serial.println("KNOP: ZENDEN");
      i2s_zero_dma_buffer(I2S_PORT_SPK);
      xQueueReset(audioRxQueue);
    }
    else
    {
      Serial.println("KNOP: ONTVANGEN");
      i2s_zero_dma_buffer(I2S_PORT_MIC);
      xQueueReset(audioTxQueue);
    }
    lastButtonState = currentButtonState;
  }

  // Stats elke 3 seconden
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 3000)
  {
    Serial.printf("Sent=%lu Rcv=%lu Btn=%d", packetsSent, packetsReceived, currentButtonState);
    if (lastRSSI > -100) {
      // Log-distance padverliesdmodel: RSSI_ref=-40dBm op 1m, n=2.7 (binnen)
      float dist = pow(10.0f, (-40.0f - lastRSSI) / 27.0f);
      Serial.printf(" RSSI=%ddBm ~%.1fm", lastRSSI, dist);
    }
    Serial.println();
    lastStats = millis();
  }

  // Delay to prevent busy-waiting
  delay(10);
}
