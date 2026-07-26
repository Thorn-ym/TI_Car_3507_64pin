/*
 * GPIO bit-banged I2C transport for the on-board OLED.
 */

#include "board_oled_i2c.h"
#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

#define BOARD_OLED_I2C_HALF_PERIOD_CYCLES (CPUCLK_FREQ / 200000U)
#define BOARD_OLED_I2C_CLOCK_HIGH_TIMEOUT  1000U
#define BOARD_OLED_I2C_RECOVERY_CLOCKS     9U

static void Board_OLED_I2C_Delay(void);
static void Board_OLED_I2C_DriveSCLLow(void);
static void Board_OLED_I2C_ReleaseSCL(void);
static void Board_OLED_I2C_DriveSDALow(void);
static void Board_OLED_I2C_ReleaseSDA(void);
static bool Board_OLED_I2C_IsSCLHigh(void);
static bool Board_OLED_I2C_IsSDAHigh(void);
static bool Board_OLED_I2C_WaitForSCLHigh(void);
static bool Board_OLED_I2C_RecoverBus(void);
static bool Board_OLED_I2C_Start(void);
static bool Board_OLED_I2C_Stop(void);
static bool Board_OLED_I2C_WriteBit(uint8_t bit);
static bool Board_OLED_I2C_ReadAck(void);
static bool Board_OLED_I2C_WriteByte(uint8_t byte);

void Board_OLED_I2C_Init(void)
{
  DL_GPIO_clearPins(
      GPIO_OLED_PORT, GPIO_OLED_SCL_PIN | GPIO_OLED_SDA_PIN);
  DL_GPIO_disableOutput(
      GPIO_OLED_PORT, GPIO_OLED_SCL_PIN | GPIO_OLED_SDA_PIN);
  Board_OLED_I2C_Delay();
  (void)Board_OLED_I2C_RecoverBus();
}

bool Board_OLED_I2C_Write(uint8_t address,
                          const uint8_t *data,
                          uint16_t len)
{
  uint16_t index = 0U;
  bool success = true;

  if ((address > 0x7FU) ||
      (data == (const uint8_t *)0) ||
      (len == 0U))
  {
    return false;
  }

  if ((!Board_OLED_I2C_IsSCLHigh() || !Board_OLED_I2C_IsSDAHigh()) &&
      !Board_OLED_I2C_RecoverBus())
  {
    return false;
  }

  if (!Board_OLED_I2C_Start())
  {
    return false;
  }

  success = Board_OLED_I2C_WriteByte((uint8_t)(address << 1));

  for (index = 0U; success && (index < len); index++)
  {
    success = Board_OLED_I2C_WriteByte(data[index]);
  }

  if (!Board_OLED_I2C_Stop())
  {
    success = false;
  }

  return success;
}

static void Board_OLED_I2C_Delay(void)
{
  delay_cycles(BOARD_OLED_I2C_HALF_PERIOD_CYCLES);
}

static void Board_OLED_I2C_DriveSCLLow(void)
{
  DL_GPIO_clearPins(GPIO_OLED_PORT, GPIO_OLED_SCL_PIN);
  DL_GPIO_enableOutput(GPIO_OLED_PORT, GPIO_OLED_SCL_PIN);
}

static void Board_OLED_I2C_ReleaseSCL(void)
{
  DL_GPIO_disableOutput(GPIO_OLED_PORT, GPIO_OLED_SCL_PIN);
}

static void Board_OLED_I2C_DriveSDALow(void)
{
  DL_GPIO_clearPins(GPIO_OLED_PORT, GPIO_OLED_SDA_PIN);
  DL_GPIO_enableOutput(GPIO_OLED_PORT, GPIO_OLED_SDA_PIN);
}

static void Board_OLED_I2C_ReleaseSDA(void)
{
  DL_GPIO_disableOutput(GPIO_OLED_PORT, GPIO_OLED_SDA_PIN);
}

static bool Board_OLED_I2C_IsSCLHigh(void)
{
  return ((DL_GPIO_readPins(GPIO_OLED_PORT, GPIO_OLED_SCL_PIN) &
           GPIO_OLED_SCL_PIN) != 0U) ? true : false;
}

static bool Board_OLED_I2C_IsSDAHigh(void)
{
  return ((DL_GPIO_readPins(GPIO_OLED_PORT, GPIO_OLED_SDA_PIN) &
           GPIO_OLED_SDA_PIN) != 0U) ? true : false;
}

