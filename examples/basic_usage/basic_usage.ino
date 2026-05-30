#include <Wire.h>
#include <D7S.h>

static const int PIN_INT1 = 33;  // collapse / shutoff
static const int PIN_INT2 = 32;  // earthquake start / end

volatile bool int1Fired = false;
volatile bool int2Fired = false;

void IRAM_ATTR onInt1() { int1Fired = true; }
void IRAM_ATTR onInt2() { int2Fired = true; }

D7S d7s;

void waitNormalMode(uint32_t timeoutMs = 10000) {
    uint32_t t = millis();
    while (d7s.getState() != NORMAL_MODE) {
        if (millis() - t > timeoutMs) {
            Serial.println("[D7S] timeout waiting for NORMAL_MODE");
            return;
        }
        delay(100);
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();

    if (!d7s.begin()) {
        Serial.println("[D7S] sensor not found at 0x55");
        while (true) delay(1000);
    }
    Serial.println("[D7S] found");

    d7s.initialize();
    Serial.println("[D7S] initialize()...");
    waitNormalMode();
    Serial.println("[D7S] NORMAL_MODE after initialize");

    d7s.acquireOffset();
    Serial.println("[D7S] acquireOffset()...");
    waitNormalMode();
    Serial.println("[D7S] NORMAL_MODE after acquireOffset");

    d7s.setAxis(AXIS_AUTO_SWITCH);
    d7s.setThreshold(THRESHOLD_HIGH);
    d7s.resetEvents();

    pinMode(PIN_INT1, INPUT_PULLUP);
    pinMode(PIN_INT2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_INT1), onInt1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_INT2), onInt2, CHANGE);

    Serial.println("[D7S] ready");
}

void loop() {
    if (int1Fired) {
        int1Fired = false;
        uint8_t ev = d7s.readEventRegister();
        if (d7s.isCollapseEvent())  Serial.println("[D7S] COLLAPSE");
        if (d7s.isShutoffEvent())   Serial.println("[D7S] SHUTOFF");
    }

    if (int2Fired) {
        int2Fired = false;
        if (d7s.isEarthquakeOccurring()) {
            Serial.println("[D7S] earthquake start");
        } else {
            double si  = d7s.getLatestSI(0);
            double pga = d7s.getLatestPGA(0);
            double t   = d7s.getLatestTemperature(0);
            Serial.printf("[D7S] end — SI=%.3f cm/s  PGA=%.3f m/s²  T=%.1f°C\n",
                          si, pga, t);
        }
    }
}
