#include "D7S.h"

D7S::D7S() : _wire(nullptr), _eventCache(0) {}

bool D7S::begin(TwoWire& wire) {
    _wire = &wire;
    _wire->beginTransmission(ADDRESS);
    return _wire->endTransmission() == 0;
}

void D7S::initialize() {
    write8bit(REG_MODE, CMD_INIT);
}

void D7S::acquireOffset() {
    write8bit(REG_MODE, CMD_OFFSET);
}

void D7S::setAxis(D7SAxisMode mode) {
    uint8_t ctrl = read8bit(REG_CTRL);
    ctrl = (ctrl & 0xF8) | (mode & 0x07);
    write8bit(REG_CTRL, ctrl);
}

void D7S::setThreshold(D7SThreshold level) {
    uint8_t ctrl = read8bit(REG_CTRL);
    if (level == THRESHOLD_LOW)
        ctrl |=  0x08;
    else
        ctrl &= ~0x08;
    write8bit(REG_CTRL, ctrl);
}

void D7S::resetEvents() {
    // Lettura read-clear del registro EVENT azzera entrambi i flag.
    readEventRegister();
    _eventCache = 0;
}

void D7S::clearAllData() {
    write8bit(REG_CLEAR, CLEAR_ALL);
}

D7SState D7S::getState() {
    return static_cast<D7SState>(read8bit(REG_STATE) & 0x07);
}

bool D7S::isEarthquakeOccurring() {
    return getState() == NORMAL_MODE_NOT_STANDBY;
}

uint8_t D7S::readEventRegister() {
    // Il registro 0x1002 è read-clear: si azzera alla lettura.
    // Chiamare una sola volta per evento INT1; i flag rimangono in _eventCache.
    _eventCache = read8bit(REG_EVENT) & 0x03;
    return _eventCache;
}

bool D7S::isCollapseEvent() { return (_eventCache & 0x02) != 0; }
bool D7S::isShutoffEvent()  { return (_eventCache & 0x01) != 0; }

double D7S::getLatestSI(uint8_t index) {
    if (index > 4) return 0.0;
    return read16bit(REG_LATEST_BASE + index * 0x10 + OFF_SI) * SCALE_SI;
}

double D7S::getLatestPGA(uint8_t index) {
    if (index > 4) return 0.0;
    return read16bit(REG_LATEST_BASE + index * 0x10 + OFF_PGA) * SCALE_PGA_MS2 / G;
}

double D7S::getLatestTemperature(uint8_t index) {
    if (index > 4) return 0.0;
    return read16bitSigned(REG_LATEST_BASE + index * 0x10 + OFF_TEMP) * SCALE_TEMP;
}

double D7S::getInstantaneousSI() {
    return read16bit(REG_INST_SI) * SCALE_SI;
}

double D7S::getInstantaneousPGA() {
    return read16bit(REG_INST_PGA) * SCALE_PGA_INST_MS2 / G;
}

// ---------------------------------------------------------------------------
// I2C helpers
// ---------------------------------------------------------------------------

bool D7S::writeAddr(uint16_t reg) {
    _wire->beginTransmission(ADDRESS);
    _wire->write(static_cast<uint8_t>(reg >> 8));
    _wire->write(static_cast<uint8_t>(reg & 0xFF));
    // false = repeated start (no STOP): mantiene il bus per la lettura successiva
    uint8_t err = _wire->endTransmission(false);
    if (err != 0) {
        Serial.print("[D7S] I2C error ");
        Serial.print(err);
        Serial.print(" on reg 0x");
        Serial.println(reg, HEX);
        return false;
    }
    return true;
}

uint8_t D7S::read8bit(uint16_t reg) {
    if (!writeAddr(reg)) return 0;
    _wire->requestFrom(ADDRESS, (uint8_t)1);
    uint32_t t = millis();
    while (!_wire->available()) {
        if (millis() - t > I2C_TIMEOUT_MS) return 0;
    }
    return _wire->read();
}

uint16_t D7S::read16bit(uint16_t reg) {
    if (!writeAddr(reg)) return 0;
    _wire->requestFrom(ADDRESS, (uint8_t)2);
    uint32_t t = millis();
    while (_wire->available() < 2) {
        if (millis() - t > I2C_TIMEOUT_MS) return 0;
    }
    uint8_t msb = _wire->read();
    uint8_t lsb = _wire->read();
    return static_cast<uint16_t>((msb << 8) | lsb);
}

int16_t D7S::read16bitSigned(uint16_t reg) {
    return static_cast<int16_t>(read16bit(reg));
}

bool D7S::write8bit(uint16_t reg, uint8_t val) {
    _wire->beginTransmission(ADDRESS);
    _wire->write(static_cast<uint8_t>(reg >> 8));
    _wire->write(static_cast<uint8_t>(reg & 0xFF));
    _wire->write(val);
    // true = STOP dopo la scrittura
    uint8_t err = _wire->endTransmission(true);
    if (err != 0) {
        Serial.print("[D7S] I2C write error ");
        Serial.print(err);
        Serial.print(" on reg 0x");
        Serial.println(reg, HEX);
        return false;
    }
    return true;
}
