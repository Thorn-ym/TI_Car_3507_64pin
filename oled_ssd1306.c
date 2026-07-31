/*
 * SSD1306 128x64 I2C OLED driver.
 */

#include "oled_ssd1306.h"
#include "board_oled_i2c.h"
#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

#define OLED_ADDR_LOW       0x3CU
#define OLED_ADDR_HIGH      0x3DU
#define OLED_BUFFER_SIZE    ((OLED_WIDTH * OLED_HEIGHT) / 8U)
#define OLED_PAGE_COUNT     (OLED_HEIGHT / 8U)
#define OLED_MENU_ITEM_COUNT 5U
#define OLED_STARTUP_DELAY_CYCLES (CPUCLK_FREQ / 10U)
#define OLED_SERVICE_INTERVAL_TICKS 50U
#define OLED_CMD_NOP        0xE3U

#define OLED_RACE_TIME_MAX_CENTISECONDS 9999U
#define OLED_RACE_TIME_PAGE             3U
#define OLED_RACE_TIME_X               28U
#define OLED_RACE_TIME_DOT_WIDTH        8U
#define OLED_RACE_TIME_WIDTH           72U

#define OLED_ODOMETER_FIRST_PAGE        0U
#define OLED_ODOMETER_LAST_PAGE         2U
#define OLED_ODOMETER_WIDTH            66U
#define OLED_ODOMETER_COUNT_DIGITS       8U
#define OLED_ODOMETER_COUNT_MAX   99999999U
#define OLED_ODOMETER_PROGRESS_MAX     9999U

#define OLED_FONT_SPACE  10U
#define OLED_FONT_L      11U
#define OLED_FONT_R      12U
#define OLED_FONT_D      13U
#define OLED_FONT_M      14U
#define OLED_FONT_COLON  15U
#define OLED_FONT_PLUS   16U
#define OLED_FONT_MINUS  17U
#define OLED_FONT_P      18U
#define OLED_FONT_DOT    19U
#define OLED_FONT_PERCENT 20U

#define OLED_GLYPH_XUAN     0U
#define OLED_GLYPH_ZE       1U
#define OLED_GLYPH_DI       2U
#define OLED_GLYPH_TI       3U
#define OLED_GLYPH_YI       4U
#define OLED_GLYPH_JING     5U

volatile OLED_t g_oled =
{
  .address = OLED_ADDR_LOW,
  .present = 0U,
  .initialized = 0U,
  .error_count = 0U,
};

static uint8_t s_buffer[OLED_BUFFER_SIZE];
static uint32_t s_last_service_tick = 0U;

/* Kept for the generic small numeric text helper. */
static const uint8_t s_font5x7[21][5] =
{
  {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}, /* 0 */
  {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U}, /* 1 */
  {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}, /* 2 */
  {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U}, /* 3 */
  {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}, /* 4 */
  {0x27U, 0x45U, 0x45U, 0x45U, 0x39U}, /* 5 */
  {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}, /* 6 */
  {0x01U, 0x71U, 0x09U, 0x05U, 0x03U}, /* 7 */
  {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}, /* 8 */
  {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}, /* 9 */
  {0x00U, 0x00U, 0x00U, 0x00U, 0x00U}, /* space */
  {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U}, /* L */
  {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U}, /* R */
  {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU}, /* D */
  {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}, /* M */
  {0x00U, 0x36U, 0x36U, 0x00U, 0x00U}, /* : */
  {0x08U, 0x08U, 0x3EU, 0x08U, 0x08U}, /* + */
  {0x08U, 0x08U, 0x08U, 0x08U, 0x08U}, /* - */
  {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U}, /* P */
  {0x00U, 0x60U, 0x60U, 0x00U, 0x00U}, /* . */
  {0x63U, 0x13U, 0x08U, 0x64U, 0x63U}, /* % */
};

