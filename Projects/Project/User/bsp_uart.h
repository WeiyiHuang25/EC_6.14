#pragma once

#include "main.h"

#define UART_RX_MAX_LEN 8u

void UART_Init(void);
void UART_SendByte(uint8_t data);
void UART_SendData(uint8_t *data, uint16_t len);
void UART_SendString(char *str);

void UART_RxCallback(uint8_t *data, uint16_t len);
