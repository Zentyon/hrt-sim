/*  ==========================================
 *  Pinout Reference - ESP32 DevKit V1
 *  ==========================================
 *
 *  Disposicion fisica de pines (vista frontal):
 *
 *  LADO IZQUIERDO              LADO DERECHO
 *  (de arriba a abajo)         (de arriba a abajo)
 *  ┌─────────────────┐        ┌─────────────────┐
 *  │ VN  (GPIO 39)   │        │ GND             │
 *  │ GND             │        │ D15 (GPIO 15)   │
 *  │ D13 (GPIO 13)   │        │ D2  (GPIO 2)    │
 *  │ D12 (GPIO 12)   │        │ D4  (GPIO 4)    │
 *  │ D14 (GPIO 14)   │        │ RX2 (GPIO 16)   │
 *  │ D27 (GPIO 27)   │        │ TX2 (GPIO 17)   │
 *  │ D26 (GPIO 26)   │        │ D5  (GPIO 5)    │
 *  │ D25 (GPIO 25)   │        │ D18 (GPIO 18)   │
 *  │ D33 (GPIO 33)   │        │ D19 (GPIO 19)   │
 *  │ D32 (GPIO 32)   │        │ D21 (GPIO 21)   │
 *  │ D35 (GPIO 35)   │        │ RX0 (GPIO 3)    │
 *  │ D34 (GPIO 34)   │        │ TX0 (GPIO 1)    │
 *  │ VN  (GPIO 39)   │        │ D22 (GPIO 22)   │
 *  │ VP  (GPIO 36)   │        │ D23 (GPIO 23)   │
 *  │ EN              │        └─────────────────┘
 *  │ 3V3             │
 *  └─────────────────┘
 *
 *  ==========================================
 */

#ifndef PINOUT_H
#define PINOUT_H

// ==================== PINES LIBRES ====================
// Estos pines estan disponibles para usar:
//
// Lado izquierdo:
//   D13 (GPIO 13) - OK
//   D12 (GPIO 12) - Cuidado: strapping pin
//   D14 (GPIO 14) - OK
//   D27 (GPIO 27) - OK
//   D26 (GPIO 26) - OK (DAC2)
//   D25 (GPIO 25) - OK (DAC1)
//   D33 (GPIO 33) - OK (ADC1)
//   D32 (GPIO 32) - OK (ADC1)
//   D35 (GPIO 35) - Solo INPUT (ADC1)
//   D34 (GPIO 34) - Solo INPUT (ADC1)
//   VN  (GPIO 39) - Solo INPUT
//   VP  (GPIO 36) - Solo INPUT
//
// Lado derecho:
//   D15 (GPIO 15) - Cuidado: strapping pin
//   D2  (GPIO 2)  - Cuidado: strapping pin + LED onboard
//   D4  (GPIO 4)  - OK
//   RX2 (GPIO 16) - OK
//   TX2 (GPIO 17) - OK
//   D5  (GPIO 5)  - OK
//   D18 (GPIO 18) - SPI SCK (default)
//   D19 (GPIO 19) - SPI MISO (default)
//   D21 (GPIO 21) - I2C SDA (default)
//   D22 (GPIO 22) - I2C SCL (default)
//   D23 (GPIO 23) - SPI MOSI (default)

// ==================== PINES FIJOS ====================
// D21 (GPIO 21) = I2C SDA - Usado por MAX30102
// D22 (GPIO 22) = I2C SCL - Usado por MAX30102
// D23 (GPIO 23) = SPI MOSI - Usado por TFT
// D18 (GPIO 18) = SPI SCK  - Usado por TFT
// D19 (GPIO 19) = SPI MISO - Reservado para touch TFT

// ==================== PINES A EVITAR ====================
// D2  (GPIO 2)  - LED onboard + strapping pin
// D15 (GPIO 15) - Strapping pin (boot log)
// D12 (GPIO 12) - Strapping pin (flash voltage)
// VN  (GPIO 39) - Solo input
// VP  (GPIO 36) - Solo input
// RX0 (GPIO 3)  - Serial monitor
// TX0 (GPIO 1)  - Serial monitor
// EN            - Reset

// ==================== ASIGNACION DE PINES ====================
//
// SENSOR MAX30102 (I2C)
#define PIN_SDA         21    // D21 → MAX30102 SDA
#define PIN_SCL         22    // D22 → MAX30102 SCL
#define MAX30102_ADDRESS 0x57

// DISPLAY TFT 2.4" ILI9341 (SPI) - Configuracion verificada
#define PIN_TFT_MOSI    23    // D23 (SPI MOSI default)
#define PIN_TFT_SCK     18    // D18 (SPI SCK default)
#define PIN_TFT_CS       5    // D5  → TFT CS
#define PIN_TFT_DC       2    // D2  → TFT DC/RS (LED onboard parpadeara)
// TFT_RST conectado a EN (reset del ESP32, siempre HIGH despues de boot)
// TFT_BL conectado directo a 3.3V (siempre encendido)

// TECLADO MATRICIAL 4x4 (Cherry MX, sin diodos: max 2 teclas a la vez)
// Deducido de ren_pcb (MOD1 E..M) + matrix4x4 (J1 1..8) con bus recto:
//   ESP32 32,33,25,26 = driver = COL1..COL4 del teclado (salidas por
//     turno: una en LOW, resto en alta impedancia).
//   ESP32 34,35,36,39 = lectores = FILA1..FILA4 del teclado (entradas
//     con pull-up externo de 10k a 3V3).
// Si el bus no va recto E->1...M->8, ajustar los defines.
// REMAP: el orden es libre; si traspone filas<->columnas, trasponer
// tambien la tabla `keys` en lib/keypad/keypad.cpp.
#define PIN_KEY_R1      32    // D32 → Fila 1
#define PIN_KEY_R2      33    // D33 → Fila 2
#define PIN_KEY_R3      25    // D25 → Fila 3
#define PIN_KEY_R4      26    // D26 → Fila 4
// Columnas (INPUT_PULLUP) - Lado izquierdo
#define PIN_KEY_C1      34    // D34 → Col 1 (input-only)
#define PIN_KEY_C2      35    // D35 → Col 2 (input-only)
#define PIN_KEY_C3      36    // VP  → Col 3 (input-only)
#define PIN_KEY_C4      39    // VN  → Col 4 (input-only)

// LEDS SEMAFORO
#define PIN_LED_VERDE   13    // D13 → LED Verde  → 330Ω → GND
#define PIN_LED_AMARILLO 14   // D14 → LED Amarillo → 330Ω → GND
#define PIN_LED_ROJO    19    // D19 → LED Rojo   → 330Ω → GND

// ==================== AUDIO I2S - MAX98357A ====================
#define PIN_I2S_BCLK    27    // D27  → MAX98357A BCLK
#define PIN_I2S_LRC     15    // D15  → MAX98357A LRC
#define PIN_I2S_DIN      4    // D4   → MAX98357A DIN
#define PIN_I2S_SD      12    // D12  → MAX98357A SD (shutdown, HIGH=encendido)

// ==================== CONFIGURACION I2C ====================
#define I2C_SPEED_FAST  400000
#define I2C_SPEED_STD   100000

#endif // PINOUT_H
