#include <Arduino.h>
#include <stdio.h>
#include "display.h"
#include "profile.h"
#include "sensor.h"
#include "app.h"
#include "pinout.h"

// `tft` lo define HRT_TFT.cpp (objeto compartido ya configurado).

void displayInit()
{
    // rotation 0 = landscape 320x240 en el modulo (verificado en HRT-libs).
    // Si la imagen sale cabeza abajo, probar rotation 2.
    const bool ok = hrtTftBegin(0);
    Serial.printf("[display] TFT init %s (%ldx%ld)\n", ok ? "OK" : "FALLO",
                  (long)tft.width(), (long)tft.height());
    clearScreen();
}

void clearScreen()
{
    tft.fillScreen(TFT_BLACK);
}

void drawTitle(const char* title)
{
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(title, 30, 10);
    tft.drawFastHLine(10, 28, 300, TFT_DARKGREY);
}

// Palomita (check) dibujada con líneas
void drawCheck(int x, int y, uint16_t color)
{
    tft.drawLine(x, y + 6, x + 4, y + 10, color);
    tft.drawLine(x + 4, y + 10, x + 11, y, color);
}

// Tache (cross) dibujado con líneas
void drawCross(int x, int y, uint16_t color)
{
    tft.drawLine(x, y, x + 9, y + 9, color);
    tft.drawLine(x + 9, y, x, y + 9, color);
}

void drawOption(int num, const char* text, int y, bool selected)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%d. %s", selected ? "> " : "  ", num, text);
    tft.setTextSize(2);
    if (selected)
    {
        tft.setTextColor(TFT_BLACK, TFT_CYAN);
        tft.drawString(buf, 20, y);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
    }
    else
    {
        tft.setTextColor(TFT_WHITE);
        tft.drawString(buf, 20, y);
    }
}

void showSplash()
{
    // TEMPORAL (diagnostico de panel): barras de color. Si salen limpias y el
    // texto no, el fallo esta en el dibujo/fuentes; si tambien salen sucias,
    // el problema es el envio SPI o el panel.
    static const uint16_t bars[4] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
    for (int i = 0; i < 4; i++)
        tft.fillRect(i * 80, 0, 80, 240, bars[i]);
    delay(1200);

    clearScreen();
    tft.setTextSize(3);
    tft.setTextColor(TFT_CYAN);
    tft.drawString("HRT Monitor", 50, 60);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("v1.0", 150, 100);
}

void showOnLang()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Idioma" : "Language");
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("1. Espanol", 40, 50);
    tft.drawString("2. English", 40, 80);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(lang == 'E' ? "Presione 1 o 2" : "Press 1 or 2", 100, 140);
}

void showOnAge()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Edad" : "Age");
    tft.setTextSize(3);
    tft.setTextColor(TFT_CYAN);
    tft.drawString("_ _ _", 80, 60);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(lang == 'E' ? "0-9: numero  A: ok  C: borrar" : "0-9: number  A: ok  C: del", 40, 120);
}

void updateAgeDisplay()
{
    tft.fillRect(80, 55, 120, 40, TFT_BLACK);
    tft.setTextSize(3);
    tft.setTextColor(TFT_CYAN);
    char disp[16];
    int p = 0;
    for (int i = 0; i < ageIdx && p < (int)sizeof(disp) - 2; i++)
        p += snprintf(disp + p, sizeof(disp) - p, "%c ", ageInput[i]);
    for (int i = ageIdx; i < 3 && p < (int)sizeof(disp) - 2; i++)
        p += snprintf(disp + p, sizeof(disp) - p, "_ ");
    tft.drawString(disp, 80, 60);
}

void showOnGender()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Genero" : "Gender");
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(lang == 'E' ? "* Femenino" : "* Female", 40, 60);
    tft.drawString(lang == 'E' ? "# Masculino" : "# Male", 40, 90);
}

// ¿Practica algun deporte? (G01: A=si, B=no)
void showOnExercise()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Deporte" : "Sport");
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(lang == 'E' ? "Practica algun" : "Do you play a", 40, 45);
    tft.drawString(lang == 'E' ? "deporte?" : "sport?", 40, 70);
    tft.drawString("A", 40, 110);
    drawCheck(58, 110, TFT_GREEN);
    tft.drawString(lang == 'E' ? " Si" : " Yes", 72, 110);
    tft.drawString("B", 40, 140);
    drawCross(58, 140, TFT_RED);
    tft.drawString(" No", 72, 140);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("C. " + String(lang == 'E' ? "Volver" : "Back"), 120, 180);
}

