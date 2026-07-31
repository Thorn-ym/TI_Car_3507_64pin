/*
 * H-problem vehicle run state machine.
 */

#include "competition_tasks.h"

#include "car_control.h"
#include "line_tracker.h"
#include "odometer.h"
#include "oled_ssd1306.h"
#include "ti_msp_dl_config.h"

#include <stdint.h>

#define COMPETITION_KEY_DEBOUNCE_TICKS       3U
#define COMPETITION_LEAVE_CONFIRM_TICKS       5U
#define COMPETITION_FINISH_CONFIRM_TICKS      2U
#define COMPETITION_MIN_RUN_TICKS           100U
#define COMPETITION_DISPLAY_INTERVAL_TICKS   10U
#define COMPETITION_FINISH_CENTER_MASK      0x1CU
#define COMPETITION_MAX_CENTISECONDS        9999U
#define COMPETITION_SMOOTHSTEP_SCALE        1024U

#if COMPETITION_ACCEL_TICKS == 0U
#error COMPETITION_ACCEL_TICKS must be greater than zero
#endif
#if COMPETITION_DECEL_START_PROGRESS >= COMPETITION_DECEL_END_PROGRESS
#error COMPETITION_DECEL_START_PROGRESS must be less than the end progress
#endif
#if COMPETITION_FINAL_COUNTS > COMPETITION_CRUISE_COUNTS
#error COMPETITION_FINAL_COUNTS must not exceed the cruise speed
#endif

volatile CompetitionTaskStatus_t g_competition_task_status =
{
  .state = COMPETITION_STATE_IDLE,
  .start_tick = 0U,
  .elapsed_ticks = 0U,
  .finish_tick = 0U,
};

static uint32_t s_last_control_tick = 0U;
static uint32_t s_last_display_tick = 0U;
static uint32_t s_key_debounce_tick = 0U;
static uint32_t s_continuous_key_debounce_tick = 0U;
static uint32_t s_key_press_tick = 0U;
static uint32_t s_continuous_key_press_tick = 0U;
static uint32_t s_motion_start_tick = 0U;
static uint32_t s_last_lap_ticks = 0U;
static uint32_t s_lap_number = 0U;
static uint32_t s_finish_seen_tick = 0U;
static uint16_t s_max_lap_progress = 0U;
static int32_t s_finish_left_total = 0;
static int32_t s_finish_right_total = 0;
static uint8_t s_key_last_raw = 1U;
static uint8_t s_key_stable = 1U;
static uint8_t s_key_armed = 1U;
static uint8_t s_key_press_pending = 0U;
static uint8_t s_key_press_allowed = 0U;
static uint8_t s_continuous_key_last_raw = 1U;
static uint8_t s_continuous_key_stable = 1U;
static uint8_t s_continuous_key_armed = 1U;
static uint8_t s_continuous_key_press_pending = 0U;
static uint8_t s_continuous_key_press_allowed = 0U;
static uint8_t s_continuous_mode = 0U;
static uint8_t s_leave_confirm_count = 0U;
static uint8_t s_finish_confirm_count = 0U;
static volatile uint8_t s_display_dirty = 0U;
static volatile uint8_t s_initialized = 0U;

static uint8_t CompetitionTasks_ReadKeyRaw(void);
static uint8_t CompetitionTasks_KeyPressed(uint32_t tick);
static uint8_t CompetitionTasks_ReadContinuousKeyRaw(void);
static uint8_t CompetitionTasks_ContinuousKeyPressed(uint32_t tick);
static uint8_t CompetitionTasks_FinishLineSeen(void);
static uint32_t CompetitionTasks_AbsDelta(int32_t value, int32_t start);
static uint8_t CompetitionTasks_AdvanceReached(void);
static uint32_t CompetitionTasks_TicksToCentiseconds(uint32_t ticks);
static uint32_t CompetitionTasks_SmoothStep(uint32_t position,
                                            uint32_t span);
static void CompetitionTasks_UpdateSpeedProfile(uint32_t tick);
static void CompetitionTasks_Start(uint32_t start_tick,
                                   uint32_t now_tick,
                                   uint8_t continuous_mode);
static void CompetitionTasks_CompleteLap(uint32_t tick);
static void CompetitionTasks_Complete(uint32_t tick);

