/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : car_control.c
  * @brief          : Two motor speed PID control for TB6612 encoder car.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "car_control.h"
#include "ec11_encoder.h"
#include "line_tracker.h"
#include "mpu6050.h"

volatile CarControl_t g_car =
{
  .mode = CAR_MODE_OPEN_LOOP,
  .control_period_ms = CAR_CONTROL_PERIOD_MS,
  .control_tick = 0U,
  .driver_enabled = 0U,
  .left =
  {
    .pid =
    {
      .kp = 180,
      .ki = 0.35,
      .kd = 26,
      .integral = 3199,
      .previous_error = 0.0f,
      .output_limit = CAR_PID_OUTPUT_MAX,
      .integral_limit = CAR_PID_INTEGRAL_MAX,
    },
    .target_counts = 26,
    .measured_counts = 0,
    .encoder_delta = 0,
    .encoder_total = 0,
    .encoder_raw = 0,
    .encoder_last = 0,
    .manual_pwm = 0,
    .pwm_output = 0,
    .invert_motor = 1,
    .invert_encoder = 0U,
  },
  .right =
  {
    .pid =
    {
      .kp = 180,
      .ki = 0.28,
      .kd = 18,
      .integral = 3199,
      .previous_error = 0.0f,
      .output_limit = CAR_PID_OUTPUT_MAX,
      .integral_limit = CAR_PID_INTEGRAL_MAX,
    },
    .target_counts =  26,
    .measured_counts = 0,
    .encoder_delta = 0,
    .encoder_total = 0,
    .encoder_raw = 0,
    .encoder_last = 0,
    .manual_pwm = 0,
    .pwm_output = 0,
    .invert_motor = 1,
    .invert_encoder = 1,
  },
  .line =
  {
    .pid =
    {
      .kp = 0.9f,
      .ki = 0.05f,
      .kd = 0.2f,
      .integral = 0.0f,
      .previous_error = 0.0f,
      .output_limit = 30.0f,
      .integral_limit = 1000.0f,
    },
    .base_counts = 26,
    .correction_counts = 0,
    .left_target_counts = 0,
    .right_target_counts = 0,
    .gyro_z = 0.0f,
    .gyro_damping = 0.04f,
    .right_angle_yaw_deg = 0.0f,
    .right_angle_target_deg = 150.0f,
    .right_angle_center_min_deg = 45.0f,
    .right_angle_gyro_deadband_dps = 2.0f,
    .right_angle_base_counts = 14,
    .right_angle_turn_counts = 4,
    .right_angle_approach_counts = 100,
    .right_angle_approach_speed_counts = 18,
    .right_angle_approach_travel_counts = 0,
    .right_angle_approach_start_left_total = 0,
    .right_angle_approach_start_right_total = 0,
    .right_angle_start_tick = 0U,
    .right_angle_last_tick = 0U,
    .right_angle_timeout_ticks = 250U,
    .right_angle_approach_start_tick = 0U,
    .right_angle_approach_timeout_ticks = 45U,
    .right_angle_state = CAR_RIGHT_ANGLE_STATE_IDLE,
    .right_angle_assist_enable = 1U,
    .right_angle_assist_active = 0U,
    .right_angle_approach_active = 0U,
    .right_angle_cooldown = 0U,
    .right_angle_assist_direction = 0,
    .right_angle_approach_direction = 0,
    .right_angle_detect_direction = 0,
    .right_angle_detect_count = 0U,
    .right_angle_detect_confirm_ticks = 2U,
    .right_angle_old_line_clear_count = 0U,
    .right_angle_old_line_clear_confirm_ticks = 2U,
    .right_angle_center_seen_count = 0U,
    .right_angle_cooldown_center_count = 0U,
    .right_angle_center_confirm_ticks = 3U,
    .right_angle_recovery_count = 0U,
    .right_angle_recovery_ticks = 5U,
    .right_angle_recovery_speed_counts = 18,
    .line_lost_stop = 1U,
  },
};

static volatile int32_t s_right_encoder_count = 0;
static uint8_t s_right_encoder_state = 0U;

