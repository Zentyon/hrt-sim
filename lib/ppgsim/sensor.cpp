#include <Arduino.h>
#include "sensor.h"

// =====================================================================
// PPGSIM - synthesizer de PPG a 100 Hz.
//
// La task (core 0) genera un pulso realista (pico sistolico + muesca
// dicrotica), lo contamina con ruido y luego aplica deteccion de
// latidos por max-ventana + cruce de umbral sobre la propia senal, de
// forma identica en espiritu a como el firmware real derivaria el BPM
// del MAX30102. El BPM "objetivo" solo fija la frecuencia cardiaca de
// la senal (con HRV y deriva lenta); el numero que ve el usuario sale
// de la deteccion, con su jitter natural.
// =====================================================================

volatile byte rates[RATE_SIZE] = {0};
volatile byte rateSpot = 0;

volatile long lastBeat = 0;
volatile float bpm = 0;
volatile int avgBpm = 0;
volatile int maxBpm = 0;
volatile int minBpm = 0;
volatile int bpmCount = 0;

volatile bool normInit = false;
volatile bool schmittState = false;
volatile bool wristPresent = false;
volatile bool sensorAvailable = true;

volatile int spo2 = 0;
volatile bool spo2Valid = false;
volatile float perfusion = 0.0f;

const float ALPHA_DC = 0.05f;  // compat: el sim no filtra DC real

static portMUX_TYPE simMux = portMUX_INITIALIZER_UNLOCKED;

// --- estado de la simulacion ---
static volatile bool fingerOn = false;
static volatile int targetBpm = 75;

static float phase = 0.0f;          // 0..1 dentro del ciclo cardiaco
static float slowDrift = 0.0f;      // deriva lenta (parece fatiga/calma)
static unsigned long fingerSince = 0;
static unsigned long lastSampleMs = 0;

// acumulados de la adquisicion en curso (para el resultado final)
static long sumBpmAcc = 0;

// deteccion de latidos sobre la senal
static float winMax = 0.0f;
static int aboveCount = 0;
static bool armed = true;   // esperando pico para poder declarar latido

static float frand()  { return (float)esp_random() / (float)UINT32_MAX; }

// PPG normalizado 0..1: pico sistolico fuerte + muesca dicrotica.
static float ppgWave(float p)
{
    const float a  = p - 0.20f;
    const float b  = p - 0.47f;
    const float g1 = 0.90f * expf(-(a * a) / (2.0f * 0.085f * 0.085f));
    const float g2 = 0.28f * expf(-(b * b) / (2.0f * 0.055f * 0.055f));
    return g1 + g2;
}

void simSetFinger(bool on)
{
    if (on && !fingerOn) fingerSince = millis();
    fingerOn = on;
    if (!on)
    {
        portENTER_CRITICAL(&simMux);
        wristPresent = false;
        spo2Valid = false;
        winMax = 0.0f;
        armed = true;
        portEXIT_CRITICAL(&simMux);
    }
}

bool simGetFinger()          { return fingerOn; }
void simSetTargetBpm(int v)  { if (v >= 30 && v <= 220) targetBpm = v; }
int  simGetTargetBpm()       { return targetBpm; }

int simGetFinalAvg()
{
    int n, sum;
    portENTER_CRITICAL(&simMux);
    n = bpmCount;
    sum = (int)sumBpmAcc;
    portEXIT_CRITICAL(&simMux);
    return n > 0 ? sum / n : 0;
}

void sensorBeginMeasurement()
{
    sensorEndCalibration();
}

void sensorEndCalibration()
{
    portENTER_CRITICAL(&simMux);
    bpmCount = 0;
    sumBpmAcc = 0;
    maxBpm = 0;
    minBpm = 999;
    spo2 = 0;
    spo2Valid = false;

    // Sembrar el promedio con el BPM objetivo. Sin esto la pantalla muestra
    // un promedio bajo (o 0) durante los primeros ~16 latidos, porque
    // avgBpm sale de una ventana de 16 que arranca en ceros.
    for (int i = 0; i < RATE_SIZE; i++) rates[i] = (byte)targetBpm;
    rateSpot = 0;
    avgBpm = targetBpm;
    bpm = (float)targetBpm;
    portEXIT_CRITICAL(&simMux);
}

void sensorReadMeasure(SensorMeasure &m)
{
    portENTER_CRITICAL(&simMux);
    m.avgBpm = avgBpm;
    m.maxBpm = maxBpm;
    m.minBpm = (minBpm == 999) ? 0 : minBpm;
    m.spo2 = spo2;
    m.spo2Valid = spo2Valid;
    m.wristPresent = wristPresent;
    portEXIT_CRITICAL(&simMux);
}

