# Project Master Plan
> **Status:** ACTIVE | **Phase:** 1 (Foundation)

## 1. Project Goal
Create a "Target-First" auditing tool. The device scans the environment, builds a "State Table" of targets, and purely presents valid, context-aware actions (e.g., "Deauth" only appears if clients are connected).

## 2. Architecture
- **Layer 1 (Drivers):** Raw C++ wrappers for WiFi/BLE (Cannibalized from Bruce/Marauder).
- **Layer 2 (The Brain):** An "Assessor Engine" that evaluates the Vulnerability State of a target.
- **Layer 3 (UI):** Object-Oriented Menu System (M5Unified) that renders the Assessor's options.

## 3. The Migration Strategy
- [ ] Phase 1: Build `NetworkDriver` (Promiscuous mode, Packet Injection, Scanning).
- [ ] Phase 2: Build `AssessorEngine` (Logic Core, Threat Scoring).
- [ ] Phase 3: Build `TargetUI` (Visual Radar & Context Menu).
