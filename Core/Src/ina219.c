#include "ina219.h"

#define INA219_REG_CONFIG                       0x00
#define INA219_REG_SHUNTVOLTAGE                 0x01
#define INA219_REG_BUSVOLTAGE                   0x02
#define INA219_REG_POWER                        0x03
#define INA219_REG_CURRENT                      0x04
#define INA219_REG_CALIBRATION                  0x05

#define INA219_CONFIG_RESET                     0x8000
#define INA219_CONFIG_BVOLTAGERANGE_16V         0x0000  // 0-16V Range
#define INA219_CONFIG_BVOLTAGERANGE_32V         0x2000  // 0-32V Range
#define INA219_CONFIG_GAIN_1_40MV               0x0000  // Gain 1, 40mV Range
#define INA219_CONFIG_GAIN_2_80MV               0x0800  // Gain 2, 80mV Range
#define INA219_CONFIG_GAIN_4_160MV              0x1000  // Gain 4, 160mV Range
#define INA219_CONFIG_GAIN_8_320MV              0x1800  // Gain 8, 320mV Range
#define INA219_CONFIG_BADCRES_9BIT              0x0000  // 9-bit bus res = 0..511
#define INA219_CONFIG_BADCRES_10BIT             0x0080  // 10-bit bus res = 0..1023
#define INA219_CONFIG_BADCRES_11BIT             0x0100  // 11-bit bus res = 0..2047
#define INA219_CONFIG_BADCRES_12BIT             0x0180  // 12-bit bus res = 0..4097
#define INA219_CONFIG_SADCRES_9BIT_1S_84US      0x0000  // 1 x 9-bit shunt sample
#define INA219_CONFIG_SADCRES_10BIT_1S_148US    0x0008  // 1 x 10-bit shunt sample
#define INA219_CONFIG_SADCRES_11BIT_1S_276US    0x0010  // 1 x 11-bit shunt sample
#define INA219_CONFIG_SADCRES_12BIT_1S_532US    0x0018  // 1 x 12-bit shunt sample
#define INA219_CONFIG_SADCRES_12BIT_2S_1060US   0x0048	 // 2 x 12-bit shunt samples averaged together
#define INA219_CONFIG_SADCRES_12BIT_4S_2130US   0x0050  // 4 x 12-bit shunt samples averaged together
#define INA219_CONFIG_SADCRES_12BIT_8S_4260US   0x0058  // 8 x 12-bit shunt samples averaged together
#define INA219_CONFIG_SADCRES_12BIT_16S_8510US  0x0060  // 16 x 12-bit shunt samples averaged together
#define INA219_CONFIG_SADCRES_12BIT_32S_17MS    0x0068  // 32 x 12-bit shunt samples averaged together
#define INA219_CONFIG_SADCRES_12BIT_64S_34MS    0x0070  // 64 x 12-bit shunt samples averaged together
#define INA219_CONFIG_SADCRES_12BIT_128S_69MS   0x0078  // 128 x 12-bit shunt samples averaged together
#define INA219_CONFIG_MODE_POWERDOWN            0x0000
#define INA219_CONFIG_MODE_SVOLT_TRIGGERED      0x0001
#define INA219_CONFIG_MODE_BVOLT_TRIGGERED      0x0002
#define INA219_CONFIG_MODE_SANDBVOLT_TRIGGERED  0x0003
#define INA219_CONFIG_MODE_ADCOFF               0x0004
#define INA219_CONFIG_MODE_SVOLT_CONTINUOUS     0x0005
#define INA219_CONFIG_MODE_BVOLT_CONTINUOUS     0x0006
#define INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS 0x0007

static unsigned int _calibration = 0;
static float _current_divider = 10.0f;
static float _power_multiplier = 0;

void i2c_write(unsigned char reg, unsigned short input) {
	unsigned char value[2] = {(input >> 8) & 0xff, (input & 0xff)};
	__disable_irq();
	HAL_I2C_Mem_Write(&INA219_I2C_PORT, INA219_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, value, 2, 5000);
	__enable_irq();
}