static int16_t Car_LimitPwm(int32_t pwm);
static float Car_AbsFloat(float value);
static int32_t Car_AbsInt32(int32_t value);
static float Car_LimitFloat(float value, float limit);
static int32_t Car_LimitTargetCounts(int32_t counts);
static int16_t Car_PidStep(volatile CarMotor_t *motor);
static int32_t Car_LinePidStep(int32_t error);
static void Car_ResetPid(volatile CarMotor_t *motor);
static void Car_ResetLinePid(void);
static uint8_t Car_RightAngleCenterSeen(void);
static uint8_t Car_RightAngleNewLineSeen(void);
static void Car_RightAngleSetTargets(int32_t left_target,
                                     int32_t right_target,
                                     int16_t *left_pwm,
                                     int16_t *right_pwm);
static void Car_RightAngleApproachStart(int8_t direction);
static void Car_RightAngleApproachStop(void);
static int32_t Car_RightAngleApproachDistance(void);
static uint8_t Car_RightAngleApproachComplete(void);
static uint8_t Car_RightAngleApproachStep(int16_t *left_pwm, int16_t *right_pwm);
static void Car_RightAngleAssistStart(int8_t direction);
static void Car_RightAngleAssistStop(void);
static void Car_RightAngleAssistUpdateCooldown(void);
static int8_t Car_RightAngleDetectStep(void);
static void Car_RightAngleRecoveryStart(void);
static uint8_t Car_RightAngleRecoveryStep(int16_t *left_pwm, int16_t *right_pwm);
static uint8_t Car_RightAngleAssistStep(int16_t *left_pwm, int16_t *right_pwm);
static void Car_UpdateLeftEncoder(volatile CarMotor_t *motor);
static void Car_UpdateRightEncoder(volatile CarMotor_t *motor);
static void Car_SetDriverEnable(uint8_t enable);
static void Car_WritePin(GPIO_Regs *port, uint32_t pin, uint8_t set);
static void Car_ApplyMotor(volatile CarMotor_t *motor,
                           uint32_t channel,
                           GPIO_Regs *in1_port,
                           uint32_t in1_pin,
                           GPIO_Regs *in2_port,
                           uint32_t in2_pin);
static uint8_t Car_ReadRightEncoderState(void);
static void Car_UpdateRightEncoderCount(void);

void Car_Init(void)
{
  DL_TimerG_setTimerCount(QEI_0_INST, 0U);
  s_right_encoder_count = 0;
  s_right_encoder_state = Car_ReadRightEncoderState();

  DL_TimerA_setCaptureCompareValue(PWM_0_INST, 0U, GPIO_PWM_0_C0_IDX);
  DL_TimerA_setCaptureCompareValue(PWM_0_INST, 0U, GPIO_PWM_0_C1_IDX);

  DL_TimerG_startCounter(QEI_0_INST);
  DL_TimerA_startCounter(PWM_0_INST);

  g_car.left.encoder_last = 0;
  g_car.right.encoder_last = 0;
  g_car.left.encoder_raw = 0;
  g_car.right.encoder_raw = 0;

  Car_Stop();

  NVIC_EnableIRQ(GPIO_ENCODER_INT_IRQN);
  NVIC_EnableIRQ(GPIO_EC11_INT_IRQN);
  NVIC_EnableIRQ(TIMER_CONTROL_INST_INT_IRQN);
  DL_TimerG_startCounter(TIMER_CONTROL_INST);
}

