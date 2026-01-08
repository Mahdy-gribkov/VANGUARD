/**
 * @file AssessorEngine.cpp
 * @brief The orchestrator - WiFi scanning with proper timing
 */

#include "AssessorEngine.h"
#include "../adapters/BruceWiFi.h"
#include "../adapters/BruceBLE.h"
#include "../adapters/EvilPortal.h"
#include <WiFi.h>

namespace Assessor {

// =============================================================================
// SINGLETON
// =============================================================================

AssessorEngine& AssessorEngine::getInstance() {
    static AssessorEngine instance;
    return instance;
}

AssessorEngine::AssessorEngine()
    : m_initialized(false)
    , m_scanState(ScanState::IDLE)
    , m_scanProgress(0)
    , m_actionActive(false)
    , m_combinedScan(false)
    , m_onScanProgress(nullptr)
    , m_onActionProgress(nullptr)
    , m_scanStartMs(0)
    , m_actionStartMs(0)
{
    m_actionProgress.type = ActionType::NONE;
    m_actionProgress.result = ActionResult::SUCCESS;
    m_actionProgress.packetsSent = 0;
}

AssessorEngine::~AssessorEngine() {
    shutdown();
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool AssessorEngine::init() {
    if (m_initialized) return true;

    if (Serial) {
        Serial.println("[WiFi] Initializing...");
    }

    // Step 1: Clean shutdown any existing WiFi state
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Distributed delays with watchdog feeding
    for (int i = 0; i < 5; i++) { yield(); delay(20); }

    // Step 2: Set station mode
    WiFi.mode(WIFI_STA);
    for (int i = 0; i < 5; i++) { yield(); delay(20); }

    // Step 3: Delete any old scan results
    WiFi.scanDelete();
    yield();

    // Step 4: Verify mode
    if (WiFi.getMode() != WIFI_STA) {
        if (Serial) {
            Serial.println("[WiFi] ERROR: Mode not STA!");
        }
        return false;
    }

    m_initialized = true;

    if (Serial) {
        Serial.printf("[WiFi] Ready (MAC: %s)\n", WiFi.macAddress().c_str());
    }
    return true;
}

void AssessorEngine::shutdown() {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    m_initialized = false;
}

void AssessorEngine::tick() {
    // Handle ASYNC WiFi scanning
    if (m_scanState == ScanState::WIFI_SCANNING) {
        // Check if async scan is complete
        int16_t scanResult = WiFi.scanComplete();

        if (scanResult == WIFI_SCAN_RUNNING) {
            // Still scanning - update progress based on elapsed time
            uint32_t elapsed = millis() - m_scanStartMs;
            // Assume ~5 seconds for full scan, cap at 45% for WiFi portion
            m_scanProgress = min(45, (int)(elapsed / 110));
        }
        else if (scanResult == WIFI_SCAN_FAILED) {
            if (Serial) {
                Serial.println("[WiFi] Scan failed!");
            }
            // Try to continue to BLE if combined scan
            if (m_combinedScan) {
                processScanResults(0);  // Will chain to BLE
            } else {
                m_scanState = ScanState::COMPLETE;
                m_scanProgress = 100;
            }
        }
        else if (scanResult >= 0) {
            // Scan complete with results
            if (Serial) {
                Serial.printf("[WiFi] Async scan complete: %d networks\n", scanResult);
            }
            processScanResults(scanResult);
        }

        // Safety timeout - 10 seconds max for WiFi scan
        uint32_t elapsed = millis() - m_scanStartMs;
        if (elapsed > 10000 && m_scanState == ScanState::WIFI_SCANNING) {
            if (Serial) {
                Serial.println("[WiFi] Scan timeout, forcing complete");
            }
            int16_t partialResult = WiFi.scanComplete();
            if (partialResult > 0) {
                processScanResults(partialResult);
            } else if (m_combinedScan) {
                processScanResults(0);
            } else {
                m_scanState = ScanState::COMPLETE;
                m_scanProgress = 100;
            }
        }
    }

    // Handle BLE scanning (async via NimBLE)
    if (m_scanState == ScanState::BLE_SCANNING) {
        BruceBLE& ble = BruceBLE::getInstance();
        ble.tick();

        // Safety timeout - if BLE scan takes too long, force complete
        uint32_t elapsed = millis() - m_scanStartMs;
        bool timedOut = elapsed > 6000;  // 6 second max

        if (ble.isScanComplete() || timedOut) {
            if (timedOut && Serial) {
                Serial.println("[BLE] Scan timed out, forcing complete");
            }
            ble.stopScan();
            processBLEScanResults();
        }
        else {
            // Update progress - BLE is 50-100% if combined, 0-100% if standalone
            int bleProgress = min(95, (int)(elapsed / 50));
            if (m_combinedScan) {
                m_scanProgress = 50 + (bleProgress / 2);  // 50-97%
            } else {
                m_scanProgress = bleProgress;
            }
        }
    }

    // Handle active attacks
    if (m_actionActive) {
        tickAction();
    }
}

// =============================================================================
// SCANNING
// =============================================================================

void AssessorEngine::beginScan() {
    if (Serial) {
        Serial.println("[Scan] === BEGIN COMBINED SCAN ===");
    }

    // Always reinit to ensure clean state
    m_initialized = false;
    if (!init()) {
        if (Serial) {
            Serial.println("[WiFi] Init failed, aborting scan");
        }
        m_scanState = ScanState::COMPLETE;
        m_scanProgress = 100;
        return;
    }

    // Clear old results
    WiFi.scanDelete();
    yield();

    m_targetTable.clear();
    m_scanProgress = 0;
    m_scanStartMs = millis();
    m_combinedScan = true;  // Will chain to BLE after WiFi

    if (Serial) {
        Serial.println("[WiFi] Starting ASYNC scan...");
    }

    // Use ASYNC scan - non-blocking, check in tick()
    // Parameters: async=true, show_hidden=true, passive=false, max_ms=300ms per channel
    WiFi.scanNetworks(true, true, false, 300);

    m_scanState = ScanState::WIFI_SCANNING;

    if (m_onScanProgress) {
        m_onScanProgress(m_scanState, m_scanProgress);
    }
}

void AssessorEngine::beginWiFiScan() {
    if (Serial) {
        Serial.println("[WiFi] === BEGIN WIFI SCAN ===");
    }

    // Always reinit to ensure clean state
    m_initialized = false;
    if (!init()) {
        if (Serial) {
            Serial.println("[WiFi] Init failed, aborting scan");
        }
        m_scanState = ScanState::COMPLETE;
        m_scanProgress = 100;
        return;
    }

    // Clear old results
    WiFi.scanDelete();
    yield();

    m_targetTable.clear();
    m_scanProgress = 0;
    m_scanStartMs = millis();
    m_combinedScan = false;  // WiFi only

    if (Serial) {
        Serial.println("[WiFi] Starting ASYNC scan...");
    }

    // Use ASYNC scan - non-blocking, check in tick()
    WiFi.scanNetworks(true, true, false, 300);

    m_scanState = ScanState::WIFI_SCANNING;

    if (m_onScanProgress) {
        m_onScanProgress(m_scanState, m_scanProgress);
    }
}

void AssessorEngine::beginBLEScan() {
    if (Serial) {
        Serial.println("[BLE] Starting BLE-only scan...");
    }

    m_targetTable.clear();
    m_scanProgress = 0;
    m_scanStartMs = millis();
    m_combinedScan = false;

    // Feed watchdog before BLE operations
    for (int i = 0; i < 10; i++) { yield(); delay(10); }

    BruceBLE& ble = BruceBLE::getInstance();

    // Try init with proper delays
    bool bleInitOk = false;
    for (int attempt = 0; attempt < 2; attempt++) {
        for (int i = 0; i < 5; i++) { yield(); delay(10); }

        bleInitOk = ble.init();
        if (bleInitOk) break;

        if (Serial) {
            Serial.printf("[BLE] Init attempt %d failed\n", attempt + 1);
        }
    }

    if (!bleInitOk) {
        if (Serial) {
            Serial.println("[BLE] Init failed!");
        }
        m_scanState = ScanState::COMPLETE;
        m_scanProgress = 100;
        return;
    }

    m_scanState = ScanState::BLE_SCANNING;
    ble.beginScan(5000);

    if (m_onScanProgress) {
        m_onScanProgress(m_scanState, m_scanProgress);
    }
}

void AssessorEngine::stopScan() {
    WiFi.scanDelete();
    BruceBLE::getInstance().stopScan();
    m_scanState = ScanState::IDLE;
    m_combinedScan = false;
}

bool AssessorEngine::isCombinedScan() const {
    return m_combinedScan;
}

ScanState AssessorEngine::getScanState() const {
    return m_scanState;
}

uint8_t AssessorEngine::getScanProgress() const {
    return m_scanProgress;
}

void AssessorEngine::onScanProgress(ScanProgressCallback cb) {
    m_onScanProgress = cb;
}

void AssessorEngine::tickScan() {
    // Handled in tick()
}

void AssessorEngine::processScanResults(int count) {
    for (int i = 0; i < count; i++) {
        Target target;
        memset(&target, 0, sizeof(Target));

        // Get BSSID
        uint8_t* bssid = WiFi.BSSID(i);
        if (bssid) {
            memcpy(target.bssid, bssid, 6);
        }

        // Get SSID
        String ssid = WiFi.SSID(i);
        strncpy(target.ssid, ssid.c_str(), SSID_MAX_LEN);
        target.ssid[SSID_MAX_LEN] = '\0';

        target.type = TargetType::ACCESS_POINT;
        target.channel = WiFi.channel(i);
        target.rssi = WiFi.RSSI(i);

        // Map encryption
        wifi_auth_mode_t enc = WiFi.encryptionType(i);
        switch (enc) {
            case WIFI_AUTH_OPEN:
                target.security = SecurityType::OPEN;
                break;
            case WIFI_AUTH_WEP:
                target.security = SecurityType::WEP;
                break;
            case WIFI_AUTH_WPA_PSK:
                target.security = SecurityType::WPA_PSK;
                break;
            case WIFI_AUTH_WPA2_PSK:
            case WIFI_AUTH_WPA_WPA2_PSK:
                target.security = SecurityType::WPA2_PSK;
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                target.security = SecurityType::WPA2_ENTERPRISE;
                break;
            case WIFI_AUTH_WPA3_PSK:
                target.security = SecurityType::WPA3_SAE;
                break;
            default:
                target.security = SecurityType::UNKNOWN;
        }

        target.isHidden = (strlen(target.ssid) == 0);
        if (target.isHidden) {
            strcpy(target.ssid, "[Hidden]");
        }

        target.firstSeenMs = millis();
        target.lastSeenMs = millis();
        target.beaconCount = 1;
        target.clientCount = 0;

        m_targetTable.addOrUpdate(target);
    }

    WiFi.scanDelete();  // Free memory

    // If combined scan, chain to BLE scan
    if (m_combinedScan) {
        if (Serial) {
            Serial.println("[Scan] WiFi done, preparing BLE...");
        }

        // Shut down WiFi before starting BLE (ESP32 shares radio)
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(200);  // Brief transition delay

        BruceBLE& ble = BruceBLE::getInstance();
        ble.shutdown();
        delay(100);

        // Try BLE init with retries
        bool bleInitOk = false;
        for (int attempt = 0; attempt < 3; attempt++) {
            if (Serial) {
                Serial.printf("[BLE] Init attempt %d...\n", attempt + 1);
            }

            bleInitOk = ble.init();
            if (bleInitOk) {
                if (Serial) {
                    Serial.println("[BLE] Init success!");
                }
                break;
            }

            if (Serial) {
                Serial.printf("[BLE] Init attempt %d failed\n", attempt + 1);
            }
            delay(100);  // Wait before retry
        }

        if (!bleInitOk) {
            if (Serial) {
                Serial.println("[BLE] Init failed after retries, completing without BLE");
            }
            m_scanState = ScanState::COMPLETE;
            m_scanProgress = 100;
            if (m_onScanProgress) {
                m_onScanProgress(m_scanState, m_scanProgress);
            }
            return;
        }

        m_scanState = ScanState::BLE_SCANNING;
        m_scanProgress = 50;
        m_scanStartMs = millis();
        ble.beginScan(3000);

        if (m_onScanProgress) {
            m_onScanProgress(m_scanState, m_scanProgress);
        }
    } else {
        m_scanState = ScanState::COMPLETE;
        m_scanProgress = 100;

        if (m_onScanProgress) {
            m_onScanProgress(m_scanState, m_scanProgress);
        }
    }
}

void AssessorEngine::processBLEScanResults() {
    BruceBLE& ble = BruceBLE::getInstance();
    const std::vector<BLEDeviceInfo>& devices = ble.getDevices();

    if (Serial) {
        Serial.printf("[BLE] Scan complete: %d devices found\n", devices.size());
    }

    for (const auto& device : devices) {
        Target target;
        memset(&target, 0, sizeof(Target));

        // Copy BLE address as BSSID
        memcpy(target.bssid, device.address, 6);

        // Copy device name as SSID
        strncpy(target.ssid, device.name, SSID_MAX_LEN);
        target.ssid[SSID_MAX_LEN] = '\0';

        target.type = TargetType::BLE_DEVICE;
        target.channel = 0;  // BLE doesn't use WiFi channels
        target.rssi = device.rssi;
        target.security = SecurityType::UNKNOWN;  // BLE security is different

        target.isHidden = (strlen(target.ssid) == 0);
        if (target.isHidden) {
            // Format address as name
            snprintf(target.ssid, SSID_MAX_LEN, "BLE %02X:%02X:%02X:%02X:%02X:%02X",
                     device.address[0], device.address[1], device.address[2],
                     device.address[3], device.address[4], device.address[5]);
        }

        target.firstSeenMs = device.lastSeenMs;
        target.lastSeenMs = device.lastSeenMs;
        target.beaconCount = 1;
        target.clientCount = 0;

        m_targetTable.addOrUpdate(target);
    }

    m_scanState = ScanState::COMPLETE;
    m_scanProgress = 100;

    if (m_onScanProgress) {
        m_onScanProgress(m_scanState, m_scanProgress);
    }
}

// =============================================================================
// TARGETS
// =============================================================================

const std::vector<Target>& AssessorEngine::getTargets() const {
    return m_targetTable.getAll();
}

size_t AssessorEngine::getTargetCount() const {
    return m_targetTable.count();
}

std::vector<Target> AssessorEngine::getFilteredTargets(const TargetFilter& filter,
                                                        SortOrder order) const {
    return m_targetTable.getFiltered(filter, order);
}

const Target* AssessorEngine::findTarget(const uint8_t* bssid) const {
    return m_targetTable.findByBssid(bssid);
}

void AssessorEngine::clearTargets() {
    m_targetTable.clear();
}

// =============================================================================
// ACTIONS
// =============================================================================

std::vector<AvailableAction> AssessorEngine::getActionsFor(const Target& target) const {
    return m_actionResolver.getActionsFor(target);
}

bool AssessorEngine::executeAction(ActionType action, const Target& target) {
    // Reset progress
    m_actionProgress.type = action;
    m_actionProgress.result = ActionResult::IN_PROGRESS;
    m_actionProgress.packetsSent = 0;
    m_actionProgress.elapsedMs = 0;
    m_actionProgress.statusText = nullptr;
    m_actionStartMs = millis();
    m_actionActive = true;

    // Check for 5GHz limitation (ESP32 can only transmit on 2.4GHz)
    if (target.channel > 14) {
        m_actionProgress.result = ActionResult::FAILED_NOT_SUPPORTED;
        m_actionProgress.statusText = "5GHz not supported";
        m_actionActive = false;
        return false;
    }

    BruceWiFi& wifi = BruceWiFi::getInstance();

    switch (action) {
        case ActionType::DEAUTH_SINGLE:
        case ActionType::DEAUTH_ALL: {
            if (!wifi.init()) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "WiFi init failed";
                m_actionActive = false;
                return false;
            }

            m_actionProgress.statusText = "Sending deauth...";

            bool success = wifi.deauthAll(target.bssid, target.channel);
            if (!success) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "Deauth start failed";
                m_actionActive = false;
                return false;
            }

            if (Serial) {
                Serial.printf("[Attack] Deauth on ch%d\n", target.channel);
            }
            return true;
        }

        case ActionType::BEACON_FLOOD: {
            if (!wifi.init()) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "WiFi init failed";
                m_actionActive = false;
                return false;
            }

            m_actionProgress.statusText = "Beacon flood...";

            static const char* fakeSSIDs[] = {
                "Free WiFi", "xfinity", "ATT-WiFi", "NETGEAR",
                "linksys", "FBI Van", "Virus.exe", "GetYourOwn"
            };

            bool success = wifi.beaconFlood(fakeSSIDs, 8, target.channel);
            if (!success) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "Beacon start failed";
                m_actionActive = false;
                return false;
            }
            return true;
        }

        case ActionType::BLE_SPAM: {
            BruceBLE& ble = BruceBLE::getInstance();
            if (!ble.init()) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "BLE init failed";
                m_actionActive = false;
                return false;
            }

            m_actionProgress.statusText = "BLE spam...";
            bool success = ble.startSpam(BLESpamType::RANDOM);
            if (!success) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "BLE spam failed";
                m_actionActive = false;
                return false;
            }
            if (Serial) {
                Serial.println("[Attack] BLE spam started");
            }
            return true;
        }

        case ActionType::BLE_SOUR_APPLE: {
            BruceBLE& ble = BruceBLE::getInstance();
            if (!ble.init()) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "BLE init failed";
                m_actionActive = false;
                return false;
            }

            m_actionProgress.statusText = "Sour Apple...";
            bool success = ble.startSpam(BLESpamType::SOUR_APPLE);
            if (!success) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "Sour Apple failed";
                m_actionActive = false;
                return false;
            }
            if (Serial) {
                Serial.println("[Attack] Sour Apple started");
            }
            return true;
        }

        case ActionType::EVIL_TWIN: {
            // Use the new Evil Portal (full captive portal with credential capture)
            EvilPortal& portal = EvilPortal::getInstance();

            m_actionProgress.statusText = "Starting evil portal...";

            // Stop any existing portal first
            if (portal.isRunning()) {
                portal.stop();
            }

            // Start with generic template (could add template selection later)
            bool success = portal.start(target.ssid, target.channel, PortalTemplate::GENERIC_WIFI);
            if (!success) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "Portal failed to start";
                m_actionActive = false;
                return false;
            }

            if (Serial) {
                Serial.printf("[Attack] Evil Portal started: %s\n", target.ssid);
            }
            return true;
        }

        case ActionType::CAPTURE_HANDSHAKE: {
            if (!wifi.init()) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "WiFi init failed";
                m_actionActive = false;
                return false;
            }

            m_actionProgress.statusText = "Capturing...";
            bool success = wifi.captureHandshake(target.bssid, target.channel, true);
            if (!success) {
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "Capture failed";
                m_actionActive = false;
                return false;
            }
            return true;
        }

        default:
            m_actionProgress.result = ActionResult::FAILED_NOT_SUPPORTED;
            m_actionProgress.statusText = "Not implemented";
            m_actionActive = false;
            return false;
    }
}

