# HRT-Sim

Firmware de **imitación** del monitor HRT para ESP32 DevKit V1: el flujo
completo del diagrama (registro → menú → medición 2 min → clasificación
verde/amarillo/rojo) **sin sensor MAX30102 real** — el PPG es simulado
(`lib/ppgsim`) con la misma superficie de API que el firmware real, así
que la UI, el teclado, la voz y el BLE se comportan igual.

> El objetivo es poder desarrollar y demostrar el sistema entero aunque
> el bus I2C de la placa física hoy tenga SCL retenido (diagnóstico
> aparte en omnipulsera-firmware).

## Requisitos cubiertos

1. **Pinout final** — `include/pinout.h` idéntico al firmware real
   (TFT ILI9341, teclado 4×4, I2S MAX98357A, LEDs).
2. **Display y teclado** — vía HRT-libs (submódulo `HRT-libs/`):
   HRT_TFT + HRT_KEYPAD, con el fix del fantasma de '1' incluido.
3. **Voz** — 124 frases × 2 idiomas convertidas a **Opus ~8 kbps**
   (16 kHz mono, frames de 20 ms) en el contenedor `.bop` (ver abajo),
   viviendo en LittleFS (`data/v/{es,en}/`). Idioma según `profile.lang`.
4. **Algoritmo** — máquina de estados de `lib/app/app.cpp` fiel al
   diagrama de flujo (ver esquema en app.h).
5. **Medición simulada** — `lib/ppgsim` genera un PPG sintético a 100 Hz
   (pico sistólico + muesca dicrotica + ruido + HRV) en task dedicada
   (core 0) y **detecta los latidos sobre la propia señal**; el BPM
   mostrado sale de esa detección, no del número "a mano".
6. **BLE** — NimBLE GATT (perfil JSON R/W, medición notify, comando
   write). Protocolo abajo; UUIDs marcados como TODO.
7. **Sin error de sensor** — `sensorInit()` del sim siempre tiene éxito;
   no existe la pantalla "Sensor no detectado".

## Compilar y flashear

Las librerías viven en el submódulo `HRT-libs/` (repo `Zentyon/hrt-libs`), así
que hay que clonar con submódulos:

```bash
git clone --recursive https://github.com/Zentyon/hrt-sim.git
# si ya está clonado sin --recursive:
git submodule update --init --recursive
```

```bash
pio run -e esp32dev                 # compila
pio run -e esp32dev -t upload       # flashea la app (COM4; si falla, reintentar)
pio run -e esp32dev -t uploadfs     # sube LittleFS (los .bop de data/v/)
```

Partición personalizada sin OTA (`partitions.csv`): app 1.63 MB +
LittleFS 2.31 MB (los audios ocupan ~1.02 MB). Monitor: 115200.

## Nota de hardware: habilitar la entrada de GPIO 34-39 (teclado)

Las columnas del teclado son GPIO 34/35/36/39 (solo-entrada, con pull-up
externo de 10k a 3V3). Tras el arranque esos pines pueden quedar con el
**buffer de entrada digital deshabilitado** (`FUN_IE = 0`): `digitalRead()`
devuelve 0 aunque el pin esté a 3,3 V. Síntoma exacto que tuvimos: las 4
columnas leían `L` en reposo y la matriz completa se veía como "16 teclas
pulsadas" (teclado muerto), mientras el ADC medía **3155 mV** en las mismas
4 líneas. Ningún pin del ampli/LEDs/TFT tenía la culpa.

Arreglo: `HrtKeypad::begin()` hace `pinMode(pin, INPUT)` explícito en las 4
filas y las 4 columnas antes del primer barrido (HRT-libs/HRT_KEYPAD).
Cualquier proyecto que lea 34-39 debe hacer lo mismo.

Aparte, `lib/audio` mantiene **SD del MAX98357A siempre en HIGH** y silencia
vaciando el DMA con ceros (`i2s_zero_dma_buffer`): evita el "pop" al
encender/apagar el amplificador.

## Teclas (leyendas físicas del teclado)

| Tecla | Leyenda | Función |
|-------|---------|---------|
| A | ● Aceptar | confirmar / sí |
| B | ✕ Cancelar | cancelar / no |
| C | │ Regresar | borrar / anterior |
| D | ☰ Menú | repetir opciones del menú |
| * | F rosa | femenino |
| # | M azul | masculino |
| 0-9 | números | edad, listas 1-7 / 1-6, menú 1-2 |

