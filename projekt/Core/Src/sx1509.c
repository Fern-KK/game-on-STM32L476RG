/*
 * sx1509.c
 *
 *  Created on: Jul 9, 2024
 *      Author: krzysztofc
 */

#include "main.h"
#include "sx1509_registers.h"
#include "sx1509.h"

static void delayNOP(uint32_t numberNOP)
{
  uint32_t i = 0;

  for (i = 0; i < numberNOP; i++)
    __NOP();

  return;
} // end void delayNOP(uint32_t numberNOP)


uint8_t readByte(uint8_t reg)
{
	HAL_StatusTypeDef ret;
	uint8_t buf[1] = {reg};
	ret = HAL_I2C_Master_Transmit(&SX1509_I2C_PORT, SXAddress, buf, 1, i2CTimeout);
	if ( ret == HAL_OK )
	{
		ret = HAL_I2C_Master_Receive(&SX1509_I2C_PORT, SXAddress, buf, 1, 1000);
		if ( ret == HAL_OK )
		{
			return buf[0];
		}
	}
	return 0xFF;
}

uint16_t readWord(uint8_t reg)
{
	HAL_StatusTypeDef ret;
	uint8_t buf[2] = {reg};
	ret = HAL_I2C_Master_Transmit(&SX1509_I2C_PORT, SXAddress, buf, 1, i2CTimeout);
	if ( ret == HAL_OK )
	{
		ret = HAL_I2C_Master_Receive(&SX1509_I2C_PORT, SXAddress, buf, 2, i2CTimeout);
		if ( ret == HAL_OK )
		{
			uint16_t tmp = buf[0];
			tmp <<= 8;
			tmp |= buf[1];
			return tmp;
		}
	}
	return 0xFFFF;
}

void reset(uint8_t hardware)
{
	HAL_StatusTypeDef ret;
	// if hardware bool is set
	if (hardware != 0)
	{
		// Check if bit 2 of REG_MISC is set
		// if so nReset will not issue a POR, we'll need to clear that bit first
		uint8_t regMisc = readByte(REG_MISC);
		if (regMisc & (1 << 2))
		{
			uint8_t buf[2] = {REG_MISC, 0};
			regMisc &= ~(1 << 2);
			buf[1] = regMisc;
			ret = HAL_I2C_Master_Transmit(&SX1509_I2C_PORT, SXAddress, buf, 2, i2CTimeout);
		}
		// Reset the SX1509, the pin is active low
		HAL_GPIO_WritePin(SX1509_nRST_PORT, SX1509_nRST_Pin, GPIO_PIN_RESET); // pull reset pin low
		delayNOP(25);					  // Wait for the pin to settle
		HAL_GPIO_WritePin(SX1509_nRST_PORT, SX1509_nRST_Pin, GPIO_PIN_SET);// pull reset pin back high
	}
	else
	{
		// Software reset command sequence:
		uint8_t buf[2] = {REG_RESET, 0x12};
		ret = HAL_I2C_Master_Transmit(&SX1509_I2C_PORT, SXAddress, buf, 2, i2CTimeout);
		buf[1] = 0x34;
		ret = HAL_I2C_Master_Transmit(&SX1509_I2C_PORT, SXAddress, buf, 2, i2CTimeout);
	}
}
