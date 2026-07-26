/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : debug_uart.c
  * @brief          : HC-05 bluetooth debug UART for VOFA+ and online tuning.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "debug_uart.h"
#include "car_control.h"
#include "line_tracker.h"
#include "mpu6050.h"
#include "ti_msp_dl_config.h"

#include <stddef.h>
#include <stdint.h>

#define DEBUG_UART_RX_BUFFER_SIZE    128U
#define DEBUG_UART_LINE_BUFFER_SIZE  96U
#define DEBUG_UART_VOFA_PERIOD_TICKS 5U
#define DEBUG_UART_VOFA_MODE_TEST    0U
#define DEBUG_UART_VOFA_MODE_LINE    1U
#define DEBUG_UART_VOFA_MODE_SPEED   2U
#define DEBUG_UART_VOFA_MODE_GYRO    3U

#define DEBUG_UART_SEND_LITERAL(text) \
  Debug_UART_SendTextIfAllowed((text), (uint16_t)(sizeof(text) - 1U))

#if (CAR_DEBUG_MODE != DEBUG_MODE_OFF)
static volatile uint8_t s_rx_buffer[DEBUG_UART_RX_BUFFER_SIZE];
static volatile uint16_t s_rx_head = 0U;
static volatile uint16_t s_rx_tail = 0U;

static char s_line_buffer[DEBUG_UART_LINE_BUFFER_SIZE];
static uint16_t s_line_length = 0U;
static uint8_t s_line_overflow = 0U;
static uint32_t s_last_vofa_tick = 0U;
static uint8_t s_vofa_stream_enabled = 0U;
static uint8_t s_vofa_mode = DEBUG_UART_VOFA_MODE_TEST;

static uint16_t Debug_UART_NextIndex(uint16_t index, uint16_t size);
static uint32_t Debug_UART_EnterCritical(void);
static void Debug_UART_ExitCritical(uint32_t primask);
static void Debug_UART_SendTextBuffer(const char *text, uint16_t length);
static void Debug_UART_SendTextIfAllowed(const char *text, uint16_t length);
static void Debug_UART_RxPushFromIsr(uint8_t byte);
static uint8_t Debug_UART_RxPop(uint8_t *byte);
static uint8_t Debug_UART_IsSpace(char ch);
static char Debug_UART_ToUpper(char ch);
static const char *Debug_UART_SkipSpaces(const char *text);
static uint8_t Debug_UART_GetArg(const char *line,
                                 const char *keyword,
                                 const char **arg);
static uint8_t Debug_UART_CommandEquals(const char *line, const char *keyword);
static uint8_t Debug_UART_ParseFloat(const char *text, float *value);
static uint8_t Debug_UART_ParseFloatList(const char *text,
                                         float *values,
                                         uint8_t count);
static int32_t Debug_UART_FloatToInt(float value);
static uint16_t Debug_UART_AppendChar(char *buffer, uint16_t pos, char ch);
static uint16_t Debug_UART_AppendString(char *buffer,
                                        uint16_t pos,
                                        const char *text);
static uint16_t Debug_UART_AppendUInt(char *buffer,
                                      uint16_t pos,
                                      uint32_t value,
                                      uint8_t min_width);
static uint16_t Debug_UART_AppendHexByte(char *buffer,
                                         uint16_t pos,
                                         uint8_t value);
static uint16_t Debug_UART_AppendInt(char *buffer, uint16_t pos, int32_t value);
static uint16_t Debug_UART_AppendFloat(char *buffer, uint16_t pos, float value);
static void Debug_UART_SendOkFloat(const char *name, float value);
static void Debug_UART_SendOkInt(const char *name, int32_t value);
static void Debug_UART_ResetMotorPid(volatile CarMotor_t *motor);
static void Debug_UART_ResetLinePid(void);
static void Debug_UART_SendStatus(void);
static void Debug_UART_ParseCommand(const char *line);
#endif

void Debug_UART_Init(void)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_OFF)
  return;
#else
  s_rx_head = 0U;
  s_rx_tail = 0U;
  s_line_length = 0U;
  s_line_overflow = 0U;
  s_last_vofa_tick = g_car.control_tick;
  s_vofa_stream_enabled = 0U;
  s_vofa_mode = DEBUG_UART_VOFA_MODE_TEST;

  DL_UART_Main_disableFIFOs(UART_DEBUG_INST);
  DL_UART_Main_disableInterrupt(UART_DEBUG_INST,
                                DL_UART_MAIN_INTERRUPT_TX |
                                DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);
  while (DL_UART_Main_isRXFIFOEmpty(UART_DEBUG_INST) == false)
  {
    (void)DL_UART_Main_receiveData(UART_DEBUG_INST);
  }
  DL_UART_Main_clearInterruptStatus(UART_DEBUG_INST,
                                    DL_UART_MAIN_INTERRUPT_RX |
                                    DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR |
                                    DL_UART_MAIN_INTERRUPT_TX);
  NVIC_ClearPendingIRQ(UART_DEBUG_INST_INT_IRQN);
  NVIC_EnableIRQ(UART_DEBUG_INST_INT_IRQN);