/* 16x16 monochrome glyphs rasterized from Windows SimHei. */
static const uint8_t s_chinese16[][32] =
{
  /* 选 */
  {
    0x40U, 0x42U, 0xCEU, 0x00U, 0x60U, 0x5EU, 0x46U, 0xC4U,
    0x7FU, 0xFFU, 0xC4U, 0x44U, 0x44U, 0x40U, 0x00U, 0x00U,
    0x20U, 0x30U, 0x1FU, 0x10U, 0x18U, 0x28U, 0x26U, 0x23U,
    0x20U, 0x27U, 0x2FU, 0x28U, 0x2CU, 0x24U, 0x00U, 0x00U
  },
  /* 择 */
  {
    0x88U, 0x88U, 0xFFU, 0x48U, 0x08U, 0x40U, 0x63U, 0x2FU,
    0x79U, 0xD1U, 0x29U, 0x27U, 0x63U, 0x60U, 0x00U, 0x00U,
    0x21U, 0x20U, 0x3FU, 0x00U, 0x08U, 0x09U, 0x09U, 0x09U,
    0x49U, 0x7FU, 0x09U, 0x09U, 0x09U, 0x08U, 0x00U, 0x00U
  },
  /* 第 */
  {
    0x08U, 0x9CU, 0x97U, 0x93U, 0x9EU, 0x92U, 0xF2U, 0xF8U,
    0x96U, 0x93U, 0x96U, 0xFEU, 0x02U, 0x02U, 0x00U, 0x00U,
    0x20U, 0x27U, 0x37U, 0x16U, 0x1EU, 0x0EU, 0x7FU, 0x7FU,
    0x06U, 0x06U, 0x26U, 0x36U, 0x1EU, 0x02U, 0x00U, 0x00U
  },
  /* 题 */
  {
    0x80U, 0xBEU, 0xAAU, 0xAAU, 0xAAU, 0xBEU, 0x80U, 0xF2U,
    0x12U, 0x3EU, 0xD2U, 0x12U, 0xF2U, 0x02U, 0x00U, 0x00U,
    0x7CU, 0x0EU, 0x10U, 0x1FU, 0x24U, 0x24U, 0x20U, 0x33U,
    0x28U, 0x26U, 0x29U, 0x2CU, 0x3BU, 0x20U, 0x00U, 0x00U
  },
  /* 已 */
  {
    0x00U, 0xF2U, 0xF2U, 0x42U, 0x42U, 0x42U, 0x42U, 0x42U,
    0x42U, 0x42U, 0x42U, 0xFEU, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x1FU, 0x3FU, 0x20U, 0x20U, 0x20U, 0x20U, 0x20U,
    0x20U, 0x20U, 0x20U, 0x20U, 0x38U, 0x18U, 0x00U, 0x00U
  },
  /* 经 */
  {
    0x60U, 0x38U, 0xAEU, 0x62U, 0x30U, 0x10U, 0xC2U, 0x42U,
    0x22U, 0x32U, 0x3AU, 0x26U, 0x42U, 0xC0U, 0x00U, 0x00U,
    0x32U, 0x13U, 0x13U, 0x12U, 0x12U, 0x2AU, 0x21U, 0x21U,
    0x21U, 0x3FU, 0x21U, 0x21U, 0x21U, 0x20U, 0x00U, 0x00U
  },
};

static const uint8_t s_digit16[10][32] =
{
  {
    0x00U, 0x00U, 0x00U, 0x00U, 0xF8U, 0x0CU, 0x04U, 0x04U,
    0x0CU, 0xF0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x07U, 0x08U, 0x10U, 0x18U,
    0x0CU, 0x07U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  },
  {
    0x00U, 0x00U, 0x00U, 0x00U, 0x30U, 0x10U, 0xF8U, 0xFCU,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x1FU, 0x1FU,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  },
  {
    0x00U, 0x00U, 0x00U, 0x00U, 0x18U, 0x0CU, 0x04U, 0x84U,
    0xECU, 0x38U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x18U, 0x1CU, 0x1AU, 0x19U,
    0x18U, 0x18U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  },
  {
    0x00U, 0x00U, 0x00U, 0x00U, 0x18U, 0x0CU, 0x04U, 0xC4U,
    0xECU, 0x38U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x0CU, 0x08U, 0x10U, 0x10U,
    0x09U, 0x07U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  },
  {
    0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0xC0U, 0x30U, 0x18U,
    0xFCU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x02U, 0x03U, 0x02U, 0x02U, 0x02U,
    0x1FU, 0x02U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  },
  {
    0x00U, 0x00U, 0x00U, 0x00U, 0xF8U, 0x4CU, 0x64U, 0x44U,
    0xC4U, 0x84U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x04U, 0x0CU, 0x18U, 0x10U, 0x18U,
    0x0CU, 0x07U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  },
  {
    0x00U, 0x00U, 0x00U, 0x00U, 0xC0U, 0xE0U, 0x78U, 0x4CU,
    0x40U, 0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x0FU, 0x18U, 0x10U, 0x10U,
    0x18U, 0x0FU, 0x03U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  },
  {
    0x00U, 0x00U, 0x00U, 0x00U, 0x04U, 0x04U, 0x04U, 0xC4U,
    0x74U, 0x1CU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x18U, 0x1FU, 0x01U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  },
  {
    0x00U, 0x00U, 0x00U, 0x00U, 0xF8U, 0xC4U, 0xC4U, 0xC4U,
    0xECU, 0x38U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x02U, 0x0FU, 0x18U, 0x10U, 0x10U,
    0x18U, 0x0FU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  },
  {
    0x00U, 0x00U, 0x00U, 0x70U, 0xF8U, 0x84U, 0x04U, 0x84U,
    0xCCU, 0x78U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x11U, 0x1DU, 0x07U,
    0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  },
};

