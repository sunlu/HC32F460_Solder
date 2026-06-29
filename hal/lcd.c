#include "lcd.h"
#include "lcd_font.h"
#include <string.h>

// --- TFT液晶 SPI (ST7789) ---
#define TFT_CS_PORT GPIO_PORT_A
#define TFT_CS_PIN GPIO_PIN_15
#define TFT_SCK_PORT GPIO_PORT_B
#define TFT_SCK_PIN GPIO_PIN_03
#define TFT_MOSI_PORT GPIO_PORT_B
#define TFT_MOSI_PIN GPIO_PIN_05
#define TFT_DC_PORT GPIO_PORT_B
#define TFT_DC_PIN GPIO_PIN_06
#define TFT_RST_PORT GPIO_PORT_B
#define TFT_RST_PIN GPIO_PIN_07
#define TFT_BLK_PORT GPIO_PORT_B
#define TFT_BLK_PIN GPIO_PIN_09

/* ST7789 commands */
#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT 0x11
#define ST7789_NORON 0x13
#define ST7789_INVON 0x21
#define ST7789_DISPON 0x29
#define ST7789_CASET 0x2A
#define ST7789_RASET 0x2B
#define ST7789_RAMWR 0x2C
#define ST7789_MADCTL 0x36
#define ST7789_COLMOD 0x3A

#define TFT_SPI_UNIT CM_SPI3

/* LCD control pin macros */
#define TFT_CS_L() GPIO_ResetPins(TFT_CS_PORT, TFT_CS_PIN)
#define TFT_CS_H() GPIO_SetPins(TFT_CS_PORT, TFT_CS_PIN)

#define TFT_DC_L() GPIO_ResetPins(TFT_DC_PORT, TFT_DC_PIN)
#define TFT_DC_H() GPIO_SetPins(TFT_DC_PORT, TFT_DC_PIN)

#define TFT_RST_L() GPIO_ResetPins(TFT_RST_PORT, TFT_RST_PIN)
#define TFT_RST_H() GPIO_SetPins(TFT_RST_PORT, TFT_RST_PIN)

#define TFT_BLK_L() GPIO_ResetPins(TFT_BLK_PORT, TFT_BLK_PIN)
#define TFT_BLK_H() GPIO_SetPins(TFT_BLK_PORT, TFT_BLK_PIN)

/* Forward declarations */
static void lcd_write_cmd(uint8_t cmd);
static void spi_write_data8(uint8_t data);
static void lcd_write_data8(uint8_t data);
static void lcd_write_data16(uint16_t data);

static void lcd_write_cmd(uint8_t cmd) {
  TFT_CS_H(); /* CS high first - match STM32 working pattern */
  TFT_DC_L(); /* DC low = command */
  TFT_CS_L(); /* CS low = chip select active */
  spi_write_data8(cmd);
  TFT_CS_H(); /* CS high = deselect */
}

static void spi_write_data8(uint8_t data) {
  uint32_t u32Timeout;

  /* Wait for TX buffer empty before writing */
  u32Timeout = 0xFFFFU;
  while ((SPI_GetStatus(TFT_SPI_UNIT, SPI_FLAG_TX_BUF_EMPTY) == RESET) &&
         (u32Timeout > 0U)) {
    u32Timeout--;
  }

  /* Write data to TX buffer */
  SPI_WriteData(TFT_SPI_UNIT, (uint32_t)data);

  /* Wait for TX buffer empty after write (data loaded into shift register).
   * At 50MHz SPI clock, the shift register drains in ~160ns per byte,
   * which is negligible compared to the next TX_BUF_EMPTY poll. */
  u32Timeout = 0xFFFFU;
  while ((SPI_GetStatus(TFT_SPI_UNIT, SPI_FLAG_TX_BUF_EMPTY) == RESET) &&
         (u32Timeout > 0U)) {
    u32Timeout--;
  }
}

static void spi_write_date16(uint16_t data){
   spi_write_data8((uint8_t)(data >> 8));   /* MSB first */
  spi_write_data8((uint8_t)(data & 0xFF)); /* LSB second */
}

