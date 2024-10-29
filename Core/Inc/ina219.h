#ifndef INA219_H
#define INA219_H

#include <stm32f0xx_hal.h>
#include <stm32f0xx_hal_i2c.h>

#define INA219_I2C_PORT hi2c1
#define INA219_I2C_ADDR (0x40 << 1)
extern I2C_HandleTypeDef INA219_I2C_PORT;

//#define INA219_BUS_32V_2A
//#define INA219_BUS_32V_1A
#define INA219_BUS_16V_6A
//#define INA219_BUS_16V_400mA

char ina219_Init(void);
void ina219_Calibrate(void);
short ina219_getRawBusVoltage(void);
short ina219_getRawShuntVoltage(void);
short ina219_getRawCurrent(void);
short ina219_getRawPower(void);
float ina219_getBusVoltage(void);
float ina219_getShuntVoltage(void);
float ina219_getCurrent(void);
float ina219_getPower(void);
unsigned char ina219_getFlags(void);

#endif//INA219
