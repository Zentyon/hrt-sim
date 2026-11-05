#pragma once
#include <Arduino.h>
#include <HRT_TFT.h>

// Pantallas TFT (ILI9341 320x240 landscape). El objeto `tft` (TFT_eSPI ya
// configurado: pines, driver, fuentes) viene de la libreria HRT_TFT.
// Lee el perfil (profile.h), la medicion simulada (ppgsim/sensor.h) y
// los tiempos de estado (app.h). No escribe estado.
// TODOS los strings de UI en ASCII (regla del renderer: fuentes
// built-in sin ñ/acentos/simbolos especiales).

void displayInit();
void clearScreen();
void drawTitle(const char* title);
void drawCheck(int x, int y, uint16_t color);
void drawCross(int x, int y, uint16_t color);
void drawOption(int num, const char* text, int y, bool selected);

// Arranque y onboarding
void showSplash();
void showOnLang();
void showOnAge();
void updateAgeDisplay();
void showOnGender();
void showOnExercise();               // ¿practica deporte? A/B
void showOnExerciseIntensity();      // deportes 1-7 o actividad 1-3
void showOnConditions();             // ¿padecimiento? A/B
void showOnConditionType();          // padecimientos 1-6
void showReview();                   // verificar registro

// Menu y configuracion
void showMenu();
void showConfig();
void showConfigLang();
void showConfigProfile();
void showConfigReset();

// Medicion
void showFinger();                   // esperando dedo
void updateFingerDisplay(bool signalOk, bool retryMsg);
void showCountdown();
void updateCountdownDisplay(int secondsLeft);
void showMeasure();
void updateMeasureDisplay();

// Resultado (cat: 0 verde, 1 amarillo, 2 rojo, -1 sin datos)
void showResult(int cat);
void showAgain();                    // ¿otra medicion? A/B
