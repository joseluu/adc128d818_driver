# ADC128D818 driver

Arduino library for the Texas Instruments **ADC128D818** — an 8‑channel,
12‑bit I²C ADC with an internal temperature sensor, an internal 2.56 V
reference, and configurable operation modes.

Originally forked from Bryan Duxbury's driver:
<https://github.com/dslc/adc128d818_driver>. Reworked as an Arduino
library (adds `library.properties`, `src/`, and `Examples/`) and
extended with per‑channel conversion callbacks.

Tested on ESP32 (Heltec WiFi Kit 32). Should work on any Arduino core
that provides `Wire` / `TwoWire`.

## Installation

Clone this repository into your Arduino `libraries/` folder, or use
Arduino IDE's "Add .ZIP Library…" feature. The sketch includes:

```cpp
#include <ADC128D818.h>
```

## Wiring

The ADC128D818 is an I²C device. On ESP32 the example uses the
secondary I²C peripheral (`Wire1`) for the ADC so that the on‑board
OLED can keep using `Wire`. **External I²C pull‑ups on SDA/SCL are
recommended** (typically 4.7 kΩ to 3.3 V); the ESP32's internal
pull‑ups are too weak for reliable operation at 100 kHz on anything
longer than a few centimetres of wire.

## Basic usage

```cpp
#include <Wire.h>
#include <ADC128D818.h>

// address 0x1D, SDA, SCL
ADC128D818 adc(0x1D, SDA, SCL);

void setup() {
  Serial.begin(115200);

  adc.setReferenceMode(INTERNAL_REF);
  adc.setReference(2.56);               // nominal internal reference
  adc.setOperationMode(SINGLE_ENDED_WITH_TEMP);
  adc.setConversionMode(CONTINUOUS);
  adc.begin();
}

void loop() {
  for (int ch = 0; ch < 7; ch++) {
    Serial.print(ch);
    Serial.print(": ");
    Serial.print(adc.readMilliVolts(ch));
    Serial.println(" mV");
  }
  Serial.print("internal temp: ");
  Serial.print(adc.readTemperatureInternal());
  Serial.println(" C");
  delay(500);
}
```

## API

### Configuration

| method | purpose |
|---|---|
| `setReferenceMode(INTERNAL_REF \| EXTERNAL_REF)` | pick internal 2.56 V reference or an external one |
| `setReference(float volts)` | set the reference voltage used for mV conversion |
| `setOffset(float mV)` | additive offset applied to every `readMilliVolts()` — useful for zero‑error calibration |
| `setOperationMode(mode)` | `SINGLE_ENDED_WITH_TEMP`, `SINGLE_ENDED`, `DIFFERENTIAL`, `MIXED` |
| `setConversionMode(mode)` | `LOW_POWER`, `CONTINUOUS`, `ONE_SHOT` |
| `setDisabledMask(uint8_t mask)` | bitmask of channels to disable (bit N disables IN N) |
| `setConversionCallback(ch, fn)` | register a per‑channel mV→value callback (see below) |
| `begin()` | program the configuration registers and start the device |

### Readout

| method | returns |
|---|---|
| `uint16_t read(uint8_t channel)` | raw 12‑bit conversion (in the top 12 bits of a 16‑bit word) |
| `float readMilliVolts(uint8_t channel)` | calibrated voltage in millivolts (`raw → ref → + offset`) |
| `float readConverted(uint8_t channel)` | `readMilliVolts(ch)` passed through the channel's conversion callback |
| `float readTemperatureInternal()` | die temperature in °C from the internal sensor |

### Calibration

Per‑device gain/offset errors can be compensated against an external
precision meter. The example uses a two‑point fit:

```cpp
adc.setReference(2.5479);   // effective reference after gain correction
adc.setOffset(5.95);        // mV added to every readMilliVolts() result
```

## Per‑channel conversion callbacks

Each channel can have a conversion callback attached. A callback
takes the millivolt reading produced by `readMilliVolts()` and returns
whatever derived value makes sense for that channel — typically a
temperature, but it can be any `float`.

```cpp
typedef float (*adc_conversion_cb_t)(float mV);
void setConversionCallback(uint8_t channel, adc_conversion_cb_t cb);
```

- The default callback is the identity function, so a freshly‑created
  instance simply returns the mV value from `readConverted()`.
- Passing `nullptr` resets a channel back to identity.
- Callbacks are plain C function pointers, so no captures — pass in
  any state via globals or `constexpr`.

Example:

```cpp
float tmp235Convert(float mV) { return (mV - 500.0f) / 10.0f; }

adc.setConversionCallback(1, tmp235Convert);
// ...
float tempC = adc.readConverted(1);   // 21.5 etc.
```

## Example: `Examples/temperature`

`Examples/temperature/temperature.ino` is a worked example targeting a
Heltec WiFi Kit 32 with several temperature sensors wired to the ADC
and the result shown on the on‑board SSD1306 OLED plus the Serial
monitor.

Channel assignment:

| channel | sensor                | callback        | displayed |
|---------|-----------------------|-----------------|-----------|
| IN0     | PT100 RTD via divider | `pt100Convert`  | mV + °C  |
| IN1     | TMP235AEDBZRQ1        | `tmp235Convert` | mV + °C  |
| IN2     | *(disabled)*          | —               | —         |
| IN3     | MCP9701               | `mcp9701Convert`| mV + °C  |
| IN4     | raw voltage           | identity        | mV        |
| IN5     | raw voltage           | identity        | mV        |
| IN6     | *(disabled)*          | —               | —         |
| IN7     | internal temp sensor  | —               | °C        |

The three provided callbacks are:

- **`pt100Convert(mV)`** — assumes a divider `Vdd → R1 → IN0 → PT100 → GND`
  with `Vdd = 3300 mV` and `R1 = 217.5 Ω`. Computes the PT100 resistance
  and inverts the Callendar–Van Dusen equation to get the temperature.
- **`tmp235Convert(mV)`** — TI TMP235A: `Vout = 10 mV/°C · T + 500 mV`.
- **`mcp9701Convert(mV)`** — Microchip MCP9701: `Vout = 19.5 mV/°C · T + 400 mV`.

Unused channels are removed from the scan via
`setDisabledMask(...)` to avoid wasting conversion cycles on floating
inputs.

### Building the example

```bash
arduino-cli compile --fqbn esp32:esp32:heltec_wifi_kit_32 \
    --library . Examples/temperature
arduino-cli upload  --fqbn esp32:esp32:heltec_wifi_kit_32 \
    -p COM6 --library . Examples/temperature
```

(Adjust the port and FQBN to match your board.)

## License

MIT for the changes in this fork. The original code was published
without an explicit license.
