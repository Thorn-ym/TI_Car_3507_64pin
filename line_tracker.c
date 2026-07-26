/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : line_tracker.c
  * @brief          : Seven channel digital line tracker input.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "line_tracker.h"

volatile LineTracker_t g_line =
{
  .raw_mask = 0U,
  .active_mask = 0U,
  .active_count = 0U,
  .line_seen = 0U,
  .active_low = 0U,
  .position = 0,
  .error = 0,
  .sample_tick = 0U,
  .right_angle_enable = 1U,
  .right_angle_detected = 0U,
  .right_angle_direction = 0,
  .right_angle_error = 50,
};

static GPIO_Regs * const line_ports[LINE_TRACKER_CHANNELS] =
{
  GPIO_LINE_PORT,
  GPIO_LINE_PORT,
  GPIO_LINE_PORT,
  GPIO_LINE_PORT,
  GPIO_LINE_PORT,
  GPIO_LINE_PORT,
  GPIO_LINE_PORT,
};

static const uint32_t line_pins[LINE_TRACKER_CHANNELS] =
{
  GPIO_LINE_S1_PIN,
  GPIO_LINE_S2_PIN,
  GPIO_LINE_S3_PIN,
  GPIO_LINE_S4_PIN,
  GPIO_LINE_S5_PIN,
  GPIO_LINE_S6_PIN,
  GPIO_LINE_S7_PIN,
};

static const int16_t line_weights[LINE_TRACKER_CHANNELS] =
{
  50,
  33,
  16,
  0,
  -16,
  -33,
  -50,
};

void LineTracker_Init(void)
{
  LineTracker_Update();
}

void LineTracker_Update(void)
{
  uint8_t raw_mask = 0U;
  uint8_t active_mask = 0U;
  uint8_t active_count = 0U;
  int16_t error = 0;
  int32_t weighted_sum = 0;
  uint32_t index = 0U;

  for (index = 0U; index < LINE_TRACKER_CHANNELS; index++)
  {
    uint8_t is_set = (DL_GPIO_readPins(line_ports[index], line_pins[index]) != 0U) ? 1U : 0U;
    uint8_t is_active = 0U;

    if (is_set != 0U)
    {
      raw_mask |= (uint8_t)(1U << index);
    }

    is_active = (g_line.active_low != 0U) ? (uint8_t)(is_set == 0U) : is_set;
    if (is_active != 0U)
    {
      active_mask |= (uint8_t)(1U << index);
      weighted_sum += line_weights[index];
      active_count++;
    }
  }

  g_line.raw_mask = raw_mask;
  g_line.active_mask = active_mask;
  g_line.active_count = active_count;
  g_line.line_seen = (active_count > 0U) ? 1U : 0U;

  if (active_count > 0U)
  {
    error = (int16_t)(weighted_sum / (int32_t)active_count);

    g_line.right_angle_detected = 0U;
    g_line.right_angle_direction = 0;

    if (g_line.right_angle_enable != 0U)
    {
      uint8_t left_right_angle = ((active_mask & 0x0FU) == 0x0FU) ? 1U : 0U;
      uint8_t right_right_angle = ((active_mask & 0x78U) == 0x78U) ? 1U : 0U;

      if ((left_right_angle != 0U) && (right_right_angle == 0U))
      {
        error = g_line.right_angle_error;
        g_line.right_angle_detected = 1U;
        g_line.right_angle_direction = 1;
      }
      else if ((right_right_angle != 0U) && (left_right_angle == 0U))
      {
        error = (int16_t)-g_line.right_angle_error;
        g_line.right_angle_detected = 1U;
        g_line.right_angle_direction = -1;
      }
    }

    g_line.position = error;
    g_line.error = error;
  }
  else
  {
    g_line.right_angle_detected = 0U;
    g_line.right_angle_direction = 0;
  }

  g_line.sample_tick++;
}
