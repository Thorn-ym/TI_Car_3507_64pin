/*
 * EC11 rotary encoder input for local problem selection.
 */

#ifndef EC11_ENCODER_H
#define EC11_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define EC11_BUTTON_NONE       0U
#define EC11_BUTTON_CLICK      1U
#define EC11_BUTTON_LONG_PRESS 2U

#ifndef EC11_DIRECTION
#define EC11_DIRECTION 1
#endif

typedef struct
{
  int32_t position;
  uint32_t step_count;
  uint32_t invalid_count;
  uint8_t button_level;
} EC11_t;

extern volatile EC11_t g_ec11;

void EC11_Init(void);
void EC11_HandleABInterrupt(uint32_t gpioa_status);
void EC11_Task(void);
int8_t EC11_GetDelta(void);
uint8_t EC11_GetButtonEvent(void);

#ifdef __cplusplus
}
#endif

#endif /* EC11_ENCODER_H */
