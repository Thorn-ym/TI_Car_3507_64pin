/*
 * MPU6050 gyroscope input for line-following damping.
 */

#ifndef MPU6050_H
#define MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  uint8_t address;
  uint8_t present;
  uint8_t valid;
  int16_t gyro_z_raw;
  float gyro_z_dps;
  float gyro_z_filtered_dps;
  float gyro_z_bias_dps;
  uint32_t sample_tick;
  uint32_t error_count;
} Mpu6050_t;

extern volatile Mpu6050_t g_mpu6050;

void MPU6050_Init(void);
void MPU6050_Task(void);
void MPU6050_Calibrate(void);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