short i2c_read(unsigned char reg) {
	unsigned char value[3] = {0};
	__disable_irq();
	unsigned char result = HAL_I2C_Mem_Read(&INA219_I2C_PORT, INA219_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, value, 3, 5000);
	__enable_irq();
	if (result != HAL_OK) {
		return result;
	} else {
		return (value[0] << 8) | value[1];
	}
}

char ina219_Init(void) {
	if (HAL_I2C_IsDeviceReady(&INA219_I2C_PORT, INA219_I2C_ADDR, 3, 1000) != HAL_OK) {
		return 0;
	}
	i2c_write(INA219_REG_CONFIG, INA219_CONFIG_RESET);
	HAL_Delay(1);
	return 1;
}

void ina219_Calibrate(void) {
	unsigned short config = INA219_CONFIG_BADCRES_12BIT | INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
#ifdef INA219_BUS_32V_2A
	_calibration = 4096;
	_current_divider = 10.0f;
	_power_multiplier = 2;
	config |= INA219_CONFIG_BVOLTAGERANGE_32V | INA219_CONFIG_GAIN_8_320MV | INA219_CONFIG_SADCRES_12BIT_1S_532US;
#elif defined(INA219_BUS_32V_1A)
	_calibration = 10240;
	_current_divider = 25.0f;
	_power_multiplier = 0.8f;
	config |= INA219_CONFIG_BVOLTAGERANGE_32V | INA219_CONFIG_GAIN_8_320MV | INA219_CONFIG_SADCRES_12BIT_1S_532US;
#elif defined(INA219_BUS_16V_6A)
	_calibration = 4096;
//	_current_divider = 5;
	_current_divider = 5.0f;
	_power_multiplier = 4;
	config |= INA219_CONFIG_BVOLTAGERANGE_16V | INA219_CONFIG_GAIN_8_320MV | INA219_CONFIG_SADCRES_12BIT_1S_532US;
#elif defined(INA219_BUS_16V_400mA)
	_calibration = 8192;
	_current_divider = 20.0f;
	_power_multiplier = 1.0f;
	config |= INA219_CONFIG_BVOLTAGERANGE_16V | INA219_CONFIG_GAIN_1_40MV | INA219_CONFIG_SADCRES_12BIT_128S_69MS;
#else
#error "Choose bus voltage"
#endif
	i2c_write(INA219_REG_CALIBRATION, _calibration);
	i2c_write(INA219_REG_CONFIG, config);
}

short ina219_getRawBusVoltage(void) {
	short value = i2c_read(INA219_REG_BUSVOLTAGE);
	return (int16_t)((value >> 3) * 4);
}

short ina219_getRawShuntVoltage(void) {
	return i2c_read(INA219_REG_SHUNTVOLTAGE);
}

short ina219_getRawCurrent(void) {
	// Just in case
//	i2c_write(INA219_REG_CALIBRATION, _calibration);
	return i2c_read(INA219_REG_CURRENT);
}

short ina219_getRawPower(void) {
	// Just in case
//	i2c_write(INA219_REG_CALIBRATION, _calibration);
	return i2c_read(INA219_REG_POWER);
}

float ina219_getBusVoltage(void) {
	short value = ina219_getRawBusVoltage();
	return value * 0.001f;
}

float ina219_getShuntVoltage(void) {
	short value = ina219_getRawShuntVoltage();
	return value * 0.01f;
}

float ina219_getCurrent(void) {
	float value = ina219_getRawCurrent();
	value /= _current_divider;
	return value;
}

float ina219_getPower(void) {
	float value = ina219_getRawPower();
	value *= _power_multiplier;
	return value;
}

unsigned char ina219_getFlags(void) {
	short value = i2c_read(INA219_REG_BUSVOLTAGE);
	return value & 0x03;
}