void Car_ControlStep(void)
{
  int16_t left_pwm = 0;
  int16_t right_pwm = 0;

  g_car.control_tick++;

  Car_UpdateLeftEncoder(&g_car.left);
  Car_UpdateRightEncoder(&g_car.right);
  LineTracker_Update();

  switch (g_car.mode)
  {
    case CAR_MODE_OPEN_LOOP:
      left_pwm = Car_LimitPwm(g_car.left.manual_pwm);
      right_pwm = Car_LimitPwm(g_car.right.manual_pwm);
      break;

    case CAR_MODE_SPEED_PID:
      left_pwm = Car_PidStep(&g_car.left);
      right_pwm = Car_PidStep(&g_car.right);
      break;

    case CAR_MODE_LINE_FOLLOW:
      if (Car_RightAngleAssistStep(&left_pwm, &right_pwm) != 0U)
      {
        break;
      }

      if ((g_line.line_seen != 0U) || (g_car.line.line_lost_stop == 0U))
      {
        int32_t correction = Car_LinePidStep((int32_t)g_line.error);
        int32_t left_target = g_car.line.base_counts - correction;
        int32_t right_target = g_car.line.base_counts + correction;

        g_car.line.correction_counts = correction;
        g_car.line.left_target_counts = left_target;
        g_car.line.right_target_counts = right_target;
        g_car.left.target_counts = left_target;
        g_car.right.target_counts = right_target;

        left_pwm = Car_PidStep(&g_car.left);
        right_pwm = Car_PidStep(&g_car.right);
      }
      else
      {
        Car_RightAngleAssistStop();
        g_car.line.correction_counts = 0;
        g_car.line.left_target_counts = 0;
        g_car.line.right_target_counts = 0;
        g_car.left.target_counts = 0;
        g_car.right.target_counts = 0;
        Car_ResetLinePid();
        Car_ResetPid(&g_car.left);
        Car_ResetPid(&g_car.right);
        left_pwm = 0;
        right_pwm = 0;
      }
      break;

    case CAR_MODE_DISABLED:
    default:
      Car_RightAngleAssistStop();
      Car_ResetLinePid();
      Car_ResetPid(&g_car.left);
      Car_ResetPid(&g_car.right);
      left_pwm = 0;
      right_pwm = 0;
      break;
  }

  g_car.left.pwm_output = left_pwm;
  g_car.right.pwm_output = right_pwm;

  if (g_car.mode == CAR_MODE_DISABLED)
  {
    Car_SetDriverEnable(0U);
  }
  else
  {
    Car_SetDriverEnable(1U);
  }

  Car_ApplyMotor(&g_car.left,
                 GPIO_PWM_0_C0_IDX,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_AIN1_PIN,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_AIN2_PIN);
  Car_ApplyMotor(&g_car.right,
                 GPIO_PWM_0_C1_IDX,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_BIN1_PIN,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_BIN2_PIN);
}

void Car_Stop(void)
{
  g_car.mode = CAR_MODE_DISABLED;
  g_car.left.target_counts = 0;
  g_car.right.target_counts = 0;
  g_car.left.manual_pwm = 0;
  g_car.right.manual_pwm = 0;
  g_car.left.pwm_output = 0;
  g_car.right.pwm_output = 0;
  Car_ResetPid(&g_car.left);
  Car_ResetPid(&g_car.right);
  Car_SetDriverEnable(0U);
  Car_ApplyMotor(&g_car.left,
                 GPIO_PWM_0_C0_IDX,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_AIN1_PIN,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_AIN2_PIN);
  Car_ApplyMotor(&g_car.right,
                 GPIO_PWM_0_C1_IDX,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_BIN1_PIN,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_BIN2_PIN);
}

void Car_StartLineFollow(void)
{
  Car_ResetLinePid();
  Car_ResetPid(&g_car.left);
  Car_ResetPid(&g_car.right);
  g_car.mode = CAR_MODE_LINE_FOLLOW;
}

void Car_SetSpeedTargets(int32_t left_counts, int32_t right_counts)
{
  g_car.left.target_counts = left_counts;
  g_car.right.target_counts = right_counts;
  g_car.mode = CAR_MODE_SPEED_PID;
}

void GROUP1_IRQHandler(void)
{
  uint32_t gpiob = DL_GPIO_getEnabledInterruptStatus(
      GPIO_ENCODER_PORT,
      GPIO_ENCODER_E2A_PIN | GPIO_ENCODER_E2B_PIN);
  uint32_t gpioa = DL_GPIO_getEnabledInterruptStatus(
      GPIO_EC11_PORT,
      GPIO_EC11_A_PIN | GPIO_EC11_B_PIN);

  if ((gpiob & (GPIO_ENCODER_E2A_PIN | GPIO_ENCODER_E2B_PIN)) != 0U)
  {
    Car_UpdateRightEncoderCount();
    DL_GPIO_clearInterruptStatus(
        GPIO_ENCODER_PORT,
        gpiob & (GPIO_ENCODER_E2A_PIN | GPIO_ENCODER_E2B_PIN));
  }

  if ((gpioa & (GPIO_EC11_A_PIN | GPIO_EC11_B_PIN)) != 0U)
  {
    EC11_HandleABInterrupt(gpioa);
    DL_GPIO_clearInterruptStatus(
        GPIO_EC11_PORT,
        gpioa & (GPIO_EC11_A_PIN | GPIO_EC11_B_PIN));
  }
}