static void lcd_write_data8(uint8_t data) {
  TFT_DC_H(); /* DC high = data */
  TFT_CS_L(); /* CS low = chip select active */
  spi_write_data8(data);
  TFT_CS_H(); /* CS high = deselect */
}

static void lcd_write_data16(uint16_t data) {
  TFT_DC_H();                             /* DC high = data */
  TFT_CS_L();                             /* CS low = chip select active */
  spi_write_date16(data); 
  TFT_CS_H();                             /* CS high = deselect */
}

/* DMA uses color value directly (fixed source address) */

/* ========================================================================
 * SPI DMA fill (STM32 pattern: 16-bit SPI + 16-bit DMA + fixed source)
 *
 * For solid color fills, DMA sends the same 16-bit value repeatedly
 * without incrementing the source address. One DMA transfer fills the
 * entire rectangle at hardware speed with SPI flow control.
 * ======================================================================== */

#define LCD_DMA_UNIT CM_DMA1
#define LCD_DMA_CH DMA_CH2
#define LCD_DMA_TRIG AOS_DMA1_2
#define LCD_DMA_FLAG_TC DMA_FLAG_TC_CH2

#if defined(__ICCARM__)
#pragma data_alignment = 4
static uint16_t s_dma_color_buf[2];
#elif defined(__CC_ARM)
__align(4) static uint16_t s_dma_color_buf[2];
#else
static uint16_t s_dma_color_buf[2] __attribute__((aligned(4)));
#endif

static void lcd_dma_init(void) {
  stc_dma_init_t stcDma;
  FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_DMA1 | FCG0_PERIPH_AOS, ENABLE);

  DMA_StructInit(&stcDma);
  stcDma.u32DataWidth = DMA_DATAWIDTH_16BIT;
  stcDma.u32BlockSize = 1U;
  stcDma.u32TransCount = 1U;
  stcDma.u32SrcAddr = (uint32_t)&s_dma_color_buf[0];
  stcDma.u32DestAddr =
      (uint32_t)(&(CM_SPI3->DR));            // 指向正确的SPI数据寄存器基地址
  stcDma.u32SrcAddrInc = DMA_SRC_ADDR_FIX;   // 固定源地�（重复发送同��颜色�
  stcDma.u32DestAddrInc = DMA_DEST_ADDR_FIX; // 固定�的地�

  (void)DMA_Init(LCD_DMA_UNIT, LCD_DMA_CH, &stcDma);

  AOS_SetTriggerEventSrc(LCD_DMA_TRIG, EVT_SRC_SPI3_SPTI);

  DMA_Cmd(LCD_DMA_UNIT, ENABLE);
}

/** * @brief DMA 刷屏/充优化函
 * @param color: RGB565 颜色
 * @param count: 像素总数
 */
static void lcd_dma_fill(uint16_t color, uint32_t count) {
  if (count == 0)
    return;

  s_dma_color_buf[0] = color;

  uint32_t remain = count;

  while (remain > 0) {
    uint16_t transfer_size = (remain > 60000U) ? 60000U : (uint16_t)remain;
    remain -= transfer_size;

    DMA_ClearTransCompleteStatus(LCD_DMA_UNIT, LCD_DMA_FLAG_TC);

    // 华大推荐的动态安全重载寄存器方法
    DMA_SetSrcAddr(LCD_DMA_UNIT, LCD_DMA_CH, (uint32_t)&s_dma_color_buf[0]);
    DMA_SetBlockSize(LCD_DMA_UNIT, LCD_DMA_CH, 1U);
    DMA_SetTransCount(LCD_DMA_UNIT, LCD_DMA_CH, transfer_size);

    DMA_ChCmd(LCD_DMA_UNIT, LCD_DMA_CH, ENABLE);

    while (DMA_GetTransCompleteStatus(LCD_DMA_UNIT, LCD_DMA_FLAG_TC) == RESET)
      ;

    // 关闭通道以准备
    DMA_ChCmd(LCD_DMA_UNIT, LCD_DMA_CH, DISABLE);
  }

  DMA_ClearTransCompleteStatus(LCD_DMA_UNIT, LCD_DMA_FLAG_TC);
  while (SPI_GetStatus(TFT_SPI_UNIT, SPI_FLAG_TX_BUF_EMPTY) == RESET)
    ;
  while (SPI_GetStatus(TFT_SPI_UNIT, SPI_FLAG_IDLE) == RESET)
    ;
}

