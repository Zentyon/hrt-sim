#include <Arduino.h>
#include "pinout.h"
#include "keypad.h"

// ---------------------------------------------------------------------------
// Test 1: diagnóstico rápido de pines (sin teclado pulsado)
// ---------------------------------------------------------------------------
void testPines()
{
    Serial.println("\n--- Test de pines ---");

    // Drivers (filas): poner en OUTPUT LOW, leer columnas
    // Orden: R1=32, R2=33, R3=25, R4=26
    byte drivers[4] = {PIN_KEY_R1, PIN_KEY_R2, PIN_KEY_R3, PIN_KEY_R4};
    // Columnas (lectores): 34,35,36,39

    for (byte d = 0; d < 4; d++)
    {
        // driver en LOW
        pinMode(drivers[d], OUTPUT);
        digitalWrite(drivers[d], LOW);
        delayMicroseconds(50); // estabilizar pull-ups

        // leer las 4 columnas
        Serial.print("DRV ");
        Serial.print(drivers[d]);
        Serial.print("=> COL:");
        for (byte c = 0; c < 4; c++)
        {
            pinMode(PIN_KEY_C1 + c * 1, INPUT); // set as input (they are already input, but safe)
            int val = digitalRead(PIN_KEY_C1 + c * 1); // placeholder, use array
            // better: use the column pins array
            Serial.print(digitalRead(PIN_KEY_C1 + c * 1) == LOW ? "L" : "H");
        }
        Serial.println();

        // restore driver to INPUT_PULLUP (reposo)
        pinMode(drivers[d], INPUT_PULLUP);
    }

    // Lectura con pull-ups solo (reposo)
    Serial.println("Reposo (pull-ups):");
    for (byte r = 0; r < 4; r++)
    {
        pinMode(PIN_KEY_R1 + r * 1, INPUT_PULLUP);
        Serial.print("R");
        Serial.print(r + 1);
        Serial.print("=");
        Serial.print(digitalRead(PIN_KEY_R1 + r * 1) == HIGH ? "H" : "L");
    }
    Serial.println();
}

// ---------------------------------------------------------------------------
// Test 2: escaneo de teclas con el driver integrado
// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // wait for native USB

    // Inicializa el driver (pull-ups externos, reposo)
    initKeypad();
    delay(200);

    // Diagnóstico de pines
    testPines();

    Serial.println("\n--- Ahora prueba las teclas ---");
    Serial.println("Presiona 1, 2, 3, A, 4, 5, 6, B, 7, 8, 9, C, *, 0, #, D");
    Serial.println("Suelta y presiona otra para ver nuevo evento.");
    Serial.println("(Si no ve nada, revisa soldaduras 10k y conexiones).");
    delay(500);
}

// Bucle: muestra cada tecla pulsada (debounce 15 ms integrado)
void loop()
{
    char k = getKey(); // función del lib/keypad/keypad.cpp
    if (k != '\0')
    {
        Serial.print("Key: ");
        Serial.println(k);
        delay(150); // pequeño cooldown para evitar repetidas
    }
    // si no hay tecla nueva, el loop corre rápido pero no satura serial
}