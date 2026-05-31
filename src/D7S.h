#pragma once

#include <Arduino.h>
#include <Wire.h>

enum D7SState : uint8_t {
    NORMAL_MODE             = 0x00,  // standby, nessun terremoto
    NORMAL_MODE_NOT_STANDBY = 0x01,  // terremoto in corso
    INITIAL_INSTALLATION    = 0x02,
    OFFSET_ACQUISITION      = 0x03,
    SELFTEST_MODE           = 0x04,
};

enum D7SAxisMode : uint8_t {
    AXIS_YZ             = 0x00,
    AXIS_XZ             = 0x01,
    AXIS_XY             = 0x02,
    AXIS_AUTO_SWITCH    = 0x03,
    AXIS_SWITCH_AT_INIT = 0x04,
};

enum D7SThreshold : uint8_t {
    THRESHOLD_HIGH = 0x00,
    THRESHOLD_LOW  = 0x01,
};

class D7S {
public:
    D7S();

    // Inizializza il bus I2C e verifica la presenza del sensore.
    // Ritorna false se il sensore non risponde all'indirizzo 0x55.
    bool begin(TwoWire& wire = Wire);

    // Avvia INITIAL_INSTALLATION_MODE (calibrazione asse).
    // Attendere NORMAL_MODE con getState() prima di procedere.
    void initialize();

    // Avvia OFFSET_ACQUISITION_MODE (calibrazione offset accelerometro).
    // Attendere NORMAL_MODE con getState() prima di abilitare gli interrupt.
    void acquireOffset();

    void setAxis(D7SAxisMode mode);
    void setThreshold(D7SThreshold level);

    // Azzera i flag evento nel registro 0x1002 (necessario prima di abilitare interrupt).
    void resetEvents();

    // Pulisce i dati delle ultime misurazioni (latest + ranked).
    void clearAllData();

    D7SState getState();

    bool isEarthquakeOccurring();

    // Legge il registro EVENT 0x1002 (read-clear).
    // Il registro si azzera alla lettura: chiamare UNA SOLA VOLTA per evento INT1,
    // poi interrogare isCollapseEvent() / isShutoffEvent() dalla cache.
    uint8_t readEventRegister();

    // Interrogano _eventCache — NON rileggono il bus.
    bool isCollapseEvent();  // bit1
    bool isShutoffEvent();   // bit0

    // Spettral Intensity delle ultime misurazioni, in cm/s (kine).
    // index 0 = più recente, 4 = più vecchia.
    double getLatestSI(uint8_t index);

    // PGA delle ultime misurazioni, in g.
    double getLatestPGA(uint8_t index);

    // Temperatura interna al momento della misurazione, in °C.
    double getLatestTemperature(uint8_t index);

    // Valori istantanei durante il terremoto (aggiornati ogni 100 ms dal D7S).
    double getInstantaneousSI();

    // PGA istantaneo in g (registro 0x2002, 1 LSB = 0.01 m/s²).
    double getInstantaneousPGA();

private:
    // Indirizzi registro D7S (16-bit: high byte + low byte via I2C)
    static constexpr uint16_t REG_STATE     = 0x1000;
    static constexpr uint16_t REG_AXIS      = 0x1001;
    static constexpr uint16_t REG_EVENT     = 0x1002;
    static constexpr uint16_t REG_MODE      = 0x1003;
    static constexpr uint16_t REG_CTRL      = 0x1004;
    static constexpr uint16_t REG_CLEAR     = 0x1005;
    static constexpr uint16_t REG_INST_SI   = 0x2000;
    static constexpr uint16_t REG_INST_PGA  = 0x2002;
    // Base per i 5 set di misurazioni più recenti; stride = 0x10
    // Offset interni: temperatura=+0x06 (int16), SI=+0x08 (uint16), PGA=+0x0A (uint16)
    static constexpr uint16_t REG_LATEST_BASE = 0x3000;
    static constexpr uint8_t  OFF_TEMP = 0x06;
    static constexpr uint8_t  OFF_SI   = 0x08;
    static constexpr uint8_t  OFF_PGA  = 0x0A;

    // Comandi MODE (scritti in REG_MODE): il byte corrisponde al valore di stato target
    static constexpr uint8_t CMD_INIT     = 0x02;  // → INITIAL_INSTALLATION (stato 0x02)
    static constexpr uint8_t CMD_OFFSET   = 0x03;  // → OFFSET_ACQUISITION   (stato 0x03)
    static constexpr uint8_t CMD_SELFTEST = 0x04;  // → SELFTEST_MODE        (stato 0x04)

    // Comandi CLEAR (scritti in REG_CLEAR)
    static constexpr uint8_t CLEAR_LATEST = 0x01;
    static constexpr uint8_t CLEAR_RANKED = 0x02;
    static constexpr uint8_t CLEAR_ALL    = 0x03;

    static constexpr uint8_t  ADDRESS = 0x55;
    static constexpr uint32_t I2C_TIMEOUT_MS = 50;

    // Scalings raw → unità fisiche (da spec OMRON D7S + derivazione empirica)
    static constexpr double SCALE_SI           = 0.01;    // cm/s per LSB (tutti i reg SI)
    static constexpr double SCALE_PGA_MS2      = 0.001;   // m/s² per LSB (latest, reg 0x300X)
    static constexpr double SCALE_PGA_INST_MS2 = 0.01;    // m/s² per LSB (instantaneous, reg 0x2002)
    static constexpr double G                  = 9.80665;  // m/s² per g
    static constexpr double SCALE_TEMP         = 0.1;     // °C per LSB (signed)

    TwoWire* _wire;
    uint8_t  _eventCache;

    uint8_t  read8bit(uint16_t reg);
    int16_t  read16bitSigned(uint16_t reg);
    uint16_t read16bit(uint16_t reg);
    bool     write8bit(uint16_t reg, uint8_t val);
    bool     writeAddr(uint16_t reg);
};
