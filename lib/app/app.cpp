#include <Arduino.h>
#include <driver/i2s.h>
#include "app.h"
#include "profile.h"
#include "keypad.h"
#include "audio.h"
#include "voice.h"
#include "sensor.h"   // ppgsim (sensor simulado, misma API que el real)
#include "display.h"
#include "ble.h"
#include "pinout.h"

// Teclas fisicas del teclado 4x4 (leyendas del proyecto):
//   A = Aceptar (●)   B = Cancelar (✕)   C = Regresar (│)   D = Menú (☰)
//   * = F (rosa)      # = M (azul)       0-9 = digitos

State state = ST_SPLASH;

unsigned long stateStart = 0;
unsigned long measureStart = 0;

static bool j07Said = false;           // aviso J07 tras perder la senal
static unsigned long stableSince = 0;  // ms desde que hay senal continua

// ---------------------------------------------------------------- LEDs

void ledsAllOff()
{
    digitalWrite(PIN_LED_VERDE, LOW);
    digitalWrite(PIN_LED_AMARILLO, LOW);
    digitalWrite(PIN_LED_ROJO, LOW);
}

// Verde = dentro del rango, amarillo = borde (15%), rojo = fuera.
void ledsSetByBpm(int v)
{
    if (v <= 0) { ledsAllOff(); return; }
    int rLo, rHi;
    computeRange(rLo, rHi);
    int band = (rHi - rLo) * 15 / 100;
    if (band < 8) band = 8;
    digitalWrite(PIN_LED_VERDE, (v > rLo + band && v < rHi - band) ? HIGH : LOW);
    digitalWrite(PIN_LED_AMARILLO,
        ((v >= rLo && v <= rLo + band) || (v >= rHi - band && v <= rHi)) ? HIGH : LOW);
    digitalWrite(PIN_LED_ROJO, (v < rLo || v > rHi) ? HIGH : LOW);
}

// Banda solida para la pantalla de resultado (cat: 0 verde, 1 amarillo,
// 2 rojo, -1 sin datos -> rojo de alerta como el firmware real).
void ledsSetCategory(int cat)
{
    ledsAllOff();
    if (cat == 0)      digitalWrite(PIN_LED_VERDE, HIGH);
    else if (cat == 1) digitalWrite(PIN_LED_AMARILLO, HIGH);
    else               digitalWrite(PIN_LED_ROJO, HIGH);
}

// --------------------------------------------------- clasificacion BPM

// TODO(tabla de referencia): hoy la clasificacion usa los rangos de
// computeRange() (aproximacion por edad/deporte/padecimiento). Cuando
// pases la tabla completa de referencia del proyecto, sustituir el
// calculo de computeRange() en profile.cpp (aqui NO cambia nada).
static int classifyBpm(int v)
{
    if (v <= 0) return -1;
    int lo, hi;
    computeRange(lo, hi);
    int band = (hi - lo) * 15 / 100;
    if (band < 8) band = 8;
    if (v >= lo && v <= hi) return 0;                       // verde
    if (v > hi && v <= hi + band) return 1;                 // amarillo
    if (v < lo && v >= lo - band) return 1;                 // amarillo
    return 2;                                               // rojo
}

// BPM objetivo del simulador: reposo normal ~90 ppm (lo que se ve en la
// pantalla y en el resultado). La senal anade HRV/jitter, asi que el numero
// final oscila un par de ppm alrededor de 90.
static int pickTargetBpm()
{
    return 90;
}

// ------------------------------------------------------------- flujo

static void say(const char *id)
{
    voicePlay(id);
}

static void goMenu()
{
    state = ST_MENU;
    stateStart = millis();
    showMenu();
    say("C01");
}

static void startMeasurement()
{
    // Sim: la "muneca" esta puesta desde el inicio (sin fase de
    // no-deteccion). La pantalla de "coloque la muneca" dura 3 s y de ahi
    // salta directo a la medicion.
    simSetFinger(true);
    simSetTargetBpm(pickTargetBpm());
    sensorBeginMeasurement();
    j07Said = false;
    stableSince = 0;
    state = ST_FINGER;
    stateStart = millis();
    showFinger();
    say("J01");
}

// Arranque de la fase de medicion (lo usan el fin de la pantalla de muneca
// y la cuenta atras).
static void beginMeasurePhase()
{
    sensorBeginMeasurement();     // reinicia BPM/SpO2 de esta medicion
    state = ST_MEASURE;
    measureStart = millis();
    showMeasure();
    say("J04");                   // iniciando medicion
    beep(1000, 100);
}