bool sensorBeatLost()
{
    if (!wristPresent) return true;
    return (millis() - (unsigned long)lastBeat) > 3000;
}

// --- un paso de 10 ms: sintetiza la muestra y detecta latidos ---
static void simStep()
{
    // Frecuencia instantanea: objetivo + HRV respiratorio + deriva lenta
    // + jitter fino. Todo sobre la senal, no sobre el BPM mostrado.
    slowDrift += ((frand() - 0.5f) * 0.02f);
    slowDrift = constrain(slowDrift, -3.0f, 3.0f);
    float hrv = 2.5f * sinf(millis() / 7000.0f);
    float inst = targetBpm + hrv + slowDrift + (frand() - 0.5f) * 1.5f;
    inst = constrain(inst, 30.0f, 220.0f);

    phase += inst / 60.0f / 100.0f;      // 100 muestras/s
    if (phase >= 1.0f) phase -= 1.0f;

    bool beatThisSample = false;

    if (fingerOn)
    {
        // senal = AC*onda + ruido, centrada en 0. OJO: antes llevaba un DC de
        // 90000 y el detector compara contra el 50% del maximo de ventana:
        // con ese offset la senal nunca bajaba del umbral, no se detectaba ni
        // un latido y la app se quedaba para siempre en "coloque la muneca".
        float noise = (frand() + frand() + frand() - 1.5f) * 60.0f;
        float sig = 2600.0f * ppgWave(phase) + noise;

        // deteccion de latidos por max-ventana (1.2 s) + cruce al 50%
        if (sig > winMax) winMax = sig;
        else              winMax *= 0.995f;   // el max decae solo

        bool above = sig > (winMax * 0.5f);
        if (above) aboveCount++;
        if (armed && above && aboveCount >= 3)
        {
            armed = false;                    // entro al pulso: armado
        }
        else if (!armed && !above && aboveCount > 0)
        {
            // salio del pulso: latido declarado
            armed = true;
            beatThisSample = true;
        }
        if (!above) aboveCount = 0;

        if (beatThisSample)
        {
            unsigned long now = millis();
            long interval = now - (long)lastBeat;
            if (lastBeat > 0 && interval > 250 && interval < 3000)
            {
                float f = 60000.0f / (float)interval;
                byte bv = (byte)constrain((int)f, 30, 220);

                portENTER_CRITICAL(&simMux);
                rates[rateSpot++] = bv;
                if (rateSpot >= RATE_SIZE) rateSpot = 0;
                bpm = f;

                // promedio estilo firmware real: media de los ultimos 16
                int sum = 0;
                for (int i = 0; i < RATE_SIZE; i++) sum += rates[i];
                avgBpm = sum / RATE_SIZE;

                bpmCount++;
                sumBpmAcc += (long)f;
                if ((int)f > maxBpm) maxBpm = (int)f;
                if ((int)f < minBpm) minBpm = (int)f;

                // SpO2 simulado con su jitter (solo informativo)
                if (bpmCount >= 5)
                {
                    spo2 = 96 + (int)(frand() * 4.0f);   // 96-99
                    spo2Valid = true;
                }
                portEXIT_CRITICAL(&simMux);
            }
            lastBeat = (long)millis();
        }

        // senal estable ~1.5 s tras poner el dedo
        bool stable = (millis() - fingerSince) > 1500;
        portENTER_CRITICAL(&simMux);
        wristPresent = stable;
        perfusion = 1.4f + 0.3f * sinf(millis() / 5000.0f);
        portEXIT_CRITICAL(&simMux);
    }
    else
    {
        // linea plana con ruido: lo que veria el sensor sin dedo
        if (millis() - lastSampleMs >= 10)
            winMax *= 0.9f;
    }

    lastSampleMs = millis();
}

static void simTask(void *)
{
    TickType_t last = xTaskGetTickCount();
    for (;;)
    {
        simStep();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10));   // 100 Hz
    }
}

bool sensorInit()
{
    sensorAvailable = true;     // siempre: el sim no tiene I2C
    portENTER_CRITICAL(&simMux);
    bpmCount = 0;
    sumBpmAcc = 0;
    maxBpm = 0;
    minBpm = 999;
    avgBpm = 0;
    spo2Valid = false;
    wristPresent = false;
    portEXIT_CRITICAL(&simMux);

    xTaskCreatePinnedToCore(simTask, "ppgsim", 3072, nullptr, 2, nullptr, 0);
    Serial.println("[ppgsim] task PPG simulada iniciada (core 0, 100 Hz)");
    return true;
}
