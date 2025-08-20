#ifndef UART_CLI_H
#define UART_CLI_H

#include "stm32f4xx_hal.h"

void UART_CLI_Init(UART_HandleTypeDef *huart);
void UART_CLI_Process(void);
void UART_CLI_Printf(const char *format, ...);

#endif
