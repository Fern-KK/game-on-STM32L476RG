/*
 * sx1509.h
 *
 *  Created on: Jul 9, 2024
 *      Author: krzysztofc
 */

#ifndef INC_SX1509_H_
#define INC_SX1509_H_

#include <stddef.h>
#include <stdint.h>
#include <_ansi.h>

_BEGIN_STD_C


#include "stm32l4xx_hal.h"


#define I2C_ERROR_OK 0


#define i2CTimeout 1000 // Timeout for I2C receive
#define SXAddress (0x3E << 1)
#define INAAddress (0x40 << 1)





void reset(uint8_t hardware);
uint8_t readByte(uint8_t reg);
uint16_t readWord(uint8_t reg);

_END_STD_C

#endif /* INC_SX1509_H_ */
