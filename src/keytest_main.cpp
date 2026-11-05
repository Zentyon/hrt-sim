#include <Arduino.h>
#include <HRT_KEYPAD.h>

// ---------------------------------------------------------------------------
// Teclado AISLADO: mismo codigo que examples/Basic/Basic.ino de HRT-libs
// (HRT_KEYPAD), validado en hardware. Sin BLE, sin I2S, sin task PPG, sin
// display: si aqui el teclado responde, el hardware y el driver estan bien y
// la culpa es de otro subsistema del firmware completo. Si tampoco responde
// aqui, el problema es electrico/hardware, no de software.
//
// Flashear: pio run -e keytest -t upload --upload-port COM4
// ---------------------------------------------------------------------------

HrtKeypad keypad;

// Lectura cruda de las 4 columnas (34/35/36/39) en distintos estados de fila.
// Si una columna lee BAJO con todas las filas en high-Z, su pull-up externo
// no llega (o hay corto a GND): el problema es electrico, no de firmware.
static void dumpCols(const char *tag, uint8_t rowDriven)
{
    Serial.printf("%s COL[34,35,36,39] =", tag);
    for (uint8_t c = 0; c < HrtKeypad::COLS; c++)
        Serial.print(keypad.diagCol(c, rowDriven) ? " L" : " H");
    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    keypad.begin(15); // mismo debounce que usa el sim (facada lib/keypad)
    Serial.println("Teclado 4x4 (test aislado HRT-libs). Pulsa teclas...");
    delay(300);
    dumpCols("[reposo high-Z]  ", 255);
    for (uint8_t r = 0; r < HrtKeypad::ROWS; r++)
    {
        char tag[24];
        snprintf(tag, sizeof(tag), "[fila %u activa]  ", r + 1);
        dumpCols(tag, r);
    }
}

void loop()
{
    keypad.update();

    if (keypad.hasPress())
    {
        Serial.printf("PRESS  %c  (fila %u, col %u, sostenidas %u)\n",
                      keypad.pressedKey(),
                      keypad.pressedRow() + 1, keypad.pressedCol() + 1,
                      keypad.downCount());
    }

    if (keypad.hasRelease())
        Serial.printf("RELEASE %c\n", keypad.releasedKey());

    // Estadistica de rebote + columnas crudas cada 10 s
    static uint32_t lastStat = 0;
    if (millis() - lastStat > 10000)
    {
        lastStat = millis();
        Serial.printf("[diag] rebotes=%u max=%ums teclas_abajo=%u\n",
                      keypad.bounceCount(), keypad.bounceMaxMs(),
                      keypad.downCount());
        dumpCols("[reposo high-Z]  ", 255);
    }
}