/* ========================================================================
 * lcd_init() - ST7789 display initialization
 *
 * Single clean init sequence:
 *   1. GPIO + SPI3 peripheral setup
 *   2. Hardware reset
 *   3. Software reset
 *   4. Sleep out -> color format -> orientation -> inversion -> display on
 *   5. Set window for 240x320 panel with 20px Y offset
 * ======================================================================== */
void lcd_init(void) {
  /* --- Clock gate for SPI3 --- */
  FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_SPI3, ENABLE);

  /* --- 0. Release JTAG debug pins for GPIO use --- */
  /* PB3 = JTAG_TDO, PA15 = JTAG_TDI. Keep SWCLK/SWDIO for debugging. */
  // GPIO_SetDebugPort(GPIO_PIN_TDO, DISABLE);  /* Release PB3 from JTAG TDO */
  // GPIO_SetDebugPort(GPIO_PIN_TDI, DISABLE);  /* Release PA15 from JTAG TDI */

  GPIO_SetDebugPort(GPIO_PIN_TDO, DISABLE);
  GPIO_SetDebugPort(GPIO_PIN_TDI, DISABLE);
  GPIO_SetDebugPort(GPIO_PIN_SWO, DISABLE);

  /* --- 1. GPIO for control pins: CS, DC, RST, BLK --- */
  stc_gpio_init_t stcGpioInit;
  (void)GPIO_StructInit(&stcGpioInit);
  stcGpioInit.u16PinDir = PIN_DIR_OUT;
  stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
  stcGpioInit.u16PinDrv = PIN_HIGH_DRV;

  stcGpioInit.u16PinState = PIN_STAT_SET; /* default high */
  (void)GPIO_Init(TFT_CS_PORT, TFT_CS_PIN, &stcGpioInit);
  (void)GPIO_Init(TFT_DC_PORT, TFT_DC_PIN, &stcGpioInit);
  (void)GPIO_Init(TFT_RST_PORT, TFT_RST_PIN, &stcGpioInit);
  (void)GPIO_Init(TFT_BLK_PORT, TFT_BLK_PIN, &stcGpioInit);

  /* --- 2. GPIO for SPI pins: SCK, MOSI --- */
  stcGpioInit.u16PinState = PIN_STAT_RST; /* default low */
  (void)GPIO_Init(TFT_SCK_PORT, TFT_SCK_PIN, &stcGpioInit);
  (void)GPIO_Init(TFT_MOSI_PORT, TFT_MOSI_PIN, &stcGpioInit);

  /* Remap GPIO to SPI3 alternate functions */
  GPIO_SetFunc(TFT_SCK_PORT, TFT_SCK_PIN, GPIO_FUNC_43); /* PB3 -> SPI3_SCK */
  GPIO_SetFunc(TFT_MOSI_PORT, TFT_MOSI_PIN,
               GPIO_FUNC_40); /* PB5 -> SPI3_MOSI */

  /* --- 3. SPI3 peripheral configuration --- */
  stc_spi_init_t stcSpiInit;
  SPI_StructInit(&stcSpiInit);
  stcSpiInit.u32WireMode = SPI_3_WIRE;
  stcSpiInit.u32TransMode = SPI_SEND_ONLY;
  stcSpiInit.u32MasterSlave = SPI_MASTER;
  stcSpiInit.u32SpiMode = SPI_MD_0; /* CPOL=0, CPHA=0 (Mode 0) */
  stcSpiInit.u32BaudRatePrescaler = SPI_BR_CLK_DIV2;
  stcSpiInit.u32DataBits = SPI_DATA_SIZE_8BIT;
  stcSpiInit.u32FirstBit = SPI_FIRST_MSB;
  stcSpiInit.u32SuspendMode = SPI_COM_SUSP_FUNC_OFF;
  stcSpiInit.u32FrameLevel = SPI_1_FRAME;

  SPI_Init(TFT_SPI_UNIT, &stcSpiInit);

  /* Enable SPI3 */
  SPI_Cmd(TFT_SPI_UNIT, ENABLE);

  // lcd_dma_init();

  /* direct rendering */

  /* --- 4. ST7789 init sequence (matching STM32 working driver) --- */

  /* 4a. Hardware + Software reset */
  TFT_RST_L();
  DDL_DelayMS(50);
  TFT_RST_H();
  lcd_write_cmd(ST7789_SWRESET);
  DDL_DelayMS(150); /* 150ms - matching STM32 */

  /* 4c. Sleep out */
  lcd_write_cmd(ST7789_SLPOUT);
  DDL_DelayMS(50); /* 50ms - matching STM32 */

  /* 4d. Color mode: 16-bit/pixel (RGB565) */
  lcd_write_cmd(ST7789_COLMOD);
  lcd_write_data8(0x55);
  DDL_DelayMS(10); /* 10ms - matching STM32 */

  /* 4e. Memory data access control: rotation based on TFT_ROTATE
   *     MADCTL takes exactly 1 byte:
   *       D7=MY(DirY), D6=MX(DirX), D5=MV(X/Y swap), D4=ML, D3=RGB, D2=MH,
   * D1-0=0 Landscape (MV=1): 0x20=origin top-left, 0x60=origin top-right
   *                       0xA0=origin bottom-left, 0xE0=origin bottom-right */
  lcd_write_cmd(ST7789_MADCTL);