## Comportamiento simulado (a proposito)

- **La muneca esta "puesta" desde el inicio**: no hay fase de
  no-deteccion; la senal se estabiliza (~1.5 s) y arranca el cuenta
  atras de 3 s hacia la medicion de 2 minutos.
- **La lectura fake sale siempre VERDE**: el BPM simulado es el centro
  del rango del perfil → led verde + banda "Verde: adecuado" siempre.
  (Cuando llegue la tabla de referencia real, aqui se eligira el caso.)
- **Tecla D en la pantalla de senal**: alterna la muneca
  puesta/quitada a mano (depuración; no está en la UI). Si la quitas
  a mitad de medicion, vuelve el flujo real de reintento (J07).
- **SpO2** simulado 96-99 %, informativo en pantalla; no entra en la
  clasificación.

## BLE (NimBLE)

UUIDs hoy de **reemplazo** (TODO: sustituir por los de la app existente
— están solo en `lib/ble/ble.cpp`, 4 strings):

- Servicio `b1e50001-6f2d-4a4e-9c11-5f8a2d3e7b01` (nombre "HRT-Sim")
- `...0002` PROFILE — R/W JSON del perfil:
  `{"lang":"E","age":30,"gender":"M","exercise":1,"sport":0,"level":0,"condition":0}`
- `...0003` MEAS — READ/NOTIFY JSON de la última medición:
  `{"bpm":72,"avg":71,"max":78,"min":66,"spo2":98,"spo2Valid":true,"cat":0,"age":30,"gender":"M"}`
  (`cat`: 0 verde, 1 amarillo, 2 rojo, -1 sin datos)
- `...0004` CMD — WRITE JSON `{"cmd":"..."}` (`measure|cancel|ping`;
  receiver todavia no dispara nada, ver TODO)

La app puede **escribir el perfil** (se aplica con `profileFromJson` y se
guarda en NVS — hoy el handler de CMD está pendiente de definir con la
app real). Tras cada medición se notifica MEAS.

## Formato de audio .bop

```
"BOP1" (4 B) | u16 LE sampleRate (16000) | u16 LE flags (0)
luego N veces: u16 LE len | payload Opus (frame de 20 ms = 320 muestras)
```

Origen: `.ogg` TTS (24 kHz) de tus Descargas → ffmpeg
(`libopus -b:a 8000 -ac 1 -ar 16000 -frame_duration 20 -application voip
-vbr on`) → parseo Ogg → packets → `.bop`. El script quedó en
`tools/convert_audio.py`; los archivos generados están en `data/v/`
(ES 580 KB + EN 490 KB, ~20 min de voz en total). Decoder en el
dispositivo: `lib/voice` sobre `sh123/esp32_opus` (libopus fixed-point).

## Pendientes (TODO)

1. **UUIDs BLE reales** de tu app → sustituir en `lib/ble/ble.cpp`.
2. **Tabla completa de referencia BPM** → sustituir la aproximación de
   `computeRange()` en `lib/profile/profile.cpp` (marcado con TODO).
3. **CMD remoto** (`{"cmd":"measure"}`) → definir con tu app.
4. **K02**: el audio dice un número fijo; el BPM real va en la pantalla
   y en el JSON de BLE. Si quieres, se regenera el TTS con número
   variable (sería sintetizar la frase en el dispositivo o añadir más
   audios).
5. Registro editable desde la app (PROFILE write ya llega; falta
   refrescar la UI si cambia en caliente).

## Estructura

```
include/pinout.h      pines definitivos
lib/ppgsim            sensor PPG simulado (misma API que lib/sensor real)
lib/app               máquina de estados (diagrama de flujo)
lib/display           pantallas TFT (HRT-libs)
lib/keypad            teclado 4x4 (HRT-libs)
lib/audio             I2S MAX98357A (beeps + PCM, 16 kHz)
lib/voice             reproductor de frases .bop (Opus + LittleFS)
lib/ble               NimBLE GATT (JSON)
lib/profile           perfil NVS + tablas de rangos + JSON BLE
data/v/{es,en}        248 archivos .bop (subir con uploadfs)
tools/convert_audio.py  conversor ogg→bop (reproducible)
```
