#include <Arduino.h>
#include <LittleFS.h>
#include <opus.h>
#include "voice.h"
#include "audio.h"
#include "profile.h"

// Decoder Opus mono a 16 kHz (la tasa de los .bop). opus_decode emite
// int16; el maximo por frame aqui es de 320 muestras (20 ms), pero se
// pasa un tope mayor por si un packet trae frames mas largos.
#define VOICE_RATE      16000
#define VOICE_MAX_FRAME 960

static bool fsOk = false;
static OpusDecoder *dec = nullptr;

static const char *langDir()
{
    return (lang == 'I') ? "en" : "es";
}

bool voiceInit()
{
    // true = formatear si no esta montado (primera vez sin uploadfs)
    Serial.println("[voice] montando LittleFS...");
    fsOk = LittleFS.begin(true);
    Serial.printf("[voice] mount=%d heap=%lu\n", fsOk ? 1 : 0,
                  (unsigned long)ESP.getFreeHeap());
    if (!fsOk)
    {
        Serial.println("[voice] ERROR: LittleFS no se pudo montar");
        return false;
    }

    int err = OPUS_OK;
    Serial.println("[voice] creando decoder Opus...");
    dec = opus_decoder_create(VOICE_RATE, 1, &err);
    Serial.printf("[voice] dec=%p err=%d heap=%lu\n", (void *)dec, err,
                  (unsigned long)ESP.getFreeHeap());
    if (err != OPUS_OK || dec == nullptr)
    {
        dec = nullptr;
        Serial.printf("[voice] ERROR: opus_decoder_create fallo (%d)\n", err);
        return false;
    }

    // Contar archivos presentes (feedback de boot)
    int nEs = 0, nEn = 0;
    {
        File d = LittleFS.open("/v/es");
        if (d && d.isDirectory())
        {
            File e = d.openNextFile();
            while (e) { nEs++; e = d.openNextFile(); }
        }
        if (d) d.close();
        d = LittleFS.open("/v/en");
        if (d && d.isDirectory())
        {
            File e = d.openNextFile();
            while (e) { nEn++; e = d.openNextFile(); }
        }
        if (d) d.close();
    }
    Serial.printf("[voice] LittleFS OK, Opus OK, frases: es=%d en=%d\n", nEs, nEn);
    return true;
}

bool voiceAvailable()
{
    return fsOk && dec != nullptr;
}

void voicePlay(const char *id)
{
    if (!fsOk || dec == nullptr || id == nullptr) return;

    char path[32];
    snprintf(path, sizeof(path), "/v/%s/%s.bop", langDir(), id);

    File f = LittleFS.open(path, "r");
    if (!f)
    {
        Serial.printf("[voice] falta %s (¿uploadfs?)\n", path);
        return;
    }

    // Cabecera
    uint8_t hdr[8];
    if (f.read(hdr, 8) != 8 || memcmp(hdr, "BOP1", 4) != 0)
    {
        Serial.printf("[voice] %s: cabecera invalida\n", path);
        f.close();
        return;
    }
    uint16_t rate = hdr[4] | (hdr[5] << 8);
    if (rate != VOICE_RATE)
    {
        Serial.printf("[voice] %s: tasa %u != %d\n", path, rate, VOICE_RATE);
        f.close();
        return;
    }

    static uint8_t pkt[512];
    static int16_t pcm[VOICE_MAX_FRAME];

    audioUnmute();

    uint8_t lb[2];
    while (f.available() >= 2)
    {
        if (f.read(lb, 2) != 2) break;
        int len = lb[0] | (lb[1] << 8);
        if (len <= 0 || len > (int)sizeof(pkt)) break;
        if (f.read(pkt, len) != len) break;

        int n = opus_decode(dec, (const unsigned char *)pkt, len,
                            pcm, VOICE_MAX_FRAME, 0);
        if (n < 0)
        {
            Serial.printf("[voice] decode error %d en %s\n", n, path);
            break;
        }
        audioWritePCM(pcm, (size_t)n);
    }

    f.close();

    // Cola de silencio corta + mute (mismo patron que beep())
    int16_t silence[256] = {0};
    audioWritePCM(silence, 256);
    delay(20);
    audioMute();

    // Reiniciar el estado del decoder para la proxima frase
    opus_decoder_ctl(dec, OPUS_RESET_STATE);
}
