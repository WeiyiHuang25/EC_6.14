#include "app_task.h"
#include "bsp_encoder.h"
#include "bsp_pwm.h"
#include "bsp_uart.h"
#include "drv_tb6612.h"

#include <string.h>

volatile uint32_t flag100hz = 0;

void app_task()
{
    if(flag100hz)
    {
        float rpm;
        rpm = Encoder_GetRPM();
        uint8_t buf[4] = {0};
        memcpy(buf, &rpm, sizeof(rpm));
        UART_SendData(buf, sizeof(buf));
        flag100hz = 0;
    }
}

void app_setFlag()
{
    flag100hz = 1;
}

void app_startMotor()
{
    TB6612_SetRPM(100.0f);
}

void User_init()
{
    Encoder_Init();
    TB6612_Init();
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim == &htim6)
    {
        app_setFlag();
    }
};
