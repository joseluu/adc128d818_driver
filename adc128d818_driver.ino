#include "Wire.h"
#include "ADC128D818.h"

ADC128D818 adc(0x1D, 21, 22);

void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  adc.setReferenceMode(INTERNAL_REF);
  adc.setReference(2.56);
  adc.begin();
}

float mcp9700Convert(float measurement){ // or any sensor with slope 10mV/dC like TMP235-Q1
  float refTemp=13.0;
  float refV=0.6;
  float slope=0.010;

  float temp;
  temp = refTemp +  (measurement - refV)/slope ;
  return temp;
}

float mcp9701Convert(float measurement){ // or any sensor with slope 19.5mV/dC like TMP236-Q1
  float refTemp=13.0;
  float refV=0.6;
  float slope=0.0195;

  float temp;
  temp = refTemp +  (measurement - refV)/slope ;
  return temp;
}

void loop() {
  // IN0-IN6 ...
  for (int i = 0; i < 7; i++) {
    float value=adc.readConverted(i);
    Serial.print(i);
    Serial.print(": ");
    Serial.print(value);
    Serial.print("V/ ");
    Serial.print(mcp9700Convert(value));
    Serial.print(" deg C/ ");
    Serial.print(mcp9701Convert(value));
    Serial.println(" deg C");
  }
  // ... and the internal temp sensor
  Serial.print("Temp: ");
  Serial.print(adc.readTemperatureConverted());
  Serial.println(" deg C");

  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);
  delay(500);
}
