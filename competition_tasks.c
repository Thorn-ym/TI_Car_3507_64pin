/*
 * H-problem vehicle run state machine.
 */

#include "competition_tasks.h"

#include "car_control.h"
#include "line_tracker.h"
#include "oled_ssd1306.h"
#include "ti_msp_dl_config.h"

#include <stdint.h>

#define COMPETITION_KEY_DEBOUNCE_TICKS       3U
#define COMPETITION_LEAVE_CONFIRM_TICKS       5U
#define COMPETITION_FINISH_CONFIRM_TICKS      2U
#define COMPETITION_MIN_RUN_TICKS           100U
#define COMPETITION_DISPLAY_INTERVAL_TICKS   10U
#define COMPETITION_FINISH_ACTIVE_MIN         5U
#define COMPETITION_FINISH_LEFT_MASK        0x07U
#define COMPETITION_FINISH_RIGHT_MASK       0x70U
#define COMPETITION_MAX_CENTISECONDS        9999U

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
static uint32_t s_key_press_tick = 0U;
static int32_t s_finish_left_total = 0;
static int32_t s_finish_right_total = 0;
static uint8_t s_key_last_raw = 1U;
static uint8_t s_key_stable = 1U;
static uint8_t s_key_armed = 1U;
static uint8_t s_key_press_pending = 0U;
static uint8_t s_key_press_allowed = 0U;
static uint8_t s_leave_confirm_count = 0U;
static uint8_t s_finish_confirm_count = 0U;
static volatile uint8_t s_display_dirty = 0U;
static volatile uint8_t s_initialized = 0U;

static uint8_t CompetitionTasks_ReadKeyRaw(void);
static uint8_t CompetitionTasks_KeyPressed(uint32_t tick);
static uint8_t CompetitionTasks_FinishLineSeen(void);
static uint32_t CompetitionTasks_AbsDelta(int32_t value, int32_t start);
static uint8_t CompetitionTasks_AdvanceReached(void);
static uint32_t CompetitionTasks_TicksToCentiseconds(uint32_t ticks);
static void CompetitionTasks_Start(uint32_t start_tick, uint32_t now_tick);
static void CompetitionTasks_Complete(uint32_t tick);

void CompetitionTasks_Init(void)
{
  uint32_t tick = g_car.control_tick;
  uint8_t raw = CompetitionTasks_ReadKeyRaw();

  s_initialized = 0U;
  g_competition_task_status.state = COMPETITION_STATE_IDLE;
  g_competition_task_status.start_tick = tick;
  g_competition_task_status.elapsed_ticks = 0U;
  g_competition_task_status.finish_tick = tick;

  s_last_control_tick = tick;
  s_last_display_tick = tick;
  s_key_debounce_tick = tick;
  s_key_press_tick = tick;
  s_key_last_raw = raw;
  s_key_stable = raw;
  s_key_armed = (raw != 0U) ? 1U : 0U;
  s_key_press_pending = 0U;
  s_key_press_allowed = 0U;
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

  if ((s_initialized == 0U) || (tick == s_last_control_tick))
  {
    return;
  }
  s_last_control_tick = tick;

  key_pressed = CompetitionTasks_KeyPressed(tick);
  if ((key_pressed != 0U) &&
      ((g_competition_task_status.state == COMPETITION_STATE_IDLE) ||
       (g_competition_task_status.state == COMPETITION_STATE_FINISHED)))
  {
    CompetitionTasks_Start(s_key_press_tick, tick);
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
        if (s_finish_confirm_count < COMPETITION_FINISH_CONFIRM_TICKS)
        {
          s_finish_confirm_count++;
        }
      }
      else
      {
        s_finish_confirm_count = 0U;
      }

      if (s_finish_confirm_count >= COMPETITION_FINISH_CONFIRM_TICKS)
      {
        s_finish_left_total = g_car.left.encoder_total;
        s_finish_right_total = g_car.right.encoder_total;

        if (COMPETITION_FINISH_ADVANCE_COUNTS == 0U)
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
  }
}

void CompetitionTasks_Service(void)
{
  uint32_t tick = g_car.control_tick;
  uint32_t elapsed_ticks = 0U;
  uint32_t primask = 0U;
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

  if (s_display_dirty != 0U)
  {
    s_display_dirty = 0U;
    s_last_display_tick = tick;
    draw = 1U;
  }
  else if (((state == COMPETITION_STATE_LEAVING_A) ||
            (state == COMPETITION_STATE_RUNNING) ||
            (state == COMPETITION_STATE_FINISH_APPROACH)) &&
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
    OLED_DrawRaceTime(CompetitionTasks_TicksToCentiseconds(elapsed_ticks));
  }
}

void CompetitionTasks_Enter(uint8_t problem)
{
  if ((problem == 0U) || (problem > COMPETITION_TASK_COUNT))
  {
    return;
  }

  /* The retired menu remains link-compatible; only KEY2 starts a run. */
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

static uint8_t CompetitionTasks_FinishLineSeen(void)
{
  uint8_t mask = g_line.active_mask;
  uint8_t left_seen = ((mask & COMPETITION_FINISH_LEFT_MASK) != 0U) ? 1U : 0U;
  uint8_t right_seen = ((mask & COMPETITION_FINISH_RIGHT_MASK) != 0U) ? 1U : 0U;

  return ((g_line.active_count >= COMPETITION_FINISH_ACTIVE_MIN) &&
          (left_seen != 0U) &&
          (right_seen != 0U)) ? 1U : 0U;
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

static void CompetitionTasks_Start(uint32_t start_tick, uint32_t now_tick)
{
  g_competition_task_status.state = COMPETITION_STATE_LEAVING_A;
  g_competition_task_status.start_tick = start_tick;
  g_competition_task_status.elapsed_ticks = now_tick - start_tick;
  g_competition_task_status.finish_tick = 0U;

  s_leave_confirm_count = 0U;
  s_finish_confirm_count = 0U;
  s_finish_left_total = 0;
  s_finish_right_total = 0;
  s_display_dirty = 1U;

  g_line.right_angle_enable = 0U;
  g_car.line.right_angle_assist_enable = 0U;
  Car_StartLineFollow();
}

static void CompetitionTasks_Complete(uint32_t tick)
{
  g_competition_task_status.finish_tick = tick;
  g_competition_task_status.elapsed_ticks =
      tick - g_competition_task_status.start_tick;
  g_competition_task_status.state = COMPETITION_STATE_FINISHED;
  s_display_dirty = 1U;

  Car_BrakeHold();
}