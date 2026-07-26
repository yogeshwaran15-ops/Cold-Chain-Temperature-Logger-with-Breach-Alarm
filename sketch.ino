/* ============================================================
   Cold-Chain Temperature Logger with Breach Alarm
   Tasks 1-3: Requirements constants, non-blocking sensing with
   filtering, local decision + store-and-forward.
   Board: ESP32 DevKit-V1 | Sensor: DS18B20 on GPIO4
   Buzzer: GPIO25 | Network toggle button: GPIO27 | Status LED: GPIO26
   ============================================================ */

#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- Task 1: Requirement constants (design decisions) ----------
const unsigned long SAMPLE_INTERVAL_MS = 5000;   // sampling interval
const float SAFE_MIN = 2.0;                      // safe range lower bound (deg C)
const float SAFE_MAX = 8.0;                      // safe range upper bound (deg C)
const int   BREACH_CONSEC_REQUIRED = 3;           // consecutive readings before alarm
const int   STUCK_CONSEC_REQUIRED  = 6;           // consecutive identical reads = stuck sensor
const float PLAUSIBLE_MIN = -20.0;                // plausibility floor
const float PLAUSIBLE_MAX = 60.0;                 // plausibility ceiling
const int   SMOOTH_WINDOW = 5;                    // moving-average window size

// ---------- Pins ----------
#define ONE_WIRE_BUS 4
#define BUZZER_PIN   25
#define NETWORK_BTN_PIN 27
#define NETWORK_LED_PIN 26

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ---------- Task 2: sensing state ----------
unsigned long lastSampleTime = 0;
float smoothBuf[SMOOTH_WINDOW] = {0};
int smoothIdx = 0;
bool smoothFilled = false;

float lastRawValue = -999.0;
int stuckCount = 0;
bool faultState = false;

// ---------- Task 3: local decision + store-and-forward state ----------
int outOfRangeStreak = 0;
bool alarmActive = false;

struct Reading {
  float temp;
  unsigned long timestamp;
};
const int BUFFER_CAPACITY = 200;
Reading buffer[BUFFER_CAPACITY];
int bufferCount = 0;

bool lastNetworkState = false;

void setup() {
  Serial.begin(115200);
  sensors.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(NETWORK_LED_PIN, OUTPUT);
  pinMode(NETWORK_BTN_PIN, INPUT_PULLUP); // button LOW = pressed = "network available"

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(NETWORK_LED_PIN, LOW);

  Serial.println("=== Cold-Chain Logger booting ===");
  Serial.println("State: no data yet (loading state)");
}

// Returns true if network is currently available (button held = online)
bool isNetworkAvailable() {
  return digitalRead(NETWORK_BTN_PIN) == LOW;
}

// ---------- Task 2: plausibility + smoothing ----------
bool isPlausible(float t) {
  return (t > PLAUSIBLE_MIN && t < PLAUSIBLE_MAX);
}

float smooth(float newVal) {
  smoothBuf[smoothIdx % SMOOTH_WINDOW] = newVal;
  smoothIdx++;
  if (smoothIdx >= SMOOTH_WINDOW) smoothFilled = true;

  int count = smoothFilled ? SMOOTH_WINDOW : smoothIdx;
  float sum = 0;
  for (int i = 0; i < count; i++) sum += smoothBuf[i];
  return sum / count;
}

// ---------- Task 4-style fault check (stuck sensor) - needed to gate Task3 logic ----------
bool checkStuck(float raw) {
  if (raw == lastRawValue) {
    stuckCount++;
  } else {
    stuckCount = 0;
  }
  lastRawValue = raw;
  return (stuckCount >= STUCK_CONSEC_REQUIRED);
}

// ---------- Task 3: local breach decision ----------
void evaluateBreach(float smoothed, unsigned long ts) {
  if (smoothed < SAFE_MIN || smoothed > SAFE_MAX) {
    outOfRangeStreak++;
  } else {
    outOfRangeStreak = 0;
    if (alarmActive) {
      alarmActive = false;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("[ALARM CLEARED] back in safe range");
    }
  }

  if (outOfRangeStreak >= BREACH_CONSEC_REQUIRED && !alarmActive) {
    alarmActive = true;
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.print("[ALARM] Breach confirmed after ");
    Serial.print(BREACH_CONSEC_REQUIRED);
    Serial.print(" consecutive readings. Temp=");
    Serial.println(smoothed);
  }
}

// ---------- Task 3: store-and-forward ----------
void storeReading(float t, unsigned long ts) {
  if (bufferCount < BUFFER_CAPACITY) {
    buffer[bufferCount].temp = t;
    buffer[bufferCount].timestamp = ts; // timestamped at READ time
    bufferCount++;
  } else {
    Serial.println("[WARN] buffer full, oldest data at risk - flush needed");
  }
}

void flushBuffer() {
  if (bufferCount == 0) {
    Serial.println("[SYNC] nothing to send (empty state)");
    return;
  }
  Serial.print("[SYNC] Sending ");
  Serial.print(bufferCount);
  Serial.println(" buffered readings, oldest first:");
  for (int i = 0; i < bufferCount; i++) {
    Serial.print("  -> t=");
    Serial.print(buffer[i].timestamp);
    Serial.print("ms  temp=");
    Serial.println(buffer[i].temp);
    // publishToServer(buffer[i].temp, buffer[i].timestamp); // real network call goes here
  }
  bufferCount = 0; // cleared only after "successful" send
}

void loop() {
  // --- network status handling (non-blocking, checked every loop) ---
  bool netNow = isNetworkAvailable();
  digitalWrite(NETWORK_LED_PIN, netNow ? HIGH : LOW);
  if (netNow && !lastNetworkState) {
    Serial.println("[NET] Reconnected - flushing buffer");
    flushBuffer();
  }
  lastNetworkState = netNow;

  // --- sampling on fixed non-blocking schedule (Task 2) ---
  unsigned long now = millis();
  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;

    sensors.requestTemperatures();
    float raw = sensors.getTempCByIndex(0);

    Serial.print("[RAW] "); Serial.println(raw);

    bool stuck = checkStuck(raw);
    bool plausible = isPlausible(raw);

    if (stuck || !plausible) {
      faultState = true;
      Serial.println("[FAULT] sensor reading not trustworthy - not acting on it");
      // Task 4 will fully own this branch; for now we simply skip decision logic.
    } else {
      faultState = false;
      float smoothed = smooth(raw);
      Serial.print("[SMOOTHED] "); Serial.println(smoothed);

      evaluateBreach(smoothed, now);
      storeReading(smoothed, now);

      if (netNow) {
        flushBuffer(); // send immediately if already online
      } else {
        Serial.print("[OFFLINE] buffering, count=");
        Serial.println(bufferCount);
      }
    }
  }
}