#endif
}

void Debug_UART_Task(void)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_OFF)
  return;
#else
  Debug_UART_ProcessRx();

#if (CAR_DEBUG_MODE != DEBUG_MODE_VOFA)
  return;
#else
  uint32_t tick = 0U;

  if (s_vofa_stream_enabled == 0U)
  {
    return;
  }

  tick = g_car.control_tick;
  if ((uint32_t)(tick - s_last_vofa_tick) >= DEBUG_UART_VOFA_PERIOD_TICKS)
  {
    s_last_vofa_tick = tick;
    switch (s_vofa_mode)
    {
      case DEBUG_UART_VOFA_MODE_SPEED:
        vofa_speed_send();
        break;

      case DEBUG_UART_VOFA_MODE_GYRO:
        vofa_gyro_send();
        break;

      case DEBUG_UART_VOFA_MODE_LINE:
        vofa_line_send();
        break;

      case DEBUG_UART_VOFA_MODE_TEST:
      default:
        vofa_test_send();
        break;
    }
  }
#endif
#endif
}

void Debug_UART_ProcessRx(void)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_OFF)
  return;
#else
  uint8_t byte = 0U;

  while (Debug_UART_RxPop(&byte) != 0U)
  {
    if ((byte == (uint8_t)'\r') || (byte == (uint8_t)'\n'))
    {
      if (s_line_overflow != 0U)
      {
        s_line_overflow = 0U;
        s_line_length = 0U;
        DEBUG_UART_SEND_LITERAL("ERR line too long\r\n");
      }
      else if (s_line_length > 0U)
      {
        s_line_buffer[s_line_length] = '\0';
        Debug_UART_ParseCommand(s_line_buffer);
        s_line_length = 0U;
      }
    }
    else if (s_line_overflow == 0U)
    {
      if (s_line_length < (DEBUG_UART_LINE_BUFFER_SIZE - 1U))
      {
        s_line_buffer[s_line_length] = (char)byte;
        s_line_length++;
      }
      else
      {
        s_line_overflow = 1U;
        s_line_length = 0U;
      }
    }
  }
#endif
}

void Debug_UART_SendVofa(void)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_VOFA)
  vofa_pid_send();
#endif
}

void vofa_test_send(void)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_VOFA)
  static float x = 0.0f;
  float data[8];
  static const uint8_t tail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};

  x += 0.1f;
  if (x > 1000.0f)
  {
    x = 0.0f;
  }

  data[0] = x;
  data[1] = x * 2.0f;
  data[2] = 100.0f;
  data[3] = -100.0f;
  data[4] = 0.0f;
  data[5] = 0.0f;
  data[6] = 0.0f;
  data[7] = 0.0f;

  debug_uart_send_bytes((const uint8_t *)data, (uint32_t)sizeof(data));
  debug_uart_send_bytes(tail, (uint32_t)sizeof(tail));
#endif
}

void vofa_pid_send(void)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_VOFA)
  vofa_line_send();
#endif
}

void vofa_line_send(void)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_VOFA)
  float data[8];
  static const uint8_t tail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};

  data[0] = (float)g_line.error;
  data[1] = (float)g_car.line.correction_counts;
  data[2] = (float)g_car.line.left_target_counts;
  data[3] = (float)g_car.line.right_target_counts;
  data[4] = (float)g_car.left.pwm_output;
  data[5] = (float)g_car.right.pwm_output;
  data[6] = g_car.line.pid.kp;
  data[7] = g_car.line.pid.kd;

  debug_uart_send_bytes((const uint8_t *)data, (uint32_t)sizeof(data));
  debug_uart_send_bytes(tail, (uint32_t)sizeof(tail));
#endif
}

void vofa_speed_send(void)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_VOFA)
  float data[8];
  static const uint8_t tail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};

  data[0] = (float)g_car.left.target_counts;
  data[1] = (float)g_car.left.measured_counts;
  data[2] = (float)g_car.right.target_counts;
  data[3] = (float)g_car.right.measured_counts;
  data[4] = (float)g_car.left.pwm_output;
  data[5] = (float)g_car.right.pwm_output;
  data[6] = (float)(g_car.left.target_counts - g_car.left.measured_counts);
  data[7] = (float)(g_car.right.target_counts - g_car.right.measured_counts);

  debug_uart_send_bytes((const uint8_t *)data, (uint32_t)sizeof(data));
  debug_uart_send_bytes(tail, (uint32_t)sizeof(tail));
