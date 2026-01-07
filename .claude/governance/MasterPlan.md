# The Assessor - Master Plan
> **Status:** ACTIVE | **Phase:** 0 (Architecture Design)

## 1. Vision Statement

**The Assessor** is a philosophical fork of [Bruce](https://github.com/pr3y/Bruce) that inverts the user experience.

### The Problem with Traditional Pentest Tools
```
Traditional Flow:
[Menu] → [Category: WiFi] → [Attack: Deauth] → [Scan] → [Pick Target] → [Execute]

User thinks: "I want to do a deauth" → then picks what to hit
```

### The Assessor Philosophy
```
Assessor Flow:
[Boot] → [Auto-Scan] → [Target Radar] → [Tap Target] → [See Valid Actions] → [Execute]

User thinks: "What's around me?" → "That looks interesting" → "What can I do to it?"
```

**Core Insight:** The TARGET is the noun. The ATTACK is the verb. You pick the noun first.

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      THE ASSESSOR                                │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │   ASSESSOR UI   │  │ ASSESSOR ENGINE │  │  BRUCE ENGINE   │  │
│  │                 │  │                 │  │                 │  │
│  │ • Boot Sequence │  │ • Target State  │  │ • WiFi Attacks  │  │
│  │ • Target Radar  │◄─┤ • Vuln Scoring  │◄─┤ • BLE Attacks   │  │
│  │ • Context Menus │  │ • Action Filter │  │ • RF Attacks    │  │
│  │ • Animations    │  │ • Event Bus     │  │ • IR Attacks    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
│         Layer 3              Layer 2             Layer 1        │
│        (We Build)          (We Build)         (From Bruce)      │
└─────────────────────────────────────────────────────────────────┘
```

### Layer 1: Bruce Engine (Cannibalised)
- We pull Bruce's attack modules as-is
- WiFi: Deauth, Beacon, Evil Twin, PMKID, Handshake Capture
- BLE: Spam, Skimmer Detect, Sour Apple
- RF: Sub-GHz replay, jamming (if hardware supports)
- IR: TV-B-Gone, custom signals
- **We do NOT rewrite these. We call them.**

### Layer 2: Assessor Engine (Our Logic)
- Maintains a "State Table" of all discovered targets
- Each target has: type, security, clients, signal strength, capabilities
- When user taps a target, Engine returns: "Here's what you CAN do"
- Example: Deauth only offered if target has connected clients

### Layer 3: Assessor UI (Our Interface)
- Boot animation with project branding
- Quick onboarding (skippable after first run)
- Live "Target Radar" - visual representation of RF environment
- Tap-to-select target interaction
- Context-aware action menu
- Attack progress/result feedback

---

## 3. Boot Sequence Design

```
┌────────────────────────────────────────┐
│                                        │
│           ◉ THE ASSESSOR               │  ← Logo fade-in (0.5s)
│                                        │
│     "Know your target first."          │  ← Tagline (0.5s)
│                                        │
└────────────────────────────────────────┘
                    ↓ (1s)
┌────────────────────────────────────────┐
│  ┌──────────────────────────────────┐  │
│  │  THE ASSESSOR                    │  │
│  │                                  │  │
│  │  A target-first auditing tool.   │  │
│  │  Scan. Select. Strike.           │  │
│  │                                  │  │
│  │  [Skip] [Learn More]             │  │  ← First-run only
│  └──────────────────────────────────┘  │
└────────────────────────────────────────┘
                    ↓ (auto or tap)
┌────────────────────────────────────────┐
│  SCANNING...              ◌◌◌◌◌◌◌◌    │  ← Auto-scan begins
│  ════════════════════════════════════  │
│                                        │
│  Found: 3 APs, 7 Stations, 2 BLE      │
│                                        │
└────────────────────────────────────────┘
                    ↓
┌────────────────────────────────────────┐
│  TARGET RADAR                  [MENU]  │
│  ════════════════════════════════════  │
│                                        │
│     ●───── NETGEAR-5G (-42dB)         │  ← Strongest first
│     ◐───── iPhone-Sarah (-58dB)       │  ← Client indicator
│     ○───── Hidden Network (-71dB)     │
│     ◦───── Weak_Signal (-89dB)        │  ← Barely visible
│                                        │
│  ▼ 8 more targets                      │
└────────────────────────────────────────┘
                    ↓ (tap target)
┌────────────────────────────────────────┐
│  ◀ NETGEAR-5G                          │
│  ════════════════════════════════════  │
│  BSSID: AA:BB:CC:DD:EE:FF              │
│  Security: WPA2-PSK    Channel: 6      │
│  Signal: -42dB (Excellent)             │
│  Clients: 3 connected                  │
│  ──────────────────────────────────────│
│  AVAILABLE ACTIONS:                    │
│                                        │
│  [■ Deauth All]     [■ Deauth One]    │  ← Only if clients
│  [□ Clone Beacon]   [□ Evil Twin]     │
│  [□ Capture PMKID]  [□ Monitor]       │
│                                        │
└────────────────────────────────────────┘
```

---

## 4. File Structure (Viral-Ready Repo)

```
The-Assessor/
├── README.md                    ← Hero README with GIFs
├── LICENSE                      ← GPL-3.0 (matching Bruce)
├── PHILOSOPHY.md                ← Why target-first matters
├── CONTRIBUTING.md              ← How to contribute
├── docs/
│   ├── ARCHITECTURE.md          ← Technical deep-dive
│   ├── INSTALLATION.md          ← Step-by-step flash guide
│   ├── SUPPORTED_HARDWARE.md    ← Cardputer, variants, etc.
│   └── images/
│       ├── boot-sequence.gif
│       ├── target-radar.gif
│       └── architecture.png
├── src/
│   ├── main.cpp                 ← Entry point
│   ├── core/
│   │   ├── AssessorEngine.h/cpp ← The brain
│   │   ├── TargetTable.h/cpp    ← State management
│   │   └── ActionResolver.h/cpp ← "What can I do?"
│   ├── ui/
│   │   ├── BootSequence.h/cpp   ← Splash & onboarding
│   │   ├── TargetRadar.h/cpp    ← Main target view
│   │   ├── TargetDetail.h/cpp   ← Single-target view
│   │   ├── ActionMenu.h/cpp     ← Context actions
│   │   └── Theme.h              ← Colors, fonts, spacing
│   ├── adapters/
│   │   ├── BruceWiFi.h/cpp      ← Wraps Bruce WiFi attacks
│   │   ├── BruceBLE.h/cpp       ← Wraps Bruce BLE attacks
│   │   └── BruceRF.h/cpp        ← Wraps Bruce RF attacks
│   └── utils/
│       ├── EventBus.h/cpp       ← Pub/sub for loose coupling
│       └── Config.h/cpp         ← User preferences
├── assets/
│   ├── fonts/                   ← Custom fonts
│   └── sprites/                 ← Icons, animations
├── platformio.ini               ← Build config
└── .github/
    └── workflows/
        └── build.yml            ← CI for releases
```

---

## 5. Development Phases

### Phase 0: Architecture (CURRENT)
- [x] Define philosophy and UX flow
- [x] Design file structure
- [ ] Create skeleton project
- [ ] Stub all classes with interfaces

### Phase 1: Bruce Integration
- [ ] Fork/clone Bruce source
- [ ] Identify attack function signatures
- [ ] Build adapter layer (BruceWiFi, BruceBLE, BruceRF)
- [ ] Test: Can we trigger a deauth from our code?

### Phase 2: Core Engine
- [ ] TargetTable: Store and update targets
- [ ] ActionResolver: Map target state → valid actions
- [ ] AssessorEngine: Orchestrate scan/attack lifecycle

### Phase 3: UI Shell
- [ ] BootSequence: Animation + onboarding
- [ ] TargetRadar: List view of targets
- [ ] TargetDetail: Single target + actions
- [ ] Theme: Consistent visual language

### Phase 4: Polish & Virality
- [ ] README with GIFs and screenshots
- [ ] Video demo
- [ ] GitHub Actions for auto-builds
- [ ] Community feedback round

---

## 6. Success Metrics

1. **Usability:** First-time user can scan + deauth in <30 seconds
2. **Clarity:** Action availability is never confusing
3. **Performance:** UI never freezes (non-blocking architecture)
4. **Virality:** README is compelling, screenshots are beautiful
5. **Extensibility:** Adding new attacks is trivial (adapter pattern)