void CompetitionTasks_Init(void)
{
  uint32_t tick = g_car.control_tick;
  uint8_t raw = CompetitionTasks_ReadKeyRaw();
  uint8_t continuous_raw = CompetitionTasks_ReadContinuousKeyRaw();

  s_initialized = 0U;
  g_competition_task_status.state = COMPETITION_STATE_IDLE;
  g_competition_task_status.start_tick = tick;
  g_competition_task_status.elapsed_ticks = 0U;
  g_competition_task_status.finish_tick = tick;

  s_last_control_tick = tick;
  s_last_display_tick = tick;
  s_key_debounce_tick = tick;
  s_continuous_key_debounce_tick = tick;
  s_key_press_tick = tick;
  s_continuous_key_press_tick = tick;
  s_motion_start_tick = tick;
  s_last_lap_ticks = 0U;
  s_lap_number = 0U;
  s_finish_seen_tick = 0U;
  s_max_lap_progress = 0U;
  s_key_last_raw = raw;
  s_key_stable = raw;
  s_key_armed = (raw != 0U) ? 1U : 0U;
  s_key_press_pending = 0U;
  s_key_press_allowed = 0U;
  s_continuous_key_last_raw = continuous_raw;
  s_continuous_key_stable = continuous_raw;
  s_continuous_key_armed = (continuous_raw != 0U) ? 1U : 0U;
  s_continuous_key_press_pending = 0U;
  s_continuous_key_press_allowed = 0U;
  s_continuous_mode = 0U;
  s_leave_confirm_count = 0U;
  s_finish_confirm_count = 0U;
  s_finish_left_total = 0;
  s_finish_right_total = 0;
  s_display_dirty = 0U;

  g_line.right_angle_enable = 0U;
  g_car.line.right_angle_assist_enable = 0U;
  Car_Stop();
  OLED_DrawRaceTime(0U);
  s_initialized = 1U;
}

void CompetitionTasks_ControlStep(void)
{
  uint32_t tick = g_car.control_tick;
  uint8_t key_pressed = 0U;
  uint8_t continuous_key_pressed = 0U;

  if ((s_initialized == 0U) || (tick == s_last_control_tick))
  {
    return;
  }
  s_last_control_tick = tick;

  key_pressed = CompetitionTasks_KeyPressed(tick);
  continuous_key_pressed = CompetitionTasks_ContinuousKeyPressed(tick);
  if (((key_pressed != 0U) || (continuous_key_pressed != 0U)) &&
      ((g_competition_task_status.state == COMPETITION_STATE_IDLE) ||
       (g_competition_task_status.state == COMPETITION_STATE_FINISHED)))
  {
    if (continuous_key_pressed != 0U)
    {
      CompetitionTasks_Start(s_continuous_key_press_tick, tick, 1U);
    }
    else
    {
      CompetitionTasks_Start(s_key_press_tick, tick, 0U);
    }
  }

  switch (g_competition_task_status.state)
  {
    case COMPETITION_STATE_LEAVING_A:
      if ((g_line.line_seen != 0U) &&
          (CompetitionTasks_FinishLineSeen() == 0U))
      {
        if (s_leave_confirm_count < COMPETITION_LEAVE_CONFIRM_TICKS)
        {
          s_leave_confirm_count++;
        }
      }
      else
      {
        s_leave_confirm_count = 0U;
      }

      if (s_leave_confirm_count >= COMPETITION_LEAVE_CONFIRM_TICKS)
      {
        g_competition_task_status.state = COMPETITION_STATE_RUNNING;
        s_finish_confirm_count = 0U;
      }
      break;

    case COMPETITION_STATE_RUNNING:
      if (((uint32_t)(tick - g_competition_task_status.start_tick) >=
           COMPETITION_MIN_RUN_TICKS) &&
          (CompetitionTasks_FinishLineSeen() != 0U))
      {
        if (s_finish_confirm_count == 0U)
        {
          s_finish_seen_tick = tick;
        }
        if (s_finish_confirm_count < COMPETITION_FINISH_CONFIRM_TICKS)
        {
          s_finish_confirm_count++;
        }
      }
      else
      {
        s_finish_confirm_count = 0U;
        s_finish_seen_tick = 0U;
      }

      if (s_finish_confirm_count >= COMPETITION_FINISH_CONFIRM_TICKS)
      {
        s_finish_left_total = g_car.left.encoder_total;
        s_finish_right_total = g_car.right.encoder_total;

        if (s_continuous_mode != 0U)
        {
          CompetitionTasks_CompleteLap(s_finish_seen_tick);
        }
        else if (COMPETITION_FINISH_ADVANCE_COUNTS == 0U)
        {
          CompetitionTasks_Complete(tick);
        }
        else
        {
          g_competition_task_status.state =
              COMPETITION_STATE_FINISH_APPROACH;
        }
      }
      break;

    case COMPETITION_STATE_FINISH_APPROACH:
      if (CompetitionTasks_AdvanceReached() != 0U)
      {
        CompetitionTasks_Complete(tick);
      }
      break;

    case COMPETITION_STATE_IDLE:
    case COMPETITION_STATE_FINISHED:
    default:
      break;
  }

  if ((g_competition_task_status.state == COMPETITION_STATE_LEAVING_A) ||
      (g_competition_task_status.state == COMPETITION_STATE_RUNNING) ||
      (g_competition_task_status.state == COMPETITION_STATE_FINISH_APPROACH))
  {
    g_competition_task_status.elapsed_ticks =
        tick - g_competition_task_status.start_tick;
    CompetitionTasks_UpdateSpeedProfile(tick);
  }
}

