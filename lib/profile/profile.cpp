#include <Arduino.h>
#include <Preferences.h>
#include "profile.h"

static Preferences prefs;

char lang = 'E';       // 'E'=ESP, 'I'=ENG
int age = 0;
char gender = ' ';     // 'M'=masculino, 'F'=femenino
int exerciseType = 0;   // 0=Ninguno, 1=Deporte, 2=Otro(intensidad)
int sportSel = 0;       // 0-6 indice de tableSport
int exerciseLevel = 0;  // 0=Baja, 1=Media, 2=Alta (cuando exerciseType==2)
int conditionSel = 0;   // 0=Ninguna, 1-6 (ver tableDisease)
bool conditions = false; // derivado de conditionSel
char ageInput[4] = {0};
int ageIdx = 0;

// Hoja1: Edad x Genero (en reposo) [fila][0]=Mujer, [1]=Hombre
const Range tableAge[9][2] = {
    {{100,160},{100,160}},  // 0-1
    {{90,150},{90,150}},    // 1-3
    {{80,140},{80,140}},    // 4-5
    {{70,120},{70,120}},    // 6-12
    {{60,100},{60,100}},    // 13-17
    {{60,95},{55,90}},      // 18-25
    {{60,95},{55,90}},      // 26-35
    {{60,100},{60,95}},     // 36-45
    {{60,100},{60,95}}      // 46-55
};

// Hoja3: Actividad [intensidad-1][0]=Mujer, [1]=Hombre
const Range tableExercise[3][2] = {
    {{52,72},{48,68}},   // 1: Baja
    {{58,78},{55,75}},   // 2: Media
    {{43,58},{40,55}}    // 3: Alta
};

// Hoja4: Enfermedad [idx-1][0]=Mujer, [1]=Hombre
// Orden identico a los audios H03-H08 (1 diabetes ... 6 insuf. cardiaca).
const Range tableDisease[6][2] = {
    {{70,100},{65,95}},   // 1: Diabetes
    {{60,95},{60,90}},    // 2: Hipertension
    {{85,115},{80,110}},  // 3: Hipertiroidismo
    {{80,105},{75,100}},  // 4: Anemia
    {{65,100},{60,95}},   // 5: Asma
    {{75,100},{70,95}}    // 6: Insuf. cardiaca
};

// Deporte [0]=Mujer, [1]=Hombre. Orden identico a los audios G03-G09;
// la fila 7 ("Otro deporte") no tiene rango propio: computeRange usa
// la tabla por edad en ese caso.
const Range tableSport[7][2] = {
    {{53,73},{50,70}},    // 1: Correr
    {{48,68},{45,65}},    // 2: Ciclismo
    {{48,68},{45,65}},    // 3: Natacion
    {{53,73},{50,70}},    // 4: Futbol
    {{53,73},{50,70}},    // 5: Basquetbol
    {{58,78},{55,75}},    // 6: Gimnasia
    {{0,0},{0,0}}         // 7: Otro deporte (sin uso directo)
};

// Nombres de deportes [idioma][indice 0-6]
const char* sportNames[2][7] = {
    {"Correr","Ciclismo","Natacion","Futbol","Basquetbol","Gimnasia","Otro deporte"},
    {"Running","Cycling","Swimming","Soccer","Basketball","Gym","Other sport"}
};

int ageToRow(int a) {
    if (a <= 1)  return 0;
    if (a <= 3)  return 1;
    if (a <= 5)  return 2;
    if (a <= 12) return 3;
    if (a <= 17) return 4;
    if (a <= 25) return 5;
    if (a <= 35) return 6;
    if (a <= 45) return 7;
    return 8;
}

void computeRange(int &lo, int &hi)
{
    int g = (gender == 'M') ? 1 : 0;

    if (conditionSel > 0 && conditionSel <= 6)
    {
        Range r = tableDisease[conditionSel - 1][g];
        lo = r.lo; hi = r.hi;
    }
    else if (exerciseType == 1)
    {
        if (sportSel == 6)
        {
            // "Otro deporte": sin tabla propia, aproximar por edad.
            int row = ageToRow(age);
            lo = tableAge[row][g].lo;
            hi = tableAge[row][g].hi;
        }
        else
        {
            Range r = tableSport[sportSel][g];
            lo = r.lo; hi = r.hi;
        }
    }
    else if (exerciseType == 2)
    {
        Range r = tableExercise[exerciseLevel][g];
        lo = r.lo; hi = r.hi;
    }
    else
    {
        int row = ageToRow(age);
        lo = tableAge[row][g].lo;
        hi = tableAge[row][g].hi;
    }
}

void saveProfile()
{
    prefs.begin("profile", false);
    prefs.putBool("configured", true);
    prefs.putChar("lang", lang);
    prefs.putUInt("age", age);
    prefs.putChar("gender", gender);
    prefs.putUInt("exerciseType", exerciseType);
    prefs.putUInt("sportSel", sportSel);
    prefs.putUInt("exerciseLevel", exerciseLevel);
    prefs.putUInt("conditionSel", conditionSel);
    prefs.end();
}

bool loadProfile()
{
    prefs.begin("profile", true);
    bool configured = prefs.getBool("configured", false);
    if (configured)
    {
        lang = prefs.getChar("lang", 'E');
        age = prefs.getUInt("age", 0);
        gender = prefs.getChar("gender", ' ');
        exerciseType = prefs.getUInt("exerciseType", 0);
        sportSel = prefs.getUInt("sportSel", 0);
        exerciseLevel = prefs.getUInt("exerciseLevel", 0);
        conditionSel = prefs.getUInt("conditionSel", 0);
        conditions = (conditionSel > 0);
    }
    prefs.end();
    return configured;
}

void resetProfile()
{
    prefs.begin("profile", false);
    prefs.clear();
    prefs.end();
}

bool profileIsValid()
{
    return age > 0 && (gender == 'M' || gender == 'F');
}

int profileToJson(char *buf, size_t n)
{
    return snprintf(buf, n,
        "{\"lang\":\"%c\",\"age\":%d,\"gender\":\"%c\",\"exercise\":%d,"
        "\"sport\":%d,\"level\":%d,\"condition\":%d}",
        lang, age, (gender ? gender : ' '), exerciseType,
        sportSel, exerciseLevel, conditionSel);
}

// Extrae un entero por clave: busca "\"clave\":". Sin parser JSON
// completo: el protocolo es nuestro y las claves son fijas.
static bool jsonInt(const char *json, const char *key, int *out)
{
    char pat[24];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return false;
    *out = atoi(p + strlen(pat));
    return true;
}

static bool jsonChar(const char *json, const char *key, char *out)
{
    char pat[24];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(json, pat);
    if (!p) return false;
    char c = p[strlen(pat)];
    if (c) { *out = c; return true; }
    return false;
}

bool profileFromJson(const char *json)
{
    if (!json || !strchr(json, '{')) return false;
    int v;
    char c;

    if (jsonChar(json, "lang", &c) && (c == 'E' || c == 'I')) lang = c;
    if (jsonInt(json, "age", &v)) age = constrain(v, 0, 120);
    if (jsonChar(json, "gender", &c) && (c == 'M' || c == 'F')) gender = c;
    if (jsonInt(json, "exercise", &v)) exerciseType = constrain(v, 0, 2);
    if (jsonInt(json, "sport", &v)) sportSel = constrain(v, 0, 6);
    if (jsonInt(json, "level", &v)) exerciseLevel = constrain(v, 0, 2);
    if (jsonInt(json, "condition", &v)) conditionSel = constrain(v, 0, 6);
    conditions = (conditionSel > 0);
    return true;
}
