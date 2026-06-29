#ifndef LCD_FONT_H_
#define LCD_FONT_H_

#include "stdint.h"
#include "stdbool.h"
 
// 单个字形（Glyph）的数据结构
typedef struct {
    uint32_t bitmapOffset; // 该字形点阵数据在 GFXfont->bitmap 数组中的起始索引偏移量
    uint8_t  width;        // 字形点阵的宽度（像素）
    uint8_t  height;       // 字形点阵的高度（像素）
    uint8_t  xAdvance;     // 光标在绘制完该字符后，沿 X 轴前进的距离（即字符占用的水平空间）
    int8_t   xOffset;      // 从当前光标位置到字形点阵左上角（UL corner）的水平偏移量（可为负值，用于处理如 'j', 'g' 等超出基线的部分）
    int8_t   yOffset;      // 从当前光标位置到字形点阵左上角（UL corner）的垂直偏移量（通常为负值，因为屏幕坐标系 Y 轴向下，而字形通常向上绘制）
} GFXglyph;

// 整个字体库的数据结构
typedef struct {
    uint8_t  *bitmap;      // 所有字形点阵数据的连续存储数组（通过 GFXglyph 中的 bitmapOffset 索引访问具体字形）
    GFXglyph *glyph;       // 字形属性数组指针，每个元素对应一个字符的字形描述信息
    uint16_t  first;       // 字体支持的第一个字符的 ASCII 码（或 Unicode 编码起始值）
    uint16_t  last;        // 字体支持的最后一个字符的 ASCII 码（或 Unicode 编码结束值）
    uint8_t   yAdvance;    // 行间距，即换行时Cursor在 Y 轴方向前进的距离（通常等于字体高度 + 行间隔）
} GFXfont;


bool get_pixel_from_glyph(const GFXglyph *g, const uint8_t *bitmap, uint8_t x, uint8_t y);


// 整个中文字库结构体
typedef struct {
    const char * charset; // 汉字字形数组
    const uint8_t *data; 
    uint8_t width;          // 汉字点阵宽度（如 16）
    uint8_t height;         // 汉字点阵高度（如 16）
    uint8_t size;     // 单个汉字占用的字节数（如 16*16/8 = 32 字节）
} CHSFont;

extern const CHSFont FontChinese24;
extern const GFXfont DefaultFont;
extern const GFXfont FreeSans6pt7b;
extern const GFXfont FreeSansBold30pt7b;

//#include "Fonts/GFXFF/FreeSansBold12pt7b.h"
#endif

