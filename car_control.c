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
      .kp = 0.6f,
      .ki = 0.05f,
      .kd = 0.2f,
      .integral = 0.0f,
      .previous_error = 0.0f,
      .output_limit = 30.0f,
      .integral_limit = 1000.0f,
    },
    .base_counts = 28,
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
    .right_angle_approach_counts = 0,
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

typedef enum
{
  CAR_APPROACH_ENCODERS_BOTH_OK = 0,
  CAR_APPROACH_ENCODER_LEFT_SUSPECT = 1,
  CAR_APPROACH_ENCODER_RIGHT_SUSPECT = 2,
  CAR_APPROACH_ENCODERS_BOTH_INVALID = 3
} CarApproachEncoderHealth_t;

typedef enum
{
  CAR_APPROACH_RUNNING = 0,
  CAR_APPROACH_COMPLETE = 1,
  CAR_APPROACH_FAULT = 2
} CarApproachResult_t;

enum
{
  CAR_APPROACH_STARTUP_GRACE_TICKS = 3U,
  CAR_APPROACH_INVALID_CONFIRM_TICKS = 3U,
  CAR_APPROACH_MISMATCH_CONFIRM_TICKS = 2U,
  CAR_APPROACH_IDLE_STEP_COUNTS = 1,
  CAR_APPROACH_PROGRESS_MIN_COUNTS = 12,
  CAR_APPROACH_STEP_MAX_MIN_COUNTS = 32,
  CAR_APPROACH_STEP_MAX_MULTIPLIER = 3,
  CAR_APPROACH_DISTANCE_SLACK_COUNTS = 4,
  CAR_APPROACH_MISMATCH_RATIO = 3,
  CAR_APPROACH_CLOSE_MIN_COUNTS = 6,
  CAR_APPROACH_EXPECTED_HIGH_MULTIPLIER = 2,
  CAR_APPROACH_PWM_DELTA_LIMIT = CAR_PWM_MAX / 5,
  CAR_APPROACH_FALLBACK_PWM_MAX = CAR_PWM_MAX / 2
};

static CarApproachEncoderHealth_t s_approach_encoder_health =
    CAR_APPROACH_ENCODERS_BOTH_OK;
static CarApproachEncoderHealth_t s_approach_locked_health =
    CAR_APPROACH_ENCODERS_BOTH_OK;
static CarApproachEncoderHealth_t s_approach_mismatch_health =
    CAR_APPROACH_ENCODERS_BOTH_OK;
static uint8_t s_approach_invalid_ticks = 0U;
static uint8_t s_approach_left_stale_ticks = 0U;
static uint8_t s_approach_right_stale_ticks = 0U;
static uint8_t s_approach_mismatch_ticks = 0U;
static uint8_t s_approach_distance_reliable = 0U;
static int32_t s_approach_fused_distance = 0;

enum
{
  CAR_LINE_FILTER_Q8_SHIFT = 8U,
  CAR_LINE_FILTER_Q8_ONE = 1U << CAR_LINE_FILTER_Q8_SHIFT,
  CAR_LINE_FILTER_CENTER_LIMIT = 16,
  CAR_LINE_FILTER_DIRECT_LIMIT = 33,
  CAR_LINE_FILTER_CENTER_ALPHA_Q8 = 64,
  CAR_LINE_FILTER_DEADBAND = 2,
  CAR_LINE_LOST_CONFIRM_TICKS = 3U
};

static int32_t s_line_error_filter_q8 = 0;
static uint8_t s_line_error_filter_valid = 0U;
static uint8_t s_line_lost_count = 0U;

static int16_t Car_LimitPwm(int32_t pwm);
static float Car_AbsFloat(float value);
static int32_t Car_AbsInt32(int32_t value);
static float Car_LimitFloat(float value, float limit);
static int32_t Car_LimitTargetCounts(int32_t counts);
static int16_t Car_PidStep(volatile CarMotor_t *motor);
static int32_t Car_LinePidStep(int32_t error);
static int32_t Car_FilterLineError(int32_t raw_error);
static void Car_ResetPid(volatile CarMotor_t *motor);
static void Car_ResetLinePid(void);
static uint8_t Car_RightAngleCenterSeen(void);
static uint8_t Car_RightAngleNewLineSeen(void);
static void Car_RightAngleSetTargets(int32_t left_target,
                                     int32_t right_target,
                                     int16_t *left_pwm,
                                     int16_t *right_pwm);