#endif
}

void vofa_gyro_send(void)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_VOFA)
  float data[8];
  static const uint8_t tail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};

  data[0] = g_car.line.gyro_z;
  data[1] = (float)g_mpu6050.gyro_z_raw;
  data[2] = g_mpu6050.gyro_z_bias_dps;
  data[3] = g_car.line.gyro_damping;
  data[4] = g_car.line.gyro_damping * g_car.line.gyro_z;
  data[5] = (float)g_mpu6050.valid;
  data[6] = (float)g_mpu6050.error_count;
  data[7] = (float)g_mpu6050.sample_tick;

  debug_uart_send_bytes((const uint8_t *)data, (uint32_t)sizeof(data));
  debug_uart_send_bytes(tail, (uint32_t)sizeof(tail));
#endif
}

void UART_DEBUG_INST_IRQHandler(void)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_OFF)
  (void)DL_UART_Main_getPendingInterrupt(UART_DEBUG_INST);
#else
  DL_UART_IIDX interrupt_index;

  do
  {
    interrupt_index = DL_UART_Main_getPendingInterrupt(UART_DEBUG_INST);

    switch (interrupt_index)
    {
      case DL_UART_MAIN_IIDX_RX:
      case DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR:
        while (DL_UART_Main_isRXFIFOEmpty(UART_DEBUG_INST) == false)
        {
          Debug_UART_RxPushFromIsr(DL_UART_Main_receiveData(UART_DEBUG_INST));
        }
        DL_UART_Main_clearInterruptStatus(UART_DEBUG_INST,
                                          DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);
        break;

      case DL_UART_MAIN_IIDX_TX:
        DL_UART_Main_disableInterrupt(UART_DEBUG_INST, DL_UART_MAIN_INTERRUPT_TX);
        DL_UART_Main_clearInterruptStatus(UART_DEBUG_INST,
                                          DL_UART_MAIN_INTERRUPT_TX);
        break;

      default:
        break;
    }
  } while (interrupt_index != DL_UART_MAIN_IIDX_NO_INTERRUPT);
#endif
}

#if (CAR_DEBUG_MODE != DEBUG_MODE_OFF)
static uint16_t Debug_UART_NextIndex(uint16_t index, uint16_t size)
{
  index++;
  if (index >= size)
  {
    index = 0U;
  }

  return index;
}

static uint32_t Debug_UART_EnterCritical(void)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  return primask;
}

static void Debug_UART_ExitCritical(uint32_t primask)
{
  if ((primask & 1U) == 0U)
  {
    __enable_irq();
  }
}

static void Debug_UART_SendTextBuffer(const char *text, uint16_t length)
{
  debug_uart_send_bytes((const uint8_t *)text, (uint32_t)length);
}

static void Debug_UART_SendTextIfAllowed(const char *text, uint16_t length)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_CMD)
  Debug_UART_SendTextBuffer(text, length);
#else
  if (s_vofa_stream_enabled == 0U)
  {
    Debug_UART_SendTextBuffer(text, length);
  }
#endif
}
#endif

void debug_uart_send_byte(uint8_t byte)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_OFF)
  (void)byte;
#else
  DL_UART_Main_transmitDataBlocking(UART_DEBUG_INST, byte);
#endif
}

void debug_uart_send_bytes(const uint8_t *buf, uint32_t len)
{
#if (CAR_DEBUG_MODE == DEBUG_MODE_OFF)
  (void)buf;
  (void)len;
#else
  uint32_t index = 0U;

  if (buf == NULL)
  {
    return;
  }

  for (index = 0U; index < len; index++)
  {
    debug_uart_send_byte(buf[index]);
  }
#endif
}

#if (CAR_DEBUG_MODE != DEBUG_MODE_OFF)
static void Debug_UART_RxPushFromIsr(uint8_t byte)
{
  uint16_t next = Debug_UART_NextIndex(s_rx_head, DEBUG_UART_RX_BUFFER_SIZE);

  if (next != s_rx_tail)
  {
    s_rx_buffer[s_rx_head] = byte;
    s_rx_head = next;
  }
}

static uint8_t Debug_UART_RxPop(uint8_t *byte)
{
  uint32_t primask = 0U;

  primask = Debug_UART_EnterCritical();

  if (s_rx_tail == s_rx_head)
  {
    Debug_UART_ExitCritical(primask);
    return 0U;
  }

  *byte = s_rx_buffer[s_rx_tail];
  s_rx_tail = Debug_UART_NextIndex(s_rx_tail, DEBUG_UART_RX_BUFFER_SIZE);

  Debug_UART_ExitCritical(primask);
  return 1U;
}