void CompetitionTasks_Service(void)
{
  uint32_t tick = g_car.control_tick;
  uint32_t elapsed_ticks = 0U;
  uint32_t primask = 0U;
  uint32_t lap_number = 0U;
  uint32_t last_lap_ticks = 0U;
  uint8_t continuous_mode = 0U;
  uint8_t draw = 0U;
  CompetitionTaskState_t state;

  if (s_initialized == 0U)
  {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  state = g_competition_task_status.state;
  elapsed_ticks = g_competition_task_status.elapsed_ticks;
  lap_number = s_lap_number;
  last_lap_ticks = s_last_lap_ticks;
  continuous_mode = s_continuous_mode;

  if (s_display_dirty != 0U)
  {
    s_display_dirty = 0U;
    s_last_display_tick = tick;
    draw = 1U;
  }
  else if (((state == COMPETITION_STATE_LEAVING_A) ||
            (state == COMPETITION_STATE_RUNNING) ||
            (state == COMPETITION_STATE_FINISH_APPROACH)) &&
           ((continuous_mode == 0U) || (lap_number == 0U)) &&
           ((uint32_t)(tick - s_last_display_tick) >=
            COMPETITION_DISPLAY_INTERVAL_TICKS))
  {
    s_last_display_tick = tick;
    draw = 1U;
  }

  if ((primask & 1U) == 0U)
  {
    __enable_irq();
  }

  if (draw != 0U)
  {
    if ((continuous_mode != 0U) && (lap_number > 0U))
    {
      OLED_DrawLapTime(
          lap_number, CompetitionTasks_TicksToCentiseconds(last_lap_ticks));
    }
    else
    {
      OLED_DrawRaceTime(CompetitionTasks_TicksToCentiseconds(elapsed_ticks));
    }
  }
}

void CompetitionTasks_Enter(uint8_t problem)
{
  if ((problem == 0U) || (problem > COMPETITION_TASK_COUNT))
  {
    return;
  }

  /* The retired menu remains link-compatible with the KEY2/KEY3 build. */
}

void CompetitionTasks_Task(uint8_t problem)
{
  if ((problem == 0U) || (problem > COMPETITION_TASK_COUNT))
  {
    return;
  }

  CompetitionTasks_Service();
}

void CompetitionTasks_Exit(uint8_t problem)
{
  if ((problem == 0U) || (problem > COMPETITION_TASK_COUNT))
  {
    return;
  }

  Car_Stop();
  g_competition_task_status.state = COMPETITION_STATE_IDLE;
  g_competition_task_status.elapsed_ticks = 0U;
  s_continuous_mode = 0U;
  s_display_dirty = 1U;
}

static uint8_t CompetitionTasks_ReadKeyRaw(void)
{
  return (DL_GPIO_readPins(GPIO_KEY_PORT, GPIO_KEY_START_PIN) != 0U) ? 1U : 0U;
}

static uint8_t CompetitionTasks_KeyPressed(uint32_t tick)
{
  uint8_t raw = CompetitionTasks_ReadKeyRaw();

  if (raw != s_key_last_raw)
  {
    s_key_last_raw = raw;
    s_key_debounce_tick = tick;
    if ((raw == 0U) && (s_key_press_pending == 0U))
    {
      s_key_press_pending = 1U;
      s_key_press_tick = tick;
      s_key_press_allowed =
          ((s_key_armed != 0U) &&
           ((g_competition_task_status.state == COMPETITION_STATE_IDLE) ||
            (g_competition_task_status.state == COMPETITION_STATE_FINISHED)))
              ? 1U
              : 0U;
    }
  }

  if ((uint32_t)(tick - s_key_debounce_tick) >=
      COMPETITION_KEY_DEBOUNCE_TICKS)
  {
    if (raw != s_key_stable)
    {
      s_key_stable = raw;

      if (s_key_stable != 0U)
      {
        s_key_armed = 1U;
        s_key_press_pending = 0U;
        s_key_press_allowed = 0U;
      }
      else if (s_key_armed != 0U)
      {
        uint8_t pressed = s_key_press_allowed;

        s_key_armed = 0U;
        s_key_press_pending = 0U;
        s_key_press_allowed = 0U;
        return pressed;
      }
    }
    else if ((raw != 0U) && (s_key_press_pending != 0U))
    {
      s_key_press_pending = 0U;
      s_key_press_allowed = 0U;
    }
  }

  return 0U;
}

static uint8_t CompetitionTasks_ReadContinuousKeyRaw(void)
{
  return (DL_GPIO_readPins(GPIO_KEY_PORT, GPIO_KEY_CONTINUOUS_PIN) != 0U)
             ? 1U
             : 0U;
}

static uint8_t CompetitionTasks_ContinuousKeyPressed(uint32_t tick)
{
  uint8_t raw = CompetitionTasks_ReadContinuousKeyRaw();

  if (raw != s_continuous_key_last_raw)
  {
    s_continuous_key_last_raw = raw;
    s_continuous_key_debounce_tick = tick;
    if ((raw == 0U) && (s_continuous_key_press_pending == 0U))
    {
      s_continuous_key_press_pending = 1U;
      s_continuous_key_press_tick = tick;
      s_continuous_key_press_allowed =
          ((s_continuous_key_armed != 0U) &&
           ((g_competition_task_status.state == COMPETITION_STATE_IDLE) ||
            (g_competition_task_status.state == COMPETITION_STATE_FINISHED)))
              ? 1U
              : 0U;
    }
  }

  if ((uint32_t)(tick - s_continuous_key_debounce_tick) >=
      COMPETITION_KEY_DEBOUNCE_TICKS)
  {
    if (raw != s_continuous_key_stable)
    {
      s_continuous_key_stable = raw;

      if (s_continuous_key_stable != 0U)
      {
        s_continuous_key_armed = 1U;
        s_continuous_key_press_pending = 0U;
        s_continuous_key_press_allowed = 0U;
      }
      else if (s_continuous_key_armed != 0U)
      {
        uint8_t pressed = s_continuous_key_press_allowed;

        s_continuous_key_armed = 0U;
        s_continuous_key_press_pending = 0U;
        s_continuous_key_press_allowed = 0U;
        return pressed;
      }
    }
    else if ((raw != 0U) && (s_continuous_key_press_pending != 0U))
    {
      s_continuous_key_press_pending = 0U;
      s_continuous_key_press_allowed = 0U;
    }
  }

  return 0U;
}

static uint8_t CompetitionTasks_FinishLineSeen(void)
{
  uint8_t mask = g_line.active_mask;

  /* The short A-line only spans the center S3/S4/S5 sensors. */
  return ((mask & COMPETITION_FINISH_CENTER_MASK) ==
          COMPETITION_FINISH_CENTER_MASK) ? 1U : 0U;
}

static uint32_t CompetitionTasks_AbsDelta(int32_t value, int32_t start)
{
  int32_t delta = value - start;

  if (delta < 0)
  {
    delta = -delta;
  }

  return (uint32_t)delta;
}

static uint8_t CompetitionTasks_AdvanceReached(void)
{
  uint32_t threshold = (uint32_t)COMPETITION_FINISH_ADVANCE_COUNTS;
  uint32_t left = 0U;
  uint32_t right = 0U;
  uint32_t average = 0U;

  left = CompetitionTasks_AbsDelta(
      g_car.left.encoder_total, s_finish_left_total);
  right = CompetitionTasks_AbsDelta(
      g_car.right.encoder_total, s_finish_right_total);
  average = (left / 2U) + (right / 2U) + ((left & right) & 1U);

  return (average >= threshold) ? 1U : 0U;
}

static uint32_t CompetitionTasks_TicksToCentiseconds(uint32_t ticks)
{
  uint32_t saturation_ticks =
      (COMPETITION_MAX_CENTISECONDS * 10U) / CAR_CONTROL_PERIOD_MS;

  if (ticks >= saturation_ticks)
  {
    return COMPETITION_MAX_CENTISECONDS;
  }

  return (ticks * CAR_CONTROL_PERIOD_MS) / 10U;
}

static uint32_t CompetitionTasks_SmoothStep(uint32_t position,
                                            uint32_t span)
{
  uint32_t x;
  uint32_t blend;

  if ((span == 0U) || (position >= span))
  {
    return COMPETITION_SMOOTHSTEP_SCALE;
  }

  x = ((position << 10) + (span / 2U)) / span;
  blend = (x * x * ((3U * COMPETITION_SMOOTHSTEP_SCALE) - (2U * x)) +
           (1UL << 19)) >> 20;
  return blend;
}

static void CompetitionTasks_UpdateSpeedProfile(uint32_t tick)
{
  uint32_t motion_ticks = tick - s_motion_start_tick;
  uint32_t command = 0U;

  if (motion_ticks < COMPETITION_ACCEL_TICKS)
  {
    uint32_t blend = CompetitionTasks_SmoothStep(
        motion_ticks, COMPETITION_ACCEL_TICKS);
    command = (COMPETITION_CRUISE_COUNTS * blend +
               (COMPETITION_SMOOTHSTEP_SCALE / 2U)) >> 10;
  }
  else if (s_continuous_mode != 0U)
  {
    command = COMPETITION_CRUISE_COUNTS;
  }
  else
  {
    OdometerControlProgress_t progress = Odometer_GetControlProgress();

    if ((progress.valid != 0U) &&
        (progress.lap_progress_tenths > s_max_lap_progress))
    {
      s_max_lap_progress = progress.lap_progress_tenths;
    }

    if ((progress.valid == 0U) ||
        (s_max_lap_progress <= COMPETITION_DECEL_START_PROGRESS))
    {
      command = COMPETITION_CRUISE_COUNTS;
    }
    else if (s_max_lap_progress >= COMPETITION_DECEL_END_PROGRESS)
    {
      command = COMPETITION_FINAL_COUNTS;
    }
    else
    {
      uint32_t span = COMPETITION_DECEL_END_PROGRESS -
                      COMPETITION_DECEL_START_PROGRESS;
      uint32_t position = s_max_lap_progress -
                          COMPETITION_DECEL_START_PROGRESS;
      uint32_t blend = CompetitionTasks_SmoothStep(position, span);
      uint32_t speed_drop = COMPETITION_CRUISE_COUNTS -
                            COMPETITION_FINAL_COUNTS;

      command = COMPETITION_CRUISE_COUNTS -
          ((speed_drop * blend +
            (COMPETITION_SMOOTHSTEP_SCALE / 2U)) >> 10);
    }
  }

  Car_SetLineFollowBaseCounts((int32_t)command);
}

static void CompetitionTasks_Start(uint32_t start_tick,
                                   uint32_t now_tick,
                                   uint8_t continuous_mode)
{
  g_competition_task_status.state = COMPETITION_STATE_LEAVING_A;
  g_competition_task_status.start_tick = start_tick;
  g_competition_task_status.elapsed_ticks = now_tick - start_tick;
  g_competition_task_status.finish_tick = 0U;

  s_leave_confirm_count = 0U;
  s_finish_confirm_count = 0U;
  s_finish_seen_tick = 0U;
  s_last_lap_ticks = 0U;
  s_lap_number = 0U;
  s_continuous_mode = (continuous_mode != 0U) ? 1U : 0U;
  s_finish_left_total = 0;
  s_finish_right_total = 0;
  s_display_dirty = 1U;
  s_motion_start_tick = now_tick;
  s_max_lap_progress = 0U;

  g_line.right_angle_enable = 0U;
  g_car.line.right_angle_assist_enable = 0U;
  Car_SetLineFollowBaseCounts(0);
  Car_StartLineFollow();
}

static void CompetitionTasks_CompleteLap(uint32_t tick)
{
  s_last_lap_ticks = tick - g_competition_task_status.start_tick;
  if (s_lap_number < UINT32_MAX)
  {
    s_lap_number++;
  }

  g_competition_task_status.finish_tick = tick;
  g_competition_task_status.elapsed_ticks = 0U;
  g_competition_task_status.start_tick = tick;
  g_competition_task_status.state = COMPETITION_STATE_LEAVING_A;

  s_leave_confirm_count = 0U;
  s_finish_confirm_count = 0U;
  s_finish_seen_tick = 0U;
  s_display_dirty = 1U;
}

static void CompetitionTasks_Complete(uint32_t tick)
{
  g_competition_task_status.finish_tick = tick;
  g_competition_task_status.elapsed_ticks =
      tick - g_competition_task_status.start_tick;
  g_competition_task_status.state = COMPETITION_STATE_FINISHED;
  s_continuous_mode = 0U;
  s_display_dirty = 1U;

  Car_BrakeHold();
}
