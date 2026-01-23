/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "sx1509.h"
#include <LCD_KEYPAD.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "bitmaps.h"
/* Private define ------------------------------------------------------------*/
#define FLASH_ADDRESS_SCORES 0x080FF800 // Adres dla STM32L476RG
#define DATA_SIZE sizeof(flash_datatype)
#define MAP_WIDTH 128
#define MAP_HEIGHT 64
/* Private typedef -----------------------------------------------------------*/
typedef uint64_t flash_datatype;
typedef struct
{
    int x;
    int y;
    int radius;
    int score;
    int speed;
    int dx;
    int dy;
} player;
typedef struct
{
    int x;
    int y;
} Dot;
/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;
uint32_t topScores[3] = {0, 0, 0};
Dot dots[10];
/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
void Error_Handler(void);
/* Game Functions */
player createPlayer(int x, int y);
void updatePlayer(player *p);
int dotEat(player *p);
void dotPosition(int n);
void dotDraw(void);
void loadHighScores(void);
void updateHighScores(uint32_t newScore);
void menuDisplay(void);
void drawMenuInterface(void);
void loadingAnimation(void);
void showDescription(void);
void showAuthors(void);
void showScores(void);
void winAnimation(void);
void loseAnimation(void);
void thanksForPlaying(void);
void updatePlayerSpeed(player *p, int my_speed);
/* Flash Memory Functions */
void read_flash_memory(uint32_t memory_address, uint8_t *data, uint16_t data_length);
void store_flash_memory(uint32_t memory_address, uint8_t *data, uint16_t data_length);
/* Main Program --------------------------------------------------------------*/
int main(void)
{
    /* MCU Initialization */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();
    MX_SPI1_Init();
    /* Peripherals Initialization */
    LCD_init();
    ssd1306_Init();
    /* Game State Initialization */
    loadHighScores();
    player myPlayer = createPlayer(10, 10);
    player bot = createPlayer(MAP_WIDTH - 10, MAP_WIDTH - 10);
    for (int i = 0; i < 10; i++)
        dotPosition(i);
    HAL_UART_Transmit(&huart2, (uint8_t *)"\033[2J\033[HScore: 0", strlen("\033[2J\033[HScore: 0"), 30);
    /* Intro */
    // winAnimation();
    menuDisplay();
    srand(HAL_GetTick());
    /* Game Loop */
    while (1)
    {
        uint8_t received_char;
        HAL_StatusTypeDef status = HAL_UART_Receive(&huart2, &received_char, 1, 1);
        if (status == HAL_OK)
        {
            // Otrzymano znak - ustawiamy kierunek
            switch (received_char)
            {
            case 'w':
                myPlayer.dx = 0;
                myPlayer.dy = -1;
                break;
            case 's':
                myPlayer.dx = 0;
                myPlayer.dy = 1;
                break;
            case 'a':
                myPlayer.dx = -1;
                myPlayer.dy = 0;
                break;
            case 'd':
                myPlayer.dx = 1;
                myPlayer.dy = 0;
                break;
            }
        }
        // --- 2. BOT INPUT ---
        calculateBotMovement(&bot, &myPlayer);
        // --- 3. PHYSICS (REUSED FUNCTIONS!) ---
        updatePlayerSpeed(&myPlayer, 3);
        updatePlayerSpeed(&bot, 2);
        updatePlayer(&myPlayer);
        updatePlayer(&bot);
        // --- 4. EATING LOGIC ---
        // Check Human eating
        if (dotEat(&myPlayer))
        {
            // Only update Highscore/UART if the HUMAN eats
            updateHighScores(myPlayer.score);
            char _score[20];
            sprintf(_score, "\033[2J\033[HScore: %d", myPlayer.score);
            HAL_UART_Transmit(&huart2, (uint8_t *)_score, strlen(_score), 30);
        }
        // Check Bot eating (We don't print score, just let it grow)
        dotEat(&bot);
        // --- 5. PVP COLLISION ---
        // Simple check: Distance between centers < sum of radii
        int distSq = (myPlayer.x - bot.x) * (myPlayer.x - bot.x) + (myPlayer.y - bot.y) * (myPlayer.y - bot.y);
        if (bot.radius > myPlayer.radius)
        {
            // Bot is bigger: Does the Bot's radius reach the Player's center?
            if (distSq < (bot.radius * bot.radius))
            {
                loseAnimation();
                resetGame(&bot, &myPlayer);
                menuDisplay();
            }
        }
        else
        {
            // Player is bigger: Does the Player's radius reach the Bot's center?
            if (distSq < (myPlayer.radius * myPlayer.radius))
            {
                winAnimation();
                resetGame(&bot, &myPlayer);
                menuDisplay();
            }
        }
        // --- 6. DRAWING ---
        ssd1306_Fill(Black);
        dotDraw();
        // Draw Human (Filled)
        ssd1306_FillCircle(myPlayer.x + myPlayer.radius, myPlayer.y + myPlayer.radius, myPlayer.radius, White);
        // Draw Bot (Empty/Outline to differentiate)
        ssd1306_DrawCircle(bot.x + bot.radius, bot.y + bot.radius, bot.radius, White);
        ssd1306_UpdateScreen();
        HAL_Delay(30);
    }
}


