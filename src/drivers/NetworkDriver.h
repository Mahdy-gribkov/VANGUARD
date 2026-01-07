#ifndef NETWORK_DRIVER_H
#define NETWORK_DRIVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <functional>

namespace Assessor {

// ============================================================================
// CONSTANTS - No Magic Numbers (ProjectRules.md)
// ============================================================================
constexpr uint8_t  MAX_CHANNEL          = 14;
constexpr uint8_t  MIN_CHANNEL          = 1;
constexpr uint32_t SCAN_DWELL_TIME_MS   = 100;   // Per-channel dwell time
constexpr uint32_t SCAN_TIMEOUT_MS      = 15000; // Total scan timeout
constexpr size_t   MAX_TARGETS          = 64;    // Memory-safe cap

// ============================================================================
// ENUMERATIONS
// ============================================================================

enum class TargetType : uint8_t {
    ACCESS_POINT,
    STATION,
    UNKNOWN
};

enum class SecurityType : uint8_t {
    OPEN,
    WEP,
    WPA_PSK,
    WPA2_PSK,
    WPA2_ENTERPRISE,
    WPA3,
    UNKNOWN
};

enum class ScanState : uint8_t {
    IDLE,
    SCANNING,
    COMPLETE,
    ERROR
};

enum class AttackType : uint8_t {
    NONE,
    DEAUTH,
    BEACON_FLOOD,
    PROBE_FLOOD,
    // Future: EVIL_TWIN, PMKID_CAPTURE
};

enum class AttackState : uint8_t {
    IDLE,
    RUNNING,
    STOPPING,
    ERROR
};

enum class DriverError : uint8_t {
    NONE,
    WIFI_INIT_FAILED,
    PROMISCUOUS_FAILED,
    INJECTION_FAILED,
    CHANNEL_SET_FAILED,
    SCAN_TIMEOUT,
    TARGET_NOT_FOUND
};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Target {
    uint8_t      bssid[6];
    char         ssid[33];         // 32 chars + null terminator
    uint8_t      channel;
    int8_t       rssi;
    TargetType   type;
    SecurityType security;
    uint8_t      clientCount;      // For context-aware actions
    uint32_t     lastSeenMs;       // Timestamp for aging

    bool hasClients() const { return clientCount > 0; }
    bool isOpen() const { return security == SecurityType::OPEN; }
};

struct ScanResult {
    ScanState          state;
    DriverError        error;
    uint8_t            currentChannel;
    uint32_t           elapsedMs;
};

struct AttackResult {
    AttackState        state;
    DriverError        error;
    uint32_t           packetsSent;
    uint32_t           elapsedMs;
};

// ============================================================================
// CALLBACK TYPES
// ============================================================================

using TargetFoundCallback = std::function<void(const Target&)>;
using ScanCompleteCallback = std::function<void(const ScanResult&)>;
using AttackUpdateCallback = std::function<void(const AttackResult&)>;

// ============================================================================
// NetworkDriver CLASS
// ============================================================================

class NetworkDriver {
public:
    // Singleton - ESP32 has ONE WiFi radio
    static NetworkDriver& getInstance();

    // Prevent copying (hardware resource)
    NetworkDriver(const NetworkDriver&) = delete;
    NetworkDriver& operator=(const NetworkDriver&) = delete;

    // -------------------------------------------------------------------------
    // LIFECYCLE
    // -------------------------------------------------------------------------
    bool init();
    void deinit();
    bool isInitialized() const;

    // -------------------------------------------------------------------------
    // SCANNING (Non-Blocking per ProjectRules.md)
    // -------------------------------------------------------------------------
    bool beginScan(uint8_t startChannel = MIN_CHANNEL,
                   uint8_t endChannel = MAX_CHANNEL);
    void stopScan();
    ScanResult getScanState() const;

    // Target access
    const std::vector<Target>& getTargets() const;
    const Target* findTarget(const uint8_t* bssid) const;
    void clearTargets();

    // -------------------------------------------------------------------------
    // ATTACKS (Non-Blocking State Machine)
    // -------------------------------------------------------------------------
    bool beginAttack(AttackType type, const Target& target);
    void stopAttack();
    AttackResult getAttackState() const;

    // -------------------------------------------------------------------------
    // LOW-LEVEL CONTROL
    // -------------------------------------------------------------------------
    bool setChannel(uint8_t channel);
    uint8_t getChannel() const;
    bool enablePromiscuous(bool enable);
    bool isPromiscuousEnabled() const;

    // -------------------------------------------------------------------------
    // CALLBACKS (Event-Driven UI Updates)
    // -------------------------------------------------------------------------
    void onTargetFound(TargetFoundCallback cb);
    void onScanComplete(ScanCompleteCallback cb);
    void onAttackUpdate(AttackUpdateCallback cb);

    // -------------------------------------------------------------------------
    // TICK - Must be called from loop() (No Blocking!)
    // -------------------------------------------------------------------------
    void tick();

    // -------------------------------------------------------------------------
    // ERROR HANDLING
    // -------------------------------------------------------------------------
    DriverError getLastError() const;
    const char* getErrorString(DriverError err) const;

private:
    NetworkDriver();
    ~NetworkDriver();

    // Internal state
    bool              m_initialized;
    bool              m_promiscuousEnabled;
    uint8_t           m_currentChannel;
    ScanState         m_scanState;
    AttackState       m_attackState;
    AttackType        m_activeAttackType;
    DriverError       m_lastError;

    // Timing (non-blocking)
    uint32_t          m_scanStartMs;
    uint32_t          m_attackStartMs;
    uint32_t          m_lastChannelHopMs;
    uint8_t           m_scanStartChannel;
    uint8_t           m_scanEndChannel;

    // Target storage
    std::vector<Target> m_targets;
    Target              m_attackTarget;
    uint32_t            m_packetsSent;

    // Callbacks
    TargetFoundCallback   m_onTargetFound;
    ScanCompleteCallback  m_onScanComplete;
    AttackUpdateCallback  m_onAttackUpdate;

    // Internal methods
    void tickScan();
    void tickAttack();
    void processPacket(const uint8_t* payload, uint16_t len, int8_t rssi);
    bool injectPacket(const uint8_t* packet, size_t len);
    void parseBeaconFrame(const uint8_t* payload, uint16_t len, int8_t rssi);
    void parseProbeResponse(const uint8_t* payload, uint16_t len, int8_t rssi);
    void parseDataFrame(const uint8_t* payload, uint16_t len);
    Target* findOrCreateTarget(const uint8_t* bssid);
    SecurityType parseSecurityFromCapabilities(uint16_t caps, const uint8_t* ie, uint16_t ieLen);

    // Promiscuous mode callback (static for C API)
    static void promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type);
};

} // namespace Assessor

#endif // NETWORK_DRIVER_H
