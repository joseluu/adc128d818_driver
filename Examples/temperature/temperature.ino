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

// Channel assignment:
//   IN0: PT100 RTD via resistor divider (R1 in series, PT100 to GND)
//   IN1: TMP235AEDBZRQ1 (10 mV/C, 500 mV @ 0 C)
//   IN2: no connection                    -> disabled
//   IN3: MCP9701       (19.5 mV/C, 400 mV @ 0 C)
//   IN4: raw voltage (mV only)
//   IN5: raw voltage (mV only)
//   IN6: no connection                    -> disabled
//   IN7: internal temperature sensor (enabled by SINGLE_ENDED_WITH_TEMP mode)
#define DISABLED_CHANNEL_MASK  ((1<<2)|(1<<6))

void setup() {
  oledSetup();
  Serial.begin(115200);
  pinMode(SYNC_PIN, OUTPUT);
  adc.setReferenceMode(INTERNAL_REF);
  // Calibration against external precision millivoltmeter.
  // Two-point refinement over a wide range:
  //   actual   20 mV -> read   24 mV
  //   actual 2220 mV -> read 2203 mV
  // Composed with the initial correction yields:
  //   effective reference = 2.5479 V
  //   effective offset    = 5.95 mV
  adc.setReference(2.5479);
  adc.setOffset(5.95);
  adc.setDisabledMask(DISABLED_CHANNEL_MASK);
  adc.setConversionCallback(0, pt100Convert);
  adc.setConversionCallback(1, tmp235Convert);
  adc.setConversionCallback(3, mcp9701Convert);
  adc.begin();
}

// --- Channel conversion callbacks (mV -> temperature in C) ---

// PT100 resistor divider: Vdd -- R1 -- IN0 -- PT100 -- GND
// Callendar-Van Dusen, solved for T (from https://aviatechno.net/thermo/rtd03.php)
float pt100Convert(float mV) {
  const float Vdd = 3300;
  const float R1  = 217.5f; // actual series resistor value
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

void loop() {
  digitalWrite(SYNC_PIN, HIGH);

  float mv0 = adc.readMilliVolts(0);
  float pt100Temp   = adc.readConverted(0);
  float mv1 = adc.readMilliVolts(1);
  float tmp235Temp  = adc.readConverted(1);
  float mv3 = adc.readMilliVolts(3);
  float mcp9701Temp = adc.readConverted(3);
  float mv4 = adc.readMilliVolts(4);
  float mv5 = adc.readMilliVolts(5);
  float internalTemp = adc.readTemperatureInternal();

  // --- Serial ---
  char line[48];
  snprintf(line, sizeof(line), "Internal:          %5.1f C", internalTemp);
  Serial.println(line);
  snprintf(line, sizeof(line), "PT100  : %4d mV -> %5.1f C", (int)mv0, pt100Temp);
  Serial.println(line);
  snprintf(line, sizeof(line), "TMP235 : %4d mV -> %5.1f C", (int)mv1, tmp235Temp);
  Serial.println(line);
  snprintf(line, sizeof(line), "MCP9701: %4d mV -> %5.1f C", (int)mv3, mcp9701Temp);
  Serial.println(line);
  snprintf(line, sizeof(line), "CH4    : %4d mV", (int)mv4);
  Serial.println(line);
  snprintf(line, sizeof(line), "CH5    : %4d mV", (int)mv5);
  Serial.println(line);
  Serial.println();

  // --- OLED --- 6 lines at y=0,11,22,33,44,55 (ArialMT_Plain_10)
  char buf[32];
  oled.setColor(BLACK);
  oled.fillRect(0, 0, 128, 64);
  oled.setColor(WHITE);
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);

  snprintf(buf, sizeof(buf), "Internal:     %5.1f C", internalTemp);
  oled.drawString(0, 0, buf);
  snprintf(buf, sizeof(buf), "PT100  %4dmV %5.1fC", (int)mv0, pt100Temp);
  oled.drawString(0, 11, buf);
  snprintf(buf, sizeof(buf), "TMP235 %4dmV %5.1fC", (int)mv1, tmp235Temp);
  oled.drawString(0, 22, buf);
  snprintf(buf, sizeof(buf), "MCP97  %4dmV %5.1fC", (int)mv3, mcp9701Temp);
  oled.drawString(0, 33, buf);
  snprintf(buf, sizeof(buf), "CH4    %4d mV", (int)mv4);
  oled.drawString(0, 44, buf);
  snprintf(buf, sizeof(buf), "CH5    %4d mV", (int)mv5);
  oled.drawString(0, 55, buf);
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
