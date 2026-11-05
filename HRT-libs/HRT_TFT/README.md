# HRT_TFT

Configuración y arranque del display **TFT 2.8" ILI9341 320×240** en ESP32 por
SPI. Pensada para reusar el mismo cableado y la misma configuración en todos
los proyectos.

## Qué incluye

`HRT-libs/`
```
├── TFT_eSPI/          driver, ya configurado para este módulo
└── HRT_TFT/           esta librería: pinout, geometría y arranque
    ├── library.json
    ├── src/HRT_TFT.h
    ├── src/HRT_TFT.cpp
    └── examples/HRT_TFT_Basico/
```

Dos carpetas a propósito: la configuración de TFT_eSPI tiene que leerse desde
`User_Setup.h` **antes** de que se compile `TFT_eSPI.cpp`. Un módulo que solo
defina macros no alcanza a eso, así que el driver ya viene con su `User_Setup.h`
ajustado y `HRT_TFT` se apoya en él.

## Cableado (ESP32 ↔ módulo)

| Módulo | ESP32 | | Módulo | ESP32 |
|---|---|---|---|---|
| VCC | 3V3 | | SDI (MOSI) | GPIO 23 |
| GND | GND | | SCK | GPIO 18 |
| CS | GPIO 5 | | RESET | **EN** del ESP32 |
| DC | GPIO 2 | | LED | 3V3 |

- **MISO no existe** en este módulo: el panel es *write-only*, no se puede leer
  el ID ni los píxeles de vuelta.
- **LED a 3V3 directo**. El backlight consume más de lo que conviene sacar de un
  GPIO; para PWM hace falta transistor/MOSFET.
- **DC en GPIO 2** hace parpadear el LED onboard del ESP32. Es esperado.

## Uso con PlatformIO

```ini
[env:esp32dev]
platform  = espressif32
board     = esp32dev
framework = arduino
monitor_speed = 115200

lib_extra_dirs = C:/Users/Zent/Documents/HRT-libs
lib_ldf_mode   = deep+
```

No pongas `bodmer/TFT_eSPI` en `lib_deps`: entraría una segunda copia sin
configurar y daría error de símbolos duplicados.

```cpp
#include <HRT_TFT.h>

void setup()
{
    Serial.begin(115200);
    hrtTftPrintInfo(Serial);

    if (!hrtTftBegin())   // init + rotation(0) -> 320x240
        Serial.println("geometria inesperada");

    tft.fillScreen(TFT_BLACK);
    tft.drawString("hola", 8, 8);
}
```

`hrtTftBegin()` ya inicializa y fija rotación. El objeto `tft` lo define la
librería: **no declares otro `tft`** en tu sketch.

### API

| Función | Qué hace |
|---|---|
| `hrtTftBegin(rotation = 0, log = Serial)` | `init()` + `setRotation()`. Devuelve `false` y avisa por `log` si la geometría no es la esperada. |
| `hrtTftPrintInfo(Stream&)` | Vuelca pinout, geometría y driver. |
| `tft` | El objeto `TFT_eSPI` compartido. |
| `HRT_TFT_W` / `HRT_TFT_H` | `320` / `240`. |
| `HRT_TFT_PIN_*` | MOSI 23, SCK 18, CS 5, DC 2, RST `-1` (va a EN, no a GPIO). |

Rotaciones: **0 y 2 → 320×240 (landscape)**, **1 y 3 → 240×320 (portrait)**.

## Las dos correcciones que trae resueltas

1. **Geometría 320×240, no 240×320.** La serigrafía dice `240x320`, pero
   midiendo con `DisplayBounds` el vidrio tiene 320 columnas y 240 filas. Con la
   configuración heredada quedaban **80 columnas sin escribir a la derecha**:
   antes se veía como una banda de ruido de color, después como banda blanca.

   Complicación: `TFT_Drivers/ILI9341_Defines.h` redefinía `TFT_WIDTH` sin
   comprobar nada, así que un `-D TFT_WIDTH=320` por flags **era pisado
   silenciosamente**. El driver incluido está parcheado con `#ifndef`, así que
   ahora vale tanto por archivo como por flags:

   ```c
   #ifndef TFT_WIDTH
     #define TFT_WIDTH  240
   #endif
   ```

2. **Orden de color.** `0xF800` (rojo) se veía azul. El bit BGR del MADCTL
   estaba al revés para este panel: `User_Setup.h` define `TFT_RGB_ORDER 1`.

## Sobre `omnipulsera-firmware`

Sus tres envs (`esp32dev`, `esp32dev_mirror`, `voice_test`) traen:

```ini
lib_deps = bodmer/TFT_eSPI@^2.5.43
build_flags =
  -D TFT_WIDTH=320
  -D TFT_HEIGHT=240
  -D TFT_RST=-1
  ...
```

Ahí conviven los dos problemas: la copia de `libdeps` no está parcheada (sus
`-D TFT_WIDTH` se seguían pisando) y `TFT_RST=-1`, que es **correcto** para el
PCB: el RESET del panel va a EN del ESP32, no a un GPIO
(`conexiones_wiring.md:78`). Lo que había que arreglar era la geometría, no el
reset. Migrarlo significa quitar
`bodmer/TFT_eSPI` de `lib_deps`, pasar las bandas a `lib_extra_dirs` y usar
`hrtTftBegin()`.

## Comprobación

`examples/HRT_TFT_Basico` imprime la configuración por serie, y dibuja tres
bandas ROJO/VERDE/AZUL etiquetadas ocupando los 320 px. Si alguna etiqueta no
coincide con su color, o si queda un hueco a la derecha, algo volvió a
desconfigurarse.
