#include <driver/i2s.h>
#include <WiFi.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

const char* server = "YOUR_SERVER_IP";
const int port = 3000;

int seconds = 100;

#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32

const char* BOUNDARY = "----ESP32Boundary1234";

// WAV HEADER
void createWavHeader(uint8_t *header, uint32_t audio_size) {
  uint32_t file_size = audio_size + 36;
  uint32_t sample_rate = 16000;

  memcpy(&header[0], "RIFF", 4);
  *(uint32_t *)&header[4] = file_size;
  memcpy(&header[8], "WAVE", 4);
  memcpy(&header[12], "fmt ", 4);
  *(uint32_t *)&header[16] = 16;
  *(uint16_t *)&header[20] = 1;
  *(uint16_t *)&header[22] = 1;
  *(uint32_t *)&header[24] = sample_rate;
  *(uint32_t *)&header[28] = sample_rate * 2;
  *(uint16_t *)&header[32] = 2;
  *(uint16_t *)&header[34] = 16;
  memcpy(&header[36], "data", 4);
  *(uint32_t *)&header[40] = audio_size;
}

void sendAudioMultipart() {
  const int RECORD_SECONDS = 2;
  const int SAMPLE_RATE = seconds * 16000;

  const int AUDIO_SIZE = RECORD_SECONDS * SAMPLE_RATE * 2; // 16-bit output
  const int CHUNK_SAMPLES = 512;

  int32_t i2sBuffer[CHUNK_SAMPLES];   // 32-bit from mic
  int16_t pcmBuffer[CHUNK_SAMPLES];   // converted 16-bit

  WiFiClient client;

  Serial.println("Connecting...");
  if (!client.connect(server, port)) {
    Serial.println("❌ Failed");
    return;
  }

  Serial.println("✅ Connected");

  String partStart =
    String("--") + BOUNDARY + "\r\n" +
    "Content-Disposition: form-data; name=\"audio\"; filename=\"audio.wav\"\r\n" +
    "Content-Type: audio/wav\r\n\r\n";

  String partEnd =
    "\r\n--" + String(BOUNDARY) + "--\r\n";

  int contentLength =
    partStart.length() +
    44 + AUDIO_SIZE +
    partEnd.length();

  // HEADERS
  client.printf("POST /process HTTP/1.1\r\n");
  client.printf("Host: %s:%d\r\n", server, port);
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", BOUNDARY);
  client.printf("Content-Length: %d\r\n", contentLength);
  client.printf("Connection: close\r\n\r\n");

  client.print(partStart);

  // WAV HEADER
  uint8_t wav_header[44];
  createWavHeader(wav_header, AUDIO_SIZE);
  client.write(wav_header, 44);

  Serial.println("Recording...");

  size_t bytes_read;
  int total_sent = 0;

  while (total_sent < AUDIO_SIZE) {
    i2s_read(I2S_NUM_0, i2sBuffer, sizeof(i2sBuffer), &bytes_read, portMAX_DELAY);

    int samples = bytes_read / 4; // 32-bit samples

    // Convert 32-bit → 16-bit
    for (int i = 0; i < samples; i++) {
      pcmBuffer[i] = (int16_t)(i2sBuffer[i] >> 14);
    }

    int bytes_to_send = samples * 2;

    // Clamp to exact AUDIO_SIZE
    if (total_sent + bytes_to_send > AUDIO_SIZE) {
      bytes_to_send = AUDIO_SIZE - total_sent;
    }

    client.write((uint8_t*)pcmBuffer, bytes_to_send);
    total_sent += bytes_to_send;

    Serial.print(".");
  }

  client.print(partEnd);
  client.flush();

  Serial.println("\nWaiting response...");

  while (client.connected() || client.available()) {
    if (client.available()) {
      Serial.write(client.read());
    }
  }

  client.stop();
  Serial.println("\n✅ Done");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi connected");

  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);

  delay(500);

  sendAudioMultipart();

  i2s_driver_uninstall(I2S_NUM_0);
}

void loop() {}