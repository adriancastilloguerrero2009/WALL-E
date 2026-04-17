#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>

// Network credentials
const char* ssid = "YOUR_NETWORK_NAME";
const char* password = "YOUR_NETWORK_PASSWORD";
const char* serverUrl = "http://10.200.57.121:3000/";

// I2S Pin Definitions
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP address: ");
  Serial.println(WiFi.localIP());

  // I2S Configuration
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void loop() {
  int16_t s_buffer[512];
  size_t bytes_read;

  // Read from I2S
  i2s_read(I2S_NUM_0, &s_buffer, sizeof(s_buffer), &bytes_read, portMAX_DELAY);

  // Send data over HTTP if connected
  if (bytes_read > 0 && WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/octet-stream");

    int httpResponseCode = http.POST((uint8_t*)s_buffer, bytes_read);

    if (httpResponseCode < 0) {
      Serial.printf("Error occurred: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    
    http.end();
  }
}
