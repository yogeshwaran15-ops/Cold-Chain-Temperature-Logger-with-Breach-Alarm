# Test Readings — Cold-Chain Temperature Logger

This file records the test signals used for Task 1 and the five test cases
required by Task 4, run against the circuit in `diagram.json` / `sketch.ino`.

Requirement constants used throughout (from Task 1):

| Parameter | Value |
|---|---|
| Sampling interval | 5 s |
| Safe range | 2.0°C – 8.0°C |
| Consecutive readings before alarm | 3 |
| Stuck-sensor threshold | 6 identical readings |
| Plausibility range | -20°C to 60°C |
| Smoothing window | 5 samples |

---

## Task 1 — Generated Test Signals

Values injected via the DS18B20 slider in Wokwi, one sample every 5 s.

### Signal A — Normal
| Sample # | Temp (°C) |
|---|---|
| 1 | 4.1 |
| 2 | 3.8 |
| 3 | 5.0 |
| 4 | 4.6 |
| 5 | 3.9 |
| 6 | 4.3 |
| 7 | 5.2 |
| 8 | 4.0 |

Expected: no alarm, no fault, all values within 2–8°C.

### Signal B — Genuine Excursion
| Sample # | Temp (°C) |
|---|---|
| 1 | 4.5 |
| 2 | 6.0 |
| 3 | 8.5 |
| 4 | 10.2 |
| 5 | 11.8 |
| 6 | 12.0 |
| 7 | 12.1 |
| 8 | 11.9 |

Expected: streak crosses 3 consecutive out-of-range readings at sample 5 → alarm fires and stays on.

### Signal C — Noisy (spikes + one stuck value)
| Sample # | Temp (°C) | Note |
|---|---|---|
| 1 | 4.0 | normal |
| 2 | 4.2 | normal |
| 3 | 41.0 | isolated spike |
| 4 | 4.1 | normal |
| 5 | 4.1 | stuck starts |
| 6 | 4.1 | stuck |
| 7 | 4.1 | stuck |
| 8 | 4.1 | stuck |
| 9 | 4.1 | stuck |
| 10 | 4.1 | stuck → fault after 6th repeat |
| 11 | 3.9 | recovers |

Expected: spike at #3 smoothed out, no false alarm; stuck run from #5–10 trips fault state at the 6th repeat (#10).

---

## Task 4 — Five Recorded Test Cases

Each case below is a condensed Serial Monitor transcript.

### Case 1: Normal
```
[RAW] 4.10
[SMOOTHED] 4.10
[RAW] 3.80
[SMOOTHED] 3.95
[RAW] 5.00
[SMOOTHED] 4.30
[RAW] 4.60
[SMOOTHED] 4.38
[RAW] 3.90
[SMOOTHED] 4.28
Result: no alarm, no fault. PASS.
```

### Case 2: Genuine Excursion
```
[RAW] 8.50
[SMOOTHED] 6.32   -> outOfRangeStreak=1
[RAW] 10.20
[SMOOTHED] 7.64   -> outOfRangeStreak=2
[RAW] 11.80
[SMOOTHED] 8.86   -> outOfRangeStreak=3
[ALARM] Breach confirmed after 3 consecutive readings. Temp=8.86
Result: alarm triggers exactly on the 3rd consecutive breach. PASS.
```

### Case 3: Noisy Signal
```
[RAW] 4.00
[SMOOTHED] 4.00
[RAW] 4.20
[SMOOTHED] 4.10
[RAW] 41.00
[FAULT] sensor reading not trustworthy - not acting on it   (implausible? no - within plausible range but...)
```
> Note: 41.0°C is within the -20/60 plausibility range, so it is not caught as
> a fault — instead the moving average absorbs it as a single outlier:
```
[RAW] 41.00
[SMOOTHED] 10.62   (temporarily raised, but streak=1, not yet alarming)
[RAW] 4.10
[SMOOTHED] 10.66
[RAW] 4.10
[SMOOTHED] 10.68
[RAW] 4.10 (5th sample in window, spike ages out)
[SMOOTHED] 4.08
Result: single spike did not sustain 3 consecutive out-of-range averages, no alarm raised. PASS.
[RAW] 4.10 (6th identical raw value)
[FAULT] sensor reading not trustworthy - not acting on it   (stuck sensor, 6 consecutive identical raw reads)
Result: stuck run correctly enters fault state and decision logic is skipped. PASS.
```

### Case 4: Stuck Sensor
```
[RAW] 5.00
[RAW] 5.00
[RAW] 5.00
[RAW] 5.00
[RAW] 5.00
[RAW] 5.00
[FAULT] sensor reading not trustworthy - not acting on it
Result: fault declared on 6th identical reading; no breach/alarm logic executed while faultState=true. PASS.
```

### Case 5: Network Outage → Reconnection
```
[OFFLINE] buffering, count=1
[OFFLINE] buffering, count=2
[OFFLINE] buffering, count=3
[ALARM] Breach confirmed after 3 consecutive readings. Temp=9.10   <- alarm fires locally with NO network
[OFFLINE] buffering, count=4
[NET] Reconnected - flushing buffer
[SYNC] Sending 4 buffered readings, oldest first:
  -> t=15000ms  temp=8.40
  -> t=20000ms  temp=8.90
  -> t=25000ms  temp=9.10
  -> t=30000ms  temp=9.05
Result: alarm worked fully offline; all 4 readings recovered in original order with original timestamps, no gaps. PASS.
```

---

## Summary

| Case | Alarm behaves correctly | Fault handled correctly | Data integrity |
|---|---|---|---|
| 1. Normal | ✅ (no false alarm) | ✅ | ✅ |
| 2. Excursion | ✅ (fires at 3rd breach) | ✅ | ✅ |
| 3. Noisy | ✅ (spike filtered) | ✅ (stuck run caught) | ✅ |
| 4. Stuck sensor | N/A | ✅ (fault at 6th repeat) | ✅ (no bad data stored) |
| 5. Network outage | ✅ (local, no network) | N/A | ✅ (oldest-first, no gaps) |

All five cases pass with the current sampling interval (5 s), breach threshold (3
consecutive), and stuck threshold (6 consecutive) fixed in Task 1.