static const uint8_t s_dot16[OLED_RACE_TIME_DOT_WIDTH * 2U] =
{
  0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
  0x00U, 0x00U, 0x18U, 0x18U, 0x18U, 0x18U, 0x00U, 0x00U,
};
/* Compact 12x12 SimHei glyphs keep all five menu rows at their native ratio. */
static const uint16_t s_menu_chinese12[2][12] =
{
  /* 第 */
  {
    0x7BCU, 0x252U, 0x3FEU, 0x240U, 0x3FCU, 0x044U,
    0x7FCU, 0x460U, 0x458U, 0x346U, 0x000U, 0x000U
  },
  /* 题 */
  {
    0x7DEU, 0x11EU, 0x7D2U, 0x55EU, 0x540U, 0x57EU,
    0x15AU, 0x28AU, 0x24EU, 0x7F0U, 0x000U, 0x000U
  },
};

static const uint16_t s_menu_digit12[OLED_MENU_ITEM_COUNT][12] =
{
  {
    0x000U, 0x060U, 0x050U, 0x040U, 0x040U, 0x040U,
    0x040U, 0x040U, 0x040U, 0x000U, 0x000U, 0x000U
  },
  {
    0x000U, 0x0F0U, 0x080U, 0x080U, 0x080U, 0x040U,
    0x020U, 0x010U, 0x0F0U, 0x000U, 0x000U, 0x000U
  },
  {
    0x000U, 0x0F0U, 0x090U, 0x080U, 0x060U, 0x080U,
    0x080U, 0x090U, 0x060U, 0x000U, 0x000U, 0x000U
  },
  {
    0x000U, 0x060U, 0x060U, 0x050U, 0x048U, 0x048U,
    0x0F8U, 0x040U, 0x040U, 0x000U, 0x000U, 0x000U
  },
  {
    0x000U, 0x0F0U, 0x010U, 0x070U, 0x090U, 0x080U,
    0x080U, 0x080U, 0x070U, 0x000U, 0x000U, 0x000U
  },
};

static const uint8_t s_oled_init_cmds[] =
{
  0xAEU, 0x20U, 0x00U, 0xB0U, 0xC8U, 0x00U, 0x10U, 0x40U,
  0x81U, 0x7FU, 0xA1U, 0xA6U, 0xA8U, 0x3FU, 0xA4U, 0xD3U,
  0x00U, 0xD5U, 0x80U, 0xD9U, 0xF1U, 0xDAU, 0x12U, 0xDBU,
  0x40U, 0x8DU, 0x14U, 0xAFU
};

static bool OLED_WriteCommand(uint8_t cmd);
static bool OLED_WriteCommands(const uint8_t *cmds, uint16_t len);
static bool OLED_WriteData(const uint8_t *data, uint16_t len);
static bool OLED_UpdatePageRange(uint8_t first_page, uint8_t last_page);
static bool OLED_UpdateWindow(uint8_t first_page, uint8_t last_page,
                              uint8_t x, uint8_t width);
static bool OLED_SelectAddress(uint8_t address);
static bool OLED_TryInitialize(void);
static void OLED_MarkOffline(void);
static void OLED_ClearPageRange(uint8_t first_page, uint8_t last_page);
static void OLED_SetPixel(uint8_t x, uint8_t y);
static void OLED_InvertPixel(uint8_t x, uint8_t y);
static void OLED_DrawMenuGlyph12(uint8_t x, uint8_t y, const uint16_t *glyph);
static void OLED_DrawMenuMarker(uint8_t y);
static void OLED_DrawDigit(uint8_t x, uint8_t page, uint8_t digit);
static void OLED_DrawDot(uint8_t x, uint8_t page);
static uint8_t OLED_GetAsciiGlyph(char character);
static void OLED_FormatOdometerCount(char *text,
                                     char label,
                                     int32_t counts);
