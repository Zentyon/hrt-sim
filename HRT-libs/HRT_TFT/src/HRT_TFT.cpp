#include "HRT_TFT.h"

// Definicion del objeto compartido. El nombre `tft` es el mismo que usan los
// ejemplos de TFT_eSPI, para que cualquier ejemplo oficial compile sin cambios.
TFT_eSPI tft = TFT_eSPI();

bool hrtTftBegin(uint8_t rotation, Stream &log)
{
    tft.init();
    tft.setRotation(rotation);

    // En este panel rot 0 y 2 dan landscape 320x240; rot 1 y 3, portrait.
    const bool landscape = ((rotation % 4) == 0) || ((rotation % 4) == 2);
    const int32_t expectW = landscape ? HRT_TFT_W : HRT_TFT_H;
    const int32_t expectH = landscape ? HRT_TFT_H : HRT_TFT_W;

    if (tft.width() != expectW || tft.height() != expectH)
    {
        // printf() lo aporta Print en el core ESP32 (la plataforma que soportamos)
        log.printf("[HRT_TFT] AVISO: se esperaba %ldx%ld y TFT_eSPI reporta %ldx%ld\n",
                   (long)expectW, (long)expectH,
                   (long)tft.width(), (long)tft.height());
        log.println("[HRT_TFT] Revisa TFT_WIDTH / TFT_HEIGHT en HRT-libs/TFT_eSPI/User_Setup.h");
        return false;
    }

    return true;
}

void hrtTftPrintInfo(Stream &out)
{
    out.println("=== HRT_TFT ===");
    out.println("driver   : ILI9341 (TFT_eSPI)");
    out.printf("geometria: %ld x %ld  (rot 0/2 = landscape)\n",
               (long)HRT_TFT_W, (long)HRT_TFT_H);
    out.printf("MOSI=%u  SCK=%u  CS=%u  DC=%u  RST=%d\n",
               HRT_TFT_PIN_MOSI, HRT_TFT_PIN_SCK, HRT_TFT_PIN_CS,
               HRT_TFT_PIN_DC, HRT_TFT_PIN_RST);
    out.println("RST     : -1 = cableado a EN del ESP32 (sin GPIO propio)");
    out.println("MISO     : - (modulo sin SDO, panel write-only)");
    out.println("LED      : a 3V3 directo (backlight fijo)");
}
