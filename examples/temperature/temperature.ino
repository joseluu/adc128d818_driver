#include <SSD1306Wire.h>
#include <math.h>
#include "Wire.h"
#include "ADC128D818.h"

#define SYNC_PIN 13 // sync signal, to help with a led or scope if needed


// Board must be properly declared in the IDE or CLI parameters
// in order to get the correct symbolic pin assignments
// for instance https://github.com/espressif/arduino-esp32/blob/master/variants/heltec_wifi_kit_32/pins_arduino.h

SSD1306Wire  oled(0x3c, SDA_OLED , SCL_OLED );
ADC128D818 adc(0x1D, SDA , SCL );

// ---------------------------------------------------------------------------
// Channel configuration
//
// Comment out any of the ENABLE_CHANNEL_N macros below to mute that channel.
// Muted channels are added to the disable mask and their read/print/display
// blocks are excluded at compile time.
//
//   IN0: PT100 RTD via resistor divider (R1 in series, PT100 to GND)
//   IN1: TMP235AEDBZRQ1 (10 mV/C, 500 mV @ 0 C)
//   IN2: MCP9701       (19.5 mV/C, 400 mV @ 0 C)
//   IN3: K100 = K-type thermocouple with x100 amplifier (4.1 mV/C, 0 mV @ 0 C)
//   IN4: raw voltage (mV only)
//   IN5: 100 kohm NTC B=4250 via 100 kohm pull-up to +5 V (thermistor divider)
//   IN6: 1N4148 silicon diode via 100 kohm pull-up to +5 V (diode thermometer)
//   IN7: internal temperature sensor (enabled via SINGLE_ENDED_WITH_TEMP)
// ---------------------------------------------------------------------------
#define ENABLE_CHANNEL_0
#define ENABLE_CHANNEL_1
#define ENABLE_CHANNEL_2
#define ENABLE_CHANNEL_3
#define ENABLE_CHANNEL_4
#define ENABLE_CHANNEL_5
#define ENABLE_CHANNEL_6

// Build the disabled-channel mask from the ENABLE_CHANNEL_N defines.
#ifdef ENABLE_CHANNEL_0
  #define CH0_DISABLED_BIT 0
#else
  #define CH0_DISABLED_BIT (1<<0)
#endif
#ifdef ENABLE_CHANNEL_1
  #define CH1_DISABLED_BIT 0
#else
  #define CH1_DISABLED_BIT (1<<1)
#endif
#ifdef ENABLE_CHANNEL_2
  #define CH2_DISABLED_BIT 0
#else
  #define CH2_DISABLED_BIT (1<<2)
#endif
#ifdef ENABLE_CHANNEL_3
  #define CH3_DISABLED_BIT 0
#else
  #define CH3_DISABLED_BIT (1<<3)
#endif
#ifdef ENABLE_CHANNEL_4
  #define CH4_DISABLED_BIT 0
#else
  #define CH4_DISABLED_BIT (1<<4)
#endif
#ifdef ENABLE_CHANNEL_5
  #define CH5_DISABLED_BIT 0
#else
  #define CH5_DISABLED_BIT (1<<5)
#endif
#ifdef ENABLE_CHANNEL_6
  #define CH6_DISABLED_BIT 0
#else
  #define CH6_DISABLED_BIT (1<<6)
#endif
#define DISABLED_CHANNEL_MASK (CH0_DISABLED_BIT | CH1_DISABLED_BIT | \
                               CH2_DISABLED_BIT | CH3_DISABLED_BIT | \
                               CH4_DISABLED_BIT | CH5_DISABLED_BIT | \
                               CH6_DISABLED_BIT)

// --- Channel conversion callbacks ---

// PT100 resistor divider: Vdd -- R1 -- IN0 -- PT100 -- GND
// Callendar-Van Dusen, solved for T (from https://aviatechno.net/thermo/rtd03.php)
float pt100Convert(float mV) {
  const float Vdd = 3300;
  const float R1  = 217.35f; // actual series resistor value
  const float R0  = 100;    // pt100 !
  const float A   = 3.9083E-3;
  const float B   = -5.775E-7;
  float resistance = R1 / (Vdd / mV - 1);
  return (sqrt(A*A - 4*B*(1 - resistance/R0)) - A) / (2*B);
}

