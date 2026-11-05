#include <Arduino.h>
#include "keypad.h"
#include <HRT_KEYPAD.h>

// Fachada sobre la libreria HRT_KEYPAD (HRT-libs). El barrido high-Z, el
// debounce por estabilidad y el mapeo 180 grados (conector invertido,
// validado en hardware) viven alla; aqui solo se adapta la API historica
// initKeypad()/getKey() que usan app.cpp y tests/keypad_test.cpp.
//
// Teclado Cherry MX 4x4 sin diodos. Filas conducidas 32/33/25/26,
// columnas sensadas 34/35/36/39 (pull-up externo 10k). MAX 2 teclas a la vez.
// OJO: mientras GPIO25 este sin soldar, las teclas 4 5 6 B no responden.

// Debounce 15 ms: Cherry MX rebota ~5 ms; 15 ms da margen sin hacerse lento.
static HrtKeypad keypad;

void initKeypad()
{
    keypad.begin(15);
}

char getKey()
{
    keypad.update();

    // Diagnostico de arranque (una sola vez): tras el barrer de siembra,
    // vuelca que teclas quedaron marcadas como "ya sostenidas". Si aparece
    // alguna sin que se toque el teclado, hay un bajo real/ruido en esa
    // linea y lo vemos por serie en vez de adivinar.
    static bool diagDone = false;
    if (!diagDone)
    {
        diagDone = true;
        // KEYMAP[3][3]='1' -> R4(26)/C4(39): la unica que ha dado fantasma.
        // Prueba discriminante: si C4 baja con las filas en high-Z, el problema
        // esta en la columna (pull-up ausente/flotante o corto a GND). Si solo
        // baja con R4 activa, el cierre esta en el cruce (switch '1' pegado o
        // puente R4<->C4).
        if (keypad.isDown('1'))
        {
            Serial.println("[keypad] AVISO: '1' ya estaba baja al arrancar (fantasma de arranque suprimido)");
            Serial.printf("[keypad] C4 con filas en high-Z: %s\n",
                          keypad.diagCol(3) ? "BAJA (columna flotante/cortada)" : "alta (pull-up OK)");
            Serial.printf("[keypad] C4 solo con R4 activa : %s\n",
                          keypad.diagCol(3, 3) ? "baja (cierre en el cruce R4C4)" : "alta (sin cierre)");
        }
        if (keypad.downCount() == 0)
            Serial.println("[keypad] arranque limpio: ninguna tecla baja");
    }

    return keypad.hasPress() ? keypad.pressedKey() : '\0';
}

uint8_t keypadDownCount()
{
    return keypad.downCount();
}

bool keypadColLow(uint8_t col)
{
    return keypad.diagCol(col, 255);
}