void AssessorEngine::stopAction() {
    BruceWiFi& wifi = BruceWiFi::getInstance();
    wifi.stopAttack();

    // Also stop BLE attacks if running
    BruceBLE& ble = BruceBLE::getInstance();
    ble.stopAttack();

    // Stop Evil Portal if running
    EvilPortal& portal = EvilPortal::getInstance();
    if (portal.isRunning()) {
        portal.stop();
    }

    m_actionActive = false;
    m_actionProgress.result = ActionResult::CANCELLED;
    m_actionProgress.statusText = "Stopped";

    if (Serial) {
        Serial.println("[Attack] Stopped");
    }
}

bool AssessorEngine::isActionActive() const {
    return m_actionActive;
}

ActionProgress AssessorEngine::getActionProgress() const {
    return m_actionProgress;
}

void AssessorEngine::onActionProgress(ActionProgressCallback cb) {
    m_onActionProgress = cb;
}

void AssessorEngine::tickAction() {
    if (!m_actionActive) return;

    BruceWiFi& wifi = BruceWiFi::getInstance();
    wifi.tick();

    // Also tick BLE for BLE attacks
    BruceBLE& ble = BruceBLE::getInstance();
    ble.tick();

    // Tick Evil Portal if running
    EvilPortal& portal = EvilPortal::getInstance();
    if (portal.isRunning()) {
        portal.tick();

        // Update status with credential count
        size_t credCount = portal.getCredentialCount();
        int clientCount = portal.getClientCount();
        if (credCount > 0) {
            static char statusBuf[48];
            snprintf(statusBuf, sizeof(statusBuf), "Portal: %d clients, %d creds",
                     clientCount, (int)credCount);
            m_actionProgress.statusText = statusBuf;
        } else {
            static char statusBuf[32];
            snprintf(statusBuf, sizeof(statusBuf), "Portal: %d clients", clientCount);
            m_actionProgress.statusText = statusBuf;
        }
    }

    // Update progress from either WiFi or BLE
    uint32_t wifiPackets = wifi.getPacketsSent();
    uint32_t blePackets = ble.getAdvertisementsSent();
    m_actionProgress.packetsSent = wifiPackets + blePackets;
    m_actionProgress.elapsedMs = millis() - m_actionStartMs;

    // Report progress
    if (m_onActionProgress) {
        m_onActionProgress(m_actionProgress);
    }
}

// =============================================================================
// HARDWARE STATUS
// =============================================================================

bool AssessorEngine::hasWiFi() const {
    return m_initialized;
}

bool AssessorEngine::hasBLE() const {
    return true;
}

bool AssessorEngine::hasRF() const {
    return false;
}

bool AssessorEngine::hasIR() const {
    return true;
}

} // namespace Assessor
