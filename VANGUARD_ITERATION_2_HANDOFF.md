# PROJECT ASSIGNMENT: VANGUARD (Iteration 2)

**CONTEXT:** 
You are continuing work on **VANGUARD**, a Target-First Auditing tool for the M5Stack Cardputer. The project is a philosophical and technical fork of "Bruce," focusing on a "Radar-First" UX. Iteration 1 successfully rebranded the project, fixed basic UI logic, and established a new repository.

**CURRENT STATE:**
*   **Version:** Alpha 1.1
*   **Repository:** `https://github.com/Mahdy-gribkov/VANGUARD`
*   **Successes:** 
    - Full namespace/class rebranding completed (`VanguardEngine`, `VanguardTypes`).
    - Input handling centralized in `main.cpp` (Q/Enter/Navigation fixed).
    - Branding pervasive (VANGUARD logo and headers).
    - Basic SD logging and PCAP infrastructure ported.

**BLOCKERS (Priority 1 FIXES):**
1.  **WiFi "Radio Silence":** The `VanguardEngine::beginScan()` starts correctly, but `WiFi.scanComplete()` returns 0 networks even in dense environments. Requires investigation of radio state, passive vs active scanning, and ESP32 power management logic.
2.  **BLE Crash (45%):** Bluetooth scanning starts but causes a device crash/restart around the 45% progress mark (transition/init phase). Likely NimBLE memory pressure or Watchdog Timer (WDT) reset during heavy `tick()` operations.
3.  **Combined Scan Logic:** Pressing "OK" (Combined Scan) currently behaves identically to WiFi-only due to state machine logic. Needs a truly threaded or synchronized chain.

**STRICT GIT POLICY (MANDATORY):**
- **Commit Early, Push Often:** You MUST perform a `git push origin main` after every significant logic change or fix. Never end a session with unpushed local changes.
- **Atomic Commits:** Use clear, descriptive commit messages prefixed with `[VANGUARD]` or `[CORE]`.

**ITERATION 2 OBJECTIVES:**
1.  **M5BURNER COMPLIANCE:** Prepare the codebase for official submission to the M5Burner app store. This includes:
    - Metadata optimization (icons, screenshots, descriptions).
    - Partition table verification (`large_spiffs` or `minimal_ota` to accommodate the large binary).
    - Stability across all Core functions.
2.  **CI/CD PIPELINE:** Implement GitHub Actions to automatically build and export `firmware.bin` and combined flashing artifacts on every push.
3.  **GAMIFIED README:** Overhaul `README.md` to be a "Player Guide" for a high-tech espionage tool. Use ANSI art, clear "Mission Briefings" (installation), and "Utility Manuals" (features).
4.  **DYNAMIC SOUND & HAPTICS (NEW):** 
    - Implement a "Geiger Counter" audio feedback style for WiFi signal strength during Radar view.
    - Use the Cardputer's haptic motor (if available/supported) or buzzer to indicate successful handshakes or BLE detections.
5.  **INTERACTIVE IR TRAINING (NEW):**
    - Create a "Learn" mode within the IR adapter.
    - Save captured IR codes as user-named JSON profiles on the SD card for later playback/persistence.
6.  **UX EVOLUTION:** Move from "Functional UI" to "Elite Gamified UI." Think cyberpunk aesthetics, micro-animations, and RPG-like progression in scanning feedback.
7.  **COMPLIANCE & LEGAL:** Ensure all attribution to original Bruce/Evil-M5 authors is perfectly preserved while solidifying the VANGUARD license (GPLv3).

**GUIDING PHILOSOPHY:**
VANGUARD should be "Fun and easy to use for anyone in the future." The UX should feel like a game, but the functionality must be rock-solid and professional.

**TECHNICAL DEBT:**
- Refactor `VanguardEngine::tickTransition()` for more robust state handling.
- Investigate `BruceWiFi` promiscuous mode hooks (might be interferring with normal scanning).
- Centralize all Theme constants for "Night/Coral" palette.

---
*Proceed by reading `implementation_plan.md` and `task.md` from the artifacts directory to understand the deep history.*
