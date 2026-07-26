/*
 * Blocking I2C helpers for the I2C1 bus used by MPU6050.
 */

#include "board_i2c.h"
#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

#define BOARD_I2C_TIMEOUT_LOOPS 160000U

static bool Board_I2C_WaitForStatus(uint32_t mask, bool set);
static bool Board_I2C_WaitUntilIdle(void);
static bool Board_I2C_WaitWhileBusy(void);
static bool Board_I2C_HasError(void);
static void Board_I2C_RecoverBus(void);
static bool Board_I2C_WriteByteStream(uint8_t address,
                                      const uint8_t *data,
                                      uint16_t len);

bool Board_I2C_Probe(uint8_t address)
{
  Board_I2C_RecoverBus();

  if (!Board_I2C_WaitUntilIdle())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  DL_I2C_startControllerTransfer(I2C_MPU_INST,
                                 address,
                                 DL_I2C_CONTROLLER_DIRECTION_TX,
                                 0U);

  if (!Board_I2C_WaitWhileBusy())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  if (Board_I2C_HasError())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  return true;
}

bool Board_I2C_Write(uint8_t address, const uint8_t *data, uint16_t len)
{
  if ((data == (const uint8_t *)0) || (len == 0U))
  {
    return false;
  }

  return Board_I2C_WriteByteStream(address, data, len);
}

bool Board_I2C_WriteReg(uint8_t address, uint8_t reg, uint8_t value)
{
  uint8_t data[2];

  data[0] = reg;
  data[1] = value;

  return Board_I2C_WriteByteStream(address, data, (uint16_t)sizeof(data));
}

bool Board_I2C_ReadRegs(uint8_t address, uint8_t reg, uint8_t *data, uint8_t len)
{
  uint8_t index = 0U;

  if ((data == (uint8_t *)0) || (len == 0U))
  {
    return false;
  }

  Board_I2C_RecoverBus();

  if (!Board_I2C_WaitUntilIdle())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  if (DL_I2C_fillControllerTXFIFO(I2C_MPU_INST, &reg, 1U) != 1U)
  {
    Board_I2C_RecoverBus();
    return false;
  }

  DL_I2C_startControllerTransferAdvanced(I2C_MPU_INST,
                                         address,
                                         DL_I2C_CONTROLLER_DIRECTION_TX,
                                         1U,
                                         DL_I2C_CONTROLLER_START_ENABLE,
                                         DL_I2C_CONTROLLER_STOP_DISABLE,
                                         DL_I2C_CONTROLLER_ACK_DISABLE);

  if (!Board_I2C_WaitWhileBusy())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  if (Board_I2C_HasError())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  DL_I2C_startControllerTransferAdvanced(I2C_MPU_INST,
                                         address,
                                         DL_I2C_CONTROLLER_DIRECTION_RX,
                                         len,
                                         DL_I2C_CONTROLLER_START_ENABLE,
                                         DL_I2C_CONTROLLER_STOP_ENABLE,
                                         DL_I2C_CONTROLLER_ACK_DISABLE);

  for (index = 0U; index < len; index++)
  {
    uint32_t timeout = BOARD_I2C_TIMEOUT_LOOPS;

    while (DL_I2C_isControllerRXFIFOEmpty(I2C_MPU_INST) != false)
    {
      if ((DL_I2C_getControllerStatus(I2C_MPU_INST) &
           DL_I2C_CONTROLLER_STATUS_ERROR) != 0U)
      {
        Board_I2C_RecoverBus();
        return false;
      }

      if (timeout == 0U)
      {
        Board_I2C_RecoverBus();
        return false;
      }
      timeout--;
    }

    data[index] = DL_I2C_receiveControllerData(I2C_MPU_INST);
  }

  if (!Board_I2C_WaitWhileBusy())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  if (Board_I2C_HasError())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  return true;
}

static bool Board_I2C_WaitForStatus(uint32_t mask, bool set)
{
  uint32_t timeout = BOARD_I2C_TIMEOUT_LOOPS;

  while (timeout > 0U)
  {
    uint32_t status = DL_I2C_getControllerStatus(I2C_MPU_INST);
    bool matched = ((status & mask) != 0U) ? true : false;

    if (matched == set)
    {
      return true;
    }

    if ((status & DL_I2C_CONTROLLER_STATUS_ERROR) != 0U)
    {
      return false;
    }

    timeout--;
  }

  return false;
}

static bool Board_I2C_WaitUntilIdle(void)
{
  return Board_I2C_WaitForStatus(DL_I2C_CONTROLLER_STATUS_IDLE, true);
}

static bool Board_I2C_WaitWhileBusy(void)
{
  return Board_I2C_WaitForStatus(DL_I2C_CONTROLLER_STATUS_BUSY, false);
}

static bool Board_I2C_HasError(void)
{
  return ((DL_I2C_getControllerStatus(I2C_MPU_INST) &
           DL_I2C_CONTROLLER_STATUS_ERROR) != 0U) ? true : false;
}

static void Board_I2C_RecoverBus(void)
{
  DL_I2C_flushControllerTXFIFO(I2C_MPU_INST);
  DL_I2C_flushControllerRXFIFO(I2C_MPU_INST);
  DL_I2C_resetControllerTransfer(I2C_MPU_INST);
  DL_I2C_clearInterruptStatus(I2C_MPU_INST,
                              DL_I2C_INTERRUPT_CONTROLLER_RX_DONE |
                              DL_I2C_INTERRUPT_CONTROLLER_TX_DONE |
                              DL_I2C_INTERRUPT_CONTROLLER_NACK |
                              DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST |
                              DL_I2C_INTERRUPT_CONTROLLER_RXFIFO_TRIGGER |
                              DL_I2C_INTERRUPT_CONTROLLER_TXFIFO_TRIGGER |
                              DL_I2C_INTERRUPT_CONTROLLER_RXFIFO_FULL |
                              DL_I2C_INTERRUPT_CONTROLLER_TXFIFO_EMPTY |
                              DL_I2C_INTERRUPT_CONTROLLER_START |
                              DL_I2C_INTERRUPT_CONTROLLER_STOP);
  delay_cycles(3200U);
}

static bool Board_I2C_WriteByteStream(uint8_t address,
                                      const uint8_t *data,
                                      uint16_t len)
{
  Board_I2C_RecoverBus();

  if (!Board_I2C_WaitUntilIdle())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  if (DL_I2C_fillControllerTXFIFO(I2C_MPU_INST, data, len) != len)
  {
    Board_I2C_RecoverBus();
    return false;
  }

  DL_I2C_startControllerTransfer(I2C_MPU_INST,
                                 address,
                                 DL_I2C_CONTROLLER_DIRECTION_TX,
                                 len);

  if (!Board_I2C_WaitWhileBusy())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  if (Board_I2C_HasError())
  {
    Board_I2C_RecoverBus();
    return false;
  }

  return true;
}
