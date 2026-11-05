#include "HRT_KEYPAD.h"

// Definiciones fuera de linea de los constexpr estaticos (C++14, el toolchain
// de Arduino-ESP32 no esta en C++17): sin esto el enlazador las da como
// indefinidas.
constexpr uint8_t HrtKeypad::PIN_R[HrtKeypad::ROWS];
constexpr uint8_t HrtKeypad::PIN_C[HrtKeypad::COLS];
constexpr char    HrtKeypad::KEYMAP[HrtKeypad::ROWS][HrtKeypad::COLS];

/*
 * Barrido high-Z: la fila activa va OUTPUT LOW, las otras se dejan INPUT
 * (alta impedancia). Sin diodos, conducir las inactivas en HIGH generaria
 * un camino fila->tecla->columna->tecla->otra-fila que el ESP32 no puede
 * distinguir de una pulsacion real: eso es el clasico fantasma. High-Z corta
 * ese camino porque las filas inactivas no pueden absorber corriente.
 */

void HrtKeypad::scanRow(uint8_t er, bool out[COLS])
{
    digitalWrite(PIN_R[er], LOW);
    for (uint8_t i = 0; i < ROWS; i++)
        pinMode(PIN_R[i], (i == er) ? OUTPUT : INPUT);
    delayMicroseconds(5); // asentamiento de la linea

    for (uint8_t c = 0; c < COLS; c++)
        out[c] = (digitalRead(PIN_C[c]) == LOW);

    pinMode(PIN_R[er], INPUT); // volver a high-Z
}

void HrtKeypad::begin(uint16_t debounceMs)
{
    _debounceMs = debounceMs;
    _seeded = false; // el barrer de arranque se hace en el primer update()

    // FIX IMPRESCINDIBLE: declarar explicitamente las entradas.
    // Tras el arranque, GPIO 34-39 (columnas, solo-entrada) pueden quedar con
    // el buffer de entrada digital deshabilitado (FUN_IE=0): digitalRead()
    // devuelve 0 aunque el pin este a 3.3 V por el pull-up externo, y la
    // matriz completa se lee como "16 teclas pulsadas" (teclado muerto).
    // Medido con ADC: 3155 mV en las 4 columnas mientras digitalRead daba LOW.
    // Solo pinMode(INPUT) garantiza que la entrada quede habilitada.
    for (uint8_t i = 0; i < ROWS; i++) pinMode(PIN_R[i], INPUT);
    for (uint8_t c = 0; c < COLS; c++) pinMode(PIN_C[c], INPUT);
}

void HrtKeypad::update()
{
    const uint32_t now = millis();

    // --- primer update(): barrer una vez y sembrar, sin emitir eventos -----
    // Lo que arranque en LOW se marca como sostenido-de-antes, no como
    // pulsacion nueva. Se hace AQUI y no en begin() porque entre initKeypad()
    // y el primer getKey() pueden pasar segundos de delay (splash, reintentos
    // de sensor) donde el ruido de TFT/I2S aun puede meter un bajo transitorio
    // en una columna de alta impedancia: sembrar en el primer update captura
    // el estado real justo cuando la app empieza a leer teclas.
    if (!_seeded)
    {
        bool line[COLS];
        for (uint8_t r = 0; r < ROWS; r++)
        {
            scanRow(r, line);
            for (uint8_t c = 0; c < COLS; c++)
            {
                _raw[r][c]    = line[c];
                _stable[r][c] = line[c];
                _edgeMs[r][c] = now;
            }
        }
        _pressKey = _releaseKey = 0;
        _pressR = _pressC = 0;
        _seeded = true;
        return;
    }

    _pressKey = _releaseKey = 0;

    for (uint8_t r = 0; r < ROWS; r++)
    {
        bool line[COLS];
        scanRow(r, line);

        for (uint8_t c = 0; c < COLS; c++)
        {
            if (line[c] != _raw[r][c])
            {
                // Medida de rebote: dos bordes del mismo cruce muy juntos.
                // Solo cuenta los primeros 10 ms, que es donde vive el rebote
                // del contacto; mas alla ya es una pulsacion real.
                const uint32_t dt = now - _edgeMs[r][c];
                _edgeMs[r][c] = now;
                if (dt > 0 && dt < 10)
                {
                    _bounceTotal++;
                    if (dt > _bounceMaxMs) _bounceMaxMs = (uint16_t)dt;
                }
                _raw[r][c] = line[c];
            }

            // Debounce por estabilidad: el cruce tiene que quedarse _debounceMs
            // ms en el mismo estado antes de declarar el evento.
            if (_raw[r][c] && !_stable[r][c] && (now - _edgeMs[r][c]) >= _debounceMs)
            {
                _stable[r][c] = true;
                _pressKey  = KEYMAP[r][c];
                _pressR = r; _pressC = c;
            }
            else if (!_raw[r][c] && _stable[r][c] && (now - _edgeMs[r][c]) >= _debounceMs)
            {
                _stable[r][c] = false;
                _releaseKey = KEYMAP[r][c];
            }
        }
    }
}

bool HrtKeypad::diagCol(uint8_t col, uint8_t rowDriven)
{
    if (col >= COLS) return false;

    if (rowDriven == 255)
    {
        // Ninguna fila conducida: la columna solo ve su pull-up externo.
        for (uint8_t i = 0; i < ROWS; i++)
            pinMode(PIN_R[i], INPUT);
    }
    else if (rowDriven < ROWS)
    {
        digitalWrite(PIN_R[rowDriven], LOW);
        for (uint8_t i = 0; i < ROWS; i++)
            pinMode(PIN_R[i], (i == rowDriven) ? OUTPUT : INPUT);
    }
    delayMicroseconds(5);

    const bool low = (digitalRead(PIN_C[col]) == LOW);

    if (rowDriven != 255 && rowDriven < ROWS)
        pinMode(PIN_R[rowDriven], INPUT); // volver a high-Z

    return low;
}

bool HrtKeypad::isDown(char key) const
{
    for (uint8_t r = 0; r < ROWS; r++)
        for (uint8_t c = 0; c < COLS; c++)
            if (_stable[r][c] && KEYMAP[r][c] == key) return true;
    return false;
}

uint8_t HrtKeypad::downCount() const
{
    uint8_t n = 0;
    for (uint8_t r = 0; r < ROWS; r++)
        for (uint8_t c = 0; c < COLS; c++)
            if (_stable[r][c]) n++;
    return n;
}
