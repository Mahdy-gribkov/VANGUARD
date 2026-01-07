/**
 * @file AssessorEngine.cpp
 * @brief The orchestrator - WiFi scanning with proper timing
 */

#include "AssessorEngine.h"
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
    , m_onScanProgress(nullptr)
    , m_onActionProgress(nullptr)
    , m_scanStartMs(0)
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
    
    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    m_initialized = true;
    return true;
}

void AssessorEngine::shutdown() {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    m_initialized = false;
}

void AssessorEngine::tick() {
    // Check if async scan completed
    if (m_scanState == ScanState::WIFI_SCANNING) {
        int result = WiFi.scanComplete();
        
        if (result >= 0) {
            // Scan done - process results
            processScanResults(result);
        } 
        else if (result == WIFI_SCAN_FAILED) {
            // Scan failed - mark complete with 0 results
            m_scanState = ScanState::COMPLETE;
            m_scanProgress = 100;
        }
        else {
            // Still scanning - update progress (scan takes ~3-5 seconds)
            uint32_t elapsed = millis() - m_scanStartMs;
            m_scanProgress = min(95, (int)(elapsed / 50));  // Max 95% until done
        }
    }
}

// =============================================================================
// SCANNING
// =============================================================================

void AssessorEngine::beginScan() {
    if (!m_initialized) {
        init();
    }
    
    m_targetTable.clear();
    m_scanState = ScanState::WIFI_SCANNING;
    m_scanProgress = 0;
    m_scanStartMs = millis();
    
    // Start async scan
    // Parameters: async=true, show_hidden=true, passive=false, max_ms=5000
    WiFi.scanNetworks(true, true, false, 300);  // 300ms per channel
    
    if (m_onScanProgress) {
        m_onScanProgress(m_scanState, m_scanProgress);
    }
}

void AssessorEngine::beginWiFiScan() {
    beginScan();
}

void AssessorEngine::beginBLEScan() {
    // Not implemented yet
    m_scanState = ScanState::COMPLETE;
}

void AssessorEngine::stopScan() {
    WiFi.scanDelete();
    m_scanState = ScanState::IDLE;
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
// ACTIONS (Stubs for now)
// =============================================================================

std::vector<AvailableAction> AssessorEngine::getActionsFor(const Target& target) const {
    return m_actionResolver.getActionsFor(target);
}

bool AssessorEngine::executeAction(ActionType action, const Target& target) {
    // TODO: Re-enable attacks once UI is solid
    m_actionProgress.result = ActionResult::FAILED_NOT_SUPPORTED;
    m_actionProgress.statusText = "Coming soon...";
    return false;
}

void AssessorEngine::stopAction() {
    m_actionActive = false;
    m_actionProgress.result = ActionResult::CANCELLED;
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
    // No-op for now
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
