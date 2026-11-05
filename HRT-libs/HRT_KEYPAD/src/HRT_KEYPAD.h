#pragma once

/*
 * HRT_KEYPAD - Teclado matricial 4x4 Cherry MX en ESP32
 * =====================================================
 * Barrido de la matriz con debounce por estabilidad y conversion a la tecla
 * fisica impresa. Sin diodos: las filas inactivas se dejan high-Z (INPUT),
 * nunca HIGH, para no generar contention ni caminos de fantasma entre filas.
 *
 * Cableado (placa de omnipulsera, ver omnipulsera-firmware/include/pinout.h)
 *   Filas conducidas (OUTPUT): R1=32  R2=33  R3=25  R4=26
 *   Columnas sensadas (INPUT + pull-up 10k externo): C1=34  C2=35  C3=36  C4=39
 *
 * Orientacion (validada en hardware con el conector de 8 pines invertido)
 *   La tecla fisica (r,c) -serigrafia, vista normal, '1' arriba a la izquierda-
 *   cierra el cruce electrico (3-r, 3-c). O sea: 180 grados. KEYMAP ya lo trae
 *   aplicado, keyPressed() devuelve la tecla impresa tal cual.
 *
 * Uso:
 *     #include <HRT_KEYPAD.h>
 *     HrtKeypad keypad;
 *
 *     void setup() { keypad.begin(); }
 *
 *     void loop()
 *     {
 *         keypad.update();
 *         if (keypad.hasPress())  Serial.printf("tecla %c\n", keypad.pressedKey());
 *         if (keypad.hasRelease())Serial.printf("suelta %c\n", keypad.releasedKey());
 *     }
 */

#include <Arduino.h>

class HrtKeypad
{
public:
    static constexpr uint8_t ROWS = 4;
    static constexpr uint8_t COLS = 4;

    // --- barrido electrico -------------------------------------------------
    // Filas: solo pueden ser pines OUTPUT (32/33/25/26). Las GPIO 34-39 son
    // solo-entrada, no podrian conducir el barrido.
    static constexpr uint8_t PIN_R[ROWS] = {32, 33, 25, 26};
    static constexpr uint8_t PIN_C[COLS] = {34, 35, 36, 39}; // pull-up 10k externo

    // --- tecla fisica por cruce electrico (fila barrida, columna sensada) --
    // 180 grados respecto de la serigrafia: el '1' cierra R4C4, el 'D' cierra
    // R1C1. Si alguna vez se devuelve el conector a la orientacion normal,
    // esta tabla se transpone (code(r,c) = fisica(c,r)).
    static constexpr char KEYMAP[ROWS][COLS] = {
        {'D', '#', '0', '*'},
        {'C', '9', '8', '7'},
        {'B', '6', '5', '4'},
        {'A', '3', '2', '1'},
    };

    // Sembrar el estado. Sin esto, cualquier linea que arranque en LOW
    // (tecla sostenida, o ruido acoplado desde TFT/I2S sobre una columna de
    // alta impedancia) genera un press "nuevo" en los primeros update() y la
    // UI puede entrar sola en un estado. Escanear una vez y marcar lo que ya
    // esta bajo como sostenido-de-antes: no emite press hasta que se suelte.
    void begin(uint16_t debounceMs = 25);

    // Llamar en cada vuelta de loop(). Escanea las 4 filas y aplica debounce.
    void update();

    // --- eventos del ultimo update() ---------------------------------------
    bool hasPress()   const { return _pressKey   != 0; }
    bool hasRelease() const { return _releaseKey != 0; }
    char pressedKey() const { return _pressKey;   }  // tecla impresa o 0
    char releasedKey()const { return _releaseKey; }  // tecla impresa o 0
    uint8_t pressedRow() const { return _pressR; }   // celda fisica 0..3
    uint8_t pressedCol() const { return _pressC; }

    // --- estado -------------------------------------------------------------
    bool     isDown(char key) const;   // true si la tecla impresa esta sostenida
    uint8_t downCount()      const;    // cuantas teclas sostenidas a la vez

    // --- diagnostico electrico ---------------------------------------------
    // Sonda cruda de una columna: devuelve true si la linea esta en bajo.
    //   rowDriven = 255 -> todas las filas en alta impedancia (sensa el
    //                       pull-up externo de la columna: si aun asi lee
    //                       bajo, la linea esta flotando o tirada a GND).
    //   rowDriven = 0..3 -> solo esa fila se pone en HIGH; si la columna
    //                       solo baja con una fila concreta activa, el cierre
    //                       esta en el cruce (switch o puente), no en el
    //                       pull-up.
    bool diagCol(uint8_t col, uint8_t rowDriven = 255);

    // --- medicion de rebote (para calibrar el debounce) ---------------------
    // bounceMaxMs() es el intervalo maximo medido entre dos bordes consecutivos
    // del mismo cruce durante los primeros <10 ms: ese numero es el debounce
    // real que necesita el teclado.
    uint16_t bounceCount() const { return _bounceTotal; }
    uint16_t bounceMaxMs() const { return _bounceMaxMs; }

private:
    void scanRow(uint8_t er, bool out[COLS]);

    uint16_t _debounceMs = 25;
    bool     _seeded     = false; // true tras barrer y sembrar en el 1er update()

    bool     _raw[ROWS][COLS]    = {};
    bool     _stable[ROWS][COLS] = {};
    uint32_t _edgeMs[ROWS][COLS] = {};

    char     _pressKey = 0, _releaseKey = 0;
    uint8_t  _pressR = 0, _pressC = 0;

    uint16_t _bounceTotal = 0;
    uint16_t _bounceMaxMs = 0;
};