// TMP235A: Vout = 10 mV/C * T + 500 mV  =>  T = (mV - 500) / 10
float tmp235Convert(float mV) {
  return (mV - 500.0f) / 10.0f;
}

// MCP9701: Vout = 19.5 mV/C * T + 400 mV  =>  T = (mV - 400) / 19.5
float mcp9701Convert(float mV) {
  return (mV - 400.0f) / 19.5f;
}

// K-type thermocouple + x100 amplifier.
// K-type sensitivity ~41 uV/C => after x100: 4.1 mV/C.
// Assumes the amplifier outputs 0 mV at 0 C (no cold-junction compensation done here).
float k100Convert(float mV) {
  return mV / 4.1f;
}

// 1N4148 silicon diode thermometer.
// Wiring: +5V -- 100 kohm -- IN6 -- anode/cathode -- GND, so V_IN6 = Vd (forward drop).
// SPICE Shockley with temperature-dependent Is:
//   I  = Is(T) * (exp(Vd / (n*Vt)) - 1),  Vt = k*T/q
//   Is(T) = Is0 * (T/T0)^(Xti/n) * exp( (Eg/(n*Vt_per_K)) * (1/T0 - 1/T) )
// Current is set by the external resistor: I = (Vsupply - Vd) / R.
// The equation is transcendental in T, so we invert with Newton-Raphson
// (numerical derivative, converges in a handful of iterations).
// SPICE params below are typical for a generic 1N4148 model; for absolute
// accuracy, calibrate at one known temperature and tweak Is0.
float diodeConvert(float mV) {
  const float Vsupply  = 5.0f;
  const float R        = 100000.0f;
  const float Is0      = 4.352e-9f;      // A at T0
  const float n        = 1.906f;         // ideality factor
  const float Eg       = 1.11f;          // eV (silicon bandgap)
  const float Xti      = 3.0f;           // Is temperature exponent
  const float T0       = 298.15f;        // 25 C reference
  const float kq       = 8.617333e-5f;   // k/q, V/K
  const float EgScale  = Eg / (n * kq);  // appears in Is(T)

  float Vd = mV * 1e-3f;
  if (Vd <= 0.0f || Vd >= Vsupply) return NAN;
  float I = (Vsupply - Vd) / R;
  if (I <= 0.0f) return NAN;

  float T = T0;
  for (int iter = 0; iter < 30; iter++) {
    float IsT   = Is0 * powf(T  / T0, Xti / n) * expf(EgScale * (1.0f/T0 - 1.0f/T));
    float fT    = Vd - n * kq * T * logf(I / IsT);
    float Tp    = T + 0.1f;
    float IsTp  = Is0 * powf(Tp / T0, Xti / n) * expf(EgScale * (1.0f/T0 - 1.0f/Tp));
    float fTp   = Vd - n * kq * Tp * logf(I / IsTp);
    float slope = (fTp - fT) / 0.1f;
    if (fabsf(slope) < 1e-10f) break;
    float step  = fT / slope;
    T -= step;
    if (T < 100.0f) T = 100.0f;
    if (T > 600.0f) T = 600.0f;
    if (fabsf(step) < 0.01f) break;
  }
  float Traw_C = T - 273.15f;

  // Two-point heat-gun calibration.
  //   actual  25 C -> read  25 C   (anchor, OK)
  //   actual 100 C -> read 120 C   (slope too steep)
  // Linear rescale around the 25 C anchor.
  const float T_ANCHOR = 25.0f;
  const float T_HOT_TRUE = 100.0f;
  const float T_HOT_READ = 120.0f;
  return T_ANCHOR + (Traw_C - T_ANCHOR) * (T_HOT_TRUE - T_ANCHOR) / (T_HOT_READ - T_ANCHOR);
}

