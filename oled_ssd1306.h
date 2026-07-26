/*
 * SSD1306 128x64 I2C OLED driver.
 */

#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define OLED_WIDTH  128U
#define OLED_HEIGHT 64U

typedef struct
{
  uint8_t address;
  uint8_t present;
  uint8_t initialized;
  uint32_t error_count;
} OLED_t;

extern volatile OLED_t g_oled;

void OLED_Init(void);
void OLED_Task(uint32_t tick);
void OLED_Clear(void);
void OLED_Update(void);
void OLED_DrawAscii(uint8_t x, uint8_t page, const char *text);
void OLED_DrawChinese(uint8_t x, uint8_t page, uint8_t glyph_id);
void OLED_DrawProblemSelect(uint8_t problem);
void OLED_DrawProblemSelected(uint8_t problem);

#ifdef __cplusplus
}
#endif

#endif /* OLED_SSD1306_H */