static int16_t Car_RightAngleLimitSuspectPwm(int16_t suspect_pwm,
                                             int16_t healthy_pwm);
static void Car_RightAngleApproachSetTargets(int32_t target,
                                             int16_t *left_pwm,
                                             int16_t *right_pwm);
static void Car_RightAngleApproachStart(int8_t direction);
static void Car_RightAngleApproachStop(void);
static int32_t Car_RightAngleApproachDistance(int32_t target);
static CarApproachResult_t Car_RightAngleApproachResult(int32_t target);
static uint8_t Car_RightAngleApproachStep(int16_t *left_pwm, int16_t *right_pwm);
static void Car_RightAngleAssistStart(int8_t direction);
static void Car_RightAngleAssistStop(void);
static void Car_RightAngleAssistUpdateCooldown(void);
static int8_t Car_RightAngleDetectStep(void);
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
                           uint32_t in2_pin,
                           uint8_t short_brake);
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

  DL_GPIO_clearInterruptStatus(
      GPIO_EC11_PORT,
      GPIO_EC11_A_PIN | GPIO_EC11_B_PIN);
  DL_GPIO_enableInterrupt(
      GPIO_EC11_PORT,
      GPIO_EC11_A_PIN | GPIO_EC11_B_PIN);
  NVIC_EnableIRQ(GPIO_ENCODER_INT_IRQN);
  NVIC_EnableIRQ(GPIO_EC11_INT_IRQN);
  NVIC_EnableIRQ(TIMER_CONTROL_INST_INT_IRQN);
  DL_TimerG_startCounter(TIMER_CONTROL_INST);
}

