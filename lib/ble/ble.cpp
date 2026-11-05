#include <Arduino.h>
#include <NimBLEDevice.h>
#include "ble.h"
#include "profile.h"

// =====================================================================
// TODO(UUIDS): UUIDs de REEMPLAZO. Sustituir por los de la app movil
// existente (solo cambiar estos 4 strings; el protocolo JSON de abajo
// y en README.md se mantiene).
// =====================================================================
static const char *SVC_UUID     = "b1e50001-6f2d-4a4e-9c11-5f8a2d3e7b01";
static const char *CHAR_PROFILE = "b1e50002-6f2d-4a4e-9c11-5f8a2d3e7b01";
static const char *CHAR_MEAS    = "b1e50003-6f2d-4a4e-9c11-5f8a2d3e7b01";
static const char *CHAR_CMD     = "b1e50004-6f2d-4a4e-9c11-5f8a2d3e7b01";

static NimBLEServer *server = nullptr;
static NimBLECharacteristic *chrProfile = nullptr;
static NimBLECharacteristic *chrMeas = nullptr;
static NimBLECharacteristic *chrCmd = nullptr;
static volatile bool connected = false;

class ServerCB : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *, NimBLEConnInfo &) override
    {
        connected = true;
        Serial.println("[ble] app conectada");
    }
    void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override
    {
        connected = false;
        Serial.println("[ble] app desconectada");
    }
};

class CmdCB : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override
    {
        NimBLEAttValue v = c->getValue();
        std::string s((const char *)v.data(), v.size());
        Serial.printf("[ble] cmd recibido: %s\n", s.c_str());
        // TODO(comando remoto): "measure" podria disparar el flujo de
        // medicion vía una bandera que lea appLoop. Definir con la app.
    }
};

static ServerCB serverCB;
static CmdCB cmdCB;

void bleInit()
{
    NimBLEDevice::init("HRT-Sim");
    NimBLEDevice::setMTU(247);

    server = NimBLEDevice::createServer();
    server->setCallbacks(&serverCB);

    NimBLEService *svc = server->createService(SVC_UUID);
    chrProfile = svc->createCharacteristic(
        CHAR_PROFILE, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    chrMeas = svc->createCharacteristic(
        CHAR_MEAS, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    chrCmd = svc->createCharacteristic(
        CHAR_CMD, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrCmd->setCallbacks(&cmdCB);

    // Valores iniciales
    blePublishProfile();
    chrMeas->setValue("{\"bpm\":0}");

    // (en NimBLE 2.x los servicios arrancan con el server al iniciar
    // advertising; svc->start() es un no-op deprecado)

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SVC_UUID);
    adv->enableScanResponse(true);
    if (adv->start())
        Serial.println("[ble] advertising HRT-Sim activo");
    else
        Serial.println("[ble] ERROR: advertising no arranco");
}

void bleUpdate()
{
    if (connected) return;
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    if (!adv->isAdvertising())
        adv->start();
}

bool bleIsConnected()
{
    return connected;
}

void bleNotifyMeasurement(const char *json)
{
    if (!connected || !chrMeas || !json) return;
    chrMeas->setValue((uint8_t *)json, strlen(json));
    chrMeas->notify();
    Serial.printf("[ble] medicion notificada: %s\n", json);
}

void blePublishProfile()
{
    if (!chrProfile) return;
    char buf[192];
    int n = profileToJson(buf, sizeof(buf));
    chrProfile->setValue((uint8_t *)buf, n > 0 ? (size_t)n : 0);
}
