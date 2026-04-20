# ADC128D818 driver

Arduino library for the Texas Instruments **ADC128D818** — an 8‑channel,
12‑bit I²C ADC with an internal temperature sensor, an internal 2.56 V
reference, and configurable operation modes.

Originally forked from Bryan Duxbury's driver:
<https://github.com/dslc/adc128d818_driver>. Reworked as an Arduino
library (adds `library.properties`, `src/`, and `examples/`) and
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

## Example: `examples/temperature`

`examples/temperature/temperature.ino` is a worked example targeting a
Heltec WiFi Kit 32 with six temperature sensors plus one raw voltage
probe wired to the ADC. Readings go to both the Serial monitor and
the on‑board SSD1306 OLED. The OLED is 64 px tall and fits six 10‑px
rows, so the eight displayed lines (internal temperature + seven
channels) scroll upward one row every 3 seconds and wrap around — a
full cycle takes ~24 s, and every line is visible at some point.

Channel assignment:

| channel | sensor                              | callback           | displayed |
|---------|-------------------------------------|--------------------|-----------|
| IN0     | PT100 RTD via resistor divider      | `pt100Convert`     | mV + °C   |
| IN1     | TMP235AEDBZRQ1 analog Si sensor     | `tmp235Convert`    | mV + °C   |
| IN2     | MCP9701 analog Si sensor            | `mcp9701Convert`   | mV + °C   |
| IN3     | K‑type thermocouple + x100 amp      | `k100Convert`      | mV + °C   |
| IN4     | raw voltage                         | identity           | mV        |
| IN5     | 100 kΩ NTC B=4250 via divider       | `thermistorConvert`| mV + °C   |
| IN6     | 1N4148 silicon diode                | `diodeConvert`     | mV + °C   |
| IN7     | internal temperature sensor         | —                  | °C        |

### Wiring and equations per probe

All external dividers share the same convention: a pull‑up resistor
from the reference supply down to the ADC input, and the sense element
from that input to ground. The ADC then reads the node voltage.

#### IN0 — PT100 RTD

```
+3.3V ── R1 (217.35 Ω) ── IN0 ── PT100 ── GND
```

Resistance recovered from the divider, then the quadratic
Callendar–Van Dusen equation `R(T) = R0·(1 + A·T + B·T²)` is inverted:

```
R_pt100 = R1 / (Vdd/V_IN0 - 1)
T       = (√(A² - 4B·(1 - R/R0)) - A) / (2B)
          with A = 3.9083e-3, B = -5.775e-7, R0 = 100 Ω
```

The ADC's 2.56 V ceiling caps the useful range at about
+135 °C with `Vdd = 3.3 V`. Below 0 °C the two‑parameter inversion is
approximate (the standard form adds a C·(T-100)·T³ term there);
error is a few °C near -100 °C, negligible near 0 °C.

#### IN1 — TMP235AEDBZRQ1

3‑pin analog sensor, `Vout` wired directly to IN1 (no divider).
Linear: `V = 10 mV/°C · T + 500 mV` → `T = (mV − 500) / 10`.

#### IN2 — MCP9701

3‑pin analog sensor, `Vout` wired directly to IN2 (no divider).
Linear: `V = 19.5 mV/°C · T + 400 mV` → `T = (mV − 400) / 19.5`.

#### IN3 — K‑type thermocouple with x100 amplifier

A K‑type TC (~41 µV/°C) feeds a gain‑100 instrumentation amp whose
output drives IN3. The callback assumes the amplifier outputs 0 mV
at 0 °C (no cold‑junction compensation). `T = mV / 4.1`. If your
amp adds a bias for negative‑temperature coverage (e.g. AD8495),
subtract it inside the callback.

#### IN4 — raw voltage

Direct input, no callback. Useful for sanity‑checking an external
voltage or for adding your own converter later.

#### IN5 — 100 kΩ NTC thermistor, B = 4250

```
+5V ── 100 kΩ ── IN5 ── NTC(100 kΩ @ 25 °C, β=4250) ── GND
```

Recover the NTC resistance, then apply the β equation:

```
R_ntc = V_IN5 · 100 kΩ / (5 V − V_IN5)
1/T   = 1/T0 + (1/β) · ln(R/R0)
        with T0 = 298.15 K, R0 = 100 kΩ, β = 4250
```

At 25 °C the divider outputs ~2.5 V — right at the ADC's 2.56 V
ceiling. **Readings saturate below ~25 °C** with this configuration;
swap the pull‑up supply for 3.3 V or use a larger series resistor if
you need to read colder temperatures.

#### IN6 — 1N4148 silicon diode

```
+5V ── 100 kΩ ── IN6 ── 1N4148 (anode) ── GND (cathode)
```

Full SPICE Shockley equation with temperature‑dependent saturation
current. The external resistor sets the bias current, and `IN6`
reads the forward drop Vd:

```
I      = (V_supply − Vd) / R
I      = Is(T) · (exp(Vd / (n·Vt)) − 1),  Vt = k·T/q
Is(T)  = Is0 · (T/T0)^(Xti/n) · exp((Eg/(n·Vt_per_K)) · (1/T0 − 1/T))
```

Since this is transcendental in T, the callback inverts it with
Newton–Raphson (numerical derivative, converges in ≤5 iterations).
Default SPICE parameters are for a generic 1N4148 model
(`Is0 = 4.352e-9 A, n = 1.906, Eg = 1.11 eV, Xti = 3`). The provided
callback also applies a two‑point linear trim so that the reference
point at 25 °C stays put and a 100 °C heat‑gun check reads 100 °C —
expect a few °C of drift outside that range, and re‑tune the trim (or
`Is0`) to match your specific diode.

### Enabling/disabling channels

Channels are selected at compile time via `ENABLE_CHANNEL_N` defines at
the top of the sketch:

```cpp
#define ENABLE_CHANNEL_0
#define ENABLE_CHANNEL_1
#define ENABLE_CHANNEL_2
#define ENABLE_CHANNEL_3
#define ENABLE_CHANNEL_4
#define ENABLE_CHANNEL_5
#define ENABLE_CHANNEL_6
```

Commenting one out adds it to `setDisabledMask(...)` (so the ADC
doesn't waste conversion cycles on a floating input) *and* drops its
read/Serial/OLED blocks from the build via `#ifdef` guards. The OLED
line list auto‑shrinks; scrolling turns off entirely when the remaining
lines already fit in six rows.

### Building the example

```bash
arduino-cli compile --fqbn esp32:esp32:heltec_wifi_kit_32 \
    --library . examples/temperature
arduino-cli upload  --fqbn esp32:esp32:heltec_wifi_kit_32 \
    -p COM6 --library . examples/temperature
```

(Adjust the port and FQBN to match your board.)

## License

MIT for the changes in this fork. The original code was published
without an explicit license.
