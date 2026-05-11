#include <driver/i2s.h>
#include <WiFi.h>
#include <SD.h>
#include <SPI.h>

const char* ssid = "YOUR_NETWORK_NAME";
const char* password = "YOUR_NETWORK_PASSWORD";
const char* server = "YOUR_SERVER_IP";
const int port = 3000;
int seconds = 3;

// BUTTON PIN
#define BUTTON_PIN 4  // Change to your preferred GPIO pin

// I2S MICROPHONE PINS (INPUT)
#define I2S_WS_IN 25
#define I2S_SD_IN 33
#define I2S_SCK_IN 32

// I2S MAX98357A PINS (OUTPUT)
#define I2S_BCLK_OUT 26
#define I2S_LRC_OUT 27
#define I2S_DOUT 22

// SD CARD PINS
#define SD_CS 5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18

const char* BOUNDARY = "----ESP32Boundary1234";

// Button state variables
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

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

// SETUP I2S FOR INPUT (MICROPHONE)
void setupI2SInput() {
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
    .bck_io_num = I2S_SCK_IN,
    .ws_io_num = I2S_WS_IN,
    .data_out_num = -1,
    .data_in_num = I2S_SD_IN
  };

  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
}

// SETUP I2S FOR OUTPUT (SPEAKER)
void setupI2SOutput() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK_OUT,
    .ws_io_num = I2S_LRC_OUT,
    .data_out_num = I2S_DOUT,
    .data_in_num = -1
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// PLAY WAV FILE FROM SD CARD
void playWavFile(const char* filename) {
  File audioFile = SD.open(filename);
  if (!audioFile) {
    Serial.println("Failed to open audio file");
    return;
  }

  audioFile.seek(44);
  Serial.println("Playing audio...");
  
  const int bufferSize = 512;
  uint8_t buffer[bufferSize];
  size_t bytesWritten;
  
  while (audioFile.available()) {
    int bytesRead = audioFile.read(buffer, bufferSize);
    i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }
  
  audioFile.close();
  Serial.println("Audio playback complete");
}

void sendAudioMultipart() {
  const int RECORD_SECONDS = 2;
  const int SAMPLE_RATE = seconds * 16000;
  const int AUDIO_SIZE = RECORD_SECONDS * SAMPLE_RATE * 2;
  const int CHUNK_SAMPLES = 512;
  int32_t i2sBuffer[CHUNK_SAMPLES];
  int16_t pcmBuffer[CHUNK_SAMPLES];

  WiFiClient client;
  Serial.println("Connecting...");
  if (!client.connect(server, port)) {
    Serial.println("Failed");
    return;
  }
  Serial.println("Connected");

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

  client.printf("POST /process HTTP/1.1\r\n");
  client.printf("Host: %s:%d\r\n", server, port);
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", BOUNDARY);
  client.printf("Content-Length: %d\r\n", contentLength);
  client.printf("Connection: close\r\n\r\n");

  client.print(partStart);

  uint8_t wav_header[44];
  createWavHeader(wav_header, AUDIO_SIZE);
  client.write(wav_header, 44);

  Serial.println("Recording...");
  size_t bytes_read;
  int total_sent = 0;
  while (total_sent < AUDIO_SIZE) {
    i2s_read(I2S_NUM_0, i2sBuffer, sizeof(i2sBuffer), &bytes_read, portMAX_DELAY);
    int samples = bytes_read / 4;

    for (int i = 0; i < samples; i++) {
      pcmBuffer[i] = (int16_t)(i2sBuffer[i] >> 14);
    }

    int bytes_to_send = samples * 2;
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
  
  // Switch to output mode
  i2s_driver_uninstall(I2S_NUM_0);
  setupI2SOutput();
  
  bool responseStarted = false;
  
  while (client.connected() || client.available()) {
    if (client.available()) {
      if (!responseStarted) {
        responseStarted = true;
        playWavFile("/audio.wav");
      }
      Serial.write(client.read());
    }
  }

  client.stop();
  Serial.println("\nDone");
  
  // Switch back to input mode for next recording
  i2s_driver_uninstall(I2S_NUM_0);
  setupI2SInput();
  
  Serial.println("Press button to record again");
}

bool isButtonPressed() {
  bool reading = digitalRead(BUTTON_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) {  // Button pressed (active LOW)
      lastButtonState = reading;
      return true;
    }
  }
  
  lastButtonState = reading;
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Setup button with internal pull-up
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  Serial.println("ESP32 Voice Recorder");

  // Initialize SD card
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    while (1);
  }
  Serial.println("D Card initialized");

  if (!SD.exists("/audio.wav")) {
    Serial.println("audio.wav not found on SD card!");
  }

  // Connect to WiFi
  Serial.print("📡 Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Setup I2S for microphone
  setupI2SInput();
  delay(500);

  Serial.println("Press button to start recording");
}

void loop() {
  if (isButtonPressed()) {
    Serial.println("\nButton pressed!");
    delay(200);  // Small delay to avoid double-trigger
    sendAudioMultipart();
  }
  
  delay(10);  // Small delay to prevent excessive CPU usage
}