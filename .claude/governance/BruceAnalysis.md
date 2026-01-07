# Bruce Source Code Analysis
> **Analyzed:** Jan 7, 2026 | **Bruce Version:** Latest main branch

## 1. Repository Structure

```
Bruce/
├── src/
│   ├── main.cpp                    ← Entry point
│   ├── core/                       ← Core functionality
│   │   ├── wifi/                   ← WiFi connection management
│   │   ├── display.cpp/h           ← Screen rendering
│   │   ├── mykeyboard.cpp/h        ← Input handling
│   │   ├── config.cpp/h            ← Configuration
│   │   ├── main_menu.cpp/h         ← Menu system
│   │   └── theme.cpp/h             ← Visual theming
│   └── modules/                    ← Attack modules
│       ├── wifi/                   ← WiFi attacks (19 files)
│       ├── ble/                    ← BLE attacks (6 files)
│       ├── rf/                     ← Sub-GHz (if equipped)
│       ├── ir/                     ← Infrared
│       └── rfid/                   ← RFID operations
└── lib/                            ← External libraries
```

---

## 2. WiFi Attack Functions

### 2.1 Deauthentication

**File:** `src/modules/wifi/deauther.h`

```cpp
void stationDeauth(Host host);
```

- Takes a `Host` struct (IP + MAC)
- Sends deauth frames to disconnect station from AP
- **Integration:** We call this after user selects a client

### 2.2 Attack Orchestration

**File:** `src/modules/wifi/wifi_atks.h`

```cpp
// Raw frame transmission
void wsl_bypasser_send_raw_frame();  // Uses esp_wifi_80211_tx
void send_raw_frame(uint8_t* buf, size_t size);

// Attack menu (we replace this with our UI)
void wifi_atk_menu();
void target_atk_menu();
void target_atk();

// Specific attacks
void capture_handshake();       // WPA handshake capture
void beaconAttack();            // Beacon frame spam
void deauthFloodAttack();       // Mass deauth
```

**Default Deauth Frame (26 bytes):**
```cpp
uint8_t deauth_frame[26] = {
    0xC0, 0x00,                         // Frame control (deauth)
    0x00, 0x00,                         // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination (broadcast)
    // ... sender MAC, BSSID, sequence, reason code
};
```

### 2.3 Packet Sniffing

**File:** `src/modules/wifi/sniffer.h`

```cpp
// Setup and callback
void sniffer_setup();
void sniffer(void *buf, wifi_promiscuous_pkt_type_t type);

// Mode control
enum SnifferMode { Full, HandshakesOnly, DeauthOnly };
void sniffer_set_mode(SnifferMode mode);
SnifferMode sniffer_get_mode();

// Storage
bool sniffer_prepare_storage(FS *fs, bool sdDetected);
void newPacketSD(uint32_t ts_sec, uint32_t ts_usec,
                 uint32_t len, uint8_t *buf, File pcap_file);

// Handshake tracking
void setHandshakeSniffer();
void sniffer_reset_handshake_cache();
void markHandshakeReady(uint64_t key);
```

### 2.4 Evil Portal

**File:** `src/modules/wifi/evil_portal.h`

```cpp
// Class-based implementation
class EvilPortal {
public:
    bool setup();
    void beginAP();
    void setupRoutes();

    // Credential capture
    void credsController(AsyncWebServerRequest *request);
    bool verifyCreds(String &Ssid, String &Password);
    void saveToCSV(const String &csvLine, bool IsAPname = false);
    void resetCapturedCredentials();

    // Deauth integration
    void printDeauthStatus();
};
```

### 2.5 AP Scanning

**File:** `src/core/wifi/wifi_common.h`

```cpp
void wifiConnectMenu();           // Interactive scan + connect
void wifiConnecttoKnownNet();     // Auto-connect to saved
void wifiDisconnect();            // Clean shutdown
String checkMAC();                // Get device MAC
```

**Note:** Bruce uses Arduino WiFi.scanNetworks() for AP discovery.

---

## 3. BLE Attack Functions

### 3.1 BLE Spam

**File:** `src/modules/ble/ble_spam.h`

```cpp
// Advertising attacks
void aj_adv(int ble_choice);  // Choice determines spam type

// iBeacon spoofing
void ibeacon(
    String name = "Bruce iBeacon",
    String uuid = "8ec76ea3-6668-48da-9866-75be8bc86f4d",
    uint16_t manufacturer = 0x4C00  // Apple
);
```

**Dependencies:** NimBLE-Arduino library
- NimBLEBeacon.h
- NimBLEDevice.h
- NimBLEServer.h

---

## 4. Data Structures

### 4.1 Host (Station/Client)

**File:** `src/modules/wifi/scan_hosts.h`