static uint8_t Debug_UART_IsSpace(char ch)
{
  return ((ch == ' ') || (ch == '\t')) ? 1U : 0U;
}

static char Debug_UART_ToUpper(char ch)
{
  if ((ch >= 'a') && (ch <= 'z'))
  {
    ch = (char)(ch - ('a' - 'A'));
  }

  return ch;
}

static const char *Debug_UART_SkipSpaces(const char *text)
{
  while (Debug_UART_IsSpace(*text) != 0U)
  {
    text++;
  }

  return text;
}

static uint8_t Debug_UART_GetArg(const char *line,
                                 const char *keyword,
                                 const char **arg)
{
  const char *cursor = Debug_UART_SkipSpaces(line);

  while (*keyword != '\0')
  {
    if (Debug_UART_ToUpper(*cursor) != Debug_UART_ToUpper(*keyword))
    {
      return 0U;
    }
    cursor++;
    keyword++;
  }

  if ((*cursor != '\0') && (Debug_UART_IsSpace(*cursor) == 0U))
  {
    return 0U;
  }

  *arg = Debug_UART_SkipSpaces(cursor);
  return 1U;
}

static uint8_t Debug_UART_CommandEquals(const char *line, const char *keyword)
{
  const char *arg = NULL;

  if (Debug_UART_GetArg(line, keyword, &arg) == 0U)
  {
    return 0U;
  }

  return (*Debug_UART_SkipSpaces(arg) == '\0') ? 1U : 0U;
}

static uint8_t Debug_UART_ParseFloat(const char *text, float *value)
{
  float result = 0.0f;
  float fraction = 0.1f;
  float sign = 1.0f;
  uint8_t digit_seen = 0U;

  text = Debug_UART_SkipSpaces(text);

  if (*text == '-')
  {
    sign = -1.0f;
    text++;
  }
  else if (*text == '+')
  {
    text++;
  }

  while ((*text >= '0') && (*text <= '9'))
  {
    result = (result * 10.0f) + (float)(*text - '0');
    digit_seen = 1U;
    text++;
  }

  if (*text == '.')
  {
    text++;
    while ((*text >= '0') && (*text <= '9'))
    {
      result += (float)(*text - '0') * fraction;
      fraction *= 0.1f;
      digit_seen = 1U;
      text++;
    }
  }

  text = Debug_UART_SkipSpaces(text);
  if ((digit_seen == 0U) || (*text != '\0'))
  {
    return 0U;
  }

  *value = result * sign;
  return 1U;
}

static uint8_t Debug_UART_ParseFloatList(const char *text,
                                         float *values,
                                         uint8_t count)
{
  uint8_t index = 0U;

  for (index = 0U; index < count; index++)
  {
    float result = 0.0f;
    float fraction = 0.1f;
    float sign = 1.0f;
    uint8_t digit_seen = 0U;

    text = Debug_UART_SkipSpaces(text);

    if (*text == '-')
    {
      sign = -1.0f;
      text++;
    }
    else if (*text == '+')
    {
      text++;
    }

    while ((*text >= '0') && (*text <= '9'))
    {
      result = (result * 10.0f) + (float)(*text - '0');
      digit_seen = 1U;
      text++;
    }

    if (*text == '.')
    {
      text++;
      while ((*text >= '0') && (*text <= '9'))
      {
        result += (float)(*text - '0') * fraction;
        fraction *= 0.1f;
        digit_seen = 1U;
        text++;
      }
    }

    if (digit_seen == 0U)
    {
      return 0U;
    }

    values[index] = result * sign;

    if ((index + 1U) < count)
    {
      if (Debug_UART_IsSpace(*text) == 0U)
      {
        return 0U;
      }
    }
  }

  text = Debug_UART_SkipSpaces(text);
  return (*text == '\0') ? 1U : 0U;
}

static int32_t Debug_UART_FloatToInt(float value)
{
  if (value >= 0.0f)
  {
    return (int32_t)(value + 0.5f);
  }

  return (int32_t)(value - 0.5f);
}

static uint16_t Debug_UART_AppendChar(char *buffer, uint16_t pos, char ch)
{
  if (pos < (DEBUG_UART_LINE_BUFFER_SIZE - 1U))
  {
    buffer[pos] = ch;
    pos++;
  }

  return pos;
}

static uint16_t Debug_UART_AppendString(char *buffer,
                                        uint16_t pos,
                                        const char *text)
{
  while (*text != '\0')
  {
    pos = Debug_UART_AppendChar(buffer, pos, *text);
    text++;
  }

  return pos;
}

