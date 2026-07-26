/*
 * Competition problem slots.
 */

#ifndef COMPETITION_TASKS_H
#define COMPETITION_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define COMPETITION_TASK_COUNT 5U

void CompetitionTasks_Enter(uint8_t problem);
void CompetitionTasks_Task(uint8_t problem);
void CompetitionTasks_Exit(uint8_t problem);

#ifdef __cplusplus
}
#endif

#endif /* COMPETITION_TASKS_H */