```cpp
struct Host {
    Host(ip4_addr_t *ip, eth_addr *eth)
        : ip(ip->addr), mac(MAC(eth->addr)) {}

    IPAddress ip;
    String mac;
};
```

**Issue:** This is minimal - no RSSI, no SSID association. We need to extend for our TargetTable.

### 4.2 Access Point

Bruce doesn't have a dedicated AP struct - it uses Arduino's WiFi.scanNetworks() which returns:
- `WiFi.SSID(i)` - Network name
- `WiFi.BSSID(i)` - MAC address
- `WiFi.RSSI(i)` - Signal strength
- `WiFi.encryptionType(i)` - Security type
- `WiFi.channel(i)` - Channel number

---

## 5. Integration Strategy

### 5.1 What We Reuse (Layer 1 Adapters)

| Bruce Function | Our Wrapper | Purpose |
|----------------|-------------|---------|
| `stationDeauth(Host)` | `BruceWiFi::deauthStation()` | Single client deauth |
| `deauthFloodAttack()` | `BruceWiFi::deauthAll()` | Mass deauth |
| `beaconAttack()` | `BruceWiFi::beaconFlood()` | Fake AP spam |
| `capture_handshake()` | `BruceWiFi::captureHandshake()` | WPA handshake |
| `EvilPortal` class | `BruceWiFi::startEvilTwin()` | Credential capture |
| `sniffer_setup()` | `BruceWiFi::startMonitor()` | Passive capture |
| `aj_adv(choice)` | `BruceBLE::spam()` | BLE spam attacks |
| `ibeacon()` | `BruceBLE::spoofBeacon()` | iBeacon spoofing |

### 5.2 What We Build (Layer 2 Engine)

1. **Scanning Wrapper**
   - Bruce's scan is synchronous (blocks UI)
   - We wrap with async: `WiFi.scanNetworks(true)`
   - Poll `WiFi.scanComplete()` in tick()

2. **Target State Table**
   - Richer than Bruce's Host struct
   - Tracks: RSSI, clients, security, last seen, handshake status
   - Auto-prunes stale targets

3. **Action Resolver**
   - Bruce shows all options always
   - We filter by target state
   - No "deauth" if clientCount == 0

### 5.3 What We Replace (Layer 3 UI)

- Bruce's `wifi_atk_menu()` → Our `TargetRadar`
- Bruce's `target_atk_menu()` → Our `TargetDetail`
- Bruce's menu hierarchy → Our flat target-first flow

---

## 6. Technical Challenges

### 6.1 Async Conversion

**Problem:** Bruce's attacks are often blocking:
```cpp
void deauthFloodAttack() {
    while (true) {
        send_deauth_frame();
        delay(10);  // BLOCKS!
    }
}
```

**Solution:** Convert to state machine:
```cpp
class DeauthAttack {
    enum State { IDLE, RUNNING, STOPPING };
    State state;
    uint32_t lastSendMs;

    void tick() {
        if (state == RUNNING && millis() - lastSendMs > 10) {
            send_deauth_frame();
            lastSendMs = millis();
        }
    }
};
```

### 6.2 Client Discovery

**Problem:** Bruce doesn't automatically track AP clients. The `Host` struct is for ARP-discovered hosts, not WiFi associations.

**Solution:** We need promiscuous mode to see data frames and extract client MACs:
```cpp
// In sniffer callback:
if (frame_type == DATA_FRAME) {
    mac_t client = extract_source_mac(frame);
    mac_t ap = extract_bssid(frame);
    targetTable.associateClient(client, ap);
}
```

### 6.3 Library Conflicts

**Potential Issue:** Bruce uses specific library versions. Our platformio.ini must match:
```ini
lib_deps =
    m5stack/M5Unified @ ^0.1.16
    h2zero/NimBLE-Arduino @ ^1.4.2
    # May need Bruce's exact versions
```

---

## 7. Files to Cannibalize

Priority order for copying into our project:

1. **`modules/wifi/wifi_atks.cpp`** - Raw frame sending
2. **`modules/wifi/deauther.cpp`** - Deauth implementation
3. **`modules/wifi/sniffer.cpp`** - Promiscuous mode setup
4. **`modules/wifi/evil_portal.cpp`** - Portal with credential capture
5. **`modules/ble/ble_spam.cpp`** - BLE attack implementations

---

## 8. Next Steps

1. [ ] Clone Bruce repo locally for reference
2. [ ] Create `src/adapters/BruceWiFi.h` interface
3. [ ] Copy and adapt `wifi_atks.cpp` raw frame logic
4. [ ] Implement async scanning wrapper
5. [ ] Build client tracking via promiscuous mode
6. [ ] Test deauth on real hardware

---

## 9. License Compliance

Bruce is **AGPL-3.0**. Our project must:
- Maintain AGPL-3.0 license (compatible with GPL-3.0)
- Credit Bruce contributors
- Publish all modifications
- Include license notices in copied code