// 100 kohm NTC, B=4250, wired as divider: +5V -- 100 kohm -- IN5 -- NTC -- GND
//   V_IN5 = 5000 mV * Rntc / (100 k + Rntc)  =>  Rntc = V_IN5 * 100 k / (5000 - V_IN5)
// Beta equation: 1/T = 1/T0 + (1/B) * ln(R/R0), with T0 = 298.15 K, R0 = 100 kohm.
// NOTE: ADC full-scale is ~2.56 V, so readings saturate below ~25 C.
float thermistorConvert(float mV) {
  const float Vsupply = 5000.0f;
  const float Rseries = 100000.0f;
  const float R0      = 100000.0f;
  const float Beta    = 4250.0f;
  const float T0_K    = 298.15f;
  float Rntc = mV * Rseries / (Vsupply - mV);
  float invT = 1.0f / T0_K + logf(Rntc / R0) / Beta;
  return 1.0f / invT - 273.15f;
}

void setup() {
  oledSetup();
  Serial.begin(115200);
  pinMode(SYNC_PIN, OUTPUT);
  adc.setReferenceMode(INTERNAL_REF);
  // Reference kept from prior two-point calibration; offset bumped so that
  // a true 0 mV input reads 20 mV (per-device zero-error adjustment).
  adc.setReference(2.5479);
  adc.setOffset(20.0);
  adc.setDisabledMask(DISABLED_CHANNEL_MASK);
#ifdef ENABLE_CHANNEL_0
  adc.setConversionCallback(0, pt100Convert);
#endif
#ifdef ENABLE_CHANNEL_1
  adc.setConversionCallback(1, tmp235Convert);
#endif
#ifdef ENABLE_CHANNEL_2
  adc.setConversionCallback(2, mcp9701Convert);
#endif
#ifdef ENABLE_CHANNEL_3
  adc.setConversionCallback(3, k100Convert);
#endif
  // IN4 left on identity (raw mV).
#ifdef ENABLE_CHANNEL_5
  adc.setConversionCallback(5, thermistorConvert);
#endif
#ifdef ENABLE_CHANNEL_6
  adc.setConversionCallback(6, diodeConvert);
#endif
  adc.begin();
}