// --------------------------------------------------------- onboarding

static void handleOnLang(char key)
{
    if (key == '1')
    {
        lang = 'E';
        showOnLang();
        say("B02");
    }
    else if (key == '2')
    {
        lang = 'I';
        showOnLang();
        say("B03");
    }
    else if (key == 'A')
    {
        if (lang != ' ')   // idioma siempre arranca en 'E'; el pick es 1/2
        {
            beep(1000, 60);
            state = ST_ON_AGE;
            stateStart = millis();
            ageIdx = 0;
            ageInput[0] = 0;
            showOnAge();
            updateAgeDisplay();
            say("E01");
        }
        else say("S01");
    }
    else if (key == 'C')
    {
        say("B01");
    }
}

static void handleOnAge(char key)
{
    if (key >= '0' && key <= '9')
    {
        if (ageIdx < 3)
        {
            ageInput[ageIdx++] = key;
            ageInput[ageIdx] = 0;
            updateAgeDisplay();
        }
    }
    else if (key == 'C')
    {
        if (ageIdx > 0)
        {
            ageInput[--ageIdx] = 0;
            updateAgeDisplay();
            say("S06");
        }
    }
    else if (key == 'A')
    {
        int v = atoi(ageInput);
        if (v >= 1 && v <= 120)
        {
            age = v;
            say("E04");
            state = ST_ON_GENDER;
            stateStart = millis();
            showOnGender();
            say("D01");
            say("D02");
            say("D03");
        }
        else
        {
            say("E05");
        }
    }
    else if (key == 'B')
    {
        say("S05");
        state = ST_ON_LANG;
        stateStart = millis();
        showOnLang();
        say("B01");
    }
}

static void handleOnGender(char key)
{
    if (key == '*')          // tecla F (rosa)
    {
        gender = 'F';
        showOnGender();
        say("D04");
    }
    else if (key == '#')     // tecla M (azul)
    {
        gender = 'M';
        showOnGender();
        say("D05");
    }
    else if (key == 'A')
    {
        if (gender == 'M' || gender == 'F')
        {
            beep(1000, 60);
            state = ST_ON_EXERCISE;
            stateStart = millis();
            showOnExercise();
            say("G01");
        }
        else say("D06");
    }
    else if (key == 'C' || key == 'B')
    {
        say("S05");
        state = ST_ON_AGE;
        stateStart = millis();
        showOnAge();
        updateAgeDisplay();
        say("E01");
    }
}

static void handleOnExercise(char key)
{
    if (key == 'A')          // si: lista de deportes 1-7
    {
        exerciseType = 1;
        sportSel = 0;
        beep(1000, 60);
        state = ST_ON_EXERCISE_INTENSITY;
        stateStart = millis();
        showOnExerciseIntensity();
        say("G02");
        for (int i = 0; i < 7; i++)       // lee la lista completa
        {
            char vid[4];
            snprintf(vid, sizeof(vid), "G%02d", 3 + i);
            say(vid);
        }
    }
    else if (key == 'B')     // no: nivel de actividad 1-3
    {
        exerciseType = 2;
        exerciseLevel = 1;
        beep(1000, 60);
        state = ST_ON_EXERCISE_INTENSITY;
        stateStart = millis();
        showOnExerciseIntensity();
        say("G17");
        say("F01");
    }
    else if (key == 'C')
    {
        say("S05");
        state = ST_ON_GENDER;
        stateStart = millis();
        showOnGender();
        say("D01");
    }
}

