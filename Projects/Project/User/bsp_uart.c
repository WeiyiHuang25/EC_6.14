#include "bsp_uart.h"
#include "usart.h"
#include <string.h>

static uint8_t uart_rx_data[UART_RX_MAX_LEN];

void UART_Init(void)
{
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rx_data, UART_RX_MAX_LEN);
}

void UART_SendByte(uint8_t data)
{
    HAL_UART_Transmit(&huart1, &data, 1, HAL_MAX_DELAY);
}

void UART_SendData(uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
}

void UART_SendString(char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

__weak void UART_RxCallback(uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if(huart->Instance == USART1)
    {
        UART_RxCallback(uart_rx_data, Size);
        HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rx_data, UART_RX_MAX_LEN);
    }
}
