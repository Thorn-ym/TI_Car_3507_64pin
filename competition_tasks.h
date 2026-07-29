/*
 * H-problem vehicle run state machine.
 */

#ifndef COMPETITION_TASKS_H
#define COMPETITION_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define COMPETITION_TASK_COUNT 5U

#ifndef COMPETITION_FINISH_ADVANCE_COUNTS
#define COMPETITION_FINISH_ADVANCE_COUNTS 0U
#endif

typedef enum
{
  COMPETITION_STATE_IDLE = 0,
  COMPETITION_STATE_LEAVING_A,
  COMPETITION_STATE_RUNNING,
  COMPETITION_STATE_FINISH_APPROACH,
  COMPETITION_STATE_FINISHED
} CompetitionTaskState_t;

typedef struct
{
  CompetitionTaskState_t state;
  uint32_t start_tick;
  uint32_t elapsed_ticks;
  uint32_t finish_tick;
} CompetitionTaskStatus_t;

extern volatile CompetitionTaskStatus_t g_competition_task_status;

void CompetitionTasks_Init(void);
void CompetitionTasks_ControlStep(void);
void CompetitionTasks_Service(void);

/* Kept so the retired problem menu still compiles in existing CCS builds. */
void CompetitionTasks_Enter(uint8_t problem);
void CompetitionTasks_Task(uint8_t problem);
void CompetitionTasks_Exit(uint8_t problem);

#ifdef __cplusplus
}
#endif

#endif /* COMPETITION_TASKS_H */