// Si eligio deporte: lista 1-7 (G02-G09). Si no: nivel de actividad
// 1-3 (F01). Mismo estado, dos pantallas.
void showOnExerciseIntensity()
{
    clearScreen();
    if (exerciseType == 1)
    {
        drawTitle(lang == 'E' ? "Deporte" : "Sport");
        const char** sn = sportNames[lang == 'E' ? 0 : 1];
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE);
        char buf[48];
        for (int i = 0; i < 6; i++)
        {
            snprintf(buf, sizeof(buf), "%d. %s", i + 1, sn[i]);
            tft.drawString(buf, 30, 38 + i * 16);
        }
        tft.drawString(lang == 'E' ? "7. Otro deporte" : "7. Other sport", 30, 38 + 6 * 16);
        tft.setTextSize(1);
        tft.setTextColor(TFT_DARKGREY);
        tft.drawString(lang == 'E' ? "A. Ok   C. Volver" : "A. Ok   C. Back", 60, 165);
    }
    else
    {
        drawTitle(lang == 'E' ? "Actividad" : "Activity level");
        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE);
        tft.drawString(lang == 'E' ? "1. Baja" : "1. Low", 40, 55);
        tft.drawString(lang == 'E' ? "2. Media" : "2. Medium", 40, 85);
        tft.drawString(lang == 'E' ? "3. Alta" : "3. High", 40, 115);
        tft.setTextSize(1);
        tft.setTextColor(TFT_DARKGREY);
        tft.drawString("A. " + String(lang == 'E' ? "Ok" : "Ok") + "   C. " +
                       String(lang == 'E' ? "Volver" : "Back"), 60, 160);
    }
}

void showOnConditions()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Enf. Cardiacas" : "Heart Cond.");
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("A", 40, 70);
    drawCheck(58, 70, TFT_GREEN);
    tft.drawString(lang == 'E' ? " Si" : " Yes", 72, 70);
    tft.drawString("B", 40, 100);
    drawCross(58, 100, TFT_RED);
    tft.drawString(lang == 'E' ? " No" : " No", 72, 100);
}

void showOnConditionType()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Padecimiento" : "Condition");
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    // Orden identico a los audios H03-H08
    const char* dn[2][6] = {
        {"1. Diabetes","2. Hipertension","3. Hipertiroidismo","4. Anemia","5. Asma","6. Insuf. cardiaca"},
        {"1. Diabetes","2. Hypertension","3. Hyperthyroidism","4. Anemia","5. Asthma","6. Heart failure"}
    };
    int li = (lang == 'E') ? 0 : 1;
    tft.drawString(dn[li][0], 30, 45);
    tft.drawString(dn[li][1], 30, 65);
    tft.drawString(dn[li][2], 30, 85);
    tft.drawString(dn[li][3], 30, 105);
    tft.drawString(dn[li][4], 30, 125);
    tft.drawString(dn[li][5], 30, 145);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("A. " + String(lang == 'E' ? "Ok" : "Ok") + "   C. " +
                   String(lang == 'E' ? "Volver" : "Back"), 60, 175);
}

// Verificar registro antes de entrar al menu (I01/I02)
void showReview()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Verificar datos" : "Verify data");

    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN);

    char buf[64];
    const char* genStr = (gender == 'M') ? (lang == 'E' ? "Masculino" : "Male")
                       : (gender == 'F') ? (lang == 'E' ? "Femenino" : "Female")
                                         : (lang == 'E' ? "Sin definir" : "Not set");
    snprintf(buf, sizeof(buf), "%s%d  %s",
             lang == 'E' ? "Edad: " : "Age: ", age, genStr);
    tft.drawString(buf, 20, 42);

    char exBuf[48];
    if (exerciseType == 1)
        snprintf(exBuf, sizeof(exBuf), "%s%s",
                 lang == 'E' ? "Deporte: " : "Sport: ",
                 sportNames[lang == 'E' ? 0 : 1][sportSel]);
    else if (exerciseType == 2)
    {
        const char* lvl[3] = {"Baja", "Media", "Alta"};
        const char* lvlEn[3] = {"Low", "Medium", "High"};
        snprintf(exBuf, sizeof(exBuf), "%s%s",
                 lang == 'E' ? "Actividad: " : "Activity: ",
                 (lang == 'E' ? lvl : lvlEn)[exerciseLevel]);
    }
    else
        snprintf(exBuf, sizeof(exBuf), "%s%s",
                 lang == 'E' ? "Actividad: " : "Activity: ",
                 lang == 'E' ? "Ninguna" : "None");
    tft.drawString(exBuf, 20, 62);

    const char* condNames[2][7] = {
        {"", "Diabetes", "Hipertension", "Hipertiroidismo", "Anemia", "Asma", "Insuf. cardiaca"},
        {"", "Diabetes", "Hypertension", "Hyperthyroidism", "Anemia", "Asthma", "Heart failure"}
    };
    int ci = (lang == 'E') ? 0 : 1;
    snprintf(buf, sizeof(buf), "%s%s",
             lang == 'E' ? "Padecim.: " : "Condition: ",
             (conditionSel > 0) ? condNames[ci][conditionSel]
                                : (lang == 'E' ? "Ninguno" : "None"));
    tft.drawString(buf, 20, 82);

    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("A", 40, 120);
    drawCheck(58, 120, TFT_GREEN);
    tft.drawString(lang == 'E' ? " Confirmar" : " Confirm", 72, 120);
    tft.drawString("C", 40, 150);
    tft.drawString(lang == 'E' ? " Regresar" : " Back", 72, 150);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(lang == 'E' ? "B: cancelar registro" : "B: cancel registration", 70, 190);
}

