/*
 * MPU6050 gyroscope input for line-following damping.
 */

#include "mpu6050.h"
#include "board_i2c.h"
#include "car_control.h"
#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

#define MPU6050_ADDR_LOW             0x68U
#define MPU6050_ADDR_HIGH            0x69U

#define MPU6050_REG_SMPLRT_DIV       0x19U
#define MPU6050_REG_CONFIG           0x1AU
#define MPU6050_REG_GYRO_CONFIG      0x1BU
#define MPU6050_REG_GYRO_ZOUT_H      0x47U
#define MPU6050_REG_PWR_MGMT_1       0x6BU
#define MPU6050_REG_WHO_AM_I         0x75U

#define MPU6050_WHO_AM_I_VALUE       0x68U
#define MPU6050_GYRO_LSB_PER_DPS     131.0f
#define MPU6050_FILTER_ALPHA         0.30f
#define MPU6050_CALIBRATION_SAMPLES  64U

volatile Mpu6050_t g_mpu6050 =
{
  .address = MPU6050_ADDR_LOW,
  .present = 0U,
  .valid = 0U,
  .gyro_z_raw = 0,
  .gyro_z_dps = 0.0f,
  .gyro_z_filtered_dps = 0.0f,
  .gyro_z_bias_dps = 0.0f,
  .sample_tick = 0U,
  .error_count = 0U,
};

static uint32_t s_last_sample_tick = 0U;

static void MPU6050_ClearGyroOutput(void);
static void MPU6050_MarkError(void);
static bool MPU6050_WriteReg(uint8_t reg, uint8_t value);
static bool MPU6050_ReadRegs(uint8_t reg, uint8_t *data, uint8_t len);
static bool MPU6050_ReadGyroZRaw(int16_t *raw);
static bool MPU6050_ProbeAddress(uint8_t address);
static bool MPU6050_Configure(void);
static void MPU6050_UpdateGyro(int16_t raw);

void MPU6050_Init(void)
{
  uint8_t found = 0U;

  MPU6050_ClearGyroOutput();
  g_mpu6050.address = MPU6050_ADDR_LOW;
  g_mpu6050.present = 0U;
  g_mpu6050.valid = 0U;
  g_mpu6050.gyro_z_bias_dps = 0.0f;
  g_mpu6050.sample_tick = 0U;
  g_mpu6050.error_count = 0U;
  s_last_sample_tick = g_car.control_tick;

  if (MPU6050_ProbeAddress(MPU6050_ADDR_LOW))
  {
    g_mpu6050.address = MPU6050_ADDR_LOW;
    found = 1U;
  }
  else if (MPU6050_ProbeAddress(MPU6050_ADDR_HIGH))
  {
    g_mpu6050.address = MPU6050_ADDR_HIGH;
    found = 1U;
  }

  if (found == 0U)
  {
    MPU6050_MarkError();
    return;
  }

  g_mpu6050.present = 1U;

  if (MPU6050_Configure())
  {
    MPU6050_Calibrate();
  }
  else
  {
    MPU6050_MarkError();
  }
}

void MPU6050_Task(void)
{
  int16_t raw = 0;
  uint32_t tick = g_car.control_tick;

  if ((uint32_t)(tick - s_last_sample_tick) < 1U)
  {
    return;
  }
  s_last_sample_tick = tick;

  if (g_mpu6050.present == 0U)
  {
    MPU6050_ClearGyroOutput();
    return;
  }

  if (MPU6050_ReadGyroZRaw(&raw))
  {
    MPU6050_UpdateGyro(raw);
  }
  else
  {
    MPU6050_MarkError();
  }
}

