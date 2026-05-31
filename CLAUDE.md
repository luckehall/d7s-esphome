# d7s-esphome — Claude Code context

## Progetto

Libreria Arduino/ESPHome per il sensore sismico OMRON D7S (modulo RAK12027).
Riscritta da zero senza reverse engineering della libreria RAK12027_D7S (bug critici).

Repo: `luckehall/d7s-esphome`
Usata da: `luckehall/sismasens` via `libraries: - "D7S=https://github.com/luckehall/d7s-esphome"`

---

## Struttura

```
src/
  D7S.h       ← dichiarazione classe, enum, costanti registri e scaling
  D7S.cpp     ← implementazione
examples/
  basic_usage/basic_usage.ino
library.properties
```

---

## Mappa registri OMRON D7S

Tutti i registri sono a 16-bit di indirizzo (high byte + low byte via I2C).

| Registro | Indirizzo | Tipo | Note |
|----------|-----------|------|------|
| STATE | 0x1000 | R, 8-bit | 3 LSBs = stato macchina |
| AXIS IN USE | 0x1001 | R, 8-bit | asse attivo |
| EVENT | 0x1002 | R, 8-bit, **read-clear** | bit1=collapse, bit0=shutoff |
| MODE | 0x1003 | W, 8-bit | comandi: scrivi il valore di stato target |
| CTRL | 0x1004 | RW, 8-bit | bit[2:0]=axis, bit3=threshold |
| CLEAR | 0x1005 | W, 8-bit | 0x01=latest, 0x02=ranked, 0x03=all |
| INST SI | 0x2000 | R, 16-bit | SI istantaneo |
| INST PGA | 0x2002 | R, 16-bit | PGA istantaneo, 0.01 m/s²/LSB |
| LATEST BASE | 0x3000 | R, 16-bit | 5 set × stride 0x10 |

LATEST offsets interni: temperatura=+0x06 (int16), SI=+0x08 (uint16), PGA=+0x0A (uint16).

---

## Comandi registro MODE (0x1003)

Il byte scritto corrisponde al valore di stato target (NON un codice separato).

| Costante | Valore | Effetto |
|----------|--------|---------|
| CMD_INIT | 0x02 | Entra in INITIAL_INSTALLATION_MODE |
| CMD_OFFSET | 0x03 | Entra in OFFSET_ACQUISITION_MODE |
| CMD_SELFTEST | 0x04 | Entra in SELFTEST_MODE |

**ATTENZIONE**: CMD_INIT = 0x01 è sbagliato (il D7S lo ignora, rimane in NORMAL_MODE,
nessun orientamento di riferimento acquisito → collapse mai rilevato).

---

## Valori di stato (registro STATE 0x1000)

```cpp
enum D7SState : uint8_t {
    NORMAL_MODE             = 0x00,  // standby
    NORMAL_MODE_NOT_STANDBY = 0x01,  // terremoto in corso
    INITIAL_INSTALLATION    = 0x02,
    OFFSET_ACQUISITION      = 0x03,
    SELFTEST_MODE           = 0x04,
};
```

---

## Scaling unità fisiche

| Costante | Valore | Applicata a |
|----------|--------|-------------|
| SCALE_SI | 0.01 cm/s/LSB | tutti i registri SI (0x2000, 0x3000+) |
| SCALE_PGA_MS2 | 0.001 m/s²/LSB | latest PGA (0x3000+) |
| SCALE_PGA_INST_MS2 | 0.01 m/s²/LSB | instantaneous PGA (0x2002) |
| G | 9.80665 m/s²/g | divisore per conversione m/s² → g |
| SCALE_TEMP | 0.1 °C/LSB | temperatura (signed int16) |

Le funzioni `getLatestPGA()` e `getInstantaneousPGA()` restituiscono **g** direttamente
(conversione m/s² → g avviene dentro la libreria).

---

## I2C — decisioni implementative

- Indirizzo: `0x55`
- `writeAddr()` usa `endTransmission(true)` (STOP + nuovo START), NON `false`.
  Su ESP32 Arduino 3.x (ESP-IDF 5.x) il repeated start genera `ESP_ERR_INVALID_STATE`.
- Timeout su `available()`: 50 ms, poi ritorna 0.
- Nessun `delay()` tra le write.
- `begin()` è un semplice address probe (beginTransmission + endTransmission).
  Wire deve essere inizializzato dal chiamante prima di `begin()`.

---

## Registro EVENT 0x1002 — regola critica

Il registro è **read-clear**: si azzera alla prima lettura.

- Chiamare `readEventRegister()` **una sola volta** per ogni evento INT1.
- Tutti i flag vengono catturati in `_eventCache`.
- `isCollapseEvent()` e `isShutoffEvent()` leggono solo dalla cache (non toccano il bus).
- Chiamare `resetEvents()` prima di abilitare gli interrupt per partire da uno stato pulito.

---

## Sequenza di avvio obbligatoria (per collapse detection)

```
Wire.begin() → d7s.begin(Wire) → d7s.setAxis() → d7s.setThreshold()
→ d7s.initialize() → attendi NORMAL_MODE (max 10 s)
→ d7s.acquireOffset() → attendi NORMAL_MODE (max 10 s)
→ d7s.resetEvents() → abilita interrupt INT1/INT2
```

Se `initialize()` non completa (perché CMD_INIT è sbagliato o I2C fallisce),
il D7S non acquisisce l'orientamento di riferimento e collapse rimane sempre 0.

---

## Note hardware RAK12027 / ESP32

- Il D7S richiede ~1000 ms dopo il rilascio del pin hardware reset (SET) prima
  di essere pronto per I2C. 500 ms non sono sufficienti (I2C INVALID_STATE).
- Framework testato: ESP32 Arduino 3.x con ESP-IDF 5.x.