static void handleOnIntensity(char key)
{
    if (exerciseType == 1)   // lista de deportes
    {
        if (key >= '1' && key <= '7')
        {
            sportSel = key - '1';
            showOnExerciseIntensity();
            char vid[4];
            snprintf(vid, sizeof(vid), "G%02d", 10 + sportSel);  // G10..G16
            say(vid);
        }
        else if (key == 'A')
        {
            if (sportSel >= 0 && sportSel <= 6)
            {
                beep(1000, 60);
                state = ST_ON_CONDITIONS;
                stateStart = millis();
                showOnConditions();
                say("H01");
            }
            else say("G18");
        }
        else if (key == 'C' || key == 'B')
        {
            say("S05");
            state = ST_ON_EXERCISE;
            stateStart = millis();
            showOnExercise();
            say("G01");
        }
        else say("G18");
    }
    else                     // nivel de actividad 1-3
    {
        if (key >= '1' && key <= '3')
        {
            exerciseLevel = key - '1';
            showOnExerciseIntensity();
            char vid[4];
            snprintf(vid, sizeof(vid), "F%02d", 2 + exerciseLevel);  // F02..F04
            say(vid);
        }
        else if (key == 'A')
        {
            beep(1000, 60);
            state = ST_ON_CONDITIONS;
            stateStart = millis();
            showOnConditions();
            say("H01");
        }
        else if (key == 'C' || key == 'B')
        {
            say("S05");
            state = ST_ON_EXERCISE;
            stateStart = millis();
            showOnExercise();
            say("G01");
        }
        else say("S01");
    }
}

static void handleOnConditions(char key)
{
    if (key == 'A')          // si: lista de padecimientos 1-6
    {
        conditionSel = 0;
        beep(1000, 60);
        state = ST_ON_CONDITION_TYPE;
        stateStart = millis();
        showOnConditionType();
        say("H02");
        for (int i = 0; i < 6; i++)       // lee la lista completa
        {
            char vid[4];
            snprintf(vid, sizeof(vid), "H%02d", 3 + i);
            say(vid);
        }
    }
    else if (key == 'B')     // no
    {
        conditionSel = 0;
        conditions = false;
        beep(1000, 60);
        say("H15");
        state = ST_ON_REVIEW;
        stateStart = millis();
        showReview();
        say("I01");
        say("I02");
    }
    else if (key == 'C')
    {
        say("S05");
        state = ST_ON_EXERCISE_INTENSITY;
        stateStart = millis();
        showOnExerciseIntensity();
    }
}

static void handleOnConditionType(char key)
{
    if (key >= '1' && key <= '6')
    {
        conditionSel = key - '0';
        conditions = true;
        showOnConditionType();
        char vid[4];
        snprintf(vid, sizeof(vid), "H%02d", 9 + conditionSel);  // H09..H14
        say(vid);
    }
    else if (key == 'A')
    {
        if (conditionSel >= 1 && conditionSel <= 6)
        {
            beep(1000, 60);
            state = ST_ON_REVIEW;
            stateStart = millis();
            showReview();
            say("I01");
            say("I02");
        }
        else say("H16");
    }
    else if (key == 'C' || key == 'B')
    {
        say("S05");
        state = ST_ON_CONDITIONS;
        stateStart = millis();
        showOnConditions();
        say("H01");
    }
    else say("H16");
}

static void handleOnReview(char key)
{
    if (key == 'A')
    {
        saveProfile();
        blePublishProfile();
        say("I03");
        goMenu();
    }
    else if (key == 'C')
    {
        say("S05");
        state = ST_ON_CONDITION_TYPE;
        stateStart = millis();
        showOnConditionType();
    }
    else if (key == 'B')
    {
        say("I04");
        goMenu();
    }
}

// --------------------------------------------------------------- menu

static void handleMenu(char key)
{
    if (key == '1')
    {
        say("C02");
        startMeasurement();
    }
    else if (key == '2')
    {
        say("C03");
        state = ST_CONFIG;
        stateStart = millis();
        showConfig();
        say("Q01");
    }
    else if (key == 'D' || key == 'A')
    {
        say("C01");   // repite las opciones
    }
}

// ------------------------------------------------------------ config

static void handleConfig(char key)
{
    if (key == '1')
    {
        say("Q02");
        state = ST_CONFIG_PROFILE;
        stateStart = millis();
        showConfigProfile();
        say("Q05");   // los datos se modifican desde la app movil
    }
    else if (key == '2')
    {
        say("Q03");
        state = ST_CONFIG_LANG;
        stateStart = millis();
        showConfigLang();
    }
    else if (key == '3')
    {
        state = ST_CONFIG_RESET;
        stateStart = millis();
        showConfigReset();
    }
    else if (key == 'B' || key == 'C')
    {
        goMenu();
    }
}

static void handleConfigLang(char key)
{
    if (key == '1')
    {
        lang = 'E';
        saveProfile();
        blePublishProfile();
        say("B02");
        state = ST_CONFIG;
        stateStart = millis();
        showConfig();
    }
    else if (key == '2')
    {
        lang = 'I';
        saveProfile();
        blePublishProfile();
        say("B03");
        state = ST_CONFIG;
        stateStart = millis();
        showConfig();
    }
    else if (key == 'B' || key == 'C')
    {
        say("Q06");
        state = ST_CONFIG;
        stateStart = millis();
        showConfig();
    }
}