void MPU6050_Calibrate(void)
{
  uint32_t index = 0U;
  uint32_t valid_count = 0U;
  float sum = 0.0f;

  if (g_mpu6050.present == 0U)
  {
    MPU6050_ClearGyroOutput();
    return;
  }

  g_mpu6050.valid = 0U;
  g_mpu6050.gyro_z_bias_dps = 0.0f;

  for (index = 0U; index < MPU6050_CALIBRATION_SAMPLES; index++)
  {
    int16_t raw = 0;

    if (MPU6050_ReadGyroZRaw(&raw))
    {
      sum += (float)raw / MPU6050_GYRO_LSB_PER_DPS;
      valid_count++;
    }
    else
    {
      MPU6050_MarkError();
      break;
    }

    delay_cycles(32000U);
  }

  if (valid_count > 0U)
  {
    g_mpu6050.gyro_z_bias_dps = sum / (float)valid_count;
    g_mpu6050.gyro_z_filtered_dps = 0.0f;
    g_mpu6050.gyro_z_dps = 0.0f;
    g_car.line.gyro_z = 0.0f;
    g_mpu6050.valid = 1U;
  }
}

static void MPU6050_ClearGyroOutput(void)
{
  g_mpu6050.valid = 0U;
  g_mpu6050.gyro_z_raw = 0;
  g_mpu6050.gyro_z_dps = 0.0f;
  g_mpu6050.gyro_z_filtered_dps = 0.0f;
  g_car.line.gyro_z = 0.0f;
}

static void MPU6050_MarkError(void)
{
  g_mpu6050.error_count++;
  g_mpu6050.present = 0U;
  MPU6050_ClearGyroOutput();
}

static bool MPU6050_WriteReg(uint8_t reg, uint8_t value)
{
  return Board_I2C_WriteReg(g_mpu6050.address, reg, value);
}

static bool MPU6050_ReadRegs(uint8_t reg, uint8_t *data, uint8_t len)
{
  return Board_I2C_ReadRegs(g_mpu6050.address, reg, data, len);
}

static bool MPU6050_ReadGyroZRaw(int16_t *raw)
{
  uint8_t data[2];

  if (MPU6050_ReadRegs(MPU6050_REG_GYRO_ZOUT_H, data, 2U) == false)
  {
    return false;
  }

  *raw = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
  return true;
}

static bool MPU6050_ProbeAddress(uint8_t address)
{
  uint8_t who_am_i = 0U;
  uint8_t previous_address = g_mpu6050.address;
  bool ok = false;

  g_mpu6050.address = address;
  ok = MPU6050_ReadRegs(MPU6050_REG_WHO_AM_I, &who_am_i, 1U);
  g_mpu6050.address = previous_address;

  return ((ok != false) && (who_am_i == MPU6050_WHO_AM_I_VALUE)) ? true : false;
}

static bool MPU6050_Configure(void)
{
  bool ok = true;

  ok = ok && MPU6050_WriteReg(MPU6050_REG_PWR_MGMT_1, 0x00U);
  delay_cycles(320000U);
  ok = ok && MPU6050_WriteReg(MPU6050_REG_SMPLRT_DIV, 0x09U);
  ok = ok && MPU6050_WriteReg(MPU6050_REG_CONFIG, 0x03U);
  ok = ok && MPU6050_WriteReg(MPU6050_REG_GYRO_CONFIG, 0x00U);

  return ok;
}

static void MPU6050_UpdateGyro(int16_t raw)
{
  float gyro = ((float)raw / MPU6050_GYRO_LSB_PER_DPS) -
               g_mpu6050.gyro_z_bias_dps;

  g_mpu6050.gyro_z_raw = raw;
  g_mpu6050.gyro_z_dps = gyro;
  g_mpu6050.gyro_z_filtered_dps +=
      MPU6050_FILTER_ALPHA * (gyro - g_mpu6050.gyro_z_filtered_dps);
  g_mpu6050.valid = 1U;
  g_mpu6050.present = 1U;
  g_mpu6050.sample_tick++;
  g_car.line.gyro_z = g_mpu6050.gyro_z_filtered_dps;
}