void showMenu()
{
    ledsAllOff();
    clearScreen();
    drawTitle("HRT Monitor");
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(lang == 'E' ? "1. Medir" : "1. Measure", 40, 60);
    tft.drawString(lang == 'E' ? "2. Config" : "2. Settings", 40, 90);
}

void showConfig()
{
    ledsAllOff();
    clearScreen();
    drawTitle(lang == 'E' ? "Configuracion" : "Settings");
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(lang == 'E' ? "1. Idioma" : "1. Language", 40, 50);
    tft.drawString(lang == 'E' ? "2. Mi perfil" : "2. My profile", 40, 75);
    tft.drawString(lang == 'E' ? "3. Reset" : "3. Reset", 40, 100);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("B. " + String(lang == 'E' ? "Volver" : "Back"), 120, 140);
}

void showConfigLang()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Idioma" : "Language");
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(lang == 'E' ? "1. Espanol" : "1. Spanish", 40, 60);
    tft.drawString("2. English", 40, 90);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("B. " + String(lang == 'E' ? "Volver" : "Back"), 120, 140);
}

void showConfigProfile()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Mi Perfil" : "My Profile");
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN);

    String langStr = (lang == 'E') ? "Espanol" : "English";
    tft.drawString((lang == 'E' ? "Idioma: " : "Language: ") + langStr, 20, 45);
    tft.drawString((lang == 'E' ? "Edad: " : "Age: ") + String(age), 20, 70);

    String genStr;
    if (gender == 'M')      genStr = (lang == 'E' ? "Masculino" : "Male");
    else if (gender == 'F') genStr = (lang == 'E' ? "Femenino" : "Female");
    else                    genStr = (lang == 'E' ? "Sin definir" : "Not set");
    tft.drawString((lang == 'E' ? "Genero: " : "Gender: ") + genStr, 20, 95);

    String exDisplay;
    if (exerciseType == 0)
        exDisplay = (lang == 'E' ? "Ninguno" : "None");
    else if (exerciseType == 1)
        exDisplay = String(sportNames[lang == 'E' ? 0 : 1][sportSel]);
    else
    {
        String lvl[] = {"Baja", "Media", "Alta"};
        String lvlEn[] = {"Low", "Medium", "High"};
        exDisplay = (lang == 'E' ? "Otro: " : "Other: ") + (lang == 'E' ? lvl[exerciseLevel] : lvlEn[exerciseLevel]);
    }
    tft.drawString((lang == 'E' ? "Actividad: " : "Activity: ") + exDisplay, 20, 120);

    const char* condNames[2][8] = {
        {"", "Diabetes", "Hipertension", "Hipertiroidismo", "Hipotiroidismo", "Anemia", "Asma controlada", "Insuf. cardiaca"},
        {"", "Diabetes", "Hypertension", "Hyperthyroidism", "Hypothyroidism", "Anemia", "Controlled asthma", "Heart failure"}
    };
    int ci = (lang == 'E') ? 0 : 1;
    String condStr = (conditionSel > 0) ? String(condNames[ci][conditionSel]) : (lang == 'E' ? "Ninguna" : "None");
    tft.drawString((lang == 'E' ? "Enf. Card.: " : "Heart Cond: ") + condStr, 20, 145);

    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("A. " + String(lang == 'E' ? "Volver" : "Back"), 120, 170);
}

