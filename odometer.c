/*
 * Encoder odometer used by the odometer calibration branch.
 */

#include "odometer.h"

#include "car_control.h"
#include "competition_tasks.h"
#include "oled_ssd1306.h"

#include <stdint.h>

#define ODOMETER_DISPLAY_INTERVAL_TICKS 20U
#define ODOMETER_LAP_PROGRESS_FULL_SCALE 1000U

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
static uint32_t Odometer_CountsToLapProgress(int32_t counts,
                                             uint32_t counts_per_lap);

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
  uint32_t left_progress = 0U;
  uint32_t right_progress = 0U;
  uint32_t lap_progress = 0U;
  uint8_t lap_progress_valid = 0U;

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

  if ((ODOMETER_LEFT_COUNTS_PER_LAP > 0U) &&
      (ODOMETER_RIGHT_COUNTS_PER_LAP > 0U) &&
      (left_counts >= 0) &&
      (right_counts >= 0))
  {
    left_progress = Odometer_CountsToLapProgress(
        left_counts, ODOMETER_LEFT_COUNTS_PER_LAP);
    right_progress = Odometer_CountsToLapProgress(
        right_counts, ODOMETER_RIGHT_COUNTS_PER_LAP);
    lap_progress = (uint32_t)(((uint64_t)left_progress +
                               right_progress + 1U) / 2U);
    lap_progress_valid = 1U;
  }

  OLED_DrawOdometer(left_counts,
                    right_counts,
                    lap_progress,
                    lap_progress_valid);
}

static uint8_t Odometer_StateIsRunning(CompetitionTaskState_t state)
{
  return ((state == COMPETITION_STATE_LEAVING_A) ||
          (state == COMPETITION_STATE_RUNNING) ||
          (state == COMPETITION_STATE_FINISH_APPROACH)) ? 1U : 0U;
}

static uint32_t Odometer_CountsToLapProgress(int32_t counts,
                                             uint32_t counts_per_lap)
{
  uint64_t progress;

  if ((counts < 0) || (counts_per_lap == 0U))
  {
    return 0U;
  }

  progress = (((uint64_t)(uint32_t)counts *
               ODOMETER_LAP_PROGRESS_FULL_SCALE) +
              (counts_per_lap / 2U)) / counts_per_lap;

  if (progress > UINT32_MAX)
  {
    return UINT32_MAX;
  }

  return (uint32_t)progress;
}
