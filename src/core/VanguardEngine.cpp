/**
 * @file VanguardEngine.cpp
 * @brief The orchestrator - WiFi scanning with proper timing
 */

#include "VanguardEngine.h"
#include "../adapters/BruceWiFi.h"
#include "../adapters/BruceBLE.h"
#include "../adapters/EvilPortal.h"
#include "../adapters/BruceIR.h"
#include "SDManager.h"
#include "RadioWarden.h"
#include "../ui/FeedbackManager.h"
#include <WiFi.h>

namespace Vanguard {

// =============================================================================
// HELPER: Watchdog-safe delay
// =============================================================================

static void yieldDelay(uint32_t ms) {
    uint32_t start = millis();
    while (millis() - start < ms) {
        yield();
        delay(1);
    }
}

// =============================================================================
// SINGLETON
// =============================================================================

VanguardEngine& VanguardEngine::getInstance() {
    static VanguardEngine instance;
    return instance;
}

VanguardEngine::VanguardEngine()
    : m_initialized(false)
    , m_scanState(ScanState::IDLE)
    , m_scanProgress(0)
    , m_actionActive(false)
    , m_combinedScan(false)
    , m_onScanProgress(nullptr)
    , m_onActionProgress(nullptr)
    , m_scanStartMs(0)
    , m_actionStartMs(0)
    , m_transitionStep(0)
    , m_transitionStartMs(0)
    , m_bleInitAttempts(0)
{
    m_actionProgress.type = ActionType::NONE;
    m_actionProgress.result = ActionResult::SUCCESS;
    m_actionProgress.packetsSent = 0;
}

VanguardEngine::~VanguardEngine() {
    shutdown();
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool VanguardEngine::init() {
    if (m_initialized) return true;

    if (Serial) {
        Serial.println("[Engine] Initializing...");
    }

    // Initialize Init-Once hardware
    BruceBLE::getInstance().init();
    BruceIR::getInstance().init();

    // Start in Station mode by default
    RadioWarden::getInstance().requestRadio(RadioOwner::WIFI_STA);

    m_initialized = true;

    // Step 5: Initialize SD Card
    SDManager::getInstance().init();

    // Step 7: Add virtual targets
    m_targetTable.addVirtualTarget("Universal Remote", TargetType::IR_DEVICE);

    // Wire up BruceWiFi associations
    BruceWiFi::getInstance().onAssociation([this](const uint8_t* client, const uint8_t* ap) {
        if (this->m_targetTable.addAssociation(client, ap)) {
             FeedbackManager::getInstance().pulse(50); // Feedback on client discovery
        }
    });

    if (Serial) {
        Serial.printf("[Engine] Ready\n");
    }
    return true;
}

void VanguardEngine::shutdown() {
    RadioWarden::getInstance().releaseRadio();
    m_initialized = false;
}

void VanguardEngine::tick() {
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
                processScanResults(0);  // Will start transition
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

    // Handle NON-BLOCKING WiFi→BLE transition
    if (m_scanState == ScanState::TRANSITIONING_TO_BLE) {
        tickTransition();
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

void VanguardEngine::beginScan() {
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
        Serial.println("[WiFi] Starting PASSIVE scan...");
    }

    // Delegates to RadioWarden via BruceWiFi::onEnable
    BruceWiFi::getInstance().beginScan();


    m_scanState = ScanState::WIFI_SCANNING;

    if (m_onScanProgress) {
        m_onScanProgress(m_scanState, m_scanProgress);
    }
}

void VanguardEngine::beginWiFiScan() {
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
void VanguardEngine::beginBLEScan() {
    if (Serial) {
        Serial.println("[BLE] Starting non-blocking BLE-only scan...");
    }

    m_targetTable.clear();
    m_scanProgress = 0;
    m_scanStartMs = millis();
    m_combinedScan = false;

    // Start non-blocking initiation of BLE
    m_scanState = ScanState::TRANSITIONING_TO_BLE;
    m_transitionStep = 2; // Jump to step 2 (BLE shutdown/init)
    m_transitionStartMs = millis();
    m_bleInitAttempts = 0;

    if (m_onScanProgress) {
        m_onScanProgress(m_scanState, m_scanProgress);
    }
}

void VanguardEngine::stopScan() {
    WiFi.scanDelete();
    BruceBLE::getInstance().stopScan();
    m_scanState = ScanState::IDLE;
    m_combinedScan = false;
}

bool VanguardEngine::isCombinedScan() const {
    return m_combinedScan;
}

ScanState VanguardEngine::getScanState() const {
    return m_scanState;
}

uint8_t VanguardEngine::getScanProgress() const {
    return m_scanProgress;
}

void VanguardEngine::onScanProgress(ScanProgressCallback cb) {
    m_onScanProgress = cb;
}

void VanguardEngine::tickScan() {
    // Handled in tick()
}

void VanguardEngine::processScanResults(int count) {
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

    // If combined scan, start NON-BLOCKING transition to BLE
    if (m_combinedScan) {
        if (Serial) {
            Serial.println("[Scan] WiFi done, starting BLE transition...");
        }

        // Start the transition state machine
        m_scanState = ScanState::TRANSITIONING_TO_BLE;
        m_transitionStep = 0;
        m_transitionStartMs = millis();
        m_bleInitAttempts = 0;
        m_scanProgress = 46;  // Just past WiFi

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

// =============================================================================
// NON-BLOCKING WIFI→BLE TRANSITION
// =============================================================================

void VanguardEngine::tickTransition() {
    // State machine for WiFi→BLE transition
    // Each step does minimal work and returns, next tick continues
    uint32_t elapsed = millis() - m_transitionStartMs;

    switch (m_transitionStep) {
        case 0:
            // Step 0: Stop WiFi activity
            BruceWiFi::getInstance().onDisable();
            m_transitionStep = 2; // Warden makes transition faster, jump to BLE
            m_transitionStartMs = millis();
            m_scanProgress = 46;
            if (Serial) Serial.println("[Trans] Step 0: WiFi disable");
            break;


        case 2:
            // Step 2: Wait 100ms for radio to fully stop
            if (elapsed >= 100) {
                BruceBLE& ble = BruceBLE::getInstance();
                ble.shutdown();
                m_transitionStep = 3;
                m_transitionStartMs = millis();
                m_scanProgress = 48;
                if (Serial) Serial.println("[Trans] Step 2: BLE shutdown");
            }
            break;

        case 3:
            // Step 3: Wait 50ms, then try BLE init
            if (elapsed >= 50) {
                m_transitionStep = 4;
                m_transitionStartMs = millis();
                m_bleInitAttempts = 0;
                if (Serial) Serial.println("[Trans] Step 3: Ready for BLE init");
            }
            break;

        case 4:
            // Step 4: Try BLE init (one attempt per tick)
            {
                BruceBLE& ble = BruceBLE::getInstance();
                m_bleInitAttempts++;

                if (Serial) {
                    Serial.printf("[Trans] Step 4: BLE init attempt %d\n", m_bleInitAttempts);
                }

                bool initOk = ble.init();

                if (initOk) {
                    // Success! Start BLE scan
                    m_transitionStep = 5;
                    m_transitionStartMs = millis();
                    m_scanProgress = 49;
                    if (Serial) Serial.println("[Trans] BLE init SUCCESS");
                } else if (m_bleInitAttempts >= 3) {
                    // Failed after 3 attempts, complete without BLE
                    if (Serial) Serial.println("[Trans] BLE init FAILED, completing without BLE");
                    m_scanState = ScanState::COMPLETE;
                    m_scanProgress = 100;
                    if (m_onScanProgress) {
                        m_onScanProgress(m_scanState, m_scanProgress);
                    }
                } else {
                    // Wait before retry
                    m_transitionStep = 100;  // Wait state
                    m_transitionStartMs = millis();
                }
            }
            break;

        case 5:
            // Step 5: Start BLE scan
            {
                BruceBLE& ble = BruceBLE::getInstance();
                ble.beginScan(3000);

                m_scanState = ScanState::BLE_SCANNING;
                m_scanProgress = 50;
                m_scanStartMs = millis();

                if (Serial) Serial.println("[Trans] Step 5: BLE scan started");

                if (m_onScanProgress) {
                    m_onScanProgress(m_scanState, m_scanProgress);
                }
            }
            break;

        case 100:
            // Wait state: wait 100ms before retrying BLE init
            if (elapsed >= 100) {
                m_transitionStep = 4;
                m_transitionStartMs = millis();
            }
            break;

        default:
            // Shouldn't happen, force complete
            m_scanState = ScanState::COMPLETE;
            m_scanProgress = 100;
            break;
    }

    // Safety timeout for entire transition: 5 seconds
    if (m_scanState == ScanState::TRANSITIONING_TO_BLE) {
        // Check total transition time (from first step)
        // We can't easily track this without another var, so use a generous per-step timeout
        if (elapsed > 2000) {
            if (Serial) Serial.println("[Trans] Step timeout, completing without BLE");
            m_scanState = ScanState::COMPLETE;
            m_scanProgress = 100;
            if (m_onScanProgress) {
                m_onScanProgress(m_scanState, m_scanProgress);
            }
        }
    }
}

void VanguardEngine::processBLEScanResults() {
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

const std::vector<Target>& VanguardEngine::getTargets() const {
    return m_targetTable.getAll();
}

size_t VanguardEngine::getTargetCount() const {
    return m_targetTable.count();
}

std::vector<Target> VanguardEngine::getFilteredTargets(const TargetFilter& filter,
                                                        SortOrder order) const {
    return m_targetTable.getFiltered(filter, order);
}

const Target* VanguardEngine::findTarget(const uint8_t* bssid) const {
    return m_targetTable.findByBssid(bssid);
}

void VanguardEngine::clearTargets() {
    m_targetTable.clear();
}

// =============================================================================
// ACTIONS
// =============================================================================

std::vector<AvailableAction> VanguardEngine::getActionsFor(const Target& target) const {
    return m_actionResolver.getActionsFor(target);
}

bool VanguardEngine::executeAction(ActionType action, const Target& target) {
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

            m_actionProgress.statusText = "Capturing handshake...";
            
            // Create filename based on BSSID
            char filename[64];
            snprintf(filename, sizeof(filename), "/captures/handshake_%02X%02X%02X.pcap",
                     target.bssid[3], target.bssid[4], target.bssid[5]);
            
            // Enable PCAP logging
            wifi.setPcapLogging(true, filename);

            // Start deauth to force a handshake
            bool success = wifi.deauthAll(target.bssid, target.channel);
            if (!success) {
                wifi.setPcapLogging(false);
                m_actionProgress.result = ActionResult::FAILED_HARDWARE;
                m_actionProgress.statusText = "Deauth start failed";
                m_actionActive = false;
                return false;
            }

            if (Serial) {
                Serial.printf("[Attack] Handshake capture started on %s\n", filename);
            }
            return true;
        }

        case ActionType::IR_REPLAY: {
            m_actionProgress.statusText = "Recording IR...";
            BruceIR::getInstance().startRecording();
            return true;
        }

        case ActionType::IR_TVBGONE: {
            m_actionProgress.statusText = "Spamming power...";
            BruceIR::getInstance().sendTVBGone();
            return true;
        }

        default:
            m_actionProgress.result = ActionResult::FAILED_NOT_SUPPORTED;
            m_actionProgress.statusText = "Not implemented";
            m_actionActive = false;
            return false;
    }
}

void VanguardEngine::stopAction() {
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

bool VanguardEngine::isActionActive() const {
    return m_actionActive;
}

ActionProgress VanguardEngine::getActionProgress() const {
    return m_actionProgress;
}

void VanguardEngine::onActionProgress(ActionProgressCallback cb) {
    m_onActionProgress = cb;
}

void VanguardEngine::tickAction() {
    if (!m_actionActive) return;

    BruceWiFi& wifi = BruceWiFi::getInstance();
    wifi.tick();

    // Also tick IR for recording
    BruceIR::getInstance().tick();

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

    // Special handling for handshake capture status
    if (m_actionProgress.type == ActionType::CAPTURE_HANDSHAKE) {
        static char statusBuf[48];
        snprintf(statusBuf, sizeof(statusBuf), "Sniffing... (EAPOL: %u)", (unsigned int)wifi.getEapolCount());
        m_actionProgress.statusText = statusBuf;
    }

    // Report progress
    if (m_onActionProgress) {
        m_onActionProgress(m_actionProgress);
    }
}

// =============================================================================
// HARDWARE STATUS
// =============================================================================

bool VanguardEngine::hasWiFi() const {
    return m_initialized;
}

bool VanguardEngine::hasBLE() const {
    return true;
}

bool VanguardEngine::hasRF() const {
    return false;
}

bool VanguardEngine::hasIR() const {
    return true;
}

} // namespace Vanguard