static void handleConfigProfile(char key)
{
    if (key == 'A' || key == 'B' || key == 'C')
    {
        say("Q06");
        state = ST_CONFIG;
        stateStart = millis();
        showConfig();
    }
}

static void handleConfigReset(char key)
{
    if (key == 'A')
    {
        resetProfile();
        beep(1200, 200);
        delay(500);
        ESP.restart();
    }
    else if (key == 'B' || key == 'C')
    {
        state = ST_CONFIG;
        stateStart = millis();
        showConfig();
    }
}

// -------------------------------------------------------- medicion

static void finishMeasurement()
{
    int finalBpm = simGetFinalAvg();

    SensorMeasure m;
    sensorReadMeasure(m);
    int cat = (bpmCount == 0) ? -1 : classifyBpm(finalBpm);

    // Notificar a la app movil (BLE)
    char json[192];
    snprintf(json, sizeof(json),
             "{\"bpm\":%d,\"avg\":%d,\"max\":%d,\"min\":%d,\"spo2\":%d,"
             "\"spo2Valid\":%s,\"cat\":%d,\"age\":%d,\"gender\":\"%c\"}",
             finalBpm, m.avgBpm, m.maxBpm, m.minBpm, m.spo2,
             m.spo2Valid ? "true" : "false", cat, age,
             (gender ? gender : ' '));
    bleNotifyMeasurement(json);

    state = ST_RESULT;
    stateStart = millis();
    showResult(cat);
    beep(1000, 100); delay(150); beep(1500, 100); delay(150); beep(2000, 150);

    // Voces del resultado (secuencia del diagrama)
    say("K01");
    say("K02");   // nota: K02 trae un numero fijo en el audio; el valor
                  // real esta en la pantalla y en el JSON de BLE
    say("K03");
    if (cat == 0)      { say("L01"); say("L02"); }
    else if (cat == 1) { say("M01"); say("M02"); say("M03"); }
    else if (cat == 2) { say("N01"); say("N02"); say("N03"); say("N04"); }
    else               { say("O01"); say("O02"); }

    ledsSetCategory(cat);

    // Preguntar ¿otra medicion? (P01)
    state = ST_AGAIN;
    stateStart = millis();
    showAgain();
    say("P01");
}

static void handleAgain(char key)
{
    if (key == 'A')
    {
        say("P02");
        startMeasurement();
    }
    else if (key == 'B' || key == 'C')
    {
        say("P03");
        ledsAllOff();
        goMenu();
    }
}

// ------------------------------------------------------------- setup

void appSetup()
{
    Serial.begin(115200);
    delay(300);

    // La voz (.bop) y los beeps necesitan el ampli encendido desde initAudio().
    // TEMPORAL: cols tras cada subsistema (ver donde se caen)
    auto dbgCols = [](const char *tag) {
        Serial.printf("[stage] %-14s cols=%c%c%c%c\n", tag,
                      keypadColLow(0) ? 'L' : 'H', keypadColLow(1) ? 'L' : 'H',
                      keypadColLow(2) ? 'L' : 'H', keypadColLow(3) ? 'L' : 'H');
    };
    dbgCols("inicio");

    pinMode(PIN_LED_VERDE, OUTPUT);
    pinMode(PIN_LED_AMARILLO, OUTPUT);
    pinMode(PIN_LED_ROJO, OUTPUT);

    // Test LEDs: Verde -> Amarillo -> Rojo -> Todos -> Apagar
    digitalWrite(PIN_LED_VERDE, HIGH); delay(300);
    digitalWrite(PIN_LED_VERDE, LOW);
    digitalWrite(PIN_LED_AMARILLO, HIGH); delay(300);
    digitalWrite(PIN_LED_AMARILLO, LOW);
    digitalWrite(PIN_LED_ROJO, HIGH); delay(300);
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);
    digitalWrite(PIN_LED_AMARILLO, HIGH);
    digitalWrite(PIN_LED_ROJO, HIGH); delay(500);
    ledsAllOff();
    dbgCols("leds");

    // El sim nunca falla el "sensor" (requisito: sin aviso al arranque)
    sensorInit();
    dbgCols("sensor");
    displayInit();
    dbgCols("display");
    initKeypad();
    dbgCols("keypad");
    initAudio();
    dbgCols("audio");
    voiceInit();
    dbgCols("voz");
    bleInit();
    dbgCols("ble");

    // Splash + bienvenida
    stateStart = millis();
    showSplash();
    beep(1000, 150);
    delay(100);
    beep(1500, 150);
    delay(1200);
    say("A01");
    dbgCols("tras A01");

    // ¿Registro completo? menu; si no, onboarding
    loadProfile();
    if (profileIsValid())
    {
        goMenu();
    }
    else
    {
        state = ST_ON_LANG;
        stateStart = millis();
        showOnLang();
        say("B01");
    }
}

