/*
 * Competition problem slots.
 */

#include "competition_tasks.h"
#include "car_control.h"

#include <stdint.h>

typedef void (*CompetitionTaskFn)(void);

static void Problem1_Enter(void);
static void Problem1_Task(void);
static void Problem1_Exit(void);
static void Problem2_Enter(void);
static void Problem2_Task(void);
static void Problem2_Exit(void);
static void Problem3_Enter(void);
static void Problem3_Task(void);
static void Problem3_Exit(void);
static void Problem4_Enter(void);
static void Problem4_Task(void);
static void Problem4_Exit(void);
static void Problem5_Enter(void);
static void Problem5_Task(void);
static void Problem5_Exit(void);
static void CompetitionTasks_DefaultEnter(void);
static void CompetitionTasks_DefaultTask(void);
static void CompetitionTasks_DefaultExit(void);

static const CompetitionTaskFn s_enter_table[COMPETITION_TASK_COUNT] =
{
  Problem1_Enter,
  Problem2_Enter,
  Problem3_Enter,
  Problem4_Enter,
  Problem5_Enter,
};

static const CompetitionTaskFn s_task_table[COMPETITION_TASK_COUNT] =
{
  Problem1_Task,
  Problem2_Task,
  Problem3_Task,
  Problem4_Task,
  Problem5_Task,
};

static const CompetitionTaskFn s_exit_table[COMPETITION_TASK_COUNT] =
{
  Problem1_Exit,
  Problem2_Exit,
  Problem3_Exit,
  Problem4_Exit,
  Problem5_Exit,
};

void CompetitionTasks_Enter(uint8_t problem)
{
  if ((problem == 0U) || (problem > COMPETITION_TASK_COUNT))
  {
    return;
  }

  s_enter_table[problem - 1U]();
}

void CompetitionTasks_Task(uint8_t problem)
{
  if ((problem == 0U) || (problem > COMPETITION_TASK_COUNT))
  {
    return;
  }

  s_task_table[problem - 1U]();
}

void CompetitionTasks_Exit(uint8_t problem)
{
  if ((problem == 0U) || (problem > COMPETITION_TASK_COUNT))
  {
    return;
  }

  s_exit_table[problem - 1U]();
  Car_Stop();
}

static void Problem1_Enter(void)
{
  CompetitionTasks_DefaultEnter();
}

static void Problem1_Task(void)
{
  CompetitionTasks_DefaultTask();
}

static void Problem1_Exit(void)
{
  CompetitionTasks_DefaultExit();
}

static void Problem2_Enter(void)
{
  CompetitionTasks_DefaultEnter();
}

static void Problem2_Task(void)
{
  CompetitionTasks_DefaultTask();
}

static void Problem2_Exit(void)
{
  CompetitionTasks_DefaultExit();
}

static void Problem3_Enter(void)
{
  CompetitionTasks_DefaultEnter();
}

static void Problem3_Task(void)
{
  CompetitionTasks_DefaultTask();
}

static void Problem3_Exit(void)
{
  CompetitionTasks_DefaultExit();
}

static void Problem4_Enter(void)
{
  CompetitionTasks_DefaultEnter();
}

static void Problem4_Task(void)
{
  CompetitionTasks_DefaultTask();
}

static void Problem4_Exit(void)
{
  CompetitionTasks_DefaultExit();
}

static void Problem5_Enter(void)
{
  CompetitionTasks_DefaultEnter();
}

static void Problem5_Task(void)
{
  CompetitionTasks_DefaultTask();
}

static void Problem5_Exit(void)
{
  CompetitionTasks_DefaultExit();
}

static void CompetitionTasks_DefaultEnter(void)
{
  Car_StartLineFollow();
}

static void CompetitionTasks_DefaultTask(void)
{
}

static void CompetitionTasks_DefaultExit(void)
{
}