static int16_t Car_LimitPwm(int32_t pwm)
{
  if (pwm > CAR_PWM_MAX)
  {
    return CAR_PWM_MAX;
  }

  if (pwm < -CAR_PWM_MAX)
  {
    return -CAR_PWM_MAX;
  }

  return (int16_t)pwm;
}

static float Car_AbsFloat(float value)
{
  return (value < 0.0f) ? -value : value;
}

static int32_t Car_AbsInt32(int32_t value)
{
  return (value < 0) ? -value : value;
}

static float Car_LimitFloat(float value, float limit)
{
  float positive_limit = Car_AbsFloat(limit);

  if (positive_limit <= 0.0f)
  {
    return 0.0f;
  }

  if (value > positive_limit)
  {
    return positive_limit;
  }

  if (value < -positive_limit)
  {
    return -positive_limit;
  }

  return value;
}

static int32_t Car_LimitTargetCounts(int32_t counts)
{
  if (counts < 0)
  {
    return 0;
  }

  if (counts > CAR_RIGHT_ANGLE_TARGET_COUNTS_MAX)
  {
    return CAR_RIGHT_ANGLE_TARGET_COUNTS_MAX;
  }

  return counts;
}

static int16_t Car_PidStep(volatile CarMotor_t *motor)
{
  float error = (float)motor->target_counts - (float)motor->measured_counts;
  float derivative = error - motor->pid.previous_error;
  float output = 0.0f;

  motor->pid.integral += error;
  motor->pid.integral = Car_LimitFloat(motor->pid.integral, motor->pid.integral_limit);

  output = (motor->pid.kp * error) +
           (motor->pid.ki * motor->pid.integral) +
           (motor->pid.kd * derivative);

  motor->pid.previous_error = error;
  output = Car_LimitFloat(output, motor->pid.output_limit);

  return Car_LimitPwm((int32_t)output);
}

static int32_t Car_LinePidStep(int32_t error_counts)
{
  volatile CarPid_t *pid = &g_car.line.pid;
  float error = (float)error_counts;
  float derivative = error - pid->previous_error;
  float output = 0.0f;

  pid->integral += error;
  pid->integral = Car_LimitFloat(pid->integral, pid->integral_limit);

  output = (pid->kp * error) +
           (pid->ki * pid->integral) +
           (pid->kd * derivative) -
           (g_car.line.gyro_damping * g_car.line.gyro_z);

  pid->previous_error = error;
  output = Car_LimitFloat(output, pid->output_limit);

  return (int32_t)output;
}

static void Car_ResetPid(volatile CarMotor_t *motor)
{
  motor->pid.integral = 0.0f;
  motor->pid.previous_error = 0.0f;
}

static void Car_ResetLinePid(void)
{
  g_car.line.pid.integral = 0.0f;
  g_car.line.pid.previous_error = 0.0f;
}

static uint8_t Car_RightAngleCenterSeen(void)
{
  return ((g_line.active_mask & 0x1CU) != 0U) ? 1U : 0U;
}

static uint8_t Car_RightAngleNewLineSeen(void)
{
  return ((g_line.right_angle_detected == 0U) &&
          (Car_RightAngleCenterSeen() != 0U) &&
          (g_line.active_count <= 3U)) ? 1U : 0U;
}

static void Car_RightAngleSetTargets(int32_t left_target,
                                     int32_t right_target,
                                     int16_t *left_pwm,
                                     int16_t *right_pwm)
{
  left_target = Car_LimitTargetCounts(left_target);
  right_target = Car_LimitTargetCounts(right_target);

  g_car.line.left_target_counts = left_target;
  g_car.line.right_target_counts = right_target;
  g_car.left.target_counts = left_target;
  g_car.right.target_counts = right_target;

  if (left_target == 0)
  {
    Car_ResetPid(&g_car.left);
    *left_pwm = 0;
  }
  else
  {
    *left_pwm = Car_PidStep(&g_car.left);
  }

  if (right_target == 0)
  {
    Car_ResetPid(&g_car.right);
    *right_pwm = 0;
  }
  else
  {
    *right_pwm = Car_PidStep(&g_car.right);
  }
}

