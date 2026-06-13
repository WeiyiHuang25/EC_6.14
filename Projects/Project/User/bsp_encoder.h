#pragma once

#include "main.h"

/* Motor-side encoder: 11 pulses per motor revolution. */
#define ENCODER_PULSE_PER_MOTOR_REV 11u
#define ENCODER_QUADRATURE_MULTIPLE 4u
#define ENCODER_GEAR_RATIO          21.3f

void Encoder_Init(void);
int32_t Encoder_GetCount(void);
void Encoder_Reset(void);
float Encoder_GetMotorRPM(void);
float Encoder_GetRPM(void);