// -------------------------------------------------------------- loop

// TEMPORAL: barrido de pines candidatos. Libera el I2S y prueba cada GPIO en
// HIGH y LOW, imprimiendo el estado de las 4 columnas en cada caso. El pin
// cuyo nivel haga cambiar las columnas es el que comparte pista con el rail
// de pull-ups. Reiniciar la placa al terminar (el audio queda sin driver).
static void pinSweep()
{
    Serial.println("[sweep] liberando I2S...");
    i2s_driver_uninstall(I2S_NUM_0);
    delay(50);

    static const uint8_t pins[] = {4, 27, 15, 16, 12, 13, 14, 19, 2, 5};
    char c0, c1, c2, c3;
    for (uint8_t i = 0; i < sizeof(pins); i++)
    {
        pinMode(pins[i], OUTPUT);
        digitalWrite(pins[i], HIGH); delay(30);
        c0 = keypadColLow(0) ? 'L' : 'H'; c1 = keypadColLow(1) ? 'L' : 'H';
        c2 = keypadColLow(2) ? 'L' : 'H'; c3 = keypadColLow(3) ? 'L' : 'H';
        Serial.printf("[sweep] GPIO%u HIGH -> cols=%c%c%c%c\n", pins[i], c0, c1, c2, c3);
        digitalWrite(pins[i], LOW); delay(30);
        c0 = keypadColLow(0) ? 'L' : 'H'; c1 = keypadColLow(1) ? 'L' : 'H';
        c2 = keypadColLow(2) ? 'L' : 'H'; c3 = keypadColLow(3) ? 'L' : 'H';
        Serial.printf("[sweep] GPIO%u LOW  -> cols=%c%c%c%c\n", pins[i], c0, c1, c2, c3);
        pinMode(pins[i], INPUT);
        delay(10);
    }
    Serial.println("[sweep] fin (reinicia la placa para recuperar el audio)");
}