static void Car_RightAngleApproachStart(int8_t direction)
{
  g_car.line.right_angle_state = CAR_RIGHT_ANGLE_STATE_APPROACH;
  g_car.line.right_angle_approach_active = 1U;
  g_car.line.right_angle_approach_direction = direction;
  g_car.line.right_angle_approach_start_left_total =
      g_car.left.encoder_total;
  g_car.line.right_angle_approach_start_right_total =
      g_car.right.encoder_total;
  g_car.line.right_angle_approach_travel_counts = 0;
  g_car.line.right_angle_approach_start_tick = g_car.control_tick;
  g_car.line.right_angle_old_line_clear_count = 0U;
  g_car.line.right_angle_center_seen_count = 0U;
  g_car.line.right_angle_cooldown_center_count = 0U;
  g_car.line.right_angle_recovery_count = 0U;

  Car_ResetLinePid();
  Car_ResetPid(&g_car.left);
  Car_ResetPid(&g_car.right);
}

static void Car_RightAngleApproachStop(void)
{
  g_car.line.right_angle_approach_active = 0U;
  g_car.line.right_angle_approach_direction = 0;
}

static int32_t Car_RightAngleApproachDistance(void)
{
  int32_t left_delta = g_car.left.encoder_total -
      g_car.line.right_angle_approach_start_left_total;
  int32_t right_delta = g_car.right.encoder_total -
      g_car.line.right_angle_approach_start_right_total;

  return (Car_AbsInt32(left_delta) + Car_AbsInt32(right_delta)) / 2;
}

static uint8_t Car_RightAngleApproachComplete(void)
{
  uint8_t complete = 0U;

  g_car.line.right_angle_approach_travel_counts =
      Car_RightAngleApproachDistance();

  if (g_car.line.right_angle_approach_counts <= 0)
  {
    complete = 1U;
  }
  else if (g_car.line.right_angle_approach_travel_counts >=
           g_car.line.right_angle_approach_counts)
  {
    complete = 1U;
  }
  else if ((g_car.line.right_angle_approach_timeout_ticks > 0U) &&
           ((g_car.control_tick -
             g_car.line.right_angle_approach_start_tick) >=
            g_car.line.right_angle_approach_timeout_ticks))
  {
    complete = 1U;
  }

  return complete;
}

static uint8_t Car_RightAngleApproachStep(int16_t *left_pwm, int16_t *right_pwm)
{
  int32_t target = g_car.line.right_angle_approach_speed_counts;
  int32_t limited_target = 0;

  if (Car_RightAngleApproachComplete() != 0U)
  {
    int8_t direction = g_car.line.right_angle_approach_direction;
    Car_RightAngleApproachStop();
    Car_RightAngleAssistStart(direction);
    return 0U;
  }

  if (target <= 0)
  {
    target = g_car.line.base_counts;
  }

  limited_target = Car_LimitTargetCounts(target);

  g_car.line.correction_counts = 0;
  Car_RightAngleSetTargets(limited_target,
                           limited_target,
                           left_pwm,
                           right_pwm);

  return 1U;
}

static void Car_RightAngleAssistStart(int8_t direction)
{
  g_car.line.right_angle_state = CAR_RIGHT_ANGLE_STATE_LEAVE_OLD_LINE;
  g_car.line.right_angle_assist_active = 1U;
  g_car.line.right_angle_assist_direction = direction;
  g_car.line.right_angle_yaw_deg = 0.0f;
  g_car.line.right_angle_start_tick = g_car.control_tick;
  g_car.line.right_angle_last_tick = g_car.control_tick;
  g_car.line.right_angle_old_line_clear_count = 0U;
  g_car.line.right_angle_center_seen_count = 0U;
  g_car.line.right_angle_cooldown_center_count = 0U;
  g_car.line.right_angle_recovery_count = 0U;

  Car_ResetLinePid();
  Car_ResetPid(&g_car.left);
  Car_ResetPid(&g_car.right);
}

