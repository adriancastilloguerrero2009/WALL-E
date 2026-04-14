#include <driver/i2s.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>

const char* ssid = "ESP32_Audio_Mic";
const char* password = "password123";

// Define your pins here
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32

WiFiUDP Udp;

void setup() {
  Serial.begin(115200);


  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("Ap IP address: ");
  Serial.println(myIP);

  // Configure I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Match L/R pin grounded
    .communication_format = I2S_COMM_FORMAT_I2S,
    .dma_buf_count = 8,
    .dma_buf_len = 1024
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1, // Not used for mic
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void loop() {
  int16_t s_buffer[512];
  size_t bytes_read;

  i2s_read(I2S_NUM_0, &s_buffer, sizeof(s_buffer), &bytes_read, portMAX_DELAY);

  if (bytes_read > 0){
    Udp.beginPacket("192.168.4.2", 4444);
    Udp.write((uint8_t*)s_buffer, bytes_read);
    Udp.endPacket();
    
  }
  

}
