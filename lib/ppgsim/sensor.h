#pragma once
#include <Arduino.h>

// =====================================================================
// PPGSIM - sustituto del MAX30102 real para el firmware de imitacion.
//
// Genera una senal PPG sintetica plausible a 100 Hz en una task
// dedicada (core 0), con deteccion de latidos sobre la propia senal
// (no con el target directo), de modo que la cadena
// senal -> latidos -> BPM -> min/max/prom/SpO2 se comporta como en el
// firmware real y toda la UI funciona igual.
//
// La superficie de API es la MISMA que lib/sensor del firmware real
// (sensorInit, sensorReadMeasure, sensorBeatLost, sensorBeginMeasurement,
// sensorEndCalibration y los globals rates/bpm/avgBpm/...), mas un
// bloque de control de la simulacion (simSetFinger, simSetTargetBpm...)
// usado por app.cpp y por depuracion por teclado.
// =====================================================================

#define RATE_SIZE 16

// Estado compartido (task core 0 escribe; loop core 1 lee).
extern volatile byte rates[RATE_SIZE];
extern volatile byte rateSpot;

extern volatile long lastBeat;
extern volatile float bpm;
extern volatile int avgBpm;
extern volatile int maxBpm;
extern volatile int minBpm;
extern volatile int bpmCount;

extern volatile bool normInit;
extern volatile bool schmittState;
extern volatile bool wristPresent;    // = dedo presente + senal estable

// El sim SIEMPRE esta "disponible": no hay bus I2C que fallar.
// (mantiene el nombre del firmware real para no romper display/app)
extern volatile bool sensorAvailable;

// SpO2 simulado (solo informativo en pantalla; se ignora en la
// clasificacion, que depende unicamente del BPM).
extern volatile int spo2;             // 0-100
extern volatile bool spo2Valid;
extern volatile float perfusion;      // PI simulado (%)

// (compatibilidad con el firmware real; el sim no usa un filtro DC)
extern const float ALPHA_DC;

struct SensorMeasure {
    int  avgBpm;
    int  maxBpm;
    int  minBpm;
    int  spo2;
    bool spo2Valid;
    bool wristPresent;
};

// Arranca la task de simulacion. Siempre true (no hay hardware que
// fallar: requisito del sim = nunca mostrar "sensor no detectado").
bool sensorInit();

// Copia coherente (atomica frente a la task) del estado de medicion.
void sensorReadMeasure(SensorMeasure &m);

// true si no hay senal util (sin dedo, o dedo pero >3 s sin latido).
bool sensorBeatLost();

// Reinicia estadisticas por medicion (BPM count, min, max, SpO2).
// sensorBeginMeasurement() llama internamente a sensorEndCalibration().
void sensorBeginMeasurement();
void sensorEndCalibration();

// =====================================================================
// Control de la simulacion (solo app/debug; el hardware real no tiene
// estos equivalentes)
// =====================================================================

// Muneca sobre el sensor (lo controla app.cpp; el sim arranca con la
// muneca puesta: siempre hay senal, sin fase de no-deteccion).
void simSetFinger(bool on);
bool simGetFinger();

// BPM objetivo de la proxima adquisicion (app elige el centro del
// rango del perfil -> la lectura fake sale siempre verde).
void simSetTargetBpm(int v);
int  simGetTargetBpm();

// Promedio real de la adquisicion en curso (suma de latidos / cuenta).
// Es el valor que se clasifica al terminar los 2 minutos.
int  simGetFinalAvg();
