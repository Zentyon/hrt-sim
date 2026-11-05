#pragma once
#include <Arduino.h>

// Maquina de estados del HRT-Sim, fiel al diagrama de flujo:
//
//   arranque -> registro valido?
//     no  -> idioma -> edad -> sexo -> deporte (si: lista 1-7 /
//           no: actividad 1-3) -> padecimiento (si: lista 1-6 / no)
//           -> revision -> menu
//     si  -> menu
//   menu (1=medir, 2=config)
//   medir -> dedo (J01; reintento J07) -> countdown -> 2 min
//           -> resultado (verde/amarillo/rojo + L/M/N) -> ¿otra? (P01)
//   config -> 1=perfil(_BLE_) 2=idioma 3=reset
//
// El sensor es SIMULADO (lib/ppgsim): no hay I2C, no hay "sensor no
// detectado"; el dedo y el BPM los controla la simulacion.

enum State {
    ST_SPLASH,
    ST_ON_LANG,
    ST_ON_AGE,
    ST_ON_GENDER,
    ST_ON_EXERCISE,            // ¿practica deporte? A=si B=no
    ST_ON_EXERCISE_INTENSITY,  // deportes 1-7 o actividad 1-3
    ST_ON_CONDITIONS,          // ¿padecimiento? A=si B=no
    ST_ON_CONDITION_TYPE,      // padecimientos 1-6
    ST_ON_REVIEW,              // verificar registro (I01/I02)
    ST_MENU,
    ST_CONFIG,
    ST_CONFIG_LANG,
    ST_CONFIG_PROFILE,
    ST_CONFIG_RESET,
    ST_FINGER,                 // esperando dedo (J01 / reintento J07)
    ST_COUNTDOWN,              // 3 s antes de adquirir
    ST_MEASURE,                // 2 min de adquisicion
    ST_RESULT,                 // K/L/M/N + banda de color
    ST_AGAIN                   // P01: ¿otra medicion?
};

extern State state;

extern unsigned long stateStart;
extern unsigned long measureStart;

#define COUNTDOWN_DURATION 3000
#define MEASURE_DURATION   120000   // 2 minutos (diagrama: "2 minutos")

void ledsAllOff();
void ledsSetByBpm(int v);           // live durante la medicion
void ledsSetCategory(int cat);      // solido en resultados (0/1/2/-1)

void appSetup();
void appLoop();