#if (TFT_ROTATE == 0)
  lcd_write_data8(0x00); /* Portrait 240x320, origin top-left */
#elif (TFT_ROTATE == 1)
  lcd_write_data8(0xE0); /* Landscape 320x240, origin top-left (MV=1) */
#elif (TFT_ROTATE == 2)
  lcd_write_data8(0x60); /* Portrait inverted 180deg */
#else
  lcd_write_data8(0xA0); /* Landscape 320x240, origin bottom-left (MV+MY) */
#endif

  /* 4f. Display inversion on */
  lcd_write_cmd(ST7789_INVON);
  DDL_DelayMS(10); /* 10ms - matching STM32 */

  /* 4g. Normal display mode on */
  lcd_write_cmd(ST7789_NORON);
  DDL_DelayMS(10); /* 10ms - matching STM32 */

  /* 4h. Set full display window (GRAM is 240x320) */
  lcd_set_window(0, 0, 239, 319);

  /* 4i. Display on */
  lcd_write_cmd(ST7789_DISPON);
  DDL_DelayMS(10);

  /* 4j. Fill screen RED as diagnostic test */
  lcd_fill_screen(COLOR_BLACK);

  /* 4b. Backlight ON */
  TFT_BLK_H();

  /* direct rendering */
}

void lcd_set_brightness(uint8_t u8Pct) {
  if (u8Pct > 0) {
    TFT_BLK_H();
  } else {
    TFT_BLK_L();
  }
}

void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  /* CASET = column address (X), RASET = row address (Y).
   * MADCTL handles rotation � we always write GRAM-native order. */
  lcd_write_cmd(ST7789_CASET);
  lcd_write_data16(x1);
  lcd_write_data16(x2);
  lcd_write_cmd(ST7789_RASET);
  lcd_write_data16(y1);
  lcd_write_data16(y2);
  lcd_write_cmd(ST7789_RAMWR);
}

