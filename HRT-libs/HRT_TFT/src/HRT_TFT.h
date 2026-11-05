#pragma once

/*
 * HRT_TFT - Display TFT 2.8" ILI9341 320x240 en ESP32 (SPI)
 * ==========================================================
 * Concentra el pinout y la geometria medidas para este modulo, y trae el
 * objeto TFT_eSPI ya configurado.
 *
 * El arreglo del driver (driver, ancho de columna, orden de color, pines)
 * vive en la libreria TFT_eSPI que acompaña a esta:  HRT-libs/TFT_eSPI
 *
 * Cableado del modulo -> ESP32
 *   VCC   -> 3V3         CS    -> GPIO 5
 *   GND   -> GND         DC    -> GPIO 2
 *   SDI   -> GPIO 23     RESET -> GPIO 4
 *   SCK   -> GPIO 18     LED   -> 3V3   (backlight fijo)
 *
 * El modulo NO expone SDO/MISO: el panel es write-only, no se puede leer el
 * ID ni leer pixeles de vuelta por SPI.
 *
 * Uso:
 *     #include <HRT_TFT.h>
 *
 *     void setup()
 *     {
 *         hrtTftBegin();          // init + rotation(0) = 320x240
 *         tft.fillScreen(TFT_BLACK);
 *     }
 */

#include <Arduino.h>
#include <TFT_eSPI.h>

// ---------------------------------------------------------------------------
// Pinout
// Debe coincidir con HRT-libs/TFT_eSPI/User_Setup.h. Aqui solo son la referencia
// util para el resto del sketch (mismos numeros, misma orden).
// ---------------------------------------------------------------------------
constexpr uint8_t HRT_TFT_PIN_MOSI = 23;
constexpr uint8_t HRT_TFT_PIN_SCK  = 18;
constexpr uint8_t HRT_TFT_PIN_CS   = 5;
constexpr uint8_t HRT_TFT_PIN_DC   = 2;
// -1 = sin pin de reset: en el PCB hrtMonitor el RST del panel va a EN del
// ESP32 (siempre HIGH tras el boot), no a un GPIO. Ver
// backup-HRT-Monitor/ESP-HRT/conexiones_wiring.md:78. int8_t y no uint8_t
// porque -1 es la senal de "sin pin" que entiende TFT_eSPI.
constexpr int8_t  HRT_TFT_PIN_RST  = -1;
// MISO  : no existe en el modulo
// LED   : cableado directo a 3V3 (consumo de backlight fuera del alcance de un GPIO)

// ---------------------------------------------------------------------------
// Geometria medida con DisplayBounds (no la de la serigrafia)
// El vidrio tiene 320 columnas x 240 filas. La serigrafia dice "240x320" y con
// ese valor quedaban 80 columnas sin escribir a la derecha.
// ---------------------------------------------------------------------------
constexpr int16_t HRT_TFT_W = 320;
constexpr int16_t HRT_TFT_H = 240;

// ---------------------------------------------------------------------------
// Objeto compartido: declarado aqui, definido en HRT_TFT.cpp.
// No declares otro `tft` en el sketch, ya existe.
// ---------------------------------------------------------------------------
extern TFT_eSPI tft;

// ---------------------------------------------------------------------------
// hrtTftBegin()
// Inicializa el panel y fija la rotacion. Devuelve false (y avisa por `log`)
// si la geometria resultante no es la esperada: asi se detecto a su vez el
// problema original de ancho de columna.
//
//   rotation 0 y 2 -> 320x240 (landscape, el que usa el modulo)
//   rotation 1 y 3 -> 240x320 (portrait)
// ---------------------------------------------------------------------------
bool hrtTftBegin(uint8_t rotation = 0, Stream &log = Serial);

// ---------------------------------------------------------------------------
// Vuelca pinout, geometria y driver a cualquier Stream (Serial, telnet, log...)
// ---------------------------------------------------------------------------
void hrtTftPrintInfo(Stream &out);