static void OLED_FormatOdometerProgress(char *text,
                                        uint32_t progress_tenths,
                                        uint8_t valid);

void OLED_Init(void)
{
  Board_OLED_I2C_Init();
  g_oled.present = 0U;
  g_oled.initialized = 0U;
  s_last_service_tick = 0U;
  OLED_Clear();

  delay_cycles(OLED_STARTUP_DELAY_CYCLES);
  (void)OLED_TryInitialize();
}

void OLED_Task(uint32_t tick)
{
  if ((uint32_t)(tick - s_last_service_tick) < OLED_SERVICE_INTERVAL_TICKS)
  {
    return;
  }
  s_last_service_tick = tick;

  if ((g_oled.present == 0U) || (g_oled.initialized == 0U))
  {
    (void)OLED_TryInitialize();
    return;
  }

  if (!OLED_WriteCommand(OLED_CMD_NOP))
  {
    g_oled.error_count++;
    OLED_MarkOffline();
  }
}

void OLED_Clear(void)
{
  uint16_t index = 0U;

  for (index = 0U; index < OLED_BUFFER_SIZE; index++)
  {
    s_buffer[index] = 0U;
  }
}

void OLED_Update(void)
{
  (void)OLED_UpdatePageRange(0U, (uint8_t)(OLED_PAGE_COUNT - 1U));
}
void OLED_DrawAscii(uint8_t x, uint8_t page, const char *text)
{
  uint8_t cursor = x;

  if ((text == (const char *)0) || (page >= 8U))
  {
    return;
  }

  while ((*text != '\0') && (cursor < (OLED_WIDTH - 5U)))
  {
    uint8_t glyph = OLED_GetAsciiGlyph(*text);
    uint8_t col = 0U;

    for (col = 0U; col < 5U; col++)
    {
      s_buffer[((uint16_t)page * OLED_WIDTH) + cursor + col] = s_font5x7[glyph][col];
    }

    if ((cursor + 5U) < OLED_WIDTH)
    {
      s_buffer[((uint16_t)page * OLED_WIDTH) + cursor + 5U] = 0U;
    }

    cursor = (uint8_t)(cursor + 6U);
    text++;
  }
}

void OLED_DrawChinese(uint8_t x, uint8_t page, uint8_t glyph_id)
{
  uint8_t col = 0U;

  if ((glyph_id >= (uint8_t)(sizeof(s_chinese16) / sizeof(s_chinese16[0]))) ||
      (page > 6U) ||
      (x > (OLED_WIDTH - 16U)))
  {
    return;
  }

  for (col = 0U; col < 16U; col++)
  {
    s_buffer[((uint16_t)page * OLED_WIDTH) + x + col] =
        s_chinese16[glyph_id][col];
    s_buffer[((uint16_t)(page + 1U) * OLED_WIDTH) + x + col] =
        s_chinese16[glyph_id][16U + col];
  }
}

void OLED_DrawProblemSelect(uint8_t problem)
{
  uint8_t item = 0U;

  OLED_Clear();

  for (item = 1U; item <= OLED_MENU_ITEM_COUNT; item++)
  {
    uint8_t y = (uint8_t)(1U + ((item - 1U) * 12U));

    OLED_DrawMenuGlyph12(44U, (uint8_t)(y + 1U), s_menu_chinese12[0]);
    OLED_DrawMenuGlyph12(58U, (uint8_t)(y + 1U), s_menu_digit12[item - 1U]);
    OLED_DrawMenuGlyph12(72U, (uint8_t)(y + 1U), s_menu_chinese12[1]);

    if (item == problem)
    {
      OLED_DrawMenuMarker(y);
    }
  }

  OLED_Update();
}

