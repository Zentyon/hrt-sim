#pragma once

// I2S al MAX98357A. El amplificador solo se enciende
// durante la reproduccion (SD en HIGH) para evitar ruido de fondo.
//
// El driver vive a 16 kHz: es la tasa de los .bop de voz (Opus) y
// tambien sirve para los beeps (tonos puros), asi beep() y el
// reproductor de voz comparten un unico driver sin reinstalarlo.

void beep(int freq, int durationMs);
bool initAudio();
void audioMute();
void audioUnmute();

// Escribe PCM mono int16 a SAMPLE_RATE (bloqueante hasta encolar).
// Lo usa voice.cpp para los frames decodificados (320 muestras = 20 ms).
void audioWritePCM(const int16_t* data, size_t samples);

// Reproduce PCM mono 16-bit a SAMPLE_RATE (datos en flash; se copian
// a RAM por bloques porque el DMA de I2S no lee de flash).
void audioPlayPCM(const int16_t* pcm, size_t sampleCount, float gain = 1.0f);