void showConfigReset()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Reset" : "Reset");
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED);
    tft.drawString(lang == 'E' ? "Borrar todos" : "Delete all", 40, 60);
    tft.drawString(lang == 'E' ? "los datos?" : "data?", 40, 85);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("A", 40, 120);
    drawCheck(58, 120, TFT_GREEN);
    tft.drawString(lang == 'E' ? " Confirmar" : " Confirm", 72, 120);
    tft.drawString("B", 40, 145);
    drawCross(58, 145, TFT_RED);
    tft.drawString(lang == 'E' ? " Cancelar" : " Cancel", 72, 145);
}

// --- Pantalla: esperando dedo (J01 / reintento J07) ---
static bool uiFingerOk = false, uiFingerRetry = false;

void showFinger()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Medicion" : "Measurement");
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(lang == 'E' ? "Coloque la muneca" : "Place wristband", 40, 55);
    tft.drawString(lang == 'E' ? "en el sensor" : "on the sensor", 60, 80);
    uiFingerOk = false;
    uiFingerRetry = false;
    updateFingerDisplay(false, false);
}

void updateFingerDisplay(bool signalOk, bool retryMsg)
{
    if (signalOk == uiFingerOk && retryMsg == uiFingerRetry) return;
    uiFingerOk = signalOk;
    uiFingerRetry = retryMsg;

    tft.fillRect(20, 115, 280, 80, TFT_BLACK);
    tft.setTextSize(2);
    if (signalOk)
    {
        tft.setTextColor(TFT_GREEN);
        tft.drawString(lang == 'E' ? "Senal detectada" : "Signal detected", 55, 125);
        tft.setTextSize(1);
        tft.setTextColor(TFT_DARKGREY);
        tft.drawString(lang == 'E' ? "No retire la muneca" : "Do not lift wristband", 75, 160);
    }
    else if (retryMsg)
    {
        tft.setTextColor(TFT_YELLOW);
        tft.drawString(lang == 'E' ? "Reacomode la muneca" : "Reposition wristband", 35, 125);
        tft.setTextSize(1);
        tft.setTextColor(TFT_DARKGREY);
        tft.drawString(lang == 'E' ? "Espere un momento" : "Wait a moment", 85, 160);
    }
    else
    {
        tft.setTextColor(TFT_WHITE);
        tft.drawString(lang == 'E' ? "Esperando senal..." : "Waiting for signal...", 40, 125);
    }
}

// Cuenta atras antes de la adquisicion de 2 minutos
void showCountdown()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Preparando..." : "Preparing...");
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(lang == 'E' ? "Mantenga la muneca" : "Keep wristband on", 40, 55);
    tft.drawString(lang == 'E' ? "en el sensor" : "the sensor", 60, 80);
    tft.setTextSize(5);
    tft.setTextColor(TFT_CYAN);
    tft.drawString("3", 150, 115);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(lang == 'E' ? "No la retire" : "Do not lift it", 90, 185);
}

void updateCountdownDisplay(int secondsLeft)
{
    tft.fillRect(130, 110, 60, 60, TFT_BLACK);
    tft.setTextSize(5);
    tft.setTextColor(TFT_CYAN);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", secondsLeft);
    tft.drawString(buf, 150, 115);
}

// --- Pantalla de medicion ---
// Refresco limitado a ~5 Hz y redibujo solo de lo que cambia: evita
// saturar el SPI (antes ~50 Hz con fillRect + String cada 20 ms).
static const unsigned long UI_REFRESH_MS = 200;
static const unsigned long WARMUP_MS = 10000;

static unsigned long uiLastDraw = 0;
static int  uiBpm = -1, uiSec = -1, uiBar = -1;
static bool uiHasBpm = false, uiAdjust = false, uiLost = false, uiAdjWrist = false;
static int  uiSAvg = -1, uiSMax = -1, uiSMin = -1, uiSO2 = -1;
static bool uiSO2v = false;

void showMeasure()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Midiendo..." : "Measuring...");
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Prom  Max  Min", 20, 35);
    tft.drawRect(20, 110, 280, 12, TFT_DARKGREY);

    // Forzar el primer render
    uiLastDraw = 0;
    uiBpm = -1; uiSec = -1; uiBar = -1;
    uiHasBpm = false; uiAdjust = false; uiLost = false; uiAdjWrist = false;
    uiSAvg = -1; uiSMax = -1; uiSMin = -1; uiSO2 = -1;
    uiSO2v = false;
}