static void Car_RightAngleAssistStop(void)
{
  if ((g_car.line.right_angle_assist_active != 0U) ||
      (g_car.line.right_angle_approach_active != 0U) ||
      (g_car.line.right_angle_state != CAR_RIGHT_ANGLE_STATE_IDLE))
  {
    g_car.line.right_angle_cooldown = 1U;
  }

  Car_RightAngleApproachStop();
  g_car.line.right_angle_state = CAR_RIGHT_ANGLE_STATE_IDLE;
  g_car.line.right_angle_assist_active = 0U;
  g_car.line.right_angle_assist_direction = 0;
  g_car.line.right_angle_detect_direction = 0;
  g_car.line.right_angle_detect_count = 0U;
  g_car.line.right_angle_old_line_clear_count = 0U;
  g_car.line.right_angle_center_seen_count = 0U;
  g_car.line.right_angle_recovery_count = 0U;
}

static void Car_RightAngleAssistUpdateCooldown(void)
{
  if (g_car.line.right_angle_cooldown == 0U)
  {
    return;
  }

  if ((g_line.right_angle_detected == 0U) && (Car_RightAngleCenterSeen() != 0U))
  {
    if (g_car.line.right_angle_cooldown_center_count <
        g_car.line.right_angle_center_confirm_ticks)
    {
      g_car.line.right_angle_cooldown_center_count++;
    }

    if (g_car.line.right_angle_cooldown_center_count >=
        g_car.line.right_angle_center_confirm_ticks)
    {
      g_car.line.right_angle_cooldown = 0U;
      g_car.line.right_angle_cooldown_center_count = 0U;
    }
  }
  else
  {
    g_car.line.right_angle_cooldown_center_count = 0U;
  }
}

static int8_t Car_RightAngleDetectStep(void)
{
  uint8_t confirm_ticks = g_car.line.right_angle_detect_confirm_ticks;

  if ((g_car.line.right_angle_assist_enable == 0U) ||
      (g_car.line.right_angle_cooldown != 0U) ||
      (g_line.right_angle_detected == 0U) ||
      (g_line.right_angle_direction == 0))
  {
    g_car.line.right_angle_detect_direction = 0;
    g_car.line.right_angle_detect_count = 0U;
    return 0;
  }

  if (confirm_ticks == 0U)
  {
    confirm_ticks = 1U;
  }

  if (g_car.line.right_angle_detect_direction ==
      g_line.right_angle_direction)
  {
    if (g_car.line.right_angle_detect_count < confirm_ticks)
    {
      g_car.line.right_angle_detect_count++;
    }
  }
  else
  {
    g_car.line.right_angle_detect_direction =
        g_line.right_angle_direction;
    g_car.line.right_angle_detect_count = 1U;
  }

  if (g_car.line.right_angle_detect_count >= confirm_ticks)
  {
    int8_t direction = g_car.line.right_angle_detect_direction;
    g_car.line.right_angle_detect_direction = 0;
    g_car.line.right_angle_detect_count = 0U;
    return direction;
  }

  return 0;
}

static void Car_RightAngleRecoveryStart(void)
{
  g_car.line.right_angle_state = CAR_RIGHT_ANGLE_STATE_RECOVER;
  g_car.line.right_angle_recovery_count = 0U;
  Car_ResetLinePid();
  Car_ResetPid(&g_car.left);
  Car_ResetPid(&g_car.right);
}

static uint8_t Car_RightAngleRecoveryStep(int16_t *left_pwm, int16_t *right_pwm)
{
  int32_t base = g_car.line.right_angle_recovery_speed_counts;
  int32_t correction = 0;

  if (g_line.line_seen == 0U)
  {
    g_car.line.correction_counts = 0;
    Car_RightAngleSetTargets(0, 0, left_pwm, right_pwm);
    Car_RightAngleAssistStop();
    return 1U;
  }

  if (base <= 0)
  {
    base = g_car.line.base_counts;
  }

  correction = Car_LinePidStep((int32_t)g_line.error);
  g_car.line.correction_counts = correction;
  Car_RightAngleSetTargets(base - correction,
                           base + correction,
                           left_pwm,
                           right_pwm);

  if (g_car.line.right_angle_recovery_count <
      g_car.line.right_angle_recovery_ticks)
  {
    g_car.line.right_angle_recovery_count++;
  }

  if (g_car.line.right_angle_recovery_count >=
      g_car.line.right_angle_recovery_ticks)
  {
    Car_RightAngleAssistStop();
  }

  return 1U;
}