void lcd_write_pixel(uint16_t x, uint16_t y, uint16_t color) {
  lcd_set_window(x, y, x, y);
  lcd_write_data16(color);
}
 

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                   uint16_t color) {
                      
  uint8_t c1 = (uint8_t)(color >> 8);
  uint8_t c2 = (uint8_t)(color & 0xFF);

  lcd_set_window(x, y, x + w - 1, y + h - 1);
                     
  TFT_DC_H();
  TFT_CS_L();
                     
  for (uint32_t i = 0; i < w * h; i++) { 
      spi_write_data8(c1); /* MSB first */
      spi_write_data8(c2); /* LSB second */ 
  }
  TFT_CS_H();
}

void lcd_fill_screen(uint16_t color) {
  lcd_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
  lcd_fill_rect(x, y, w, 1, color);
}
void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                   uint16_t color) {
  lcd_draw_hline(x, y, w, color);
  lcd_draw_hline(x, y + h - 1, w, color);
  lcd_fill_rect(x, y, 1, h, color);
  lcd_fill_rect(x + w - 1, y, 1, h, color);
}
/**
 * @brief  绘制单个汉字（适配 CHSFont 紧凑索引字库，带屏幕边界安全裁剪）
 * @param  x, y      : 汉字左上角的屏幕物理坐标
 * @param  utf8_code : 传入的当前汉字 3 字节 UTF-8 编码数组
 * @param  color     : 字体颜色 (RGB565)
 * @param  bgColor   : 背景颜色 (RGB565)
 * @param  font      : 新中文字体结构体指针
 */
void lcd_draw_ch_char(uint16_t x, uint16_t y, const uint8_t *utf8_code, uint16_t color, uint16_t bgColor, const CHSFont *font) {
    if (utf8_code == NULL || font == NULL || font->charset == NULL || font->data == NULL) return;

    int32_t glyph_index = -1;
    const char *ptr = font->charset;
    uint32_t current_index = 0;

    // 1. 在 charset 字符串中检索该 UTF-8 汉字
    while (*ptr != '\0') {
        // 如果连续 3 个字节完全匹配，说明找到了该汉字
        if ((uint8_t)ptr[0] == utf8_code[0] &&
            (uint8_t)ptr[1] == utf8_code[1] &&
            (uint8_t)ptr[2] == utf8_code[2]) {
            glyph_index = current_index;
            break;
        }
        ptr += 3;          // UTF-8 汉字跳过 3 字节
        current_index++;   // 汉字字形索引递增
    }

    // 如果没找到，直接填充一个空白背景块，防止留空
    if (glyph_index == -1) {
        lcd_fill_rect(x, y, font->width, font->height, bgColor);
        return;
    }

    // 2. 根据字形索引，计算该字在 font->data 字节流中的绝对起点指针
    // 寻址公式：起点 = data首地址 + (字形索引 * 单个字占用的字节数)
    const uint8_t *data_ptr = font->data + (glyph_index * font->size);

    // 3. 屏幕安全边界裁剪
    int32_t start_x = (int32_t)x;
    int32_t start_y = (int32_t)y;
    int32_t end_x   = start_x + font->width - 1;
    int32_t end_y   = start_y + font->height - 1;

    // 完全在屏幕外则放弃绘制
    if (end_x < 0 || end_y < 0 || start_x >= TFT_WIDTH || start_y >= TFT_HEIGHT) {
        return; 
    }

    uint16_t win_x1 = (start_x < 0) ? 0 : (uint16_t)start_x;
    uint16_t win_y1 = (start_y < 0) ? 0 : (uint16_t)start_y;
    uint16_t win_x2 = (end_x >= TFT_WIDTH)  ? (TFT_WIDTH - 1)  : (uint16_t)end_x;
    uint16_t win_y2 = (end_y >= TFT_HEIGHT) ? (TFT_HEIGHT - 1) : (uint16_t)end_y;

    uint16_t skip_left = (start_x < 0) ? (uint16_t)(-start_x) : 0;
    uint16_t skip_top  = (start_y < 0) ? (uint16_t)(-start_y) : 0;

    // 4. 正式向屏幕液晶屏开窗
    lcd_set_window(win_x1, win_y1, win_x2, win_y2);

    TFT_DC_H();
    TFT_CS_L();

    uint8_t current_byte = 0;
    uint8_t bit_count = 0;
    
    uint8_t color_h = (uint8_t)(color >> 8);
    uint8_t color_l = (uint8_t)(color & 0xFF);
    uint8_t bg_h    = (uint8_t)(bgColor >> 8);
    uint8_t bg_l    = (uint8_t)(bgColor & 0xFF);

    // 5. 连续位流式解包与发送
    for (uint16_t j = 0; j < font->height; j++) {
        for (uint16_t i = 0; i < font->width; i++) {
            if (bit_count == 0) {
                current_byte = *data_ptr++;
                bit_count = 8;
            }
            bool is_pixel_on = (current_byte & 0x80) != 0;
            current_byte <<= 1;
            bit_count--;

            // 裁剪区间内部的像素才真正输出给硬件 SPI
            if (i >= skip_left && i < (font->width - ((end_x >= TFT_WIDTH) ? (end_x - (TFT_WIDTH - 1)) : 0)) &&
                j >= skip_top  && j < (font->height - ((end_y >= TFT_HEIGHT) ? (end_y - (TFT_HEIGHT - 1)) : 0))) 
            {
                if (is_pixel_on) {
                    spi_write_data8(color_h);
                    spi_write_data8(color_l);
                } else {
                    spi_write_data8(bg_h);
                    spi_write_data8(bg_l);
                }
            }
        }
    }
    TFT_CS_H();
}

