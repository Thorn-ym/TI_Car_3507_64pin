/*
 * Encoder odometer used by the odometer calibration branch.
 */

#include "odometer.h"

#include "car_control.h"
#include "competition_tasks.h"
#include "oled_ssd1306.h"

#include <stdint.h>

#define ODOMETER_DISPLAY_INTERVAL_TICKS 20U

static volatile int32_t s_left_zero = 0;
static volatile int32_t s_right_zero = 0;
static volatile int32_t s_left_counts = 0;
static volatile int32_t s_right_counts = 0;
static volatile CompetitionTaskState_t s_last_state =
    COMPETITION_STATE_IDLE;
static volatile uint8_t s_frozen = 0U;
static volatile uint8_t s_display_dirty = 0U;
static volatile uint8_t s_initialized = 0U;
static uint32_t s_last_display_tick = 0U;

static uint8_t Odometer_StateIsRunning(CompetitionTaskState_t state);
static uint32_t Odometer_CountsToMillimetres(int32_t counts,
                                             uint32_t counts_per_metre);

void Odometer_Init(void)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  s_initialized = 0U;
  s_left_zero = g_car.left.encoder_total;
  s_right_zero = g_car.right.encoder_total;
  s_left_counts = 0;
  s_right_counts = 0;
  s_last_state = g_competition_task_status.state;
  s_frozen = 0U;
  s_display_dirty = 1U;
  s_last_display_tick = g_car.control_tick;
  s_initialized = 1U;

  if ((primask & 1U) == 0U)
  {
    __enable_irq();
  }
}

void Odometer_ControlStep(void)
{
  CompetitionTaskState_t state;
  uint8_t running;
  uint8_t was_running;

  if (s_initialized == 0U)
  {
    return;
  }

  state = g_competition_task_status.state;
  running = Odometer_StateIsRunning(state);
  was_running = Odometer_StateIsRunning(s_last_state);

  if ((state == COMPETITION_STATE_LEAVING_A) && (was_running == 0U))
  {
    s_left_zero = g_car.left.encoder_total;
    s_right_zero = g_car.right.encoder_total;
    s_left_counts = 0;
    s_right_counts = 0;
    s_frozen = 0U;
    s_display_dirty = 1U;
  }

  if ((running != 0U) && (s_frozen == 0U))
  {
    s_left_counts = g_car.left.encoder_total - s_left_zero;
    s_right_counts = g_car.right.encoder_total - s_right_zero;
  }
  else if ((state == COMPETITION_STATE_FINISHED) &&
           (was_running != 0U) &&
           (s_frozen == 0U))
  {
    s_left_counts = g_car.left.encoder_total - s_left_zero;
    s_right_counts = g_car.right.encoder_total - s_right_zero;
    s_frozen = 1U;
    s_display_dirty = 1U;
  }
  else if ((state == COMPETITION_STATE_IDLE) && (s_frozen == 0U))
  {
    /* This also supports a power-on, hand-pushed one metre calibration. */
    s_left_counts = g_car.left.encoder_total - s_left_zero;
    s_right_counts = g_car.right.encoder_total - s_right_zero;
  }

  s_last_state = state;
}

void Odometer_Service(void)
{
  int32_t left_counts;
  int32_t right_counts;
  uint32_t tick;
  uint32_t primask;
  uint32_t left_mm = 0U;
  uint32_t right_mm = 0U;
  uint32_t distance_mm = 0U;
  uint8_t distance_valid = 0U;

  if (s_initialized == 0U)
  {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  tick = g_car.control_tick;

  if ((s_display_dirty == 0U) &&
      ((uint32_t)(tick - s_last_display_tick) <
       ODOMETER_DISPLAY_INTERVAL_TICKS))
  {
    if ((primask & 1U) == 0U)
    {
      __enable_irq();
    }
    return;
  }

  left_counts = s_left_counts;
  right_counts = s_right_counts;
  s_display_dirty = 0U;
  s_last_display_tick = tick;

  if ((primask & 1U) == 0U)
  {
    __enable_irq();
  }

  if ((ODOMETER_LEFT_COUNTS_PER_METER > 0U) &&
      (ODOMETER_RIGHT_COUNTS_PER_METER > 0U))
  {
    left_mm = Odometer_CountsToMillimetres(
        left_counts, ODOMETER_LEFT_COUNTS_PER_METER);
    right_mm = Odometer_CountsToMillimetres(
        right_counts, ODOMETER_RIGHT_COUNTS_PER_METER);
    distance_mm = (uint32_t)(((uint64_t)left_mm + right_mm + 1U) / 2U);
    distance_valid = 1U;
  }

  OLED_DrawOdometer(left_counts,
                    right_counts,
                    distance_mm,
                    distance_valid);
}

static uint8_t Odometer_StateIsRunning(CompetitionTaskState_t state)
{
  return ((state == COMPETITION_STATE_LEAVING_A) ||
          (state == COMPETITION_STATE_RUNNING) ||
          (state == COMPETITION_STATE_FINISH_APPROACH)) ? 1U : 0U;
}

static uint32_t Odometer_CountsToMillimetres(int32_t counts,
                                             uint32_t counts_per_metre)
{
  uint64_t magnitude;
  uint64_t millimetres;

  if (counts_per_metre == 0U)
  {
    return 0U;
  }

  if (counts < 0)
  {
    magnitude = (uint64_t)(-(int64_t)counts);
  }
  else
  {
    magnitude = (uint64_t)counts;
  }

  millimetres = ((magnitude * 1000U) + (counts_per_metre / 2U)) /
                counts_per_metre;

  if (millimetres > UINT32_MAX)
  {
    return UINT32_MAX;
  }

  return (uint32_t)millimetres;
}