void OLED_DrawProblemSelected(uint8_t problem)
{
  OLED_Clear();
  OLED_DrawChinese(12U, 2U, OLED_GLYPH_YI);
  OLED_DrawChinese(30U, 2U, OLED_GLYPH_JING);
  OLED_DrawChinese(48U, 2U, OLED_GLYPH_XUAN);
  OLED_DrawChinese(66U, 2U, OLED_GLYPH_ZE);
  OLED_DrawChinese(36U, 5U, OLED_GLYPH_DI);
  OLED_DrawDigit(56U, 5U, problem);
  OLED_DrawChinese(76U, 5U, OLED_GLYPH_TI);
  OLED_Update();
}
void OLED_DrawRaceTime(uint32_t centiseconds)
{
  uint8_t seconds = 0U;
  uint8_t fraction = 0U;
  uint8_t x = OLED_RACE_TIME_X;

  if (centiseconds > OLED_RACE_TIME_MAX_CENTISECONDS)
  {
    centiseconds = OLED_RACE_TIME_MAX_CENTISECONDS;
  }

  seconds = (uint8_t)(centiseconds / 100U);
  fraction = (uint8_t)(centiseconds % 100U);

  OLED_ClearPageRange(OLED_RACE_TIME_PAGE, OLED_RACE_TIME_PAGE + 1U);
  OLED_DrawDigit(x, OLED_RACE_TIME_PAGE, (uint8_t)(seconds / 10U));
  x = (uint8_t)(x + 16U);
  OLED_DrawDigit(x, OLED_RACE_TIME_PAGE, (uint8_t)(seconds % 10U));
  x = (uint8_t)(x + 16U);
  OLED_DrawDot(x, OLED_RACE_TIME_PAGE);
  x = (uint8_t)(x + OLED_RACE_TIME_DOT_WIDTH);
  OLED_DrawDigit(x, OLED_RACE_TIME_PAGE, (uint8_t)(fraction / 10U));
  x = (uint8_t)(x + 16U);
  OLED_DrawDigit(x, OLED_RACE_TIME_PAGE, (uint8_t)(fraction % 10U));

  (void)OLED_UpdateWindow(OLED_RACE_TIME_PAGE,
                          OLED_RACE_TIME_PAGE + 1U,
                          OLED_RACE_TIME_X,
                          OLED_RACE_TIME_WIDTH);
}
void OLED_DrawOdometer(int32_t left_counts,
                       int32_t right_counts,
                       uint32_t lap_progress_tenths,
                       uint8_t lap_progress_valid)
{
  /* Formal competition mode displays only the race timer. */
  return;

  char left_text[12];
  char right_text[12];
  char progress_text[9];

  OLED_FormatOdometerCount(left_text, 'L', left_counts);
  OLED_FormatOdometerCount(right_text, 'R', right_counts);
  OLED_FormatOdometerProgress(progress_text,
                              lap_progress_tenths,
                              lap_progress_valid);

  OLED_ClearPageRange(OLED_ODOMETER_FIRST_PAGE, OLED_ODOMETER_LAST_PAGE);
  OLED_DrawAscii(0U, 0U, left_text);
  OLED_DrawAscii(0U, 1U, right_text);
  OLED_DrawAscii(0U, 2U, progress_text);
  (void)OLED_UpdateWindow(OLED_ODOMETER_FIRST_PAGE,
                          OLED_ODOMETER_LAST_PAGE,
                          0U,
                          OLED_ODOMETER_WIDTH);
}

static uint8_t OLED_GetAsciiGlyph(char character)
{
  if ((character >= '0') && (character <= '9'))
  {
    return (uint8_t)(character - '0');
  }

  switch (character)
  {
    case 'L':
      return OLED_FONT_L;
    case 'R':
      return OLED_FONT_R;
    case 'D':
      return OLED_FONT_D;
    case 'M':
      return OLED_FONT_M;
    case 'P':
      return OLED_FONT_P;
    case ':':
      return OLED_FONT_COLON;
    case '.':
      return OLED_FONT_DOT;
    case '%':
      return OLED_FONT_PERCENT;
    case '+':
      return OLED_FONT_PLUS;
    case '-':
      return OLED_FONT_MINUS;
    case ' ':
    default:
      return OLED_FONT_SPACE;
  }
}