/**
 * @brief  在指定坐标绘制中英文混合字符串
 * @param  font : 传入你在之前实现的英文 GFXfont，汉字库直接内部硬编码绑定或按需扩展
 */
void lcd_draw_chinese(uint16_t x, uint16_t y, const char  *text, uint16_t color, uint16_t bgColor,const CHSFont *chFont,const GFXfont *font) {
    if (text == NULL || font == NULL) return;

    uint16_t cur_x = x;
    uint16_t cur_y = y;
    uint8_t utf8_buf[3];
  
    // 获取硬编码的 16x16 汉字库指 

    while (*text != '\0') {
        // 1. 处理换行
        if (*text == '\n') {
            cur_x = x;
            cur_y += font->yAdvance; // 或者 ch_font->height
            text++;
            continue;
        }

        // 2. 判断是否为汉字 (UTF-8 汉字特征：第一个字节高3位为1，即 & 0xE0 == 0xE0)
        if ((*text & 0xE0) == 0xE0) {
            // 安全截取 3 字节的 UTF-8 编码
            
            utf8_buf[0] = text[0];
            utf8_buf[1] = text[1];
            utf8_buf[2] = text[2];

            // 自动换行检查
            if (cur_x + chFont->width > TFT_WIDTH) {
                cur_x = x;
                cur_y += chFont->height;
            }

            // 为了和英文字体高度视觉对齐，这里汉字的 Y 轴可以加上一定的负偏移修正
            // 因为英文 y 是基线，而汉字 y 我们设计的是左上角。
            // 假设英文 FreeSans12 字体基线上方高度约 12 像素：
            uint16_t target_y = cur_y - 12; 

            lcd_draw_ch_char(cur_x, target_y, utf8_buf, color, bgColor, chFont);
            
            cur_x += chFont->width; // 光标右移一个汉字的宽度
            text += 3;               // 字符串指针跳过 3 个字节
        } 
        // 3. 否则作为普通英文 ASCII 字符处理
        else {
            if (*text >= font->first && *text <= font->last) {
                uint8_t advance = font->glyph[*text - font->first].xAdvance;
                if (cur_x + advance > TFT_WIDTH) {
                    cur_x = x;
                    cur_y += font->yAdvance;
                }
            }
            // 外部原本实现的英文单字流式打印 (利用之前补全的代码)
            uint8_t advance = lcd_draw_char(cur_x, cur_y, *text, color, bgColor, font);
            cur_x += advance;
            text += 1;               // 字符串指针跳过 1 个字节
        }
    }
}
  

