/*
 * GPIO bit-banged I2C transport for the on-board OLED.
 */

#ifndef BOARD_OLED_I2C_H
#define BOARD_OLED_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

void Board_OLED_I2C_Init(void);
bool Board_OLED_I2C_Write(uint8_t address,
                          const uint8_t *data,
                          uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_OLED_I2C_H */
