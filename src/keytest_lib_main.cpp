/*
 * Copia literal de HRT-libs/HRT_KEYPAD/examples/Basic/Basic.ino
 * (env:keytest_lib). Firmware = el propio test de la libreria, sin cambios.
 */

#include <HRT_KEYPAD.h>

HrtKeypad keypad;

void setup()
{
    Serial.begin(115200);
    keypad.begin();          // 25 ms de debounce
    Serial.println("Teclado 4x4 listo. Pulsa teclas...");
}

void loop()
{
    keypad.update();

    if (keypad.hasPress())
        Serial.printf("PRESS  %c  (fila %u, col %u, sostenidas %u)\n",
                      keypad.pressedKey(),
                      keypad.pressedRow() + 1, keypad.pressedCol() + 1,
                      keypad.downCount());

    if (keypad.hasRelease())
        Serial.printf("RELEASE %c\n", keypad.releasedKey());
}