/**
 * @brief  绘制单个 Adafruit GFX 字符（流式高性能版）
 * @param  x, y   : 字符的基线起始点（Baseline）坐标
 * @param  c      : 待绘制的 ASCII 字符
 * @param  color  : 字体颜色 (RGB565)
 * @param  bgColor: 背景颜色 (RGB565)
 * @param  font   : 字体库结构体指针
 * @return uint8_t: 返回该字符的水平步进距离（xAdvance），用于光标累加
 */
/**
 * @brief  绘制单个 Adafruit GFX 字符（加入安全边界裁剪、前色/背景色流式填充）
 */
uint8_t lcd_draw_char(uint16_t x, uint16_t y, const char c, uint16_t color, uint16_t bgColor, const GFXfont *font) {
    if (c < font->first || c > font->last) {
        return 0; 
    }

    const GFXglyph *g = &(font->glyph[c - font->first]);
    const uint8_t *data_ptr = &(font->bitmap[g->bitmapOffset]);

    // 用有符号数计算，防止下溢（负数）
    int32_t start_x = (int32_t)x + g->xOffset;
    int32_t start_y = (int32_t)y + g->yOffset;
    int32_t end_x   = start_x + g->width - 1;
    int32_t end_y   = start_y + g->height - 1;

    // 1. 完全在屏幕外的情况，直接完全放弃绘制，保护安全
    if (end_x < 0 || end_y < 0 || start_x >= TFT_WIDTH || start_y >= TFT_HEIGHT) {
        return g->xAdvance; // 虽然不画，但光标还是要正常前进
    }

    // 2. 边界裁剪：计算真正要在屏幕上开窗的区域
    uint16_t win_x1 = (start_x < 0) ? 0 : (uint16_t)start_x;
    uint16_t win_y1 = (start_y < 0) ? 0 : (uint16_t)start_y;
    uint16_t win_x2 = (end_x >= TFT_WIDTH)  ? (TFT_WIDTH - 1)  : (uint16_t)end_x;
    uint16_t win_y2 = (end_y >= TFT_HEIGHT) ? (TFT_HEIGHT - 1) : (uint16_t)end_y;

    // 3. 计算由于裁剪导致的偏移量（点阵里需要跳过不画的行列）
    uint16_t skip_left = (start_x < 0) ? (uint16_t)(-start_x) : 0;
    uint16_t skip_top  = (start_y < 0) ? (uint16_t)(-start_y) : 0;

    // 4. 正式开窗
    lcd_set_window(win_x1, win_y1, win_x2, win_y2);

    TFT_DC_H();
    TFT_CS_L();

    uint8_t current_byte = 0;
    uint8_t bit_count = 0;
    
    uint8_t color_h = (uint8_t)(color >> 8);
    uint8_t color_l = (uint8_t)(color & 0xFF);
    uint8_t bg_h    = (uint8_t)(bgColor >> 8);
    uint8_t bg_l    = (uint8_t)(bgColor & 0xFF);

    // 5. 遍历整个字形点阵
    for (uint16_t j = 0; j < g->height; j++) {
        for (uint16_t i = 0; i < g->width; i++) {
            // 依然需要保持位流同步，每个像素都要读出来
            if (bit_count == 0) {
                current_byte = *data_ptr++;
                bit_count = 8;
            }
            bool is_pixel_on = (current_byte & 0x80) != 0;
            current_byte <<= 1;
            bit_count--;

            // 【关键裁剪逻辑】只有当当前像素在屏幕有效开窗范围内时，才发送给 SPI
            if (i >= skip_left && i < (g->width - ((end_x >= TFT_WIDTH) ? (end_x - (TFT_WIDTH - 1)) : 0)) &&
                j >= skip_top  && j < (g->height - ((end_y >= TFT_HEIGHT) ? (end_y - (TFT_HEIGHT - 1)) : 0))) 
            {
                if (is_pixel_on) {
                    spi_write_data8(color_h);
                    spi_write_data8(color_l);
                } else {
                    spi_write_data8(bg_h);
                    spi_write_data8(bg_l);
                }
            }
        }
    }

    TFT_CS_H();
    return g->xAdvance;
}
/**
 * @brief  在指定坐标绘制字符串
 * @param  x, y   : 文本基线（Baseline）的起始坐标
 * @param  text   : 字符串首地址
 * @param  color  : 字体颜色
 * @param  bgColor: 背景颜色
 * @param  font   : 字体库指针
 */
