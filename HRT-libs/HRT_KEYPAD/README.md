# HRT_KEYPAD

Librería para el teclado matricial 4×4 Cherry MX cableado al ESP32 (placa de
omnipulsera). Sustituye el barrido manual de `omnipulsera-firmware/lib/keypad`
con una versión verificada en hardware.

## Cableado

| Señal | GPIO | Notas |
|---|---|---|
| Fila 1..4 (barrido) | 32, 33, 25, 26 | OUTPUT |
| Columna 1..4 (lectura) | 34, 35, 36, 39 | INPUT, **pull-up 10k externo** |

Las GPIO 34-39 son solo-entrada: por eso las filas (que tienen que conducir el
barrido) son siempre las de 32-33-25-26.

## Barrido high-Z (sin diodos)

La fila activa se pone OUTPUT LOW y **las demás se dejan INPUT (alta
impedancia)**, no HIGH. Con una matriz sin diodos, mantener las inactivas en
HIGH crearía un camino fila→columna→otra fila por el que el ESP32 no podría
distinguirlas: eso es el clásico "fantasma". High-Z corta ese camino.

## Mapeo (conector invertido, validado en hardware)

La tecla física `(r,c)` (serigrafía vista normal, `1` arriba a la izquierda)
cierra el cruce eléctrico `(3-r, 3-c)`: **180°**. `KEYMAP` ya lo trae aplicado,
así que `pressedKey()` devuelve la tecla impresa tal cual.

Si se revierte el conector a la orientación normal, la tabla pasa a
`code(r,c) = física(c,r)` (transpuesta).

## Uso

```cpp
#include <HRT_KEYPAD.h>

HrtKeypad keypad;

void setup() { keypad.begin(); }          // debounce por defecto: 25 ms

void loop()
{
    keypad.update();
    if (keypad.hasPress())   Serial.printf("tecla %c\n", keypad.pressedKey());
    if (keypad.hasRelease()) Serial.printf("suelta %c\n", keypad.releasedKey());
}
```

`begin(debounceMs)` permite afinar el debounce.

## Calibración del debounce

`bounceMaxMs()` devuelve el intervalo máximo medido entre dos bordes
consecutivos del mismo cruce durante los primeros 10 ms. Con ese número se
puede bajar el debounce desde los 25 ms conservadores por defecto hasta el
mínimo que tolere el teclado concreto.

## Estado del hardware

⚠️ **GPIO25 (fila 3 eléctrica) estaba sin continuidad** en la placa de
omnipulsera (soldadura pendiente). Mientras no se repare, las teclas físicas
`4, 5, 6, B` no responden: son las que caen en esa fila tras la inversión del
conector.
