#include <Arduino.h>
#include <driver/i2s.h>
#include "pinout.h"
#include "audio.h"

// 1 = pin SD del ampli en LOW todo el tiempo (ampli apagado). Era una prueba
// de alimentacion del TFT; descartada, se deja en 0 (audio normal).
#define TEMP_SD_LOW 0

// 16 kHz: tasa de la voz Opus (.bop) y de los beeps. Un solo driver
// para todo (si cada uno usara su tasa habria que reinstalarlo).
#define SAMPLE_RATE 16000

void audioMute();
void audioUnmute();

void beep(int freq, int durationMs)
{
    // Buffer mono: 1 muestra int16 por frame (formato ONLY_LEFT).
    int frames = SAMPLE_RATE * durationMs / 1000;
    if (frames <= 0) return;
    int16_t *buf = (int16_t *)malloc(frames * sizeof(int16_t));
    if (!buf) return;

    int16_t amp = 22000;  // deja headroom para evitar recorte con el ruido electrico
    int ramp = SAMPLE_RATE * 5 / 1000;  // rampa de 5 ms para evitar clicks
    if (ramp > frames / 2) ramp = frames / 2;

    for (int i = 0; i < frames; i++)
    {
        float env = 1.0f;
        if (i < ramp)                env = (float)i / ramp;             // fade-in
        else if (i > frames - ramp)  env = (float)(frames - i) / ramp;  // fade-out

        float t = (float)i / SAMPLE_RATE;
        int16_t s = (int16_t)(sin(2.0 * 3.14159 * freq * t) * amp * env);
        buf[i] = s;  // mono
    }

    audioUnmute();

    size_t written;
    i2s_write(I2S_NUM_0, buf, frames * sizeof(int16_t), &written, portMAX_DELAY);
    free(buf);

    // Cola de silencio para separar beeps y garantizar que termine en cero
    int16_t silence[512] = {0};
    i2s_write(I2S_NUM_0, silence, sizeof(silence), &written, portMAX_DELAY);

    // Esperar a que el silencio se reproduzca (salida en 0) ANTES de mutear,
    // asi el corte de SD no produce pop/click en los beeps cortos.
    delay(25);

    // Mutear de nuevo para eliminar el ruido de fondo en idle
    audioMute();
}

bool initAudio()
{
    pinMode(PIN_I2S_SD, OUTPUT);
    digitalWrite(PIN_I2S_SD, HIGH);
    delay(10);

    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate = SAMPLE_RATE;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = 4;
    i2s_config.dma_buf_len = 256;
    i2s_config.use_apll = false;

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) return false;

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = PIN_I2S_BCLK;
    pin_config.ws_io_num = PIN_I2S_LRC;
    pin_config.data_out_num = PIN_I2S_DIN;
    // I2S_PIN_NO_CHANGE (-1) hace que el driver intente configurar un
    // pin invalido y dispare "gpio_pullup_en" en consola. Usamos un
    // GPIO real libre (RX2 = 16) como data_in (no se usa en modo TX).
    pin_config.data_in_num = 16;

    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) return false;

    i2s_zero_dma_buffer(I2S_NUM_0);

    // SD en HIGH (o LOW si TEMP_SD_LOW): el silencio se hace por datos
    // (ver audioMute()).
    digitalWrite(PIN_I2S_SD, TEMP_SD_LOW ? LOW : HIGH);
    return true;
}

void audioUnmute()
{
    digitalWrite(PIN_I2S_SD, TEMP_SD_LOW ? LOW : HIGH);
    delay(2);
}

void audioMute()
{
    // Silencio por datos, no por SD: se vacia el DMA con ceros.
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void audioWritePCM(const int16_t* data, size_t samples)
{
    if (!data || samples == 0) return;
    size_t written;
    // Timeout corto: si el DMA se colgara, mejor perder audio que
    // wedgear el loop de la app.
    i2s_write(I2S_NUM_0, data, samples * sizeof(int16_t), &written,
              pdMS_TO_TICKS(500));
}

void audioPlayPCM(const int16_t* pcm, size_t sampleCount, float gain)
{
    if (!pcm || sampleCount == 0) return;

    audioUnmute();

    const size_t CHUNK = 1024;
    int16_t* buf = (int16_t*)malloc(CHUNK * sizeof(int16_t));
    if (!buf) { audioMute(); return; }

    size_t pos = 0;
    while (pos < sampleCount)
    {
        size_t n = sampleCount - pos;
        if (n > CHUNK) n = CHUNK;
        for (size_t i = 0; i < n; i++)
        {
            float s = (float)pcm[pos + i] * gain;
            if (s > 32767.0f) s = 32767.0f;
            if (s < -32768.0f) s = -32768.0f;
            buf[i] = (int16_t)s;
        }
        size_t written;
        i2s_write(I2S_NUM_0, buf, n * sizeof(int16_t), &written, portMAX_DELAY);
        pos += n;
    }
    free(buf);

    // Cola de silencio + mute, igual que beep()
    int16_t silence[512] = {0};
    size_t written;
    i2s_write(I2S_NUM_0, silence, sizeof(silence), &written, portMAX_DELAY);
    delay(25);
    audioMute();
}
