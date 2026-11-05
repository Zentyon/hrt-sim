#pragma once

// Keypad 4x4 Cherry MX con barrido manual (sin diodos: max 2 teclas).
// Filas = GPIO 32,33,25,26 (con pull-up interno, se usan como salida
// por turno: activa en LOW, inactivas en alta impedancia para evitar
// cortos si se presionan 2 teclas de la misma columna).
// Columnas = GPIO 34,35,36,39 (solo-entrada, SIN pull-up interno).
// REQUISITO HARDWARE: red de 4x10k pull-up de cada columna a 3.3V.
// Debounce 15 ms para Cherry MX + espera liberacion (sin repeticion).

void initKeypad();
char getKey();  // '\0' si no hay tecla nueva estable

// --- diagnostico (mismas sondas que HRT-keypad-test) ---
uint8_t keypadDownCount();              // teclas sostenidas ahora
bool keypadColLow(uint8_t col);         // true si la columna lee LOW con filas high-Z
