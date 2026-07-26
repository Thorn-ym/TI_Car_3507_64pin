/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : debug_uart.h
  * @brief          : HC-05 bluetooth debug UART for VOFA+ and online tuning.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef DEBUG_UART_H
#define DEBUG_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DEBUG_MODE_OFF   0U
#define DEBUG_MODE_CMD   1U
#define DEBUG_MODE_VOFA  2U

#ifndef CAR_DEBUG_MODE
#define CAR_DEBUG_MODE   DEBUG_MODE_VOFA 
#endif

#if ((CAR_DEBUG_MODE != DEBUG_MODE_OFF) && \
     (CAR_DEBUG_MODE != DEBUG_MODE_CMD) && \
     (CAR_DEBUG_MODE != DEBUG_MODE_VOFA))
#error "CAR_DEBUG_MODE must be DEBUG_MODE_OFF, DEBUG_MODE_CMD, or DEBUG_MODE_VOFA"
#endif

void Debug_UART_Init(void);
void Debug_UART_Task(void);
void Debug_UART_ProcessRx(void);
void Debug_UART_SendVofa(void);
void Debug_UART_SetGyroZ(float gyro_z);
void debug_uart_send_byte(uint8_t byte);
void debug_uart_send_bytes(const uint8_t *buf, uint32_t len);
void vofa_test_send(void);
void vofa_pid_send(void);
void vofa_line_send(void);
void vofa_speed_send(void);
void vofa_gyro_send(void);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_UART_H */
