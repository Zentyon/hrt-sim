#pragma once
#include <Arduino.h>

// Voz: reproduce las frases del sistema en Opus (.bop) desde LittleFS.
//
// Archivos: /v/es/XXNN.bop y /v/en/XXNN.bop (subidos con
// `pio run -t uploadfs` desde data/v/). El idioma lo toma de
// profile.lang ('E' -> es, 'I' -> en).
//
// Formato .bop (ver tools/convert_audio.py):
//   "BOP1" (4 B) | u16 LE sampleRate (16000) | u16 LE flags (0)
//   luego N veces: u16 LE len | payload Opus (frames de 20 ms)
//
// playback bloqueante: la app reproduce la frase y despues sigue
// leyendo teclas (el modelo del firmware original con beep()).

// Monta LittleFS y crea el decoder Opus. false si algo fallo (la app
// sigue funcionando sin voz; solo se pierde el audio).
bool voiceInit();

bool voiceAvailable();

// "A01" -> /v/es/A01.bop (o /v/en/...). Silencio si falta el archivo.
void voicePlay(const char *id);