void updateMeasureDisplay()
{
    unsigned long elapsed = millis() - measureStart;
    if (elapsed >= MEASURE_DURATION) return;

    SensorMeasure m;
    sensorReadMeasure(m);
    bool lost = sensorBeatLost();
    bool adjusting = (elapsed < WARMUP_MS);

    // LEDs (barato, cada iteracion) fuera del throttle de dibujo.
    if (m.wristPresent && m.avgBpm > 0)
        ledsSetByBpm(m.avgBpm);
    else
        ledsAllOff();

    if (uiLastDraw != 0 && millis() - uiLastDraw < UI_REFRESH_MS) return;
    uiLastDraw = millis();

    int remaining = (MEASURE_DURATION - elapsed) / 1000;
    int mins = remaining / 60;
    int secs = remaining % 60;
    int barWidth = map(elapsed, 0, MEASURE_DURATION, 0, 280);
    bool showBpm = m.wristPresent && m.avgBpm > 0;

    // BPM grande
    if (showBpm != uiHasBpm || (showBpm && m.avgBpm != uiBpm))
    {
        tft.fillRect(20, 50, 280, 50, TFT_BLACK);
        tft.setTextSize(4);
        if (showBpm)
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d BPM", m.avgBpm);
            tft.setTextColor(TFT_CYAN);
            tft.drawString(buf, 40, 60);
        }
        else
        {
            tft.setTextColor(TFT_DARKGREY);
            tft.drawString("--", 120, 60);
        }
        uiHasBpm = showBpm;
        uiBpm = m.avgBpm;
    }

    // Tiempo restante
    if (secs != uiSec)
    {
        tft.fillRect(240, 35, 80, 15, TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
        tft.drawString(buf, 260, 35);
        uiSec = secs;
    }

    // Barra de progreso
    if (barWidth != uiBar)
    {
        tft.fillRect(20, 110, barWidth, 12, TFT_CYAN);
        if (barWidth < 280)
            tft.fillRect(20 + barWidth, 110, 280 - barWidth, 12, TFT_BLACK);
        tft.drawRect(20, 110, 280, 12, TFT_DARKGREY);
        uiBar = barWidth;
    }

    // Zona media: durante warmup muestra la guia; despues las stats.
    if (adjusting)
    {
        if (!uiAdjust || m.wristPresent != uiAdjWrist)
        {
            tft.fillRect(20, 130, 280, 55, TFT_BLACK);
            tft.setTextSize(2);
            if (m.wristPresent)
            {
                tft.setTextColor(TFT_DARKGREY);
                tft.drawString(lang == 'E' ? "Estabilizando..." : "Stabilizing...", 50, 140);
            }
            else
            {
                tft.setTextColor(TFT_YELLOW);
                tft.drawString(lang == 'E' ? "Coloque la muneca" : "Place wristband", 40, 140);
            }
            uiAdjust = true;
            uiAdjWrist = m.wristPresent;
        }
    }
    else
    {
        if (uiAdjust)
        {
            tft.fillRect(20, 130, 280, 55, TFT_BLACK);
            uiAdjust = false;
            uiSAvg = -1;      // fuerza redibujo de stats
            uiLost = !lost;   // fuerza revisar el aviso de muneca
        }
        if (m.avgBpm != uiSAvg || m.maxBpm != uiSMax || m.minBpm != uiSMin ||
            m.spo2Valid != uiSO2v || m.spo2 != uiSO2)
        {
            tft.fillRect(20, 130, 280, 55, TFT_BLACK);
            tft.setTextSize(2);
            tft.setTextColor(TFT_WHITE);
            char buf[32];
            snprintf(buf, sizeof(buf), "P:%d X:%d N:%d", m.avgBpm, m.maxBpm, m.minBpm);
            tft.drawString(buf, 20, 132);
            if (m.spo2Valid)
            {
                tft.setTextColor(TFT_CYAN);
                snprintf(buf, sizeof(buf), "O2:%d%%", m.spo2);
            }
            else
            {
                tft.setTextColor(TFT_DARKGREY);
                snprintf(buf, sizeof(buf), "O2:--");
            }
            tft.drawString(buf, 20, 152);
            uiSAvg = m.avgBpm;
            uiSMax = m.maxBpm;
            uiSMin = m.minBpm;
            uiSO2 = m.spo2;
            uiSO2v = m.spo2Valid;
        }
    }

    // Sin latido >3 s: aviso. Se borra cuando vuelve el pulso.
    if (lost != uiLost)
    {
        tft.fillRect(0, 198, 320, 30, TFT_BLACK);
        if (lost && !adjusting)
        {
            tft.setTextSize(2);
            tft.setTextColor(TFT_YELLOW);
            tft.drawString(lang == 'E' ? "Muneca retirada" : "Wristband lifted", 60, 200);
        }
        uiLost = lost;
    }
}

