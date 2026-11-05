#pragma once
#include <Arduino.h>

// BLE para intercambio de datos con la app movil (NimBLE).
//
// TODO(UUIDS): los UUIDs viven en ble.cpp y hoy son de REEMPLAZO.
// Cuando pases los de tu app existente, se sustituyen solo esos 4
// strings; el protocolo (JSON) documentado en README.md no cambia.
//
// Servicio HRT con 3 caracteristicas:
//   PROFILE (R/W): JSON del perfil; la app puede escribirlo para
//                  actualizar el registro del dispositivo.
//   MEAS    (R/N): JSON de la ultima medicion (notify al clasificar).
//   CMD     (W)  : JSON {"cmd":"..."} (measure|cancel|ping).

void bleInit();
void bleUpdate();                  // re-lanza advertising si se desconecta
bool bleIsConnected();

// Notify de la medicion final (json en ble.h? no: string armado por app).
void bleNotifyMeasurement(const char *json);

// Refresca la caracteristica PROFILE desde el perfil actual (NVS).
void blePublishProfile();