static uint16_t Debug_UART_AppendUInt(char *buffer,
                                      uint16_t pos,
                                      uint32_t value,
                                      uint8_t min_width)
{
  char digits[10];
  uint8_t count = 0U;

  do
  {
    digits[count] = (char)('0' + (value % 10U));
    value /= 10U;
    count++;
  } while ((value != 0U) && (count < (uint8_t)sizeof(digits)));

  while (count < min_width)
  {
    digits[count] = '0';
    count++;
  }

  while (count > 0U)
  {
    count--;
    pos = Debug_UART_AppendChar(buffer, pos, digits[count]);
  }

  return pos;
}

static uint16_t Debug_UART_AppendInt(char *buffer, uint16_t pos, int32_t value)
{
  uint32_t magnitude = 0U;

  if (value < 0)
  {
    pos = Debug_UART_AppendChar(buffer, pos, '-');
    magnitude = (uint32_t)(-(value + 1)) + 1U;
  }
  else
  {
    magnitude = (uint32_t)value;
  }

  return Debug_UART_AppendUInt(buffer, pos, magnitude, 1U);
}

static uint16_t Debug_UART_AppendHexByte(char *buffer,
                                         uint16_t pos,
                                         uint8_t value)
{
  static const char digits[] = "0123456789ABCDEF";

  pos = Debug_UART_AppendChar(buffer, pos, digits[(value >> 4) & 0x0FU]);
  pos = Debug_UART_AppendChar(buffer, pos, digits[value & 0x0FU]);

  return pos;
}

static uint16_t Debug_UART_AppendFloat(char *buffer, uint16_t pos, float value)
{
  uint32_t integer_part = 0U;
  uint32_t fraction_part = 0U;

  if (value < 0.0f)
  {
    pos = Debug_UART_AppendChar(buffer, pos, '-');
    value = -value;
  }

  integer_part = (uint32_t)value;
  fraction_part = (uint32_t)(((value - (float)integer_part) * 1000.0f) + 0.5f);

  if (fraction_part >= 1000U)
  {
    integer_part++;
    fraction_part -= 1000U;
  }

  pos = Debug_UART_AppendUInt(buffer, pos, integer_part, 1U);
  pos = Debug_UART_AppendChar(buffer, pos, '.');
  pos = Debug_UART_AppendUInt(buffer, pos, fraction_part, 3U);

  return pos;
}

static void Debug_UART_SendOkFloat(const char *name, float value)
{
  char buffer[DEBUG_UART_LINE_BUFFER_SIZE];
  uint16_t pos = 0U;

  pos = Debug_UART_AppendString(buffer, pos, "OK ");
  pos = Debug_UART_AppendString(buffer, pos, name);
  pos = Debug_UART_AppendChar(buffer, pos, '=');
  pos = Debug_UART_AppendFloat(buffer, pos, value);
  pos = Debug_UART_AppendString(buffer, pos, "\r\n");
  buffer[pos] = '\0';

  Debug_UART_SendTextIfAllowed(buffer, pos);
}

static void Debug_UART_SendOkInt(const char *name, int32_t value)
{
  char buffer[DEBUG_UART_LINE_BUFFER_SIZE];
  uint16_t pos = 0U;

  pos = Debug_UART_AppendString(buffer, pos, "OK ");
  pos = Debug_UART_AppendString(buffer, pos, name);
  pos = Debug_UART_AppendChar(buffer, pos, '=');
  pos = Debug_UART_AppendInt(buffer, pos, value);
  pos = Debug_UART_AppendString(buffer, pos, "\r\n");
  buffer[pos] = '\0';

  Debug_UART_SendTextIfAllowed(buffer, pos);
}

static void Debug_UART_ResetMotorPid(volatile CarMotor_t *motor)
{
  motor->pid.integral = 0.0f;
  motor->pid.previous_error = 0.0f;
}

static void Debug_UART_ResetLinePid(void)
{
  g_car.line.pid.integral = 0.0f;
  g_car.line.pid.previous_error = 0.0f;
}

