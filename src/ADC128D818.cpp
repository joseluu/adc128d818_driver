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

static float identityConversion(float mV) {
  return mV;
}

ADC128D818::ADC128D818(uint8_t i2c_addr,  uint8_t _sda, uint8_t _scl): addr(i2c_addr), sda(_sda), scl(_scl) {

  i2cInitialized=false;
  // enable all channels disabled by default
  disabled_mask = 0;

  ref_v = 2.56f;
  offset_mV = 0.0f;
  ref_mode = INTERNAL_REF;
  op_mode = SINGLE_ENDED_WITH_TEMP;
  conv_mode = CONTINUOUS;

  for (uint8_t i = 0; i < 8; i++) {
    conversion_cb[i] = identityConversion;
  }
}

void ADC128D818::setReference(float ref_voltage) {
  ref_v = ref_voltage;
}

void ADC128D818::setOffset(float mV) {
  offset_mV = mV;
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
  uint8_t statusValue=readCurrentRegister8();

  if (i2cError != 0) {
    Serial.print("I2C read error: ");
    Serial.print(i2cError);
    Serial.println("I2C initialization failure: check port pin and address");
  }
  while ((statusValue & (1 << NOT_READY_BIT)) != 0) {
    Serial.println("waiting for ready bit unset");
    delay(35);
    statusValue=readCurrentRegister8();
  }
  
  Serial.println("device ready");
  // Ensure device is shut down before programming certain registers ...
  setRegister(CONFIG_REG, 0);
  // Reset all
  setRegister(CONFIG_REG, 1<<7);

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
  wire->requestFrom(addr, (uint8_t)2);
  while (wire->available()<2) {
    delay(1);
  }
  
  uint8_t reading[2];
  wire->readBytes(reading, 2);
  uint8_t high_byte = reading[0];
  uint8_t low_byte = reading[1];
  
  uint16_t result = result=high_byte*256 + low_byte;

  return result;

}

float ADC128D818::readMilliVolts(uint8_t channel) {
  return (read(channel)>>4) / 4096.0f * ref_v * 1000.0f + offset_mV;
}

float ADC128D818::readConverted(uint8_t channel) {
  return conversion_cb[channel](readMilliVolts(channel));
}

void ADC128D818::setConversionCallback(uint8_t channel, adc_conversion_cb_t cb) {
  conversion_cb[channel] = (cb == nullptr) ? identityConversion : cb;
}

float ADC128D818::readTemperatureInternal() {
  short raw = read(7)>>7;
  if (raw > 255){
    raw = raw - 512;
  }
  return raw / 2.0f;
}

bool ADC128D818::isActive(){
  return addr != 0;
}

//
// private methods
//

void ADC128D818::ensureI2Cinitialized(){
  if (!i2cInitialized) {
    if (sda == SDA) { // by default the ESP32 display uses Wire, so we use the other I2C bus
        wire=&Wire1;
    } else {
        wire=&Wire;
    }
    wire->begin(sda,scl);
    wire->setClock(100000);
  
    wire->beginTransmission(addr);
    i2cError = wire->endTransmission();
    if (i2cError != 0) {
      Serial.print("I2C initialization error: ");
      Serial.println(i2cError);
    }
    i2cInitialized=true;
  }
}

void ADC128D818::setRegisterAddress(uint8_t reg_addr) {
  ensureI2Cinitialized();
  wire->beginTransmission(addr);
  wire->write(reg_addr);
  i2cError = wire->endTransmission();
}

void ADC128D818::setRegister(uint8_t reg_addr, uint8_t value) {
  ensureI2Cinitialized();
  wire->beginTransmission(addr);
  wire->write(reg_addr);
  wire->write(value);
  i2cError = wire->endTransmission();
}

uint8_t ADC128D818::readCurrentRegister8() {
  ensureI2Cinitialized();
  wire->requestFrom(addr, (uint8_t)1);
  while (!wire->available()) {
    delay(1);
  }
  return wire->read();
}