/* Game Logic Implementations ------------------------------------------------*/
player createPlayer(int x, int y)
{
    player p;
    p.x = x - 3;
    p.y = y - 3;
    p.radius = 3;
    p.score = 0;
    p.speed = 3;
    p.dx = 0;
    p.dy = 0;
    return p;
}
void updatePlayer(player *p)
{
    p->x += p->dx * p->speed;
    p->y += p->dy * p->speed;
    int hitbox = p->radius + 1;
    if (p->x < 1)
    {
        p->x = 1;
        p->dx = 0;
    }
    if (p->x > MAP_WIDTH - hitbox * 2)
    {
        p->x = MAP_WIDTH - hitbox * 2;
        p->dx = 0;
    }
    if (p->y < 1)
    {
        p->y = 1;
        p->dy = 0;
    }
    if (p->y > MAP_HEIGHT - hitbox * 2)
    {
        p->y = MAP_HEIGHT - hitbox * 2;
        p->dy = 0;
    }
}
void updatePlayerSpeed(player *p, int my_speed)
{
    // prędkość
    p->speed = my_speed - p->radius / 6;
    if (p->speed < 1)
        p->speed = 1;
}
int dotEat(player *p)
{
    int eaten = 0;
    int hitbox = p->radius + 1;
    int pCenterX = p->x + hitbox;
    int pCenterY = p->y + hitbox;
    int collisionDistSq = (p->radius + 2) * (p->radius + 2);
    for (int i = 0; i < 10; i++)
    {
        int diffX = pCenterX - dots[i].x;
        int diffY = pCenterY - dots[i].y;
        int distSq = (diffX * diffX) + (diffY * diffY);
        if (distSq <= collisionDistSq)
        {
            p->score++;
            p->radius = 3 + (p->score / 5);
            if (p->radius > 30)
                p->radius = 30;
            dotPosition(i);
            eaten = 1;
        }
    }
    return eaten;
}
void calculateBotMovement(player *bot, player *human)
{
    // --- 1. Identify Centers ---
    int botCenterX = bot->x + bot->radius;
    int botCenterY = bot->y + bot->radius;
    int targetX = 0;
    int targetY = 0;

    // --- 2. Find Target ---
    if (bot->radius > human->radius + 2)
    {
        targetX = human->x + human->radius;
        targetY = human->y + human->radius;
    }
    else
    {
        int minDistSq = 2000000000;
        int bestDotIndex = -1;

        for (int i = 0; i < 10; i++)
        {
            int diffX = botCenterX - dots[i].x;
            int diffY = botCenterY - dots[i].y;
            int distSq = (diffX * diffX) + (diffY * diffY);
            if (distSq < minDistSq)
            {
                minDistSq = distSq;
                bestDotIndex = i;
            }
        }

        if (bestDotIndex != -1) {
            targetX = dots[bestDotIndex].x;
            targetY = dots[bestDotIndex].y;
        } else {
            targetX = botCenterX;
            targetY = botCenterY;
        }
    }

    // --- 3. Strict Non-Diagonal Movement ---

    // CHECK X DISTANCE
    // If we are far from X, move X and STOP Y
    if (abs(botCenterX - targetX) > bot->speed)
    {
        if (botCenterX < targetX)
            bot->dx = 1;
        else
            bot->dx = -1;

        // CRITICAL: Force Y to 0 so we don't move diagonally
        bot->dy = 0;
    }
    // X IS DONE (Snap and check Y)
    else
    {
        // Snap X exactly to target
        bot->x = targetX - bot->radius;
        bot->dx = 0;

        // NOW CHECK Y DISTANCE
        if (abs(botCenterY - targetY) > bot->speed)
        {
            if (botCenterY < targetY)
                bot->dy = 1;
            else
                bot->dy = -1;
        }
        else
        {
            // Snap Y exactly to target
            bot->y = targetY - bot->radius;
            bot->dy = 0;
        }
    }
}
void dotPosition(int n)
{
    dots[n].x = rand() % MAP_WIDTH;
    dots[n].y = rand() % MAP_HEIGHT;
}
void dotDraw(void)
{
    for (int i = 0; i < 10; i++)
        ssd1306_DrawPixel(dots[i].x, dots[i].y, White);
}
void menuDisplay(void)
{
    drawMenuInterface();
    uint8_t buffer[1];
    while (1)
    {
        buffer[0] = 0;
        HAL_UART_Receive(&huart2, buffer, 1, 50);
        switch (buffer[0])
        {
        case '1':
            loadingAnimation();
            return;
        case '2':
            showDescription();
            drawMenuInterface();
            break;
        case '3':
            showAuthors();
            drawMenuInterface();
            break;
        case '4':
            showScores();
            drawMenuInterface();
            break;
        case '5':
            resetHighScores();
        }
    }
}
void drawMenuInterface(void)
{
    ssd1306_Fill(Black);
    ssd1306_DrawBitmap(0, 0, menu, 128, 64, White);
    ssd1306_SetCursor(16, 15);
    ssd1306_WriteString("1. Graj", Font_6x8, White);
    ssd1306_SetCursor(16, 25);
    ssd1306_WriteString("2. Opis gry", Font_6x8, White);
    ssd1306_SetCursor(16, 35);
    ssd1306_WriteString("3. Autorzy", Font_6x8, White);
    ssd1306_SetCursor(16, 45);
    ssd1306_WriteString("4. Tablica wynikow", Font_6x8, White);
    ssd1306_UpdateScreen();
}
void loadingAnimation(void)
{
    for (int i = 0; i < 6; i++)
    {
        ssd1306_Fill(Black);
        ssd1306_DrawCircle(64, 32, 20, White);
        ssd1306_Line(56, 38, 64, 43, White);
        ssd1306_Line(64, 43, 72, 38, White);
        if (i % 2 == 0)
        {
            ssd1306_DrawCircle(56, 26, 2, White);
            ssd1306_DrawCircle(72, 26, 2, White);
        }
        else
        {
            ssd1306_Line(54, 26, 58, 26, White);
            ssd1306_Line(70, 26, 74, 26, White);
        }
        ssd1306_SetCursor(35, 56);
        ssd1306_WriteString("Loading...", Font_6x8, White);
        ssd1306_UpdateScreen();
        HAL_Delay(500);
    }
}
void showAuthors(void)
{
    ssd1306_Fill(Black);
    ssd1306_SetCursor(40, 0);
    ssd1306_WriteString("Autorzy:", Font_6x8, White);
    ssd1306_SetCursor(19, 16);
    ssd1306_WriteString("Michal Kukielko", Font_6x8, White);
    ssd1306_SetCursor(16, 32);
    ssd1306_WriteString("Krystian Konopko", Font_6x8, White);
    ssd1306_SetCursor(16, 48);
    ssd1306_WriteString("Filip Kurpiewski", Font_6x8, White);
    ssd1306_UpdateScreen();
    HAL_Delay(3000);
}
void showDescription(void)
{
    ssd1306_Fill(Black);
    ssd1306_SetCursor(34, 0);
    ssd1306_WriteString("Opis (1/2)", Font_6x8, White);
    ssd1306_SetCursor(0, 16);
    ssd1306_WriteString("Jest to gra inspiro-", Font_6x8, White);
    ssd1306_SetCursor(0, 26);
    ssd1306_WriteString("wana gra Agario. Zro-", Font_6x8, White);
    ssd1306_SetCursor(0, 36);
    ssd1306_WriteString("biona z pasja do je-", Font_6x8, White);
    ssd1306_SetCursor(0, 46);
    ssd1306_WriteString("zyka C oraz mikrokon-", Font_6x8, White);
    ssd1306_UpdateScreen();
    HAL_Delay(5000);
    ssd1306_Fill(Black);
    ssd1306_SetCursor(34, 0);
    ssd1306_WriteString("Opis (2/2)", Font_6x8, White);
    ssd1306_SetCursor(0, 16);
    ssd1306_WriteString("trolerow. W projek-", Font_6x8, White);
    ssd1306_SetCursor(0, 26);
    ssd1306_WriteString("cie nie zostaly uzy-", Font_6x8, White);
    ssd1306_SetCursor(0, 36);
    ssd1306_WriteString("te zadne lzy... Do", Font_6x8, White);
    ssd1306_SetCursor(0, 46);
    ssd1306_WriteString("projektu uzylismy LCD", Font_6x8, White);
    ssd1306_SetCursor(0, 56);
    ssd1306_WriteString("i UART. :D", Font_6x8, White);
    ssd1306_UpdateScreen();
    HAL_Delay(5000);
}
void showScores(void)
{
    char buffer[20];
    ssd1306_Fill(Black);
    ssd1306_SetCursor(40, 0);
    ssd1306_WriteString("Wyniki:", Font_6x8, White);
    snprintf(buffer, sizeof(buffer), "1. %lu", topScores[0]);
    ssd1306_SetCursor(10, 16);
    ssd1306_WriteString(buffer, Font_6x8, White);
    snprintf(buffer, sizeof(buffer), "2. %lu", topScores[1]);
    ssd1306_SetCursor(10, 30);
    ssd1306_WriteString(buffer, Font_6x8, White);
    snprintf(buffer, sizeof(buffer), "3. %lu", topScores[2]);
    ssd1306_SetCursor(10, 44);
    ssd1306_WriteString(buffer, Font_6x8, White);
    ssd1306_UpdateScreen();
    HAL_Delay(3000);
}
void thanksForPlaying(void)
{
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("Dzieki za granie", Font_6x8, White);
    ssd1306_UpdateScreen();
    HAL_Delay(3000);
}
void winAnimation(void)
{
    for (int j = 0; j < 3; j++)
    {
        for (int i = 1; i <= 11; i++)
        {
            ssd1306_Fill(Black);
            switch (i)
            {
            case 1:
                ssd1306_DrawBitmap(0, 0, fajerwerki1, 128, 64, White);
                break;
            case 2:
                ssd1306_DrawBitmap(0, 0, fajerwerki2, 128, 64, White);
                break;
            case 3:
                ssd1306_DrawBitmap(0, 0, fajerwerki3, 128, 64, White);
                break;
            case 4:
                ssd1306_DrawBitmap(0, 0, fajerwerki4, 128, 64, White);
                break;
            case 5:
                ssd1306_DrawBitmap(0, 0, fajerwerki5, 128, 64, White);
                break;
            case 6:
                ssd1306_DrawBitmap(0, 0, fajerwerki6, 128, 64, White);
                break;
            case 7:
                ssd1306_DrawBitmap(0, 0, fajerwerki7, 128, 64, White);
                break;
            case 8:
                ssd1306_DrawBitmap(0, 0, fajerwerki8, 128, 64, White);
                break;
            case 9:
                ssd1306_DrawBitmap(0, 0, fajerwerki9, 128, 64, White);
                break;
            case 10:
                ssd1306_DrawBitmap(0, 0, fajerwerki10, 128, 64, White);
                break;
            case 11:
                ssd1306_DrawBitmap(0, 0, fajerwerki11, 128, 64, White);
                break;
            }
            ssd1306_SetCursor(37, 39);
            ssd1306_WriteString("WYGRANA", Font_6x8, White);
            ssd1306_UpdateScreen();
            HAL_Delay(100);
        }
    }
}
void loseAnimation(void)
{
  for (int i = 0; i < 6; i++)
  {
      ssd1306_Fill(Black);
      ssd1306_DrawCircle(64, 32, 20, White);
      ssd1306_Line(56, 43, 64, 38, White);
      ssd1306_Line(64, 38, 72, 43, White);
      ssd1306_DrawCircle(56, 26, 2, White);
      ssd1306_DrawCircle(72, 26, 2, White);
      if (i % 2 == 0)
      {
          ssd1306_DrawPixel(54, 26, White);
          ssd1306_DrawPixel(53, 27, White);
          ssd1306_DrawPixel(53, 28, White);
          ssd1306_DrawPixel(54, 29, White);
          ssd1306_DrawPixel(55, 29, White);
          ssd1306_DrawPixel(56, 29, White);

      }
      ssd1306_SetCursor(35, 56);
      ssd1306_WriteString("PRZEGRANA", Font_6x8, White);
      ssd1306_UpdateScreen();
      HAL_Delay(500);
  }
}

