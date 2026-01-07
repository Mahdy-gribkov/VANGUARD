# The Assessor

> **Know your target first.**

A target-first auditing tool for the M5Stack Cardputer. Instead of choosing an attack and then finding something to hit, The Assessor shows you what's around you and tells you exactly what you can do to each target.

---

## The Philosophy

Traditional pentest tools work backwards:

```
[Pick Category] → [Pick Attack] → [Scan for Targets] → [Choose Target] → [Execute]

You decide what to do before you know what's possible.
```

**The Assessor inverts this:**

```
[Auto-Scan] → [See All Targets] → [Tap One] → [See Valid Actions] → [Execute]

You see reality first. Then you act on it.
```

The target is the noun. The attack is the verb. **Pick the noun first.**

---

## Features

### Target-First Discovery
Boot up and immediately see every WiFi network, client device, and BLE beacon around you. No menus to navigate. No modes to select. Just reality.

### Context-Aware Actions
Tap a target and see ONLY the actions that work:
- **Access point with 5 clients?** → Deauth available
- **Access point with 0 clients?** → Deauth hidden (nothing to deauth)
- **Open network?** → Evil Twin available
- **WPA3 network?** → PMKID capture hidden (not vulnerable)

No more "Attack Failed - No Clients Connected" errors. If you see the button, it will work.

### Signal-Sorted Radar
Targets are sorted by signal strength. The strongest (closest, most vulnerable) targets appear first. Weak signals fade to the bottom.

### Powered by Bruce
Under the hood, The Assessor uses [Bruce](https://github.com/pr3y/Bruce)'s battle-tested attack modules. We didn't reinvent packet injection - we wrapped it in a smarter interface.

---

## Screenshots

> *Coming soon - the UI is under active development*

```
┌────────────────────────────────────────┐
│  TARGET RADAR                  [MENU]  │
│  ════════════════════════════════════  │
│                                        │
│     ●───── NETGEAR-5G (-42dB)         │
│     ◐───── iPhone-Sarah (-58dB)       │
│     ○───── Hidden Network (-71dB)     │
│     ◦───── Weak_Signal (-89dB)        │
│                                        │
│  ▼ 8 more targets                      │
└────────────────────────────────────────┘
```

---

## Installation

### Requirements
- M5Stack Cardputer (ESP32-S3)
- PlatformIO (VS Code extension or CLI)
- USB-C cable

### Build & Flash

```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/the-assessor.git
cd the-assessor

# Build
pio run

# Flash to Cardputer
pio run -t upload

# Monitor serial output (optional)
pio device monitor
```

### Pre-Built Binaries

Check the [Releases](https://github.com/YOUR_USERNAME/the-assessor/releases) page for ready-to-flash `.bin` files.

---

## Usage

1. **Boot** - Logo appears, auto-scan begins
2. **Browse** - Scroll through discovered targets (sorted by signal)
3. **Select** - Press enter on a target to see details
4. **Act** - Choose from context-aware actions
5. **Watch** - See real-time attack progress
6. **Return** - Go back to radar and pick another target

### Controls

| Button | Action |
|--------|--------|
| ▲ / ▼ | Navigate list |
| Enter | Select target / Confirm action |
| Esc | Go back |
| G | Open settings menu |

---

## Architecture

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
└─────────────────────────────────────────────────────────────────┘
```

See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for the deep dive.

---

## Supported Attacks

### WiFi
| Attack | Description | Requirement |
|--------|-------------|-------------|
| Deauth All | Disconnect all clients from AP | Target has clients |
| Deauth Single | Disconnect specific client | Target has clients |
| Beacon Flood | Spam fake networks | Any AP target |
| Evil Twin | Clone network for credential capture | Any AP target |
| Capture PMKID | Grab PMKID hash for offline cracking | WPA2 target |
| Monitor | Passive packet capture | Any target |

### BLE
| Attack | Description | Requirement |
|--------|-------------|-------------|
| BLE Spam | Flood with pairing requests | BLE device nearby |
| Sour Apple | Apple device DoS | Apple BLE device |
| Skimmer Detect | Identify suspicious BLE devices | Any |

---

## Legal Disclaimer

**This tool is for authorized security testing and educational purposes only.**

You must have explicit permission from the network owner before running any attacks. Unauthorized network intrusion is illegal in most jurisdictions.

The developers are not responsible for misuse of this software. If you can't hack responsibly, don't hack at all.

---

## Contributing

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Priority Areas
- [ ] Bruce adapter implementation
- [ ] UI polish and animations
- [ ] BLE attack support
- [ ] RF/IR support (hardware-dependent)
- [ ] Documentation and screenshots

---

## Credits

- **Bruce** - The attack engine that makes this possible ([pr3y/Bruce](https://github.com/pr3y/Bruce))
- **M5Stack** - Hardware and M5Unified library
- **ESP-IDF** - The foundation of ESP32 development

---

## License

GPL-3.0 - See [LICENSE](LICENSE) for details.

This project is a derivative work of Bruce and maintains license compatibility.

---

<p align="center">
  <b>The Assessor</b><br>
  <i>Know your target first.</i>
</p>
