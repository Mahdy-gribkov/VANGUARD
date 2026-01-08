# The Assessor - Bug Fix & Completion Plan

## Reality Check

The previous work was incomplete and the latest commit **broke what was working**. Here's what's actually wrong:

### Critical Bugs Causing Your Symptoms

| Symptom | Root Cause | Location |
|---------|------------|----------|
| Device keeps restarting | Synchronous WiFi scan blocks watchdog | `AssessorEngine.cpp:163` |
| WiFi doesn't scan properly | Blocking call times out | `AssessorEngine.cpp:163,215` |
| BLE doesn't work | WiFi/BLE radio conflict, no transition | `AssessorEngine.cpp:371-428` |
| Screen flickering/freezing | Main loop blocked, render() never called | Cascade from scan block |
| Actions freeze device | Same blocking pattern in attacks | `BruceWiFi.cpp:263-291` |

### The Specific Bug That Broke Everything

Commit `b0e7d97` changed WiFi scanning from **async** to **sync**:

```cpp
// This BLOCKS for 3-5 seconds, causing watchdog timeout:
int count = WiFi.scanNetworks(false, true, false, 300);  // async=FALSE

// Should be:
WiFi.scanNetworks(true, true, false, 500);  // async=TRUE
```

### Missing Features (vs Bruce/Evil Portal)

The project only has ~10 attacks implemented. Bruce has 100+. Missing:
- **Evil Portal** - Only stub exists (fake AP, no captive portal/credential capture)
- **PCAP capture** - TODO comment, not implemented
- **EAPOL/Handshake detection** - TODO comment, not implemented
- **Probe flood** - Defined but not wired up
- **IR attacks** - Not implemented
- **RF attacks** - Not implemented (hardware dependent)
- **Most BLE attacks** - Only spam/sour apple

---

## Fix Plan

### Phase 1: Fix Critical Crashes (Must Do First)

1. **Revert to async WiFi scanning**
   - Change `WiFi.scanNetworks(false,...)` → `WiFi.scanNetworks(true,...)`
   - Add proper async completion checking in `tick()`
   - Feed watchdog during scan wait

2. **Fix WiFi→BLE radio transition**
   - Add proper delay with yield between WiFi shutdown and BLE init
   - Ensure WiFi is fully OFF before BLE starts
   - Add retry logic with proper error handling

3. **Add yield() calls in attack loops**
   - Deauth: yield between frames
   - Beacon flood: yield between beacons
   - BLE spam: yield between advertisements

### Phase 2: Fix Display Issues

4. **Ensure render() runs during scans**
   - Async scan means main loop keeps running
   - Add scan progress animation that updates each tick

5. **Verify sprite double-buffering**
   - Confirm pushSprite() called after all drawing
   - Add frame rate limiting if needed

### Phase 3: Complete Incomplete Features

6. **Evil Portal (not just Evil Twin)**
   - Add WebServer for captive portal
   - Add DNSServer for DNS spoofing
   - Create credential capture page
   - Store captured credentials

7. **Handshake Capture**
   - Implement EAPOL frame detection
   - Save to PCAP format

8. **Add more attacks from Bruce**
   - Probe flood
   - Beacon spam variants
   - More BLE attack types

### Phase 4: Polish

9. **About screen**
10. **Better error messages**
11. **README with actual screenshots**

---

## Implementation Order

I'll fix bugs in this order:

1. **WiFi async scan** - Stops crashes immediately
2. **Watchdog yields** - Prevents remaining timeouts
3. **WiFi/BLE transition** - Makes BLE work
4. **Display during scan** - Stops freezing
5. **Attack stability** - Makes actions work
6. **Evil Portal** - Adds the missing major feature
7. **Other attacks** - Adds remaining Bruce functionality

---

## Questions Before Proceeding

1. **Do you want me to focus on stability first** (fix crashes, make what exists work) **or features** (add Evil Portal and more attacks)?

2. **For Evil Portal**, do you want:
   - Basic credential capture page (simple)
   - Clone of target AP's real login page (complex)
   - Multiple templates (Google, Facebook, etc.)

3. **Which Bruce attacks are most important to you?** I can prioritize specific ones.
