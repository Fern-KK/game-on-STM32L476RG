/*
 * myLCD.c
 *
 *  Created on: Aug 25, 2023
 *      Author: ab
 */

#include <LCD_Keypad.h>

void pulseEnable(void) {
  // E = 0
  HAL_GPIO_WritePin (GPIOC, GPIO_PIN_7, 0);
  HAL_Delay(1);
  // E = 1
  HAL_GPIO_WritePin (GPIOC, GPIO_PIN_7, 1);
  HAL_Delay(1);
  // E = 0
  HAL_GPIO_WritePin (GPIOC, GPIO_PIN_7, 0);
  HAL_Delay(1);
}

void write4bits(uint8_t value)
{
  HAL_GPIO_WritePin (GPIOB,  GPIO_PIN_5, (value >> 0) & 0x01); //DB4
  HAL_GPIO_WritePin (GPIOB,  GPIO_PIN_4, (value >> 1) & 0x01); //DB5
  HAL_GPIO_WritePin (GPIOB, GPIO_PIN_10, (value >> 2) & 0x01); //DB6
  HAL_GPIO_WritePin (GPIOA,  GPIO_PIN_8, (value >> 3) & 0x01); //DB7
  pulseEnable();
}

void write(uint8_t RS, uint8_t value)
{
	HAL_GPIO_WritePin (GPIOA, GPIO_PIN_9, RS);
	// send data
	write4bits(value>>4);
	write4bits(value);
}

void LCD_init()
{
	HAL_Delay(50);
	// Now we pull both RS and R/W low to begin commands
	// RS = 0
	HAL_GPIO_WritePin (GPIOA, GPIO_PIN_9, 0);
	// E = 0
	HAL_GPIO_WritePin (GPIOC, GPIO_PIN_7, 0);

	write4bits(0x03);
	HAL_Delay(5);

    // second try
    write4bits(0x03);
	HAL_Delay(5);

    // third go!
    write4bits(0x03);
	HAL_Delay(5);

	// finally, set to 4-bit interface
    write4bits(0x02);

	write(0, LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS);

	// turn the display on with no cursor or blinking default
	write(0, LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSORON | LCD_BLINKON);

	// clear it off
	write(0, LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
    HAL_Delay(20);  // this command takes a long time!

	// Initialize to default text direction (for romance languages)
	// set the entry mode
    write(0, LCD_ENTRYMODESET |  LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
}


void LCD_clear()
{
	write(0, LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
    HAL_Delay(20);  // this command takes a long time!
}

void LCD_print(char *s)
{
	while (*s != '\0')
		write(1, (uint8_t)*(s++));
}