void loop() {
  digitalWrite(SYNC_PIN, HIGH);

  float internalTemp = adc.readTemperatureInternal();
#ifdef ENABLE_CHANNEL_0
  float mv0        = adc.readMilliVolts(0);
  float pt100Temp  = adc.readConverted(0);
#endif
#ifdef ENABLE_CHANNEL_1
  float mv1        = adc.readMilliVolts(1);
  float tmp235Temp = adc.readConverted(1);
#endif
#ifdef ENABLE_CHANNEL_2
  float mv2          = adc.readMilliVolts(2);
  float mcp9701Temp  = adc.readConverted(2);
#endif
#ifdef ENABLE_CHANNEL_3
  float mv3       = adc.readMilliVolts(3);
  float k100Temp  = adc.readConverted(3);
#endif
#ifdef ENABLE_CHANNEL_4
  float mv4 = adc.readMilliVolts(4);
#endif
#ifdef ENABLE_CHANNEL_5
  float mv5        = adc.readMilliVolts(5);
  float ntcTemp    = adc.readConverted(5);
#endif
#ifdef ENABLE_CHANNEL_6
  float mv6        = adc.readMilliVolts(6);
  float diodeTemp  = adc.readConverted(6);
#endif

  // --- Serial ---
  char line[48];
  snprintf(line, sizeof(line), "Internal:          %5.1f C", internalTemp);
  Serial.println(line);
#ifdef ENABLE_CHANNEL_0
  snprintf(line, sizeof(line), "PT100  : %4d mV -> %5.1f C", (int)mv0, pt100Temp);
  Serial.println(line);
#endif
#ifdef ENABLE_CHANNEL_1
  snprintf(line, sizeof(line), "TMP235 : %4d mV -> %5.1f C", (int)mv1, tmp235Temp);
  Serial.println(line);
#endif
#ifdef ENABLE_CHANNEL_2
  snprintf(line, sizeof(line), "MCP9701: %4d mV -> %5.1f C", (int)mv2, mcp9701Temp);
  Serial.println(line);
#endif
#ifdef ENABLE_CHANNEL_3
  snprintf(line, sizeof(line), "K100   : %4d mV -> %5.1f C", (int)mv3, k100Temp);
  Serial.println(line);
#endif
#ifdef ENABLE_CHANNEL_4
  snprintf(line, sizeof(line), "Raw    : %4d mV", (int)mv4);
  Serial.println(line);
#endif
#ifdef ENABLE_CHANNEL_5
  snprintf(line, sizeof(line), "NTC100k: %4d mV -> %5.1f C", (int)mv5, ntcTemp);
  Serial.println(line);
#endif
#ifdef ENABLE_CHANNEL_6
  snprintf(line, sizeof(line), "Diode  : %4d mV -> %5.1f C", (int)mv6, diodeTemp);
  Serial.println(line);
#endif
  Serial.println();

  // --- OLED --- build a list of lines, then render a 6-row window that
  // scrolls upward one row every 3 seconds and wraps around, so every line
  // is reachable on a period of 3*nLines seconds.
  const uint8_t VISIBLE = 6;
  const uint8_t MAX_LINES = 10;
  static char buf[MAX_LINES][24];
  uint8_t nLines = 0;
  snprintf(buf[nLines++], 24, "Internal:     %5.1fC", internalTemp);
#ifdef ENABLE_CHANNEL_0
  snprintf(buf[nLines++], 24, "PT100  %4dmV %5.1fC", (int)mv0, pt100Temp);
#endif
#ifdef ENABLE_CHANNEL_1
  snprintf(buf[nLines++], 24, "TMP235 %4dmV %5.1fC", (int)mv1, tmp235Temp);
#endif
#ifdef ENABLE_CHANNEL_2
  snprintf(buf[nLines++], 24, "MCP97  %4dmV %5.1fC", (int)mv2, mcp9701Temp);
#endif
#ifdef ENABLE_CHANNEL_3
  snprintf(buf[nLines++], 24, "K100   %4dmV %5.1fC", (int)mv3, k100Temp);
#endif
#ifdef ENABLE_CHANNEL_4
  snprintf(buf[nLines++], 24, "Raw    %4d mV", (int)mv4);
#endif
#ifdef ENABLE_CHANNEL_5
  snprintf(buf[nLines++], 24, "NTC    %4dmV %5.1fC", (int)mv5, ntcTemp);
#endif
#ifdef ENABLE_CHANNEL_6
  snprintf(buf[nLines++], 24, "Diode  %4dmV %5.1fC", (int)mv6, diodeTemp);
#endif

  static uint32_t lastScrollMs = 0;
  static uint8_t scrollOffset = 0;
  uint32_t now = millis();
  if (now - lastScrollMs >= 3000) {
    lastScrollMs = now;
    if (nLines > VISIBLE) {
      scrollOffset = (scrollOffset + 1) % nLines;
    } else {
      scrollOffset = 0;
    }
  }
  if (nLines == 0 || scrollOffset >= nLines) scrollOffset = 0;

  oled.setColor(BLACK);
  oled.fillRect(0, 0, 128, 64);
  oled.setColor(WHITE);
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  uint8_t y = 0;
  uint8_t shown = (nLines < VISIBLE) ? nLines : VISIBLE;
  for (uint8_t i = 0; i < shown; i++) {
    uint8_t idx = (scrollOffset + i) % nLines;
    oled.drawString(0, y, buf[idx]);
    y += 11;
  }
  oled.display();

  digitalWrite(SYNC_PIN, LOW); // end of read
  delay(500);
}

void oledSetup(void) {
  // reset OLED
  pinMode(16,OUTPUT);
  digitalWrite(16,LOW);
  delay(50);
  digitalWrite(16,HIGH);

  oled.init();
  oled.clear();
  oled.flipScreenVertically();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0, 0, "ADC128D818 temps");
  oled.display();
}
