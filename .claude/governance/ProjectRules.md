# Project Rules & Standards

## 1. Core Philosophy
- **Reality Adherence:** Do not invent APIs. If a library doesn't support a function, fail loudly or pivot. Do not hallucinate.
- **Hardware Safety:** Never use blocking delays in the main loop (watchdog resets). Manage heat generation on the ESP32-S3.

## 2. Tech Stack
- **Platform:** PlatformIO (Arduino Framework).
- **Hardware:** M5Stack Cardputer (ESP32-S3).
- **Libraries:** NimBLE-Arduino, M5Unified, LGFX.

## 3. Anti-Patterns
- No "God Classes".
- No "Magic Numbers" (Use defined constants).
- No blocking WiFi scans that freeze the UI.
