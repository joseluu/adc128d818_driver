#include "Wire.h"
#include "ADC128D818.h"

#define CONFIG_REG    0x00
#define CONV_RATE_REG 0x07
#define CHANNEL_DISABLE_REG 0x08
#define ADV_CONFIG_REG 0x0B
#define BUSY_STATUS_REG 0x0C

#define READ_REG_BASE 0x20
#define LIMIT_REG_BASE 0x2A

#define TEMP_REGISTER 0x27

#define START_BIT 0
#define INIT_BIT 7

#define EXT_REF_ENABLE 0
#define MODE_SELECT_1 1
#define MODE_SELECT_2 2

#define BUSY_BIT 0
#define NOT_READY_BIT 1

ADC128D818::ADC128D818(uint8_t address) {
  this->addr = address;

  // enable all channels disabled by default
  disabled_mask = 0;

  ref_v = 2.56f;
  ref_mode = INTERNAL_REF;
  op_mode = SINGLE_ENDED_WITH_TEMP;
  conv_mode = CONTINUOUS;
}

void ADC128D818::setReference(float ref_voltage) {
  ref_v = ref_voltage;
}

void ADC128D818::setReferenceMode(reference_mode_t mode) {
  ref_mode = mode;
}

void ADC128D818::setOperationMode(operation_mode_t mode) {
  op_mode = mode;
}

void ADC128D818::setConversionMode(conv_mode_t mode) {
  conv_mode = mode;
}

void ADC128D818::setDisabledMask(uint8_t disabled_mask) {
  this->disabled_mask = disabled_mask;
}

void ADC128D818::begin() {
  // read busy reg until it returns 0
  setRegisterAddress(BUSY_STATUS_REG);
  while (1) {
    Serial.println("waiting for ready bit unset");
    if ((readCurrentRegister8() & (1 << NOT_READY_BIT)) == 0) {
      break;
    }
    delay(35);
  }
  
  Serial.println("ready!");
  // Ensure device is shut down before programming certain registers ...
  setRegister(CONFIG_REG, 0);

  delay(100);

  // program advanced config reg
  setRegister(ADV_CONFIG_REG, ref_mode | (op_mode << 1));

  // program conversion rate reg
  setRegister(CONV_RATE_REG, conv_mode);

  // program enabled channels
  setRegister(CHANNEL_DISABLE_REG, disabled_mask);

  // program limit regs
  // currently noop!

  // set start bit in configuration (interrupts disabled)
  setRegister(CONFIG_REG, 1);
}

uint8_t ADC128D818::conversions_done(void) {
  setRegisterAddress(BUSY_STATUS_REG);
  uint8_t status = readCurrentRegister8();
  if (status & (1 << BUSY_BIT)) {
    return false;
  }
  return true;
}

uint16_t ADC128D818::read(uint8_t channel) {
  setRegisterAddress(READ_REG_BASE + channel);
  Wire.requestFrom(addr, (uint8_t)2);
  while (Wire.available()<2) {
    delay(1);
  }
  
  uint8_t reading[2];
  Wire.readBytes(reading, 2);
  uint8_t high_byte = reading[0];
  uint8_t low_byte = reading[1];
  
  uint16_t result = result=high_byte*256 + low_byte;

  return result;

}

float ADC128D818::readConverted(uint8_t channel) {
  return (read(channel)>>4) / 4096.0f * ref_v;
}

float ADC128D818::readTemperatureConverted() {
  short raw = read(7)>>7;
  if (raw > 255){
    raw = raw - 512;
  }
  return raw / 2.0f;
}


//
// private methods
//

void ADC128D818::setRegisterAddress(uint8_t reg_addr) {
  Wire.beginTransmission(addr);
  Wire.write(reg_addr);
  Wire.endTransmission();
}

void ADC128D818::setRegister(uint8_t reg_addr, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg_addr);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t ADC128D818::readCurrentRegister8() {
  Wire.requestFrom(addr, (uint8_t)1);
  while (!Wire.available()) {
    delay(1);
  }
  return Wire.read();
}