static bool Board_OLED_I2C_WaitForSCLHigh(void)
{
  uint32_t timeout = BOARD_OLED_I2C_CLOCK_HIGH_TIMEOUT;

  while (timeout > 0U)
  {
    if (Board_OLED_I2C_IsSCLHigh())
    {
      return true;
    }

    Board_OLED_I2C_Delay();
    timeout--;
  }

  return false;
}

static bool Board_OLED_I2C_RecoverBus(void)
{
  uint8_t clock = 0U;

  Board_OLED_I2C_ReleaseSDA();
  Board_OLED_I2C_ReleaseSCL();

  if (!Board_OLED_I2C_WaitForSCLHigh())
  {
    return false;
  }

  for (clock = 0U;
       (clock < BOARD_OLED_I2C_RECOVERY_CLOCKS) &&
       !Board_OLED_I2C_IsSDAHigh();
       clock++)
  {
    Board_OLED_I2C_DriveSCLLow();
    Board_OLED_I2C_Delay();
    Board_OLED_I2C_ReleaseSCL();

    if (!Board_OLED_I2C_WaitForSCLHigh())
    {
      return false;
    }

    Board_OLED_I2C_Delay();
  }

  if (!Board_OLED_I2C_Stop())
  {
    return false;
  }

  return Board_OLED_I2C_IsSCLHigh() && Board_OLED_I2C_IsSDAHigh();
}

static bool Board_OLED_I2C_Start(void)
{
  Board_OLED_I2C_ReleaseSDA();
  Board_OLED_I2C_ReleaseSCL();

  if (!Board_OLED_I2C_WaitForSCLHigh() || !Board_OLED_I2C_IsSDAHigh())
  {
    return false;
  }

  Board_OLED_I2C_Delay();
  Board_OLED_I2C_DriveSDALow();
  Board_OLED_I2C_Delay();
  Board_OLED_I2C_DriveSCLLow();
  Board_OLED_I2C_Delay();

  return true;
}

static bool Board_OLED_I2C_Stop(void)
{
  Board_OLED_I2C_DriveSDALow();
  Board_OLED_I2C_Delay();
  Board_OLED_I2C_ReleaseSCL();

  if (!Board_OLED_I2C_WaitForSCLHigh())
  {
    Board_OLED_I2C_ReleaseSDA();
    return false;
  }

  Board_OLED_I2C_Delay();
  Board_OLED_I2C_ReleaseSDA();
  Board_OLED_I2C_Delay();

  return Board_OLED_I2C_IsSDAHigh();
}

static bool Board_OLED_I2C_WriteBit(uint8_t bit)
{
  if (bit != 0U)
  {
    Board_OLED_I2C_ReleaseSDA();
  }
  else
  {
    Board_OLED_I2C_DriveSDALow();
  }

  Board_OLED_I2C_Delay();
  Board_OLED_I2C_ReleaseSCL();

  if (!Board_OLED_I2C_WaitForSCLHigh())
  {
    Board_OLED_I2C_DriveSCLLow();
    return false;
  }

  Board_OLED_I2C_Delay();
  Board_OLED_I2C_DriveSCLLow();
  Board_OLED_I2C_Delay();

  return true;
}

static bool Board_OLED_I2C_ReadAck(void)
{
  bool acknowledged = false;

  Board_OLED_I2C_ReleaseSDA();
  Board_OLED_I2C_Delay();
  Board_OLED_I2C_ReleaseSCL();

  if (!Board_OLED_I2C_WaitForSCLHigh())
  {
    Board_OLED_I2C_DriveSCLLow();
    return false;
  }

  Board_OLED_I2C_Delay();
  acknowledged = !Board_OLED_I2C_IsSDAHigh();
  Board_OLED_I2C_DriveSCLLow();
  Board_OLED_I2C_Delay();

  return acknowledged;
}

static bool Board_OLED_I2C_WriteByte(uint8_t byte)
{
  uint8_t bit = 0U;

  for (bit = 0U; bit < 8U; bit++)
  {
    if (!Board_OLED_I2C_WriteBit((uint8_t)(byte & 0x80U)))
    {
      return false;
    }

    byte <<= 1;
  }

  return Board_OLED_I2C_ReadAck();
}
