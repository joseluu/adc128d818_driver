#include "Wire.h"
#include "ADC128D818.h"

ADC128D818 adc(0x1D);

void setup() {
  Wire.begin();
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  adc.setReferenceMode(INTERNAL_REF);
  adc.setReference(2.56);
  adc.begin();
}

float mcp9701Convert(float measurement){
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
    Serial.print(": ");
    Serial.print(mcp9701Convert(value));
    Serial.println();
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
