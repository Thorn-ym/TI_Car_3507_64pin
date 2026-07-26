/*
 * EC11 rotary encoder input for local problem selection.
 */

#include "ec11_encoder.h"
#include "car_control.h"
#include "ti_msp_dl_config.h"

#include <stdint.h>

#define EC11_EDGES_PER_DETENT      2
#define EC11_DEBOUNCE_TICKS        3U
#define EC11_LONG_PRESS_TICKS      100U

volatile EC11_t g_ec11 =
{
  .position = 0,
  .step_count = 0U,
  .invalid_count = 0U,
  .button_level = 1U,
};

static volatile int8_t s_pending_delta = 0;
static volatile int8_t s_edge_accumulator = 0;
static volatile uint8_t s_ab_state = 0U;

static uint8_t s_button_stable = 1U;
static uint8_t s_button_last_raw = 1U;
static uint8_t s_button_long_reported = 0U;
static uint32_t s_button_debounce_start_tick = 0U;
static uint32_t s_button_press_tick = 0U;
static volatile uint8_t s_button_event = EC11_BUTTON_NONE;

static uint8_t EC11_ReadABState(void);
static uint8_t EC11_ReadButtonRaw(void);
static void EC11_AddStep(int8_t delta);

void EC11_Init(void)
{
  s_ab_state = EC11_ReadABState();
  s_edge_accumulator = 0;
  s_pending_delta = 0;

  s_button_stable = EC11_ReadButtonRaw();
  s_button_last_raw = s_button_stable;
  s_button_debounce_start_tick = g_car.control_tick;
  s_button_long_reported = 0U;
  s_button_press_tick = g_car.control_tick;
  s_button_event = EC11_BUTTON_NONE;

  g_ec11.position = 0;
  g_ec11.step_count = 0U;
  g_ec11.invalid_count = 0U;
  g_ec11.button_level = s_button_stable;
}

void EC11_HandleABInterrupt(uint32_t gpioa_status)
{
  static const int8_t delta_table[16] =
  {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
  };
  uint8_t state = 0U;
  uint8_t index = 0U;
  int8_t edge_delta = 0;

  if ((gpioa_status & (GPIO_EC11_A_PIN | GPIO_EC11_B_PIN)) == 0U)
  {
    return;
  }

  state = EC11_ReadABState();
  index = (uint8_t)((s_ab_state << 2) | state);
  edge_delta = delta_table[index];

  if (edge_delta == 0)
  {
    if (state != s_ab_state)
    {
      g_ec11.invalid_count++;
    }
  }
  else
  {
    s_edge_accumulator += edge_delta;

    if (s_edge_accumulator >= EC11_EDGES_PER_DETENT)
    {
      EC11_AddStep((int8_t)EC11_DIRECTION);
      s_edge_accumulator = 0;
    }
    else if (s_edge_accumulator <= -EC11_EDGES_PER_DETENT)
    {
      EC11_AddStep((int8_t)-EC11_DIRECTION);
      s_edge_accumulator = 0;
    }
  }

  s_ab_state = state;
}

void EC11_Task(void)
{
  uint8_t raw = EC11_ReadButtonRaw();
  uint32_t tick = g_car.control_tick;

  if (raw != s_button_last_raw)
  {
    s_button_last_raw = raw;
    s_button_debounce_start_tick = tick;
  }

  if (((uint32_t)(tick - s_button_debounce_start_tick) >= EC11_DEBOUNCE_TICKS) &&
      (raw != s_button_stable))
  {
    s_button_stable = raw;
    g_ec11.button_level = s_button_stable;

    if (s_button_stable == 0U)
    {
      s_button_press_tick = tick;
      s_button_long_reported = 0U;
    }
    else
    {
      if (s_button_long_reported == 0U)
      {
        s_button_event = EC11_BUTTON_CLICK;
      }
      s_button_long_reported = 0U;
    }
  }

  if ((s_button_stable == 0U) &&
      (s_button_long_reported == 0U) &&
      ((uint32_t)(tick - s_button_press_tick) >= EC11_LONG_PRESS_TICKS))
  {
    s_button_event = EC11_BUTTON_LONG_PRESS;
    s_button_long_reported = 1U;
  }
}

int8_t EC11_GetDelta(void)
{
  int8_t delta = 0;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  delta = s_pending_delta;
  s_pending_delta = 0;
  if ((primask & 1U) == 0U)
  {
    __enable_irq();
  }

  return delta;
}

uint8_t EC11_GetButtonEvent(void)
{
  uint8_t event = EC11_BUTTON_NONE;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  event = s_button_event;
  s_button_event = EC11_BUTTON_NONE;
  if ((primask & 1U) == 0U)
  {
    __enable_irq();
  }

  return event;
}

static uint8_t EC11_ReadABState(void)
{
  uint8_t a = (DL_GPIO_readPins(GPIO_EC11_PORT, GPIO_EC11_A_PIN) != 0U) ? 1U : 0U;
  uint8_t b = (DL_GPIO_readPins(GPIO_EC11_PORT, GPIO_EC11_B_PIN) != 0U) ? 1U : 0U;

  return (uint8_t)((a << 1) | b);
}

static uint8_t EC11_ReadButtonRaw(void)
{
  return (DL_GPIO_readPins(GPIO_EC11_PORT, GPIO_EC11_SW_PIN) != 0U) ? 1U : 0U;
}

static void EC11_AddStep(int8_t delta)
{
  s_pending_delta += delta;
  g_ec11.position += delta;
  g_ec11.step_count++;
}
