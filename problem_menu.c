/*
 * OLED and EC11 based competition problem selector.
 */

#include "problem_menu.h"
#include "competition_tasks.h"
#include "ec11_encoder.h"
#include "oled_ssd1306.h"

#include <stdint.h>

volatile ProblemMenu_t g_problem_menu =
{
  .state = PROBLEM_MENU_STATE_SELECT,
  .selected_problem = 1U,
  .running_problem = 0U,
};

static uint8_t s_last_drawn_state = 0xFFU;
static uint8_t s_last_drawn_problem = 0U;

static void ProblemMenu_DrawIfChanged(void);
static uint8_t ProblemMenu_ClampProblem(int16_t problem);

void ProblemMenu_Init(void)
{
  g_problem_menu.state = PROBLEM_MENU_STATE_SELECT;
  g_problem_menu.selected_problem = 1U;
  g_problem_menu.running_problem = 0U;
  s_last_drawn_state = 0xFFU;
  s_last_drawn_problem = 0U;

  ProblemMenu_DrawIfChanged();
}

void ProblemMenu_Task(void)
{
  int8_t delta = EC11_GetDelta();
  uint8_t button_event = EC11_GetButtonEvent();

  if (g_problem_menu.state == PROBLEM_MENU_STATE_SELECT)
  {
    if (delta != 0)
    {
      g_problem_menu.selected_problem =
          ProblemMenu_ClampProblem((int16_t)g_problem_menu.selected_problem + delta);
    }

    if (button_event == EC11_BUTTON_CLICK)
    {
      g_problem_menu.running_problem = g_problem_menu.selected_problem;
      g_problem_menu.state = PROBLEM_MENU_STATE_RUNNING;
      ProblemMenu_DrawIfChanged();
      CompetitionTasks_Enter(g_problem_menu.running_problem);
    }
  }
  else
  {
    if (button_event == EC11_BUTTON_LONG_PRESS)
    {
      CompetitionTasks_Exit(g_problem_menu.running_problem);
      g_problem_menu.running_problem = 0U;
      g_problem_menu.state = PROBLEM_MENU_STATE_SELECT;
    }
    else
    {
      CompetitionTasks_Task(g_problem_menu.running_problem);
    }
  }

  ProblemMenu_DrawIfChanged();
}

static void ProblemMenu_DrawIfChanged(void)
{
  uint8_t display_problem = (g_problem_menu.state == PROBLEM_MENU_STATE_RUNNING) ?
      g_problem_menu.running_problem : g_problem_menu.selected_problem;

  if ((s_last_drawn_state == (uint8_t)g_problem_menu.state) &&
      (s_last_drawn_problem == display_problem))
  {
    return;
  }

  if (g_problem_menu.state == PROBLEM_MENU_STATE_RUNNING)
  {
    OLED_DrawProblemSelected(display_problem);
  }
  else
  {
    OLED_DrawProblemSelect(display_problem);
  }

  s_last_drawn_state = (uint8_t)g_problem_menu.state;
  s_last_drawn_problem = display_problem;
}

static uint8_t ProblemMenu_ClampProblem(int16_t problem)
{
  if (problem < 1)
  {
    return 1U;
  }

  if (problem > (int16_t)COMPETITION_TASK_COUNT)
  {
    return COMPETITION_TASK_COUNT;
  }

  return (uint8_t)problem;
}
