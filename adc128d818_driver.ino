#include <SSD1306Wire.h>
#include <math.h> 
#include "Wire.h"
#include "ADC128D818.h"


// HelTec ESP32 DevKit parameters
#define D3 4
#define D5 15
SSD1306Wire  oled(0x3c, D3, D5);
ADC128D818 adc(0x1D, 21, 22);

void setup() {
  oledSetup();
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  adc.setReferenceMode(INTERNAL_REF);
  adc.setReference(2.553); // device under test is slightly off
  adc.begin();
}

float pt100Resistance(float mV){
  const float Vdd=3300;
  const float R1=217.5f; // vallue of actual resistor

  float rMeasured = R1/(Vdd/mV -1);
  return rMeasured;
}
float pt100Convert(float resistance){
  // from: https://aviatechno.net/thermo/rtd03.php
  const float R0=100; // pt100 !
  const float alpha=0.003850;
  const float A=3.9083E-3;
  const float B=-5.775E-7;
  const float C=-4.18301E-12;

  float temp = (sqrt(A*A-4*B*(1-resistance/R0))-A)/(2*B);
  return temp;
}
float mcp9700Convert(float measurement){ // or any sensor with slope 10mV/dC like TMP235-Q1
  float refTemp=13.0;
  float refV=0.6;
  float slope=10;

  float temp;
  temp = refTemp +  (measurement - refV)/slope ;
  return temp;
}

float mcp9701Convert(float measurement){ // or any sensor with slope 19.5mV/dC like TMP236-Q1
  float refTemp=13.0;
  float refV=0.6;
  float slope=19.5;

  float temp;
  temp = refTemp +  (measurement - refV)/slope ;
  return temp;
}

void loop() {
  // IN0-IN6 ...
  for (int i = 0; i < 7; i++) {
    float value=adc.readConverted(i);
    if (i==0) {
      displayVoltage(value);
      displayTemperature(pt100Convert( pt100Resistance(value)));
    }
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

void displayTemperature(short temp) {
  char tempStr[30];

  sprintf(tempStr,"%3hd C",temp);
  displayAt(100,1,40,tempStr);
}

void displayVoltage(short voltage) {
  char voltageStr[30];

  sprintf(voltageStr,"%4hd mV",voltage);
  displayAt(45,1,40,voltageStr);
}

void displayRaw(unsigned short raw) {
  char rawStr[30];

  sprintf(rawStr,"0x%04hx",raw);
  displayAt(2,1,40,rawStr);
}

void displayAt(unsigned short x, unsigned short y, unsigned short xSpan, char * text){
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);

  oled.setColor(BLACK);
  oled.fillRect(x,10,xSpan,10);
  oled.setColor(WHITE);
  oled.drawString(x,10,text);
  oled.display();
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
  oled.drawString(0 , 0, "START" );
  oled.display();
}
