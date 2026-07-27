# Cold-Chain Temperature Logger with Breach Alarm

**Wokwi Simulation:** https://wokwi.com/projects/470631925290516481

SIH 2026 Internal Practical Assessment — Yogeshwaran N · Reg 411725106124 · PSVPEC · ECE

---

## The Problem, and Who It Affects

Vaccines and perishable food are moved in insulated boxes and simply *assumed*
to have stayed cold — because nothing on the box actually measures it. If the
box warms up for even a short window during a delivery, no one finds out.
The stock still looks fine on arrival, so it gets used or sold exactly as if
nothing happened.

**Who this affects:** health workers administering vaccines that may no longer
work, patients receiving them, hospitals and clinics relying on the stock, and
the transport/logistics staff who currently have no way to prove the delivery
was safe.

## The Solution, in One Sentence

*A small box-mounted device that watches the temperature the whole journey,
sounds an alarm the instant something goes wrong — even with no internet —
and reports the complete story the moment it comes back online.*

---

## Screenshots

> Replace these placeholders with your own Serial Monitor / Wokwi screenshots
> before submitting. See `test_reading.md` for the exact scenarios to capture.

| Scenario | Screenshot |
|---|---|
| Normal operation | `screenshots/01_normal.png` |
| Breach alarm triggering | `screenshots/02_alarm.png` |
| Stuck-sensor fault state | `screenshots/03_fault.png` |
| Offline buffering | `screenshots/04_offline.png` |
| Reconnect & flush (oldest-first) | `screenshots/05_reconnect.png` |

---

## How the Derived Figure Is Calculated

The only derived (calculated, not directly read) figure in this system is the
**smoothed temperature** — a 5-sample moving average that protects the alarm
logic from single-sample sensor spikes.

**Formula:**
```
smoothed_temp = (t1 + t2 + t3 + t4 + t5) / 5
```
where `t1..t5` are the 5 most recent raw readings (oldest to newest).

**Worked example** (matches `test_reading.md`, Case 3):
```
Raw readings:   4.00, 4.20, 41.00, 4.10, 4.10
Sum:            4.00 + 4.20 + 41.00 + 4.10 + 4.10 = 57.40
Average:        57.40 / 5 = 11.48 °C
```
By hand: 57.40 ÷ 5 = **11.48**, which matches the `[SMOOTHED]` value printed to
Serial at that sample — confirming the code's arithmetic is correct.

This average is what the breach logic checks against the safe range (2–8°C),
not the raw value — so one bad sample can't trigger a false alarm, but a real
sustained excursion still pulls the average out of range within a few samples.

### Second derived figure: Temperature Drift

**Formula:**
```
drift = smoothed_now - smoothed_previous
```

**Worked example** (Signal B, genuine excursion, samples 3→4 from `test_reading.md`):
```
smoothed at sample 3: 7.64
smoothed at sample 4: 8.86
drift = 8.86 - 7.64 = 1.22 °C per 5 s
```
A positive, growing drift like this warns that the box is heating up fast,
before the alarm even fires — useful for spotting a slow-failing seal ahead
of a full breach. This value is calculated in code but was not previously
shown on screen; it now prints as `[DRIFT] +1.22 C since last sample` and
updates every sampling cycle as the injected temperature changes.

---

## What Each Field Means (Serial Monitor / Log Output)

| Field | Meaning |
|---|---|
| `[RAW]` | The unfiltered value read directly from the DS18B20 sensor this cycle |
| `[SMOOTHED]` | The 5-sample moving average calculated from the last 5 raw readings |
| `[DRIFT]` | Change in smoothed temperature since the previous sample (°C per 5 s). Positive means warming, negative means cooling. Calculated but not previously shown - it flags a fast-warming box before it fully breaches the safe range |
| `[FAULT]` | Sensor reading rejected outright — either an impossible value (outside -20°C to 60°C, or the DallasTemperature -127°C disconnect code) or stuck (6+ identical readings). The reading is never smoothed, never checked against the safe range, and never stored |
| `[HOLD]` | Shown while faulted: the last trustworthy smoothed value, which the system continues to treat as current rather than reacting to the bad reading |
| `[FAULT CLEARED]` | Sensor has produced a good reading again; normal processing resumes from the next sample |
| `[ALARM]` | The smoothed temperature has stayed outside 2–8°C for 3 consecutive readings — buzzer is on |
| `[ALARM CLEARED]` | Smoothed temperature has returned to the safe range |
| `[OFFLINE] buffering, count=N` | Network unavailable; reading N is being held in memory with its original timestamp |
| `[NET] Reconnected - flushing buffer` | Network button pressed again; stored readings are about to be sent |
| `[SYNC] Sending N buffered readings, oldest first` | Buffer flush in progress — each line shows the original read timestamp (ms since boot) and temperature, sent in the order they were recorded |