void appLoop()
{
    // --- pruebas de campo por serial (TEMPORAL: quitar al terminar) ---
    // 's' alterna el pin SD del ampli (GPIO12); 'p' imprime el estado de pines;
    // 'w' barre pines candidatos para encontrar el que tumba las columnas.
    static bool sdHigh = false;
    while (Serial.available())
    {
        const int ch = Serial.read();
        if (ch == 's')
        {
            sdHigh = !sdHigh;
            digitalWrite(PIN_I2S_SD, sdHigh ? HIGH : LOW);
            Serial.printf("[test] amp SD (GPIO12) = %s\n", sdHigh ? "HIGH" : "LOW");
        }
        else if (ch == 'p')
        {
            Serial.printf("[test] SD=%d BCLK=%d LRC=%d DIN=%d LED13=%d LED14=%d LED19=%d\n",
                          digitalRead(PIN_I2S_SD), digitalRead(PIN_I2S_BCLK),
                          digitalRead(PIN_I2S_LRC), digitalRead(PIN_I2S_DIN),
                          digitalRead(PIN_LED_VERDE), digitalRead(PIN_LED_AMARILLO),
                          digitalRead(PIN_LED_ROJO));
        }
        else if (ch == 'w')
        {
            pinSweep();
        }
        else if (ch == 'm')
        {
            Serial.println("[test] medicion forzada por serial");
            startMeasurement();
        }
    }
    bleUpdate();

    char key = getKey();
    if (key)
    {
        Serial.printf("Tecla: %c\n", key);
        beep(600, 20);

        switch (state)
        {
            case ST_ON_LANG:               handleOnLang(key); break;
            case ST_ON_AGE:                handleOnAge(key); break;
            case ST_ON_GENDER:             handleOnGender(key); break;
            case ST_ON_EXERCISE:           handleOnExercise(key); break;
            case ST_ON_EXERCISE_INTENSITY: handleOnIntensity(key); break;
            case ST_ON_CONDITIONS:         handleOnConditions(key); break;
            case ST_ON_CONDITION_TYPE:     handleOnConditionType(key); break;
            case ST_ON_REVIEW:             handleOnReview(key); break;
            case ST_MENU:                  handleMenu(key); break;
            case ST_CONFIG:                handleConfig(key); break;
            case ST_CONFIG_LANG:           handleConfigLang(key); break;
            case ST_CONFIG_PROFILE:        handleConfigProfile(key); break;
            case ST_CONFIG_RESET:          handleConfigReset(key); break;
            case ST_AGAIN:                 handleAgain(key); break;

            case ST_FINGER:
                // D = depuracion: alterna la "muneca" a mano (solo sim;
                // documentado en README, no en la UI)
                if (key == 'D')
                {
                    simSetFinger(!simGetFinger());
                }
                break;

            default:
                break;
        }
    }

    // --- diagnostico temporal de teclado (quitar cuando funcione) ---
    // Mismo dato que HRT-keypad-test: si las columnas leen L en reposo, el
    // pull-up no llega; si leen H y aun asi no hay eventos, es el escaneo.
    static uint32_t lastKdbg = 0;
    if (millis() - lastKdbg >= 1000)
    {
        lastKdbg = millis();
        Serial.printf("[kdbg] abajo=%u cols=%c%c%c%c\n",
                      keypadDownCount(),
                      keypadColLow(0) ? 'L' : 'H',
                      keypadColLow(1) ? 'L' : 'H',
                      keypadColLow(2) ? 'L' : 'H',
                      keypadColLow(3) ? 'L' : 'H');
    }

    // --- Estado: pantalla "coloque la muneca" (3 s y a medir) ---
    if (state == ST_FINGER)
    {
        const bool lost = sensorBeatLost();
        updateFingerDisplay(!lost, j07Said);

        // TEMPORAL: ver que hace la senal simulada
        static uint32_t lastFdbg = 0;
        if (millis() - lastFdbg >= 1000)
        {
            lastFdbg = millis();
            SensorMeasure m;
            sensorReadMeasure(m);
            Serial.printf("[finger] on=%d wrist=%d lost=%d lastBeat=%ld bpm=%d avg=%d n=%d\n",
                          simGetFinger() ? 1 : 0, m.wristPresent ? 1 : 0, lost ? 1 : 0,
                          (long)lastBeat, (int)bpm, m.avgBpm, (int)bpmCount);
        }

        // 3 s mostrando el aviso y se pasa a la medicion (sin cuenta atras).
        if (millis() - stateStart >= 3000)
        {
            beginMeasurePhase();
        }
    }

    // --- Estado: cuenta atras (no se usa en el sim: se va directo) ---
    if (state == ST_COUNTDOWN)
    {
        unsigned long elapsed = millis() - stateStart;
        int remaining = 3 - (int)(elapsed / 1000);
        if (remaining < 0) remaining = 0;
        updateCountdownDisplay(remaining);

        if (elapsed >= COUNTDOWN_DURATION)
        {
            beginMeasurePhase();
        }
    }

    // --- Estado: medicion (2 min) ---
    if (state == ST_MEASURE)
    {
        // UI (BPM vivo, progreso, LEDs) con throttle en display.cpp
        updateMeasureDisplay();

        // TEMPORAL: valores que ve la UI durante la medicion
        static uint32_t lastMdbg = 0;
        if (millis() - lastMdbg >= 2000)
        {
            lastMdbg = millis();
            SensorMeasure mm;
            sensorReadMeasure(mm);
            Serial.printf("[meas] avg=%d max=%d min=%d spo2=%d/%d t=%lus\n",
                          mm.avgBpm, mm.maxBpm, mm.minBpm, mm.spo2,
                          mm.spo2Valid ? 1 : 0, (millis() - measureStart) / 1000);
        }

        // Muneca retirada a mitad de medicion -> reintento (J07)
        if (sensorBeatLost())
        {
            say("J07");
            j07Said = false;
            stableSince = 0;
            state = ST_FINGER;
            stateStart = millis();
            showFinger();
            say("J01");
        }
        else if (millis() - measureStart >= MEASURE_DURATION)
        {
            finishMeasurement();
        }
    }

    delay(20);
}
