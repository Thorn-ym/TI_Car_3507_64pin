/*
 * Blocking I2C helpers for the I2C1 bus used by MPU6050.
 */

#ifndef BOARD_I2C_H
#define BOARD_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

bool Board_I2C_Probe(uint8_t address);
bool Board_I2C_Write(uint8_t address, const uint8_t *data, uint16_t len);
bool Board_I2C_WriteReg(uint8_t address, uint8_t reg, uint8_t value);
bool Board_I2C_ReadRegs(uint8_t address, uint8_t reg, uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_I2C_H */
