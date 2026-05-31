# d7s-esphome

Arduino/ESPHome library for the OMRON D7S seismic sensor (RAK12027 module).

Clean re-implementation from scratch — the RAK12027_D7S library has critical bugs (`isInCollapse` / `isInShutoff` return hardcoded constants, blocking delays, infinite I2C retry, wrong unit conversion).

---

## Hardware

| Signal | Description |
|--------|-------------|
| I2C address | `0x55` |
| SDA / SCL | Standard Wire pins |
| INT1 | Collapse / shutoff interrupt |
| INT2 | Earthquake start / end interrupt |
| SET | Hardware reset (active HIGH) |

---

## Installation

**ESPHome** — add to your YAML:
```yaml
esphome:
  libraries:
    - "D7S=https://github.com/luckehall/d7s-esphome"
```

**Arduino IDE** — clone or download the repo into your libraries folder.

---

## API

### Initialization

```cpp
D7S d7s;

Wire.begin(SDA, SCL);
if (!d7s.begin(Wire)) { /* sensor not found */ }
```

### Startup sequence (required for collapse detection)

```cpp
d7s.setAxis(AXIS_AUTO_SWITCH);
d7s.setThreshold(THRESHOLD_LOW);

d7s.initialize();
while (d7s.getState() != NORMAL_MODE) delay(200);   // wait up to 10 s

d7s.acquireOffset();
while (d7s.getState() != NORMAL_MODE) delay(200);   // wait up to 10 s

d7s.resetEvents();   // clear flags before enabling interrupts
```

`initialize()` must complete before collapse detection works — it acquires the reference orientation. Skipping or shortcutting the wait will leave collapse permanently inactive.

### State

```cpp
D7SState state = d7s.getState();
// NORMAL_MODE, NORMAL_MODE_NOT_STANDBY, INITIAL_INSTALLATION,
// OFFSET_ACQUISITION, SELFTEST_MODE

bool eq = d7s.isEarthquakeOccurring();  // true while INT2 is active
```

### Events (INT1 — collapse / shutoff)

The EVENT register `0x1002` is **read-clear**: reading it resets both flags. Call `readEventRegister()` exactly once per INT1 event, then query from cache.

```cpp
// In interrupt handler (sets a flag):
volatile bool int1Fired = false;
void IRAM_ATTR onInt1() { int1Fired = true; }

// In main loop:
if (int1Fired) {
    int1Fired = false;
    d7s.readEventRegister();           // reads once, caches result
    if (d7s.isCollapseEvent())  { /* handle collapse  */ }
    if (d7s.isShutoffEvent())   { /* handle shutoff   */ }
}
```

### Measurements

```cpp
// After earthquake ends (index 0 = most recent, 4 = oldest):
double si   = d7s.getLatestSI(0);           // cm/s (kine)
double pga  = d7s.getLatestPGA(0);          // g
double temp = d7s.getLatestTemperature(0);  // °C

// Real-time during earthquake (updated every 100 ms by D7S):
double si_rt  = d7s.getInstantaneousSI();   // cm/s (kine)
double pga_rt = d7s.getInstantaneousPGA();  // g
```

### Configuration

```cpp
d7s.setAxis(AXIS_AUTO_SWITCH);    // AXIS_YZ, AXIS_XZ, AXIS_XY,
                                  // AXIS_AUTO_SWITCH, AXIS_SWITCH_AT_INIT
d7s.setThreshold(THRESHOLD_LOW);  // THRESHOLD_HIGH, THRESHOLD_LOW

d7s.clearAllData();   // clear latest + ranked measurements
d7s.resetEvents();    // clear EVENT register flags
```

---

## Units

| Method | Unit | LSB |
|--------|------|-----|
| `getLatestSI()` | cm/s | 0.01 cm/s |
| `getInstantaneousSI()` | cm/s | 0.01 cm/s |
| `getLatestPGA()` | g | 0.001 m/s² → g |
| `getInstantaneousPGA()` | g | 0.01 m/s² → g |
| `getLatestTemperature()` | °C | 0.1 °C (signed) |

No unit conversion is performed on the caller side — values are ready to use.

---

## ESP32 Arduino 3.x note

On ESP32 Arduino framework 3.x (ESP-IDF 5.x), the combined write+read without STOP (`endTransmission(false)` + `requestFrom()`) fails with `ESP_ERR_INVALID_STATE`. This library uses `endTransmission(true)` (STOP + new START) for all register reads, which the D7S accepts.