static uint8_t Car_RightAngleAssistStep(int16_t *left_pwm, int16_t *right_pwm)
{
  int32_t outer_target = 0;
  uint8_t safety_stop = 0U;

  Car_RightAngleAssistUpdateCooldown();

  if (g_car.line.right_angle_state == CAR_RIGHT_ANGLE_STATE_RECOVER)
  {
    return Car_RightAngleRecoveryStep(left_pwm, right_pwm);
  }

  if (g_car.line.right_angle_assist_active == 0U)
  {
    if (g_car.line.right_angle_approach_active != 0U)
    {
      if (Car_RightAngleApproachStep(left_pwm, right_pwm) != 0U)
      {
        return 1U;
      }
    }
    else
    {
      int8_t direction = Car_RightAngleDetectStep();

      if (direction == 0)
      {
        return 0U;
      }

      Car_RightAngleApproachStart(direction);
      if (Car_RightAngleApproachStep(left_pwm, right_pwm) != 0U)
      {
        return 1U;
      }
    }
  }

  if ((g_mpu6050.present != 0U) && (g_mpu6050.valid != 0U))
  {
    uint32_t delta_ticks =
        g_car.control_tick - g_car.line.right_angle_last_tick;
    float dt_s = ((float)delta_ticks *
                  (float)g_car.control_period_ms) * 0.001f;
    float gyro = Car_AbsFloat(g_car.line.gyro_z);

    if (gyro < g_car.line.right_angle_gyro_deadband_dps)
    {
      gyro = 0.0f;
    }

    g_car.line.right_angle_yaw_deg += gyro * dt_s;
  }
  g_car.line.right_angle_last_tick = g_car.control_tick;

  if ((g_mpu6050.present != 0U) &&
      (g_mpu6050.valid != 0U) &&
      (g_car.line.right_angle_target_deg > 0.0f) &&
      (g_car.line.right_angle_yaw_deg >=
       g_car.line.right_angle_target_deg))
  {
    safety_stop = 1U;
  }

  if ((g_car.line.right_angle_timeout_ticks > 0U) &&
      ((g_car.control_tick - g_car.line.right_angle_start_tick) >=
       g_car.line.right_angle_timeout_ticks))
  {
    safety_stop = 1U;
  }

  if (safety_stop != 0U)
  {
    if (g_line.line_seen != 0U)
    {
      Car_RightAngleAssistStop();
      return 0U;
    }

    g_car.line.correction_counts = 0;
    Car_RightAngleSetTargets(0, 0, left_pwm, right_pwm);
    Car_RightAngleAssistStop();
    return 1U;
  }

  if (g_car.line.right_angle_state ==
      CAR_RIGHT_ANGLE_STATE_LEAVE_OLD_LINE)
  {
    uint8_t clear_ticks =
        g_car.line.right_angle_old_line_clear_confirm_ticks;

    if (clear_ticks == 0U)
    {
      clear_ticks = 1U;
    }

    if (Car_RightAngleCenterSeen() == 0U)
    {
      if (g_car.line.right_angle_old_line_clear_count < clear_ticks)
      {
        g_car.line.right_angle_old_line_clear_count++;
      }
    }
    else
    {
      g_car.line.right_angle_old_line_clear_count = 0U;
    }

    if (g_car.line.right_angle_old_line_clear_count >= clear_ticks)
    {
      g_car.line.right_angle_state = CAR_RIGHT_ANGLE_STATE_FIND_NEW_LINE;
      g_car.line.right_angle_center_seen_count = 0U;
    }
  }
  else if (g_car.line.right_angle_state ==
           CAR_RIGHT_ANGLE_STATE_FIND_NEW_LINE)
  {
    uint8_t confirm_ticks = g_car.line.right_angle_center_confirm_ticks;

    if (confirm_ticks == 0U)
    {
      confirm_ticks = 1U;
    }

    if (Car_RightAngleNewLineSeen() != 0U)
    {
      if (g_car.line.right_angle_center_seen_count < confirm_ticks)
      {
        g_car.line.right_angle_center_seen_count++;
      }
    }
    else
    {
      g_car.line.right_angle_center_seen_count = 0U;
    }

    if (g_car.line.right_angle_center_seen_count >= confirm_ticks)
    {
      Car_RightAngleRecoveryStart();
      return Car_RightAngleRecoveryStep(left_pwm, right_pwm);
    }
  }

  outer_target = g_car.line.right_angle_base_counts +
      g_car.line.right_angle_turn_counts;
  outer_target = Car_LimitTargetCounts(outer_target);
  g_car.line.correction_counts =
      ((int32_t)g_car.line.right_angle_assist_direction) * outer_target;

  if (g_car.line.right_angle_assist_direction > 0)
  {
    Car_RightAngleSetTargets(0, outer_target, left_pwm, right_pwm);
  }
  else
  {
    Car_RightAngleSetTargets(outer_target, 0, left_pwm, right_pwm);
  }

  return 1U;
}

