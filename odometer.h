/*
 * Encoder odometer used by the odometer calibration branch.
 */

#ifndef ODOMETER_H
#define ODOMETER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Fill these with positive absolute counts measured over one metre forward. */
#ifndef ODOMETER_LEFT_COUNTS_PER_METER
#define ODOMETER_LEFT_COUNTS_PER_METER 0U
#endif

#ifndef ODOMETER_RIGHT_COUNTS_PER_METER
#define ODOMETER_RIGHT_COUNTS_PER_METER 0U
#endif

/* Median encoder counts from five KEY2-to-finish whole-lap runs. */
#ifndef ODOMETER_LEFT_COUNTS_PER_LAP
#define ODOMETER_LEFT_COUNTS_PER_LAP 49712U
#endif

#ifndef ODOMETER_RIGHT_COUNTS_PER_LAP
#define ODOMETER_RIGHT_COUNTS_PER_LAP 39741U
#endif

typedef struct
{
  uint16_t lap_progress_tenths;
  uint8_t valid;
  uint8_t frozen;
} OdometerControlProgress_t;

void Odometer_Init(void);
void Odometer_ControlStep(void);
void Odometer_Service(void);
OdometerControlProgress_t Odometer_GetControlProgress(void);

#ifdef __cplusplus
}
#endif

#endif /* ODOMETER_H */
