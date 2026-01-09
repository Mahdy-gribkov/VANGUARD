#include "RadioWarden.h"
#include <WiFi.h>
#include "../adapters/BruceBLE.h"

namespace Vanguard {

RadioWarden& RadioWarden::getInstance() {
    static RadioWarden instance;
    return instance;
}

RadioWarden::RadioWarden() {}

bool RadioWarden::requestRadio(RadioOwner owner) {
    if (m_currentOwner == owner) return true;

    if (Serial) {
        Serial.printf("[Warden] Requesting handover: %d -> %d\n", (int)m_currentOwner, (int)owner);
    }

    shutdownCurrent();

    bool success = false;
    switch (owner) {
        case RadioOwner::WIFI_STA:
            success = initWiFiSTA();
            break;
        case RadioOwner::WIFI_PROMISCUOUS:
            success = initWiFiPromiscuous();
            break;
        case RadioOwner::BLE:
            success = initBLE();
            break;
        default:
            success = true; // NONE
            break;
    }

    if (success) {
        m_currentOwner = owner;
    }
    return success;
}

void RadioWarden::releaseRadio() {
    shutdownCurrent();
    m_currentOwner = RadioOwner::NONE;
}

void RadioWarden::shutdownCurrent() {
    if (m_currentOwner == RadioOwner::NONE) return;

    if (Serial) Serial.println("[Warden] Shutting down current radio...");

    // WiFi Cleanup
    if (m_currentOwner == RadioOwner::WIFI_STA || m_currentOwner == RadioOwner::WIFI_PROMISCUOUS) {
        esp_wifi_set_promiscuous(false);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();
        // Give hardware time to settle
        delay(50); 
    }

    // BLE Cleanup - we don't deinit NimBLE, just stop activities
    if (m_currentOwner == RadioOwner::BLE) {
        BruceBLE::getInstance().stopAttack();
    }
}

bool RadioWarden::initWiFiSTA() {
    WiFi.mode(WIFI_STA);
    if (esp_wifi_start() != ESP_OK) return false;
    return true;
}

bool RadioWarden::initWiFiPromiscuous() {
    WiFi.mode(WIFI_STA);
    if (esp_wifi_start() != ESP_OK) return false;
    esp_wifi_set_promiscuous(true);
    return true;
}

bool RadioWarden::initBLE() {
    // NimBLE init is handled by BruceBLE (should be init-once)
    return BruceBLE::getInstance().init();
}

} // namespace Vanguard
