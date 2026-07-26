/*
 * OLED and EC11 based competition problem selector.
 */

#ifndef PROBLEM_MENU_H
#define PROBLEM_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  PROBLEM_MENU_STATE_SELECT = 0,
  PROBLEM_MENU_STATE_RUNNING = 1
} ProblemMenuState_t;

typedef struct
{
  ProblemMenuState_t state;
  uint8_t selected_problem;
  uint8_t running_problem;
} ProblemMenu_t;

extern volatile ProblemMenu_t g_problem_menu;

void ProblemMenu_Init(void);
void ProblemMenu_Task(void);

#ifdef __cplusplus
}
#endif

#endif /* PROBLEM_MENU_H */
