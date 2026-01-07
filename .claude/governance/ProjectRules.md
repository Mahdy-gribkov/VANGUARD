# Project Rules & Standards

## 1. Core Philosophy

### Reality Adherence
- **No hallucinated APIs.** If Bruce doesn't expose a function, we don't pretend it does.
- **Verify before integrating.** Read Bruce's actual source code before writing adapters.
- **Fail loudly.** If an attack can't run, show a clear error - never silently fail.

### Hardware Respect
- **ESP32-S3 Cardputer constraints:**
  - 240MHz dual-core, 512KB SRAM, 8MB PSRAM
  - 240x135 TFT display (small but crisp)
  - No blocking delays in `loop()` (watchdog bites at ~3s)
  - Heat management: sustained RF operations get hot

### User Experience First
- **Target-first, always.** Every screen should answer: "What's around me?"
- **Context-aware actions.** Never show an action that can't work.
- **Immediate feedback.** User should never wonder "did it work?"

---

## 2. Tech Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Platform | PlatformIO | Industry standard for ESP32 |
| Framework | Arduino | Bruce uses it, we stay compatible |
| Hardware | M5Stack Cardputer | Your device |
| Display | M5GFX/LGFX | Hardware-accelerated, sprite support |
| WiFi/BLE | ESP-IDF via Arduino | Low-level access for attacks |
| UI Pattern | State Machine | Non-blocking, testable |
| Attacks | Bruce (cannibalised) | Proven, maintained, legal grey-area covered |

---

## 3. Code Standards

### Naming Conventions
```cpp
// Classes: PascalCase
class TargetRadar { };

// Methods: camelCase
void renderTargetList();

// Constants: SCREAMING_SNAKE
constexpr int MAX_TARGETS = 64;

// Member variables: m_ prefix
int m_selectedIndex;

// Local variables: camelCase
int currentChannel = 6;
```

### File Organization
```cpp
// Header files: declaration only
// .cpp files: implementation only
// No inline implementations in headers (except templates)
```

### Memory Management
```cpp
// Prefer stack allocation
Target targets[MAX_TARGETS];  // Good

// Use std::vector only when size truly varies
std::vector<Target> targets;  // Only if MAX unknown

// NEVER use raw new/delete
Target* t = new Target();  // FORBIDDEN
```

---

## 4. Anti-Patterns (Hard Bans)

### God Classes
```cpp
// BAD: One class does everything
class Assessor {
    void scan();
    void deauth();
    void renderUI();
    void handleInput();
    void saveConfig();
};

// GOOD: Single responsibility
class Scanner { void scan(); };
class Attacker { void deauth(); };
class Renderer { void draw(); };
```

### Magic Numbers
```cpp
// BAD
delay(100);
if (rssi > -50) { }

// GOOD
constexpr uint32_t SCAN_DWELL_MS = 100;
constexpr int8_t STRONG_SIGNAL_THRESHOLD = -50;

delay(SCAN_DWELL_MS);
if (rssi > STRONG_SIGNAL_THRESHOLD) { }
```

### Blocking Operations
```cpp
// BAD: Freezes UI
WiFi.scanNetworks();  // Blocks for seconds!

// GOOD: Async with callback
WiFi.scanNetworks(true);  // Async mode
// Check WiFi.scanComplete() in loop()
```

### Hardcoded Strings in UI
```cpp
// BAD
display.print("Deauth Attack");

// GOOD
constexpr char STR_DEAUTH[] = "Deauth Attack";
display.print(STR_DEAUTH);
// Or use a Strings.h file for i18n readiness
```

---

## 5. Git Workflow

### Branch Naming
```
feature/target-radar
fix/deauth-crash
refactor/event-bus
docs/installation-guide
```

### Commit Messages
```
feat: add target radar view with signal strength sorting
fix: prevent crash when scanning with no WiFi chip
refactor: extract action resolution to dedicated class
docs: add architecture diagram to README
```

### Pull Request Requirements
- [ ] Code compiles without warnings
- [ ] No new anti-patterns introduced
- [ ] UI changes include screenshot
- [ ] Attack changes tested on real hardware

---

## 6. Security & Ethics

### Legal Boundaries
- This tool is for **authorized security testing only**
- All attacks require explicit permission from network owner
- We include disclaimers in README and boot sequence
- We do NOT add features designed purely for malicious use

### Responsible Disclosure
- If we find vulnerabilities in Bruce, we report upstream
- We credit all borrowed code appropriately
- We maintain GPL-3.0 license compatibility

---

## 7. Documentation Requirements

### Every Public Class Needs:
```cpp
/**
 * @brief One-line description
 *
 * Longer description if complex.
 *
 * @example
 * TargetRadar radar;
 * radar.render(display);
 */
class TargetRadar { };
```

### README Must Include:
- [ ] Hero image/GIF
- [ ] One-paragraph description
- [ ] Installation steps
- [ ] Usage example
- [ ] Architecture overview
- [ ] Contributing guide link
- [ ] License