static void Debug_UART_SendStatus(void)
{
  char buffer[DEBUG_UART_LINE_BUFFER_SIZE];
  uint16_t pos = 0U;

  if ((CAR_DEBUG_MODE == DEBUG_MODE_OFF) ||
      ((CAR_DEBUG_MODE == DEBUG_MODE_VOFA) && (s_vofa_stream_enabled != 0U)))
  {
    return;
  }

  pos = Debug_UART_AppendString(buffer, pos, "LINE P=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_car.line.pid.kp);
  pos = Debug_UART_AppendString(buffer, pos, " I=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_car.line.pid.ki);
  pos = Debug_UART_AppendString(buffer, pos, " D=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_car.line.pid.kd);
  pos = Debug_UART_AppendString(buffer, pos, " G=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_car.line.gyro_damping);
  pos = Debug_UART_AppendString(buffer, pos, " BASE=");
  pos = Debug_UART_AppendInt(buffer, pos, g_car.line.base_counts);
  pos = Debug_UART_AppendString(buffer, pos, " APP=");
  pos = Debug_UART_AppendInt(buffer, pos,
                             g_car.line.right_angle_approach_counts);
  pos = Debug_UART_AppendString(buffer, pos, " APPSPD=");
  pos = Debug_UART_AppendInt(buffer, pos,
                             g_car.line.right_angle_approach_speed_counts);
  pos = Debug_UART_AppendString(buffer, pos, " MODE=");
  pos = Debug_UART_AppendInt(buffer, pos, (int32_t)g_car.mode);
  pos = Debug_UART_AppendString(buffer, pos, "\r\n");
  buffer[pos] = '\0';

  Debug_UART_SendTextIfAllowed(buffer, pos);

  pos = 0U;
  pos = Debug_UART_AppendString(buffer, pos, "CORNER STATE=");
  pos = Debug_UART_AppendInt(buffer, pos,
                             (int32_t)g_car.line.right_angle_state);
  pos = Debug_UART_AppendString(buffer, pos, " DIR=");
  pos = Debug_UART_AppendInt(buffer, pos,
                             (int32_t)g_car.line.right_angle_assist_direction);
  pos = Debug_UART_AppendString(buffer, pos, " DET=");
  pos = Debug_UART_AppendInt(buffer, pos,
                             (int32_t)g_car.line.right_angle_detect_count);
  pos = Debug_UART_AppendString(buffer, pos, " CLEAR=");
  pos = Debug_UART_AppendInt(buffer, pos,
      (int32_t)g_car.line.right_angle_old_line_clear_count);
  pos = Debug_UART_AppendString(buffer, pos, " SEEN=");
  pos = Debug_UART_AppendInt(buffer, pos,
                             (int32_t)g_car.line.right_angle_center_seen_count);
  pos = Debug_UART_AppendString(buffer, pos, " REC=");
  pos = Debug_UART_AppendInt(buffer, pos,
                             (int32_t)g_car.line.right_angle_recovery_count);
  pos = Debug_UART_AppendString(buffer, pos, " YAW=");
  pos = Debug_UART_AppendFloat(buffer, pos,
                               g_car.line.right_angle_yaw_deg);
  pos = Debug_UART_AppendString(buffer, pos, "\r\n");
  buffer[pos] = '\0';

  Debug_UART_SendTextIfAllowed(buffer, pos);

  pos = 0U;
  pos = Debug_UART_AppendString(buffer, pos, "LEFT P=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_car.left.pid.kp);
  pos = Debug_UART_AppendString(buffer, pos, " I=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_car.left.pid.ki);
  pos = Debug_UART_AppendString(buffer, pos, " D=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_car.left.pid.kd);
  pos = Debug_UART_AppendString(buffer, pos, "\r\n");
  buffer[pos] = '\0';

  Debug_UART_SendTextIfAllowed(buffer, pos);

  pos = 0U;
  pos = Debug_UART_AppendString(buffer, pos, "RIGHT P=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_car.right.pid.kp);
  pos = Debug_UART_AppendString(buffer, pos, " I=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_car.right.pid.ki);
  pos = Debug_UART_AppendString(buffer, pos, " D=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_car.right.pid.kd);
  pos = Debug_UART_AppendString(buffer, pos, "\r\n");
  buffer[pos] = '\0';

  Debug_UART_SendTextIfAllowed(buffer, pos);

  pos = 0U;
  pos = Debug_UART_AppendString(buffer, pos, "MPU ADDR=0x");
  pos = Debug_UART_AppendHexByte(buffer, pos, g_mpu6050.address);
  pos = Debug_UART_AppendString(buffer, pos, " PRESENT=");
  pos = Debug_UART_AppendInt(buffer, pos, (int32_t)g_mpu6050.present);
  pos = Debug_UART_AppendString(buffer, pos, " VALID=");
  pos = Debug_UART_AppendInt(buffer, pos, (int32_t)g_mpu6050.valid);
  pos = Debug_UART_AppendString(buffer, pos, " GZ=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_mpu6050.gyro_z_filtered_dps);
  pos = Debug_UART_AppendString(buffer, pos, " BIAS=");
  pos = Debug_UART_AppendFloat(buffer, pos, g_mpu6050.gyro_z_bias_dps);
  pos = Debug_UART_AppendString(buffer, pos, " ERR=");
  pos = Debug_UART_AppendInt(buffer, pos, (int32_t)g_mpu6050.error_count);
  pos = Debug_UART_AppendString(buffer, pos, "\r\n");
  buffer[pos] = '\0';

  Debug_UART_SendTextIfAllowed(buffer, pos);
}

static void Debug_UART_ParseCommand(const char *line)
{
  const char *arg = NULL;
  float value = 0.0f;
  float values[6];
  int32_t base = 0;

  if (Debug_UART_GetArg(line, "P", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      g_car.line.pid.kp = value;
      Debug_UART_SendOkFloat("P", value);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR P\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "I", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      g_car.line.pid.ki = value;
      Debug_UART_SendOkFloat("I", value);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR I\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "D", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      g_car.line.pid.kd = value;
      Debug_UART_SendOkFloat("D", value);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR D\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "G", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      g_car.line.gyro_damping = value;
      Debug_UART_SendOkFloat("G", value);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR G\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "BASE", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      base = Debug_UART_FloatToInt(value);
      if (base < 0)
      {
        base = 0;
      }
      g_car.line.base_counts = base;
      Debug_UART_SendOkInt("BASE", base);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR BASE\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "APPROACH", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      base = Debug_UART_FloatToInt(value);
      if (base < 0)
      {
        base = 0;
      }
      g_car.line.right_angle_approach_counts = base;
      Debug_UART_SendOkInt("APPROACH", base);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR APPROACH\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "APPSPD", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      base = Debug_UART_FloatToInt(value);
      if (base < 0)
      {
        base = 0;
      }
      g_car.line.right_angle_approach_speed_counts = base;
      Debug_UART_SendOkInt("APPSPD", base);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR APPSPD\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "LP", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      g_car.left.pid.kp = value;
      Debug_UART_SendOkFloat("LP", value);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR LP\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "LI", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      g_car.left.pid.ki = value;
      Debug_UART_SendOkFloat("LI", value);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR LI\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "LD", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      g_car.left.pid.kd = value;
      Debug_UART_SendOkFloat("LD", value);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR LD\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "RP", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      g_car.right.pid.kp = value;
      Debug_UART_SendOkFloat("RP", value);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR RP\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "RI", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      g_car.right.pid.ki = value;
      Debug_UART_SendOkFloat("RI", value);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR RI\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "RD", &arg) != 0U)
  {
    if (Debug_UART_ParseFloat(arg, &value) != 0U)
    {
      g_car.right.pid.kd = value;
      Debug_UART_SendOkFloat("RD", value);
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR RD\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "LSPD", &arg) != 0U)
  {
    if (Debug_UART_ParseFloatList(arg, values, 3U) != 0U)
    {
      g_car.left.pid.kp = values[0];
      g_car.left.pid.ki = values[1];
      g_car.left.pid.kd = values[2];
      Debug_UART_ResetMotorPid(&g_car.left);
      DEBUG_UART_SEND_LITERAL("OK LSPD\r\n");
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR LSPD\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "RSPD", &arg) != 0U)
  {
    if (Debug_UART_ParseFloatList(arg, values, 3U) != 0U)
    {
      g_car.right.pid.kp = values[0];
      g_car.right.pid.ki = values[1];
      g_car.right.pid.kd = values[2];
      Debug_UART_ResetMotorPid(&g_car.right);
      DEBUG_UART_SEND_LITERAL("OK RSPD\r\n");
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR RSPD\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "MSPD", &arg) != 0U)
  {
    if (Debug_UART_ParseFloatList(arg, values, 6U) != 0U)
    {
      g_car.left.pid.kp = values[0];
      g_car.left.pid.ki = values[1];
      g_car.left.pid.kd = values[2];
      g_car.right.pid.kp = values[3];
      g_car.right.pid.ki = values[4];
      g_car.right.pid.kd = values[5];
      Debug_UART_ResetMotorPid(&g_car.left);
      Debug_UART_ResetMotorPid(&g_car.right);
      DEBUG_UART_SEND_LITERAL("OK MSPD\r\n");
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR MSPD\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "LINEPID", &arg) != 0U)
  {
    if (Debug_UART_ParseFloatList(arg, values, 3U) != 0U)
    {
      g_car.line.pid.kp = values[0];
      g_car.line.pid.ki = values[1];
      g_car.line.pid.kd = values[2];
      Debug_UART_ResetLinePid();
      DEBUG_UART_SEND_LITERAL("OK LINEPID\r\n");
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR LINEPID\r\n");
    }
  }
  else if (Debug_UART_GetArg(line, "SPDSET", &arg) != 0U)
  {
    if (Debug_UART_ParseFloatList(arg, values, 2U) != 0U)
    {
      g_car.left.target_counts = Debug_UART_FloatToInt(values[0]);
      g_car.right.target_counts = Debug_UART_FloatToInt(values[1]);
      g_car.mode = CAR_MODE_SPEED_PID;
      Debug_UART_ResetMotorPid(&g_car.left);
      Debug_UART_ResetMotorPid(&g_car.right);
      DEBUG_UART_SEND_LITERAL("OK SPDSET\r\n");
    }
    else
    {
      DEBUG_UART_SEND_LITERAL("ERR SPDSET\r\n");
    }
  }
  else if (Debug_UART_CommandEquals(line, "LSTOP") != 0U)
  {
    Debug_UART_ResetMotorPid(&g_car.left);
    DEBUG_UART_SEND_LITERAL("OK LSTOP\r\n");
  }
  else if (Debug_UART_CommandEquals(line, "RSTOP") != 0U)
  {
    Debug_UART_ResetMotorPid(&g_car.right);
    DEBUG_UART_SEND_LITERAL("OK RSTOP\r\n");
  }
  else if (Debug_UART_CommandEquals(line, "MSTOP") != 0U)
  {
    Debug_UART_ResetMotorPid(&g_car.left);
    Debug_UART_ResetMotorPid(&g_car.right);
    DEBUG_UART_SEND_LITERAL("OK MSTOP\r\n");
  }
  else if (Debug_UART_CommandEquals(line, "STOP") != 0U)
  {
    Car_Stop();
    DEBUG_UART_SEND_LITERAL("OK STOP\r\n");
  }
  else if (Debug_UART_CommandEquals(line, "START") != 0U)
  {
    Car_StartLineFollow();
    DEBUG_UART_SEND_LITERAL("OK START\r\n");
  }
  else if (Debug_UART_CommandEquals(line, "SHOW") != 0U)
  {
    Debug_UART_SendStatus();
  }
  else if (Debug_UART_CommandEquals(line, "MPU SHOW") != 0U)
  {
    Debug_UART_SendStatus();
  }
  else if (Debug_UART_CommandEquals(line, "MPU CAL") != 0U)
  {
    MPU6050_Calibrate();
    DEBUG_UART_SEND_LITERAL("OK MPU CAL\r\n");
  }
  else if (Debug_UART_CommandEquals(line, "VOFA ON") != 0U)
  {
#if (CAR_DEBUG_MODE == DEBUG_MODE_VOFA)
    DEBUG_UART_SEND_LITERAL("OK VOFA ON\r\n");
    s_last_vofa_tick = g_car.control_tick - DEBUG_UART_VOFA_PERIOD_TICKS;
    s_vofa_stream_enabled = 1U;
#else
    s_vofa_stream_enabled = 0U;
    DEBUG_UART_SEND_LITERAL("OK VOFA DISABLED\r\n");
#endif
  }
  else if (Debug_UART_CommandEquals(line, "VOFA OFF") != 0U)
  {
    DEBUG_UART_SEND_LITERAL("OK VOFA OFF\r\n");
    s_vofa_stream_enabled = 0U;
  }
  else if (Debug_UART_CommandEquals(line, "TEST ON") != 0U)
  {
    s_vofa_mode = DEBUG_UART_VOFA_MODE_TEST;
    DEBUG_UART_SEND_LITERAL("OK TEST ON\r\n");
  }
  else if (Debug_UART_CommandEquals(line, "PID ON") != 0U)
  {
    s_vofa_mode = DEBUG_UART_VOFA_MODE_LINE;
    DEBUG_UART_SEND_LITERAL("OK PID ON\r\n");
  }
  else if (Debug_UART_CommandEquals(line, "LINE ON") != 0U)
  {
    s_vofa_mode = DEBUG_UART_VOFA_MODE_LINE;
    DEBUG_UART_SEND_LITERAL("OK LINE ON\r\n");
  }
  else if (Debug_UART_CommandEquals(line, "SPD ON") != 0U)
  {
    s_vofa_mode = DEBUG_UART_VOFA_MODE_SPEED;
    DEBUG_UART_SEND_LITERAL("OK SPD ON\r\n");
  }
  else if (Debug_UART_CommandEquals(line, "GYRO ON") != 0U)
  {
    s_vofa_mode = DEBUG_UART_VOFA_MODE_GYRO;
    DEBUG_UART_SEND_LITERAL("OK GYRO ON\r\n");
  }
  else if (*Debug_UART_SkipSpaces(line) != '\0')
  {
    DEBUG_UART_SEND_LITERAL("ERR unknown\r\n");
  }
}
#endif

void Debug_UART_SetGyroZ(float gyro_z)
{
  g_car.line.gyro_z = gyro_z;
}
