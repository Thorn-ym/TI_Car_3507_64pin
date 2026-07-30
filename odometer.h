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

void Odometer_Init(void);
void Odometer_ControlStep(void);
void Odometer_Service(void);

#ifdef __cplusplus
}
#endif

#endif /* ODOMETER_H */
