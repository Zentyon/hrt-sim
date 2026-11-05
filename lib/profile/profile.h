#pragma once
#include <stddef.h>

struct Range { int lo; int hi; };

// Perfil de usuario (persistido en NVS)
extern char lang;          // 'E'=ESP, 'I'=ENG
extern int age;
extern char gender;        // 'M'=masculino, 'F'=femenino
extern int exerciseType;   // 0=Ninguno, 1=Deporte, 2=Otro(con intensidad)
extern int sportSel;       // 0-6 indice de tableSport (6 = "Otro deporte")
extern int exerciseLevel;  // 0=Baja, 1=Media, 2=Alta (cuando exerciseType==2)
extern int conditionSel;   // 0=Ninguna, 1-6 (ver tableDisease)
extern bool conditions;    // derivado de conditionSel
extern char ageInput[4];
extern int ageIdx;

// Tablas de rango BPM (reposo por edad / actividad / enfermedad / deporte).
// Alineadas con los audios del flujo: deportes G03-G09 (1-7, 7=Otro) y
// padecimientos H03-H08 (1-6).
// TODO(user): sustituir por la tabla completa de referencia del proyecto
// cuando me la pases; hoy es una aproximacion.
extern const Range tableAge[9][2];
extern const Range tableExercise[3][2];
extern const Range tableDisease[6][2];
extern const Range tableSport[7][2];
extern const char* sportNames[2][7];

int ageToRow(int a);
void computeRange(int &lo, int &hi);
void saveProfile();
bool loadProfile();
void resetProfile();

// Registro completo (edad y genero definidos). Si es false al arrancar,
// el firmware entra al onboarding.
bool profileIsValid();

// JSON del perfil para BLE (app movil). Serializacion a medida, sin
// dependencias: mismas claves que profileFromJson.
//   {"lang":"E","age":30,"gender":"M","exercise":1,"sport":0,"level":0,"condition":0}
int  profileToJson(char *buf, size_t n);
bool profileFromJson(const char *json);   // aplica claves presentes
