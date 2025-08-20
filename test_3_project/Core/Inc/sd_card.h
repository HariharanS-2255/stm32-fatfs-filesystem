#ifndef __SD_CARD_H
#define __SD_CARD_H

#include "stm32f4xx_hal.h"

// ===== SPI handle from main.c or CubeMX =====
extern SPI_HandleTypeDef hspi2;

// ===== Chip Select pin macros (update if needed) =====
// Example: SD card CS on PB2
#define CS_LOW()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET)

// ===== Public functions =====
int SD_Init(void);
int SD_ReadBlocks(uint8_t *buff, uint32_t sector, uint32_t count);
int SD_WriteBlocks(const uint8_t *buff, uint32_t sector, uint32_t count);

#endif