void Car_ControlStep(void)
{
  int16_t left_pwm = 0;
  int16_t right_pwm = 0;
  uint8_t left_short_brake = 0U;
  uint8_t right_short_brake = 0U;

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

      if ((g_line.line_seen != 0U) ||
          (g_car.line.line_lost_stop == 0U))
      {
        int32_t line_error = Car_FilterLineError((int32_t)g_line.error);
        float line_integral_before = g_car.line.pid.integral;
        int32_t correction = Car_LinePidStep(line_error);
        int32_t correction_before_limit = correction;
        int32_t correction_limit = g_car.line.base_counts;
        int32_t left_target = g_car.line.base_counts - correction;
        int32_t right_target = g_car.line.base_counts + correction;

        s_line_lost_count = 0U;
        if (correction_limit < 0)
        {
          correction_limit = 0;
        }
        if (correction > correction_limit)
        {
          correction = correction_limit;
        }
        else if (correction < -correction_limit)
        {
          correction = -correction_limit;
        }

        if ((correction_limit == 0) ||
            ((correction_before_limit >= correction_limit) &&
             (line_error > 0)) ||
            ((correction_before_limit <= -correction_limit) &&
             (line_error < 0)))
        {
          g_car.line.pid.integral = line_integral_before;
        }
        left_target = g_car.line.base_counts - correction;
        right_target = g_car.line.base_counts + correction;

        g_car.line.correction_counts = correction;
        g_car.line.left_target_counts = left_target;
        g_car.line.right_target_counts = right_target;
        g_car.left.target_counts = left_target;
        g_car.right.target_counts = right_target;

        left_pwm = Car_PidStep(&g_car.left);
        right_pwm = Car_PidStep(&g_car.right);
      }
      else if (s_line_lost_count < (CAR_LINE_LOST_CONFIRM_TICKS - 1U))
      {
        s_line_lost_count++;
        left_pwm = Car_PidStep(&g_car.left);
        right_pwm = Car_PidStep(&g_car.right);
      }
      else
      {
        if (s_line_lost_count < CAR_LINE_LOST_CONFIRM_TICKS)
        {
          s_line_lost_count++;
          s_line_error_filter_valid = 0U;
        }

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

    case CAR_MODE_BRAKE_HOLD:
      Car_RightAngleAssistStop();
      Car_ResetLinePid();
      Car_ResetPid(&g_car.left);
      Car_ResetPid(&g_car.right);
      left_pwm = 0;
      right_pwm = 0;
      left_short_brake = 1U;
      right_short_brake = 1U;
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

  if ((g_car.mode == CAR_MODE_LINE_FOLLOW) &&
      (g_car.line.right_angle_assist_active != 0U) &&
      ((g_car.line.right_angle_state ==
        CAR_RIGHT_ANGLE_STATE_LEAVE_OLD_LINE) ||
       (g_car.line.right_angle_state ==
        CAR_RIGHT_ANGLE_STATE_FIND_NEW_LINE)))
  {
    if (g_car.line.right_angle_assist_direction > 0)
    {
      left_short_brake = 1U;
    }
    else if (g_car.line.right_angle_assist_direction < 0)
    {
      right_short_brake = 1U;
    }
  }
  Car_ApplyMotor(&g_car.left,
                 GPIO_PWM_0_C0_IDX,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_AIN1_PIN,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_AIN2_PIN,
                 left_short_brake);
  Car_ApplyMotor(&g_car.right,
                 GPIO_PWM_0_C1_IDX,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_BIN1_PIN,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_BIN2_PIN,
                 right_short_brake);
}

void Car_Stop(void)
{
  s_line_lost_count = 0U;
  s_line_error_filter_valid = 0U;
  g_car.mode = CAR_MODE_DISABLED;
  g_car.line.correction_counts = 0;
  g_car.line.left_target_counts = 0;
  g_car.line.right_target_counts = 0;
  g_car.left.target_counts = 0;
  g_car.right.target_counts = 0;
  g_car.left.manual_pwm = 0;
  g_car.right.manual_pwm = 0;
  g_car.left.pwm_output = 0;
  g_car.right.pwm_output = 0;
  Car_RightAngleAssistStop();
  Car_ResetLinePid();
  Car_ResetPid(&g_car.left);
  Car_ResetPid(&g_car.right);
  Car_SetDriverEnable(0U);
  Car_ApplyMotor(&g_car.left,
                 GPIO_PWM_0_C0_IDX,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_AIN1_PIN,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_AIN2_PIN,
                 0U);
  Car_ApplyMotor(&g_car.right,
                 GPIO_PWM_0_C1_IDX,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_BIN1_PIN,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_BIN2_PIN,
                 0U);
}
void Car_BrakeHold(void)
{
  s_line_lost_count = 0U;
  s_line_error_filter_valid = 0U;
  Car_RightAngleAssistStop();
  g_car.mode = CAR_MODE_BRAKE_HOLD;
  g_car.line.correction_counts = 0;
  g_car.line.left_target_counts = 0;
  g_car.line.right_target_counts = 0;
  g_car.left.target_counts = 0;
  g_car.right.target_counts = 0;
  g_car.left.manual_pwm = 0;
  g_car.right.manual_pwm = 0;
  g_car.left.pwm_output = 0;
  g_car.right.pwm_output = 0;
  Car_ResetLinePid();
  Car_ResetPid(&g_car.left);
  Car_ResetPid(&g_car.right);
  Car_SetDriverEnable(1U);
  Car_ApplyMotor(&g_car.left,
                 GPIO_PWM_0_C0_IDX,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_AIN1_PIN,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_AIN2_PIN,
                 1U);
  Car_ApplyMotor(&g_car.right,
                 GPIO_PWM_0_C1_IDX,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_BIN1_PIN,
                 GPIO_MOTOR_PORT,
                 GPIO_MOTOR_BIN2_PIN,
                 1U);
}

void Car_StartLineFollow(void)
{
  s_line_lost_count = 0U;
  s_line_error_filter_valid = 0U;
  Car_ResetLinePid();
  Car_ResetPid(&g_car.left);
  Car_ResetPid(&g_car.right);
  g_car.mode = CAR_MODE_LINE_FOLLOW;
}

void Car_SetLineFollowBaseCounts(int32_t base_counts)
{
  if (base_counts < 0)
  {
    base_counts = 0;
  }
  else if (base_counts > CAR_RIGHT_ANGLE_TARGET_COUNTS_MAX)
  {
    base_counts = CAR_RIGHT_ANGLE_TARGET_COUNTS_MAX;
  }

  g_car.line.base_counts = base_counts;
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
      GPIO_EC11_PORT, GPIO_EC11_A_PIN | GPIO_EC11_B_PIN);

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

static int32_t Car_FilterLineError(int32_t raw_error)
{
  int32_t raw_q8 = raw_error * CAR_LINE_FILTER_Q8_ONE;
  int32_t raw_magnitude = Car_AbsInt32(raw_error);
  int32_t filtered_error;
  int32_t filter_magnitude;
  int32_t control_magnitude;
  int32_t alpha_q8;

  if (s_line_error_filter_valid == 0U)
  {
    s_line_error_filter_q8 = raw_q8;
    s_line_error_filter_valid = 1U;
  }
  else
  {
    if (s_line_error_filter_q8 >= 0)
    {
      filtered_error =
          (s_line_error_filter_q8 + (CAR_LINE_FILTER_Q8_ONE / 2)) >>
          CAR_LINE_FILTER_Q8_SHIFT;
    }
    else
    {
      filtered_error = -(((-s_line_error_filter_q8) +
                           (CAR_LINE_FILTER_Q8_ONE / 2)) >>
                          CAR_LINE_FILTER_Q8_SHIFT);
    }

    filter_magnitude = Car_AbsInt32(filtered_error);
    control_magnitude = (raw_magnitude > filter_magnitude) ?
                        raw_magnitude : filter_magnitude;

    if (control_magnitude >= CAR_LINE_FILTER_DIRECT_LIMIT)
    {
      s_line_error_filter_q8 = raw_q8;
    }
    else
    {
      if (control_magnitude <= CAR_LINE_FILTER_CENTER_LIMIT)
      {
        alpha_q8 = CAR_LINE_FILTER_CENTER_ALPHA_Q8;
      }
      else
      {
        alpha_q8 = CAR_LINE_FILTER_CENTER_ALPHA_Q8 +
            (12 * (control_magnitude - CAR_LINE_FILTER_CENTER_LIMIT));
        if (alpha_q8 > CAR_LINE_FILTER_Q8_ONE)
        {
          alpha_q8 = CAR_LINE_FILTER_Q8_ONE;
        }
      }

      s_line_error_filter_q8 +=
          ((raw_q8 - s_line_error_filter_q8) * alpha_q8) >>
          CAR_LINE_FILTER_Q8_SHIFT;
    }
  }

  if (s_line_error_filter_q8 >= 0)
  {
    filtered_error =
        (s_line_error_filter_q8 + (CAR_LINE_FILTER_Q8_ONE / 2)) >>
        CAR_LINE_FILTER_Q8_SHIFT;
  }
  else
  {
    filtered_error = -(((-s_line_error_filter_q8) +
                         (CAR_LINE_FILTER_Q8_ONE / 2)) >>
                        CAR_LINE_FILTER_Q8_SHIFT);
  }

  if (Car_AbsInt32(filtered_error) <= CAR_LINE_FILTER_DEADBAND)
  {
    filtered_error = 0;
  }

  return filtered_error;
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
  s_line_error_filter_q8 = 0;
  s_line_error_filter_valid = 0U;
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

static int16_t Car_RightAngleLimitSuspectPwm(int16_t suspect_pwm,
                                             int16_t healthy_pwm)
{
  int32_t limited_pwm = (int32_t)suspect_pwm;
  int32_t reference_pwm = (int32_t)healthy_pwm;
  int32_t lower_limit = 0;
  int32_t upper_limit = 0;

  if (reference_pwm < 0)
  {
    reference_pwm = 0;
  }
  else if (reference_pwm > CAR_PWM_MAX)
  {
    reference_pwm = CAR_PWM_MAX;
  }

  if (limited_pwm < 0)
  {
    limited_pwm = 0;
  }
  else if (limited_pwm > CAR_PWM_MAX)
  {
    limited_pwm = CAR_PWM_MAX;
  }

  lower_limit = reference_pwm - CAR_APPROACH_PWM_DELTA_LIMIT;
  upper_limit = reference_pwm + CAR_APPROACH_PWM_DELTA_LIMIT;
  if (lower_limit < 0)
  {
    lower_limit = 0;
  }
  if (upper_limit > CAR_PWM_MAX)
  {
    upper_limit = CAR_PWM_MAX;
  }

  if (limited_pwm < lower_limit)
  {
    limited_pwm = lower_limit;
  }
  else if (limited_pwm > upper_limit)
  {
    limited_pwm = upper_limit;
  }

  return (int16_t)limited_pwm;
}

static void Car_RightAngleApproachSetTargets(int32_t target,
                                             int16_t *left_pwm,
                                             int16_t *right_pwm)
{
  float left_integral = g_car.left.pid.integral;
  float left_previous_error = g_car.left.pid.previous_error;
  float right_integral = g_car.right.pid.integral;
  float right_previous_error = g_car.right.pid.previous_error;
  int32_t fallback_left_pwm = g_car.left.pwm_output;
  int32_t fallback_right_pwm = g_car.right.pwm_output;

  Car_RightAngleSetTargets(target, target, left_pwm, right_pwm);

  if ((s_approach_encoder_health == CAR_APPROACH_ENCODER_LEFT_SUSPECT) ||
      (s_approach_encoder_health == CAR_APPROACH_ENCODERS_BOTH_INVALID))
  {
    g_car.left.pid.integral = left_integral;
    g_car.left.pid.previous_error = left_previous_error;
  }

  if ((s_approach_encoder_health == CAR_APPROACH_ENCODER_RIGHT_SUSPECT) ||
      (s_approach_encoder_health == CAR_APPROACH_ENCODERS_BOTH_INVALID))
  {
    g_car.right.pid.integral = right_integral;
    g_car.right.pid.previous_error = right_previous_error;
  }

  if (*left_pwm < 0)
  {
    *left_pwm = 0;
  }
  if (*right_pwm < 0)
  {
    *right_pwm = 0;
  }

  if (s_approach_encoder_health == CAR_APPROACH_ENCODER_LEFT_SUSPECT)
  {
    *left_pwm = Car_RightAngleLimitSuspectPwm(*left_pwm, *right_pwm);
  }
  else if (s_approach_encoder_health == CAR_APPROACH_ENCODER_RIGHT_SUSPECT)
  {
    *right_pwm = Car_RightAngleLimitSuspectPwm(*right_pwm, *left_pwm);
  }
  else if (s_approach_encoder_health == CAR_APPROACH_ENCODERS_BOTH_INVALID)
  {
    if (fallback_left_pwm < 0)
    {
      fallback_left_pwm = 0;
    }
    else if (fallback_left_pwm > CAR_APPROACH_FALLBACK_PWM_MAX)
    {
      fallback_left_pwm = CAR_APPROACH_FALLBACK_PWM_MAX;
    }

    if (fallback_right_pwm < 0)
    {
      fallback_right_pwm = 0;
    }
    else if (fallback_right_pwm > CAR_APPROACH_FALLBACK_PWM_MAX)
    {
      fallback_right_pwm = CAR_APPROACH_FALLBACK_PWM_MAX;
    }

    *left_pwm = (int16_t)fallback_left_pwm;
    *right_pwm = (int16_t)fallback_right_pwm;
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
  s_approach_encoder_health = CAR_APPROACH_ENCODERS_BOTH_OK;
  s_approach_locked_health = CAR_APPROACH_ENCODERS_BOTH_OK;
  s_approach_mismatch_health = CAR_APPROACH_ENCODERS_BOTH_OK;
  s_approach_invalid_ticks = 0U;
  s_approach_left_stale_ticks = 0U;
  s_approach_right_stale_ticks = 0U;
  s_approach_mismatch_ticks = 0U;
  s_approach_distance_reliable = 0U;
  s_approach_fused_distance = 0;

  Car_ResetLinePid();
  Car_ResetPid(&g_car.left);
  Car_ResetPid(&g_car.right);
}

static void Car_RightAngleApproachStop(void)
{
  g_car.line.right_angle_approach_active = 0U;
  g_car.line.right_angle_approach_direction = 0;
  s_approach_encoder_health = CAR_APPROACH_ENCODERS_BOTH_OK;
  s_approach_locked_health = CAR_APPROACH_ENCODERS_BOTH_OK;
  s_approach_mismatch_health = CAR_APPROACH_ENCODERS_BOTH_OK;
  s_approach_invalid_ticks = 0U;
  s_approach_left_stale_ticks = 0U;
  s_approach_right_stale_ticks = 0U;
  s_approach_mismatch_ticks = 0U;
  s_approach_distance_reliable = 0U;
  s_approach_fused_distance = 0;
}
static int32_t Car_RightAngleApproachDistance(int32_t target)
{
  int32_t left_delta = g_car.left.encoder_total -
      g_car.line.right_angle_approach_start_left_total;
  int32_t right_delta = g_car.right.encoder_total -
      g_car.line.right_angle_approach_start_right_total;
  int32_t left_distance = Car_AbsInt32(left_delta);
  int32_t right_distance = Car_AbsInt32(right_delta);
  int32_t left_step = Car_AbsInt32(g_car.left.encoder_delta);
  int32_t right_step = Car_AbsInt32(g_car.right.encoder_delta);
  uint32_t elapsed_ticks = g_car.control_tick -
      g_car.line.right_angle_approach_start_tick;
  int32_t expected_distance = (int32_t)elapsed_ticks * target;
  int32_t plausible_max =
      (expected_distance * CAR_APPROACH_EXPECTED_HIGH_MULTIPLIER) +
      (target * 2) + CAR_APPROACH_DISTANCE_SLACK_COUNTS;
  int32_t max_step = target * CAR_APPROACH_STEP_MAX_MULTIPLIER;
  int32_t high_distance = left_distance;
  int32_t low_distance = right_distance;
  int32_t difference = 0;
  int32_t close_limit = 0;
  int32_t progress_gate = target;
  int32_t candidate = s_approach_fused_distance;
  uint8_t left_sample_valid = 0U;
  uint8_t right_sample_valid = 0U;
  uint8_t left_moving = 0U;
  uint8_t right_moving = 0U;
  uint8_t left_invalid = 0U;
  uint8_t right_invalid = 0U;
  uint8_t mismatch = 0U;
  uint8_t confirmed_both_invalid = 0U;
  CarApproachEncoderHealth_t health = CAR_APPROACH_ENCODERS_BOTH_OK;

  s_approach_distance_reliable = 0U;

  if (elapsed_ticks == 0U)
  {
    s_approach_encoder_health = CAR_APPROACH_ENCODERS_BOTH_OK;
    return s_approach_fused_distance;
  }

  if (max_step < CAR_APPROACH_STEP_MAX_MIN_COUNTS)
  {
    max_step = CAR_APPROACH_STEP_MAX_MIN_COUNTS;
  }
  if (progress_gate < CAR_APPROACH_PROGRESS_MIN_COUNTS)
  {
    progress_gate = CAR_APPROACH_PROGRESS_MIN_COUNTS;
  }

  left_sample_valid = ((left_step <= max_step) &&
                       (left_distance <= plausible_max)) ? 1U : 0U;
  right_sample_valid = ((right_step <= max_step) &&
                        (right_distance <= plausible_max)) ? 1U : 0U;
  left_moving = ((left_sample_valid != 0U) &&
                 (left_step > CAR_APPROACH_IDLE_STEP_COUNTS)) ? 1U : 0U;
  right_moving = ((right_sample_valid != 0U) &&
                  (right_step > CAR_APPROACH_IDLE_STEP_COUNTS)) ? 1U : 0U;

  if (left_moving != 0U)
  {
    s_approach_left_stale_ticks = 0U;
  }
  else if (s_approach_left_stale_ticks <
           CAR_APPROACH_INVALID_CONFIRM_TICKS)
  {
    s_approach_left_stale_ticks++;
  }

  if (right_moving != 0U)
  {
    s_approach_right_stale_ticks = 0U;
  }
  else if (s_approach_right_stale_ticks <
           CAR_APPROACH_INVALID_CONFIRM_TICKS)
  {
    s_approach_right_stale_ticks++;
  }

  if (elapsed_ticks >= CAR_APPROACH_STARTUP_GRACE_TICKS)
  {
    left_invalid =
        (s_approach_left_stale_ticks >=
         CAR_APPROACH_INVALID_CONFIRM_TICKS) ? 1U : 0U;
    right_invalid =
        (s_approach_right_stale_ticks >=
         CAR_APPROACH_INVALID_CONFIRM_TICKS) ? 1U : 0U;
  }

  if (s_approach_locked_health == CAR_APPROACH_ENCODER_LEFT_SUSPECT)
  {
    if (right_invalid != 0U)
    {
      health = CAR_APPROACH_ENCODERS_BOTH_INVALID;
      confirmed_both_invalid = 1U;
    }
    else if (right_sample_valid == 0U)
    {
      health = CAR_APPROACH_ENCODERS_BOTH_INVALID;
    }
    else
    {
      health = CAR_APPROACH_ENCODER_LEFT_SUSPECT;
      candidate = right_distance;
      s_approach_distance_reliable = 1U;
    }
  }
  else if (s_approach_locked_health ==
           CAR_APPROACH_ENCODER_RIGHT_SUSPECT)
  {
    if (left_invalid != 0U)
    {
      health = CAR_APPROACH_ENCODERS_BOTH_INVALID;
      confirmed_both_invalid = 1U;
    }
    else if (left_sample_valid == 0U)
    {
      health = CAR_APPROACH_ENCODERS_BOTH_INVALID;
    }
    else
    {
      health = CAR_APPROACH_ENCODER_RIGHT_SUSPECT;
      candidate = left_distance;
      s_approach_distance_reliable = 1U;
    }
  }
  else if ((left_invalid != 0U) && (right_invalid != 0U))
  {
    health = CAR_APPROACH_ENCODERS_BOTH_INVALID;
    confirmed_both_invalid = 1U;
  }
  else if (left_invalid != 0U)
  {
    s_approach_locked_health = CAR_APPROACH_ENCODER_LEFT_SUSPECT;
    health = CAR_APPROACH_ENCODER_LEFT_SUSPECT;
    if (right_sample_valid != 0U)
    {
      candidate = right_distance;
      s_approach_distance_reliable = 1U;
    }
  }
  else if (right_invalid != 0U)
  {
    s_approach_locked_health = CAR_APPROACH_ENCODER_RIGHT_SUSPECT;
    health = CAR_APPROACH_ENCODER_RIGHT_SUSPECT;
    if (left_sample_valid != 0U)
    {
      candidate = left_distance;
      s_approach_distance_reliable = 1U;
    }
  }
  else if ((left_sample_valid != 0U) &&
           (right_sample_valid != 0U))
  {
    CarApproachEncoderHealth_t mismatch_health =
        CAR_APPROACH_ENCODERS_BOTH_INVALID;
    int32_t left_error =
        Car_AbsInt32(left_distance - expected_distance);
    int32_t right_error =
        Car_AbsInt32(right_distance - expected_distance);

    if (right_distance > left_distance)
    {
      high_distance = right_distance;
      low_distance = left_distance;
    }

    difference = high_distance - low_distance;
    close_limit = high_distance / CAR_APPROACH_MISMATCH_RATIO;
    if (close_limit < CAR_APPROACH_CLOSE_MIN_COUNTS)
    {
      close_limit = CAR_APPROACH_CLOSE_MIN_COUNTS;
    }

    if ((difference > close_limit) ||
        ((high_distance >= progress_gate) &&
         (high_distance >=
          (low_distance * CAR_APPROACH_MISMATCH_RATIO) +
          CAR_APPROACH_DISTANCE_SLACK_COUNTS)))
    {
      mismatch = 1U;
    }

    if (mismatch == 0U)
    {
      health = CAR_APPROACH_ENCODERS_BOTH_OK;
      candidate = (left_distance + right_distance) / 2;
      s_approach_mismatch_health = CAR_APPROACH_ENCODERS_BOTH_OK;
      s_approach_mismatch_ticks = 0U;
      if ((left_distance > 0) && (right_distance > 0))
      {
        s_approach_distance_reliable = 1U;
      }
    }
    else
    {
      if (left_error < right_error)
      {
        mismatch_health = CAR_APPROACH_ENCODER_RIGHT_SUSPECT;
      }
      else if (right_error < left_error)
      {
        mismatch_health = CAR_APPROACH_ENCODER_LEFT_SUSPECT;
      }

      if (mismatch_health == CAR_APPROACH_ENCODERS_BOTH_INVALID)
      {
        health = CAR_APPROACH_ENCODERS_BOTH_OK;
        candidate = low_distance;
        s_approach_mismatch_health = CAR_APPROACH_ENCODERS_BOTH_OK;
        s_approach_mismatch_ticks = 0U;
        if (low_distance > 0)
        {
          s_approach_distance_reliable = 1U;
        }
      }
      else
      {
        if (s_approach_mismatch_health == mismatch_health)
        {
          if (s_approach_mismatch_ticks <
              CAR_APPROACH_MISMATCH_CONFIRM_TICKS)
          {
            s_approach_mismatch_ticks++;
          }
        }
        else
        {
          s_approach_mismatch_health = mismatch_health;
          s_approach_mismatch_ticks = 1U;
        }

        if ((elapsed_ticks >= CAR_APPROACH_STARTUP_GRACE_TICKS) &&
            (s_approach_mismatch_ticks >=
             CAR_APPROACH_MISMATCH_CONFIRM_TICKS))
        {
          s_approach_locked_health = mismatch_health;
          health = mismatch_health;
          if (health == CAR_APPROACH_ENCODER_LEFT_SUSPECT)
          {
            candidate = right_distance;
          }
          else
          {
            candidate = left_distance;
          }
          s_approach_distance_reliable = 1U;
        }
        else
        {
          health = CAR_APPROACH_ENCODERS_BOTH_OK;
          candidate = low_distance;
          if (low_distance > 0)
          {
            s_approach_distance_reliable = 1U;
          }
        }
      }
    }
  }
  else if ((left_sample_valid == 0U) &&
           (right_sample_valid == 0U))
  {
    health = CAR_APPROACH_ENCODERS_BOTH_INVALID;
    s_approach_mismatch_health = CAR_APPROACH_ENCODERS_BOTH_OK;
    s_approach_mismatch_ticks = 0U;
  }
  else if (left_sample_valid == 0U)
  {
    health = CAR_APPROACH_ENCODER_LEFT_SUSPECT;
    s_approach_mismatch_health = CAR_APPROACH_ENCODERS_BOTH_OK;
    s_approach_mismatch_ticks = 0U;
  }
  else
  {
    health = CAR_APPROACH_ENCODER_RIGHT_SUSPECT;
    s_approach_mismatch_health = CAR_APPROACH_ENCODERS_BOTH_OK;
    s_approach_mismatch_ticks = 0U;
  }

  s_approach_encoder_health = health;
  if (health == CAR_APPROACH_ENCODERS_BOTH_INVALID)
  {
    if (confirmed_both_invalid != 0U)
    {
      s_approach_invalid_ticks =
          CAR_APPROACH_INVALID_CONFIRM_TICKS;
    }
    else if (s_approach_invalid_ticks <
             CAR_APPROACH_INVALID_CONFIRM_TICKS)
    {
      s_approach_invalid_ticks++;
    }
  }
  else
  {
    s_approach_invalid_ticks = 0U;
  }

  if (candidate > s_approach_fused_distance)
  {
    s_approach_fused_distance = candidate;
  }

  return s_approach_fused_distance;
}
static CarApproachResult_t Car_RightAngleApproachResult(int32_t target)
{
  if (g_car.line.right_angle_approach_counts <= 0)
  {
    g_car.line.right_angle_approach_travel_counts = 0;
    return CAR_APPROACH_COMPLETE;
  }

  if (target <= 0)
  {
    return CAR_APPROACH_FAULT;
  }

  g_car.line.right_angle_approach_travel_counts =
      Car_RightAngleApproachDistance(target);

  if (s_approach_invalid_ticks >=
      CAR_APPROACH_INVALID_CONFIRM_TICKS)
  {
    return CAR_APPROACH_FAULT;
  }

  if ((g_car.line.right_angle_approach_timeout_ticks > 0U) &&
      ((g_car.control_tick -
        g_car.line.right_angle_approach_start_tick) >=
       g_car.line.right_angle_approach_timeout_ticks))
  {
    return CAR_APPROACH_FAULT;
  }

  if ((s_approach_distance_reliable != 0U) &&
      (s_approach_encoder_health !=
       CAR_APPROACH_ENCODERS_BOTH_INVALID) &&
      (g_car.line.right_angle_approach_travel_counts >=
       g_car.line.right_angle_approach_counts))
  {
    return CAR_APPROACH_COMPLETE;
  }

  return CAR_APPROACH_RUNNING;
}

static uint8_t Car_RightAngleApproachStep(int16_t *left_pwm,
                                         int16_t *right_pwm)
{
  int32_t target = g_car.line.right_angle_approach_speed_counts;
  int32_t limited_target = 0;
  CarApproachResult_t result = CAR_APPROACH_RUNNING;

  if (target <= 0)
  {
    target = g_car.line.base_counts;
  }

  limited_target = Car_LimitTargetCounts(target);
  result = Car_RightAngleApproachResult(limited_target);

  if (result == CAR_APPROACH_COMPLETE)
  {
    int8_t direction = g_car.line.right_angle_approach_direction;
    Car_RightAngleApproachStop();
    Car_RightAngleAssistStart(direction);
    return 0U;
  }

  if (result == CAR_APPROACH_FAULT)
  {
    g_car.line.correction_counts = 0;
    g_car.line.left_target_counts = 0;
    g_car.line.right_target_counts = 0;
    *left_pwm = 0;
    *right_pwm = 0;
    Car_RightAngleAssistStop();
    Car_Stop();
    return 1U;
  }

  g_car.line.correction_counts = 0;
  Car_RightAngleApproachSetTargets(limited_target,
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
      g_car.line.correction_counts = 0;
      Car_RightAngleSetTargets(0, 0, left_pwm, right_pwm);
      Car_RightAngleAssistStop();
      return 1U;
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
                           uint32_t in2_pin,
                           uint8_t short_brake)
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

  if (short_brake != 0U)
  {
    in1 = 1U;
    in2 = 1U;
    duty = 0U;
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