static void Car_UpdateLeftEncoder(volatile CarMotor_t *motor)
{
  int16_t now = (int16_t)DL_TimerG_getTimerCount(QEI_0_INST);
  int16_t delta16 = (int16_t)(now - motor->encoder_last);
  int32_t delta = (int32_t)delta16;

  motor->encoder_last = now;
  motor->encoder_raw = now;

  if (motor->invert_encoder != 0U)
  {
    delta = -delta;
  }

  motor->encoder_delta = delta;
  motor->measured_counts = delta;
  motor->encoder_total += delta;
}

static void Car_UpdateRightEncoder(volatile CarMotor_t *motor)
{
  int16_t now = (int16_t)s_right_encoder_count;
  int16_t delta16 = (int16_t)(now - motor->encoder_last);
  int32_t delta = (int32_t)delta16;

  motor->encoder_last = now;
  motor->encoder_raw = now;

  if (motor->invert_encoder != 0U)
  {
    delta = -delta;
  }

  motor->encoder_delta = delta;
  motor->measured_counts = delta;
  motor->encoder_total += delta;
}

static void Car_SetDriverEnable(uint8_t enable)
{
  g_car.driver_enabled = (enable != 0U) ? 1U : 0U;
  Car_WritePin(GPIO_MOTOR_PORT, GPIO_MOTOR_STBY_PIN, enable);
}

static void Car_WritePin(GPIO_Regs *port, uint32_t pin, uint8_t set)
{
  if (set != 0U)
  {
    DL_GPIO_setPins(port, pin);
  }
  else
  {
    DL_GPIO_clearPins(port, pin);
  }
}

static void Car_ApplyMotor(volatile CarMotor_t *motor,
                           uint32_t channel,
                           GPIO_Regs *in1_port,
                           uint32_t in1_pin,
                           GPIO_Regs *in2_port,
                           uint32_t in2_pin)
{
  int16_t pwm = motor->pwm_output;
  uint32_t duty = 0U;
  uint8_t in1 = 0U;
  uint8_t in2 = 0U;

  if (motor->invert_motor != 0U)
  {
    pwm = (int16_t)-pwm;
  }

  if (pwm > 0)
  {
    in1 = 1U;
    duty = (uint32_t)pwm;
  }
  else if (pwm < 0)
  {
    in2 = 1U;
    duty = (uint32_t)(-pwm);
  }

  if (g_car.mode == CAR_MODE_DISABLED)
  {
    in1 = 0U;
    in2 = 0U;
    duty = 0U;
  }

  Car_WritePin(in1_port, in1_pin, in1);
  Car_WritePin(in2_port, in2_pin, in2);
  DL_TimerA_setCaptureCompareValue(PWM_0_INST, duty, channel);
}

static uint8_t Car_ReadRightEncoderState(void)
{
  uint8_t a = (DL_GPIO_readPins(GPIO_ENCODER_PORT, GPIO_ENCODER_E2A_PIN) != 0U) ? 1U : 0U;
  uint8_t b = (DL_GPIO_readPins(GPIO_ENCODER_PORT, GPIO_ENCODER_E2B_PIN) != 0U) ? 1U : 0U;

  return (uint8_t)((a << 1) | b);
}

static void Car_UpdateRightEncoderCount(void)
{
  static const int8_t delta_table[16] =
  {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
  };
  uint8_t state = Car_ReadRightEncoderState();
  uint8_t index = (uint8_t)((s_right_encoder_state << 2) | state);

  s_right_encoder_count += delta_table[index];
  s_right_encoder_state = state;
}
