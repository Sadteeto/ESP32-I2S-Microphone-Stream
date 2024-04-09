#include <WiFi.h>
#include <WebServer.h>
#include <driver/i2s.h>

// Replace with your network credentials

const char* ssid = "YourSSID";
const char* password = "YourPassword";

WebServer server(80);

#define I2S_PORT I2S_NUM_0
static void I2SSetup(void)
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // the mode must be set according to DSP configuration
        .sample_rate = 44100,                                // must be the same as DSP configuration
        .bits_per_sample = (i2s_bits_per_sample_t)16,        // must be the same as DSP configuration
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,         // must be the same as DSP configuration
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = 3,
        .dma_buf_len = 300,
    };
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = 41,                  // IIS_SCLK
        .ws_io_num = 42,                   // IIS_LCLK
        .data_out_num = I2S_PIN_NO_CHANGE, // IIS_DSIN
        .data_in_num = 2                   // IIS_DOUT
    };
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT);
}


// Function to send the WAV header to the client
void sendWavHeader() {
    byte header[44] = {
        0x52, 0x49, 0x46, 0x46, // RIFF
        0xFF, 0xFF, 0xFF, 0xFF, // File size, placeholder to be replaced
        0x57, 0x41, 0x56, 0x45, // WAVE
        0x66, 0x6D, 0x74, 0x20, // fmt 
        0x10, 0x00, 0x00, 0x00, // Subchunk1Size (16 for PCM)
        0x01, 0x00,             // AudioFormat (PCM = 1)
        0x01, 0x00,             // NumChannels (Mono = 1)
        0x44, 0xAC, 0x00, 0x00, // SampleRate (44100 Hz)
        0x88, 0x58, 0x01, 0x00, // ByteRate (SampleRate * NumChannels * BitsPerSample/8)
        0x02, 0x00,             // BlockAlign (NumChannels * BitsPerSample/8)
        0x10, 0x00,             // BitsPerSample
        0x64, 0x61, 0x74, 0x61, // data
        0xFF, 0xFF, 0xFF, 0xFF  // Subchunk2Size (data size, placeholder to be replaced)
    };
    server.sendContent_P((const char*)header, sizeof(header));
}


void handleAudioStream() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "audio/wav", "");

    sendWavHeader();

    const int i2s_read_len = 1024;
    int16_t i2s_read_buff[i2s_read_len];

    while (true) {
        size_t bytes_read;
        esp_err_t i2s_read_result = i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len * sizeof(int16_t), &bytes_read, portMAX_DELAY);

        if (i2s_read_result == ESP_OK && bytes_read > 0) {
            server.sendContent_P((const char*)i2s_read_buff, bytes_read);
        }

        if (!server.client().connected()) {
            Serial.println("Client Disconnected");
            break;
        }
    }
}

void setup() {
    Serial.begin(115200);
    I2SSetup();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    server.on("/stream", HTTP_GET, handleAudioStream);

    server.begin();
}

void loop() {
    server.handleClient();
}