void lcd_draw_string(uint16_t x, uint16_t y, const char* text, uint16_t color, uint16_t bgColor,const GFXfont *font) {
    if (text == NULL || font == NULL) return;

    uint16_t cur_x = x;
    uint16_t cur_y = y;

    while (*text != '\0') {
        if (*text == '\n') {
            cur_x = x;
            cur_y += font->yAdvance;
        } 
        else {
            // 预检：如果加上当前字符已经超过屏幕右边缘，强制自动换行
            uint16_t char_idx = *text - font->first;
            if (*text >= font->first && *text <= font->last) {
                uint8_t next_advance = font->glyph[char_idx].xAdvance;
                
                // 自动换行机制
                if (cur_x + next_advance >= TFT_WIDTH) {
                    cur_x = x;               // 换行回到最左侧起始点
                    cur_y += font->yAdvance; // Y向下移动一整行
                }
            }

            // 如果向下换行后已经超过屏幕底边缘，直接结束整串打印，防止无用功
            if (cur_y + font->yAdvance > TFT_HEIGHT + 20) { // 稍微留宽一点允许部分剪裁
                break;
            }

            uint8_t advance = lcd_draw_char(cur_x, cur_y, *text, color, bgColor, font);
            cur_x += advance;
        }
        text++;
    }
}

/**
 * @brief  在指定 Y 轴高度水平居中显示字符串
 * @param  y      : 文本基线（Baseline）的 Y 坐标
 * @param  text   : 字符串首地址
 * @param  color  : 字体颜色
 * @param  bgColor: 背景颜色
 * @param  font   : 字体库指针
 */
void lcd_draw_string_center(uint16_t y, const char *text, uint16_t color, uint16_t bgColor,const GFXfont *font) {
    if (text == NULL || font == NULL) return;

    uint32_t total_width = 0;
    const char *p = text;

    // 1. 预先计算整个字符串的总像素宽度
    while (*p != '\0') {
        if (*p >= font->first && *p <= font->last) {
            total_width += font->glyph[*p - font->first].xAdvance;
        }
        p++;
    }

    // 2. 根据屏幕总宽（TFT_WIDTH）计算居中的起始 X 坐标
    int32_t start_x = ((int32_t)TFT_WIDTH - (int32_t)total_width) / 2;
    if (start_x < 0) {
        start_x = 0; // 防止超出左边界
    }

    // 3. 调用原生的字符串绘制函数
    lcd_draw_string((uint16_t)start_x, y, text, color, bgColor, font);
}

//void LCD_ShowPicture_DMA(uint16_t x, uint16_t y, uint16_t length,
//                         uint16_t width, const uint8_t *pic) {
//  uint32_t size = length * width * 2;
//  LCD_Address_Set(x, y, x + length - 1, y + width - 1);
//  LCD_CS_0();

//  // 设置 DMA 参数
//  DMA_SetSrcAddr(DMA_UNIT, DMA_TX_CH, (uint32_t)pic);
//  DMA_SetTransCount(DMA_UNIT, DMA_TX_CH, size);
//  DMA_ChCmd(DMA_UNIT, DMA_TX_CH, ENABLE);

//  // 等待 DMA 完成
//  while (RESET == DMA_GetTransCompleteStatus(DMA_UNIT, DMA_FLAG_TC_CH0)) {
//  }
//  DMA_ClearTransCompleteStatus(DMA_UNIT, DMA_FLAG_TC_CH0);

//  DMA_ChCmd(DMA_UNIT, DMA_TX_CH, DISABLE);
//  LCD_CS_1();
//}