---

## What Works

- Non-blocking sampling every 5 seconds (no `delay()` anywhere in the loop)
- Plausibility check rejects impossible readings (outside -20°C to 60°C)
- 5-sample moving average smooths isolated spikes without hiding real excursions
- Local breach alarm fires after 3 consecutive out-of-range readings, entirely independent of network state
- Stuck-sensor detection (6 identical consecutive raw readings) enters a fault state and skips acting on bad data
- Impossible readings (outside -20°C to 60°C, or the DallasTemperature -127°C error code) are rejected on the very first occurrence — no averaging in, no alarm impact, no storage; the system holds its last trustworthy value until good readings resume
- Store-and-forward: readings are timestamped at the moment they're read, buffered while offline, and flushed oldest-first on reconnection with no gaps
- All 5 required test cases (normal, excursion, noisy, stuck sensor, network outage/reconnect) pass — see `test_reading.md`

## What Is Unfinished

- No persistent storage: the buffer lives in RAM, so a power loss mid-transit would lose any un-flushed readings
- No real network stack wired in yet — network state is simulated with a pushbutton rather than actual Wi-Fi/MQTT
- No on-device display (LCD/OLED); all output is via Serial Monitor only
- No historical data view — only the current session's readings are visible, nothing is saved across reboots

## One Improvement for Next Iteration

Add a small non-volatile log (e.g. ESP32's onboard flash via Preferences/SPIFFS)
so buffered readings survive a power cycle, and swap the pushbutton network
toggle for real Wi-Fi + MQTT publishing to an actual dashboard.

---

## How to Run

### Option A — Run directly in Wokwi (recommended, no installs)
1. Open the simulation: **https://wokwi.com/projects/470631925290516481**
2. Click **Start Simulation**.
3. Click the DS18B20 sensor on the canvas to open its temperature slider — drag it to inject test values (see `test_reading.md` for exact scenarios).
4. Hold the pushbutton down to simulate "network available" (green LED lights up); release to simulate "offline."
5. Watch the Serial Monitor (bottom panel) for `[RAW]`, `[SMOOTHED]`, `[ALARM]`, `[FAULT]`, and `[SYNC]` messages.

### Option B — Run from these repo files
1. Clone this repository.
2. Go to [wokwi.com](https://wokwi.com) → **New Project** → **ESP32**.
3. Replace the default `diagram.json` and `sketch.ino` with the ones in this repo.
4. Ensure `libraries.txt` includes `OneWire` and `DallasTemperature` (Wokwi installs these automatically from the file).
5. Click **Start Simulation** and follow steps 3–5 from Option A.

---

## Repository Contents

| File | Purpose |
|---|---|
| `diagram.json` | Wokwi circuit definition (ESP32, DS18B20, buzzer, network-toggle button, status LED) |
| `sketch.ino` | Firmware: sensing, filtering, local alarm, store-and-forward |
| `wokwi.toml` | Wokwi project configuration |
| `libraries.txt` | Required Arduino libraries |
| `test_reading.md` | Test signals (Task 1) and five recorded test cases (Task 4) |
| `presentation.pdf` | 6–8 slide presentation covering the problem, solution, screenshots, and next steps |
| `screenshots/` | Screenshots referenced above |
| `demo_video.mp4` (or link below) | Short screen-capture demonstration |

**Demo video:** https://youtu.be/5CEUHLy7gvE

---

## Submission Note

This is an Easy-level assessment; the solution intentionally stays simple and
demonstrable rather than feature-heavy, per the assignment brief. All 6 tasks
are complete except where noted above under "What Is Unfinished."