// --- Resultado de la medicion ---
// cat: 0 verde (adecuado), 1 amarillo (precaucion), 2 rojo (alerta),
// -1 sin datos (bpmCount == 0). Dibuja stats + banda de color.
void showResult(int cat)
{
    clearScreen();
    drawTitle(lang == 'E' ? "Resultado" : "Result");

    SensorMeasure m;
    sensorReadMeasure(m);
    char buf[64];

    if (cat < 0 || bpmCount == 0)
    {
        tft.setTextSize(2);
        tft.setTextColor(TFT_YELLOW);
        tft.drawString(lang == 'E' ? "Sin datos" : "No data", 20, 60);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE);
        tft.drawString(lang == 'E' ? "No se detecto pulso" : "No pulse detected", 20, 95);
        tft.drawString(lang == 'E' ? "Repita la medicion" : "Repeat measurement", 20, 115);
        ledsSetCategory(-1);
        tft.setTextSize(1);
        tft.setTextColor(TFT_DARKGREY);
        snprintf(buf, sizeof(buf), "A. %s", lang == 'E' ? "Continuar" : "Continue");
        tft.drawString(buf, 110, 190);
        return;
    }

    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN);
    snprintf(buf, sizeof(buf), "%s%d BPM", lang == 'E' ? "Promedio: " : "Average: ", m.avgBpm);
    tft.drawString(buf, 20, 40);
    snprintf(buf, sizeof(buf), "%s%d BPM", lang == 'E' ? "Maximo: " : "Max: ", m.maxBpm);
    tft.drawString(buf, 20, 62);
    snprintf(buf, sizeof(buf), "%s%d BPM", lang == 'E' ? "Minimo: " : "Min: ", m.minBpm);
    tft.drawString(buf, 20, 84);

    int totalSec = MEASURE_DURATION / 1000;
    if (m.spo2Valid)
        snprintf(buf, sizeof(buf), "%s%d:%02d  O2:%d%%",
                 lang == 'E' ? "Duracion: " : "Duration: ", totalSec / 60, totalSec % 60, m.spo2);
    else
        snprintf(buf, sizeof(buf), "%s%d:%02d  O2:--",
                 lang == 'E' ? "Duracion: " : "Duration: ", totalSec / 60, totalSec % 60);
    tft.drawString(buf, 20, 106);

    // Banda de clasificacion (diagrama: L/M/N)
    const char* catName[3][2] = {
        {"Verde: adecuado",     "Green: OK"},
        {"Amarillo: precaucion","Yellow: caution"},
        {"Rojo: alerta",        "Red: alert"}
    };
    uint16_t catColor[3] = {TFT_GREEN, TFT_YELLOW, TFT_RED};
    tft.setTextSize(2);
    tft.setTextColor(catColor[cat]);
    tft.drawString(catName[cat][lang == 'E' ? 0 : 1], 20, 135);
    ledsSetCategory(cat);

    int rLo, rHi;
    computeRange(rLo, rHi);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    snprintf(buf, sizeof(buf), "%s%d-%d BPM", lang == 'E' ? "Tu rango: " : "Your range: ", rLo, rHi);
    tft.drawString(buf, 20, 162);

    snprintf(buf, sizeof(buf), "* SpO2 %s", lang == 'E' ? "estimado, sin calibrar" : "estimate, uncalibrated");
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(buf, 20, 178);

    snprintf(buf, sizeof(buf), "A. %s", lang == 'E' ? "Continuar" : "Continue");
    tft.drawString(buf, 110, 200);
}

// ¿Desea realizar otra medicion? (P01)
void showAgain()
{
    clearScreen();
    drawTitle(lang == 'E' ? "Otra medicion?" : "Measure again?");
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("A", 40, 70);
    drawCheck(58, 70, TFT_GREEN);
    tft.drawString(lang == 'E' ? " Si" : " Yes", 72, 70);
    tft.drawString("B", 40, 105);
    drawCross(58, 105, TFT_RED);
    tft.drawString(lang == 'E' ? " No (menu)" : " No (menu)", 72, 105);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(lang == 'E' ? "C: menu principal" : "C: main menu", 90, 160);
}