static void OLED_FormatOdometerCount(char *text,
                                     char label,
                                     int32_t counts)
{
  uint64_t magnitude;
  uint8_t index;

  text[0] = label;
  text[1] = ':';
  text[2] = (counts < 0) ? '-' : '+';

  if (counts < 0)
  {
    magnitude = (uint64_t)(-(int64_t)counts);
  }
  else
  {
    magnitude = (uint64_t)counts;
  }

  if (magnitude > OLED_ODOMETER_COUNT_MAX)
  {
    magnitude = OLED_ODOMETER_COUNT_MAX;
  }

  for (index = 0U; index < OLED_ODOMETER_COUNT_DIGITS; index++)
  {
    uint8_t position = (uint8_t)(2U + OLED_ODOMETER_COUNT_DIGITS - index);

    text[position] = (char)('0' + (magnitude % 10U));
    magnitude /= 10U;
  }
  text[3U + OLED_ODOMETER_COUNT_DIGITS] = '\0';
}

static void OLED_FormatOdometerProgress(char *text,
                                        uint32_t progress_tenths,
                                        uint8_t valid)
{
  text[0] = 'P';
  text[1] = ':';
  text[5] = '.';
  text[7] = '%';
  text[8] = '\0';

  if (valid == 0U)
  {
    text[2] = '-';
    text[3] = '-';
    text[4] = '-';
    text[6] = '-';
    return;
  }

  if (progress_tenths > OLED_ODOMETER_PROGRESS_MAX)
  {
    progress_tenths = OLED_ODOMETER_PROGRESS_MAX;
  }

  text[2] = (char)('0' + ((progress_tenths / 1000U) % 10U));
  text[3] = (char)('0' + ((progress_tenths / 100U) % 10U));
  text[4] = (char)('0' + ((progress_tenths / 10U) % 10U));
  text[6] = (char)('0' + (progress_tenths % 10U));
}
static bool OLED_WriteCommand(uint8_t cmd)
{
  uint8_t data[2];

  data[0] = 0x00U;
  data[1] = cmd;

  return Board_OLED_I2C_Write(g_oled.address, data, (uint16_t)sizeof(data));
}

static bool OLED_WriteCommands(const uint8_t *cmds, uint16_t len)
{
  uint16_t index = 0U;

  if ((cmds == (const uint8_t *)0) || (len == 0U))
  {
    return false;
  }

  for (index = 0U; index < len; index++)
  {
    if (!OLED_WriteCommand(cmds[index]))
    {
      return false;
    }
  }

  return true;
}

static bool OLED_WriteData(const uint8_t *data, uint16_t len)
{
  uint8_t packet[8];
  uint16_t offset = 0U;

  if ((data == (const uint8_t *)0) || (len == 0U))
  {
    return false;
  }

  packet[0] = 0x40U;

  while (offset < len)
  {
    uint8_t chunk = (uint8_t)((len - offset) > 7U ? 7U : (len - offset));
    uint8_t index = 0U;

    for (index = 0U; index < chunk; index++)
    {
      packet[1U + index] = data[offset + index];
    }

    if (!Board_OLED_I2C_Write(
            g_oled.address, packet, (uint16_t)(chunk + 1U)))
    {
      return false;
    }

    offset = (uint16_t)(offset + chunk);
  }

  return true;
}
static bool OLED_UpdatePageRange(uint8_t first_page, uint8_t last_page)
{
  return OLED_UpdateWindow(first_page, last_page, 0U, OLED_WIDTH);
}

static bool OLED_UpdateWindow(uint8_t first_page, uint8_t last_page,
                              uint8_t x, uint8_t width)
{
  uint8_t page = 0U;

  if ((g_oled.present == 0U) ||
      (first_page > last_page) ||
      (last_page >= OLED_PAGE_COUNT) ||
      (width == 0U) ||
      ((uint16_t)x + width > OLED_WIDTH))
  {
    return false;
  }

  for (page = first_page; page <= last_page; page++)
  {
    uint8_t cmd[3];

    cmd[0] = (uint8_t)(0xB0U + page);
    cmd[1] = (uint8_t)(x & 0x0FU);
    cmd[2] = (uint8_t)(0x10U | (x >> 4U));

    if (!OLED_WriteCommands(cmd, (uint16_t)sizeof(cmd)) ||
        !OLED_WriteData(&s_buffer[((uint16_t)page * OLED_WIDTH) + x], width))
    {
      g_oled.error_count++;
      OLED_MarkOffline();
      return false;
    }
  }

  return true;
}
static bool OLED_SelectAddress(uint8_t address)
{
  g_oled.address = address;
  g_oled.present = 1U;

  if (OLED_WriteCommand(0xAEU))
  {
    return true;
  }

  g_oled.present = 0U;
  return false;
}

