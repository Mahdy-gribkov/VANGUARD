# Progress Tracker
> **Last Updated:** Jan 9, 2026

## 1. Context Summary

**Phase 2 In Progress.** Critical bug fixes applied, Evil Portal implemented, firmware uploaded to Cardputer for testing.

---

## 2. Phase 0: Architecture (COMPLETE)

- [x] Initial governance files created
- [x] Misunderstanding corrected (we wrap Bruce, not replace it)
- [x] MasterPlan.md rewritten with correct architecture
- [x] ProjectRules.md expanded with coding standards
- [x] Boot sequence UX designed
- [x] File structure defined
- [x] **PlatformIO project created**
- [x] **All core class headers stubbed:**
  - Types.h (enums, structs, constants)
  - TargetTable.h (state management)
  - ActionResolver.h (context-aware filtering)
  - AssessorEngine.h (orchestrator)
- [x] **All UI class headers stubbed:**
  - Theme.h (visual constants)
  - BootSequence.h (splash/onboarding)
  - TargetRadar.h (main list view)
  - TargetDetail.h (single target + actions)
- [x] **README.md created** (viral-ready)
- [x] **PHILOSOPHY.md created** (target-first manifesto)
- [x] **main.cpp entry point created**

---

## 3. Phase 1: Bruce Integration (COMPLETE)

- [x] Analyze Bruce repository structure
- [x] Map Bruce's attack function signatures
- [x] Document findings in BruceAnalysis.md
- [x] Create BruceWiFi.h adapter interface
- [x] Create BruceBLE.h adapter interface
- [x] Extract raw frame sending logic (minimal, no bloat)
- [x] Implement BruceWiFi.cpp (deauth, beacon, handshake)
- [x] Implement BruceBLE.cpp (spam, sour apple, beacon spoofing)
- [x] Implement TargetTable.cpp
- [x] Implement ActionResolver.cpp
- [x] Implement AssessorEngine.cpp
- [x] Implement BootSequence.cpp
- [x] Implement TargetRadar.cpp
- [x] Implement TargetDetail.cpp
- [x] Implement MainMenu.cpp
- [x] Implement SettingsPanel.cpp
- [x] Implement ScanSelector.cpp

---

## 4. Phase 2: Bug Fixes & Features (IN PROGRESS)

### Critical Bug Fixes (Jan 9, 2026)
- [x] **Fixed blocking WiFi scan** - Changed from synchronous to async scanning
  - Root cause: `WiFi.scanNetworks(false,...)` blocked for 3-5 seconds causing watchdog reset
  - Fix: Changed to `WiFi.scanNetworks(true,...)` with completion checking in `tick()`
- [x] **Fixed WiFi→BLE transition** - Added proper radio shutdown between WiFi and BLE
- [x] **Removed excessive yield() spam** - Cleaned up overzealous watchdog feeding
- [x] **Fixed Evil Twin mode switch** - Reduced blocking delays

### New Features (Jan 9, 2026)
- [x] **Implemented Evil Portal** - Full captive portal with credential capture
  - EvilPortal.h/cpp with DNS spoofing and web server
  - Generic WiFi login template
  - Google-style login template
  - Credential storage and client tracking
  - Live status display (client count, captured credentials)

### Hardware Testing
- [x] Firmware uploaded to Cardputer (Jan 9, 2026)
- [ ] Verify WiFi scanning works without crash
- [ ] Verify BLE scanning works after WiFi
- [ ] Test deauth attack
- [ ] Test beacon flood
- [ ] Test Evil Portal credential capture
- [ ] Test BLE spam

### Remaining Tasks
- [ ] Add PCAP/handshake capture (EAPOL detection)
- [ ] Add more Bruce attacks (probe flood, etc.)
- [ ] README with GIFs/screenshots
- [ ] GitHub Actions CI

---

## 5. Blockers & Decisions

| Date | Issue | Resolution |
|------|-------|------------|
| Jan 7 | Initial arch was raw driver build | Pivoted to Bruce wrapper model |
| Jan 9 | Sync WiFi scan caused watchdog reset | Changed to async scanning |
| Jan 9 | yield() spammed everywhere | Cleaned up, only use delay() where needed |

---

## 6. Session Notes

### Session 1 (Jan 7, 2026)
- User clarified: has Cardputer ADV, loads Bruce from SD
- Goal: target-first UI wrapper, viral-quality repo
- Full creative control granted
- Created full project skeleton with PlatformIO
- Stubbed all core and UI classes
- Created README.md and PHILOSOPHY.md
- Analyzed Bruce source code structure
- Created adapter interfaces

### Session 2 (Jan 9, 2026)
- Previous session left project in broken state ("borken. mid plan.")
- User reported: device crashes, WiFi doesn't scan, BLE doesn't work, display freezes
- Root cause identified: WiFi scan was changed to blocking/synchronous in last commit
- Fixed async scanning, WiFi→BLE transition, removed yield spam
- Implemented full Evil Portal with captive portal and credential capture
- Added ESPAsyncWebServer, AsyncTCP, IRremote libraries
- Firmware uploaded to Cardputer for testing
