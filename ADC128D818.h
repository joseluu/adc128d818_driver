#ifndef __ADC128D818_H__
#define __ADC128D818_H__

#include "Arduino.h"

enum reference_mode_t {
  INTERNAL_REF = 0, EXTERNAL_REF = 1
};

enum conv_mode_t {
  LOW_POWER, CONTINUOUS, ONE_SHOT
};

enum operation_mode_t {
  SINGLE_ENDED_WITH_TEMP = 0,
  SINGLE_ENDED = 1,
  DIFFERENTIAL = 2,
  MIXED = 3
};

class ADC128D818 {
public:
  ADC128D818(uint8_t address,  uint8_t _sda, uint8_t _scl);
  
  void setReference(float ref_voltage);
  void setReferenceMode(reference_mode_t mode);
  void setOperationMode(operation_mode_t mode);
  void setDisabledMask(uint8_t disabled_mask);
  void setConversionMode(conv_mode_t mode);
  void begin(void);  
  uint8_t conversions_done(void);
  uint16_t read(uint8_t channel);
  float readConverted(uint8_t channel);
  float readTemperatureConverted(void);
  
  bool isActive();
      

private:
  uint8_t disabled_mask;
  float ref_v;

  TwoWire *wire;
  bool i2cInitialized;
  uint8_t addr;
  uint8_t sda;
  uint8_t scl;
  unsigned short i2cError;
      
  reference_mode_t ref_mode;
  operation_mode_t op_mode;
  conv_mode_t conv_mode;

  void initI2c();
  void setRegisterAddress(uint8_t reg_addr);
  void setRegister(uint8_t reg_addr, uint8_t value);
  uint8_t readCurrentRegister8();
};

#endif