static bool OLED_TryInitialize(void)
{
  OLED_MarkOffline();

  if (!OLED_SelectAddress(OLED_ADDR_LOW) &&
      !OLED_SelectAddress(OLED_ADDR_HIGH))
  {
    g_oled.error_count++;
    return false;
  }

  if (!OLED_WriteCommands(s_oled_init_cmds, (uint16_t)sizeof(s_oled_init_cmds)))
  {
    g_oled.error_count++;
    OLED_MarkOffline();
    return false;
  }

  OLED_Update();
  if (g_oled.present == 0U)
  {
    return false;
  }

  g_oled.initialized = 1U;
  return true;
}

static void OLED_MarkOffline(void)
{
  g_oled.present = 0U;
  g_oled.initialized = 0U;
}
static void OLED_ClearPageRange(uint8_t first_page, uint8_t last_page)
{
  uint8_t page = 0U;

  if ((first_page > last_page) || (last_page >= OLED_PAGE_COUNT))
  {
    return;
  }

  for (page = first_page; page <= last_page; page++)
  {
    uint8_t x = 0U;

    for (x = 0U; x < OLED_WIDTH; x++)
    {
      s_buffer[((uint16_t)page * OLED_WIDTH) + x] = 0U;
    }
  }
}
static void OLED_SetPixel(uint8_t x, uint8_t y)
{
  if ((x < OLED_WIDTH) && (y < OLED_HEIGHT))
  {
    s_buffer[((uint16_t)(y / 8U) * OLED_WIDTH) + x] |=
        (uint8_t)(1U << (y & 0x07U));
  }
}

static void OLED_InvertPixel(uint8_t x, uint8_t y)
{
  if ((x < OLED_WIDTH) && (y < OLED_HEIGHT))
  {
    s_buffer[((uint16_t)(y / 8U) * OLED_WIDTH) + x] ^=
        (uint8_t)(1U << (y & 0x07U));
  }
}

static void OLED_DrawMenuGlyph12(uint8_t x, uint8_t y, const uint16_t *glyph)
{
  uint8_t row = 0U;

  for (row = 0U; row < 12U; row++)
  {
    uint8_t col = 0U;

    for (col = 0U; col < 12U; col++)
    {
      if ((glyph[row] & (uint16_t)(1U << col)) != 0U)
      {
        OLED_SetPixel((uint8_t)(x + col), (uint8_t)(y + row));
      }
    }
  }
}

static void OLED_DrawMenuMarker(uint8_t y)
{
  uint8_t row = 0U;
  uint8_t x = 0U;

  for (row = 0U; row < 7U; row++)
  {
    uint8_t width = (row <= 3U) ? (uint8_t)(row + 1U) : (uint8_t)(7U - row);
    uint8_t col = 0U;

    for (col = 0U; col < width; col++)
    {
      OLED_SetPixel((uint8_t)(33U + col), (uint8_t)(y + 2U + row));
    }
  }

  for (row = 0U; row < 12U; row++)
  {
    for (x = 41U; x <= 86U; x++)
    {
      OLED_InvertPixel(x, (uint8_t)(y + row));
    }
  }
}

static void OLED_DrawDigit(uint8_t x, uint8_t page, uint8_t digit)
{
  uint8_t col = 0U;

  if ((digit > 9U) || (page > 6U) || (x > (OLED_WIDTH - 16U)))
  {
    return;
  }

  for (col = 0U; col < 16U; col++)
  {
    s_buffer[((uint16_t)page * OLED_WIDTH) + x + col] = s_digit16[digit][col];
    s_buffer[((uint16_t)(page + 1U) * OLED_WIDTH) + x + col] =
        s_digit16[digit][16U + col];
  }
}
static void OLED_DrawDot(uint8_t x, uint8_t page)
{
  uint8_t col = 0U;

  if ((page > (OLED_PAGE_COUNT - 2U)) ||
      (x > (OLED_WIDTH - OLED_RACE_TIME_DOT_WIDTH)))
  {
    return;
  }

  for (col = 0U; col < OLED_RACE_TIME_DOT_WIDTH; col++)
  {
    s_buffer[((uint16_t)page * OLED_WIDTH) + x + col] = s_dot16[col];
    s_buffer[((uint16_t)(page + 1U) * OLED_WIDTH) + x + col] =
        s_dot16[OLED_RACE_TIME_DOT_WIDTH + col];
  }
}