void resetGame(player *h, player *b)
{
    *h = createPlayer(10, 10);
    *b = createPlayer(MAP_WIDTH - 10, MAP_HEIGHT - 10);
    b->speed = h->speed / 2;
    srand(HAL_GetTick());
    for (int i = 0; i < 10; i++)
        dotPosition(i);
    HAL_UART_Transmit(&huart2, (uint8_t *)"\033[2J\033[HScore: 0", 18, 30);
}
/* Flash Memory Implementations ----------------------------------------------*/
void read_flash_memory(uint32_t memory_address, uint8_t *data, uint16_t data_length)
{
    for (int i = 0; i < data_length; i++)
    {
        *(data + i) = (*(uint8_t *)(memory_address + i));
    }
}
void store_flash_memory(uint32_t memory_address, uint8_t *data, uint16_t data_length)
{
    uint8_t double_word_data[DATA_SIZE];
    FLASH_EraseInitTypeDef flash_erase_struct = {0};
    HAL_FLASH_Unlock();
    flash_erase_struct.TypeErase = FLASH_TYPEERASE_PAGES;
    flash_erase_struct.Page = (memory_address - FLASH_BASE) / FLASH_PAGE_SIZE;
    flash_erase_struct.NbPages = 1 + data_length / FLASH_PAGE_SIZE;
#ifdef FLASH_BANK_1
    if (memory_address < FLASH_BANK1_END)
    {
        flash_erase_struct.Banks = FLASH_BANK_1;
    }
    else
    {
        flash_erase_struct.Banks = FLASH_BANK_2;
    }
#endif
    uint32_t error_status = 0;
    HAL_FLASHEx_Erase(&flash_erase_struct, &error_status);
    int i = 0;
    while (i <= data_length)
    {
        double_word_data[i % DATA_SIZE] = data[i];
        i++;
        if (i % DATA_SIZE == 0)
        {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, memory_address + i - DATA_SIZE, *((uint64_t *)double_word_data));
        }
    }
    if (i % DATA_SIZE != 0)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, memory_address + i - i % DATA_SIZE, *((flash_datatype *)double_word_data));
    }
    HAL_FLASH_Lock();
}
void resetHighScores(void)
{
    for (int i = 0; i < 3; i++)
    {
        topScores[i] = 0;
    }
    store_flash_memory(FLASH_ADDRESS_SCORES, (uint8_t *)topScores, sizeof(topScores));
}
void loadHighScores(void)
{
    read_flash_memory(FLASH_ADDRESS_SCORES, (uint8_t *)topScores, sizeof(topScores));
    if (topScores[0] == 0xFFFFFFFF)
    {
        topScores[0] = 0;
        topScores[1] = 0;
        topScores[2] = 0;
        store_flash_memory(FLASH_ADDRESS_SCORES, (uint8_t *)topScores, sizeof(topScores));
    }
}
void updateHighScores(uint32_t newScore)
{
    int updated = 0;
    if (newScore > topScores[0])
    {
        topScores[2] = topScores[1];
        topScores[1] = topScores[0];
        topScores[0] = newScore;
        updated = 1;
    }
    else if (newScore > topScores[1])
    {
        topScores[2] = topScores[1];
        topScores[1] = newScore;
        updated = 1;
    }
    else if (newScore > topScores[2])
    {
        topScores[2] = newScore;
        updated = 1;
    }
    if (updated)
    {
        store_flash_memory(FLASH_ADDRESS_SCORES, (uint8_t *)topScores, sizeof(topScores));
    }
}
/* Hardware Initialization Implementations -----------------------------------*/
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
        Error_Handler();
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 10;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
        Error_Handler();
}
static void MX_I2C1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInit.Usart2ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x10D19CE4;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        Error_Handler();
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        Error_Handler();
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
        Error_Handler();
}
static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
        Error_Handler();
}
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK)
        Error_Handler();
}
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SSD1306_DC_Port, SSD1306_DC_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SSD1306_Reset_Port, SSD1306_Reset_Pin, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = SSD1306_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SSD1306_CS_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = SSD1306_DC_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SSD1306_DC_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = SX1509_OSC_Pin | SX1509_nRST_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SX1509_nINIT_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = SX1509_nINIT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SX1509_nINIT_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = USART_TX_Pin | USART_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = SSD1306_Reset_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SSD1306_Reset_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(SX1509_nRST_PORT, SX1509_nRST_Pin, GPIO_PIN_SET);
}
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}






