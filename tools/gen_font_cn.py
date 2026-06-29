#!/usr/bin/env python3
"""
中文字库生成器 — 为焊台 LCD 生成 16×16 点阵字模

用法：
    python gen_font_cn.py

输出：
    hal/font_cn.h — 包含字模位图 + Unicode 查找表

自定义：
    修改 CHARS 字符串添加/删除字符，重新运行即可。
    需要 PIL/Pillow 库：pip install Pillow
"""

from PIL import Image, ImageDraw, ImageFont
import os

# ============================================================
# 需要生成字模的字符列表（在此添加/删除字符）
# ============================================================
CHARS = """
初始化中手柄功率电压状态设置菜单行为预温度声音系统开机偏移待间急停蜂鸣达
编码器反向休眠延时屏幕旋转位单显示曲线使调保存并重启不退出恢复默认返短按
确认消长选择待机未知急停消焊台加热过冲故障开关休眠运行确认取消修改退出手柄
类型烙铁头拔出检测摄氏度华氏度百分比瓦特是否是
"""

# 字体设置
FONT_PATH = "C:/Windows/Fonts/simsun.ttc"
FONT_SIZE = 14  # 渲染字号
GRID_SIZE = 16  # 输出点阵大小

# 输出文件
OUTPUT = os.path.join(os.path.dirname(__file__), "..", "hal", "font_cn.h")
# 如果脚本不在项目目录，改为绝对路径
if not os.path.exists(os.path.dirname(OUTPUT)):
    OUTPUT = "E:/esp/JBC245_Code/JBC245/MDK/hal/font_cn.h"


def main():
    # 收集唯一字符并排序
    chars = sorted(set(c for c in CHARS if ord(c) > 127))
    print(f"字符数: {len(chars)}")
    print(f"字符: {''.join(chars)}")

    # 加载字体
    font_path = FONT_PATH
    if not os.path.exists(font_path):
        alt_fonts = [
            "C:/Windows/Fonts/simhei.ttf",
            "C:/Windows/Fonts/msyh.ttc",
            "C:/Windows/Fonts/Deng.ttf",
        ]
        for fp in alt_fonts:
            if os.path.exists(fp):
                font_path = fp
                break
    print(f"字体: {font_path}")

    font = ImageFont.truetype(font_path, FONT_SIZE)

    # 生成位图
    def make_bitmap(char):
        img = Image.new("1", (GRID_SIZE, GRID_SIZE), 0)
        draw = ImageDraw.Draw(img)
        bbox = draw.textbbox((0, 0), char, font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        x = (GRID_SIZE - tw) // 2 - bbox[0]
        y = (GRID_SIZE - th) // 2 - bbox[1]
        draw.text((x, y), char, font=font, fill=1)

        bitmap = []
        for row in range(GRID_SIZE):
            # 左半字节
            b = 0
            for col in range(8):
                if img.getpixel((col, row)):
                    b |= 0x80 >> col
            bitmap.append(b)
            # 右半字节
            b = 0
            for col in range(8, 16):
                if img.getpixel((col, row)):
                    b |= 0x80 >> (col - 8)
            bitmap.append(b)
        return bitmap

    # 测试输出第一个字符
    test_char = chars[0]
    bm = make_bitmap(test_char)
    print(f"\n'{test_char}' 字模预览:")
    for row in range(GRID_SIZE):
        line = ""
        for col in range(GRID_SIZE):
            byte_idx = row * 2 + col // 8
            bit_pos = 7 - (col % 8)
            line += "##" if bm[byte_idx] & (1 << bit_pos) else "  "
        print(line)

    # ---- 生成 C 头文件 ----
    lines = []
    lines.append("/**")
    lines.append(" * @file    font_cn.h")
    lines.append(f" * @brief   中文字库 16×16 点阵 ({len(chars)} 字符)")
    lines.append(" *")
    lines.append(" * 编码: UTF-8 → Unicode → 字模查找表(二分查找)")
    lines.append(f" * 字模格式: {GRID_SIZE}×{GRID_SIZE} 逐行扫描, 每行 2 字节, 共 32 字节/字符")
    lines.append(f" * 字体来源: {os.path.basename(font_path)} {FONT_SIZE}pt")
    lines.append(" * 自动生成, 请勿手动编辑")
    lines.append(" */")
    lines.append("")
    lines.append("#ifndef FONT_CN_H")
    lines.append("#define FONT_CN_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define FONT_CN_COUNT {len(chars)}")
    lines.append("")

    # Unicode → 索引映射表 (按 Unicode 排序，支持二分查找)
    pairs = [(ord(c), i) for i, c in enumerate(chars)]
    pairs.sort()
    lines.append("/* Unicode 码点 → 字模索引 (按码点升序) */")
    lines.append("typedef struct {")
    lines.append("    uint16_t unicode;")
    lines.append("    uint16_t index;")
    lines.append("} FontCN_MapEntry;")
    lines.append("")
    lines.append(f"static const FontCN_MapEntry s_font_cn_map[{len(chars)}] = {{")
    for uni, idx in pairs:
        lines.append(f"    {{0x{uni:04X}, {idx:3d}}},  /* U+{uni:04X} {chr(uni)} */")
    lines.append("};")
    lines.append("")

    # 字模位图数据
    lines.append(f"/* {GRID_SIZE}×{GRID_SIZE} 点阵字模 (每字符 {GRID_SIZE * 2} bytes) */")
    lines.append(f"static const uint8_t s_font_cn_bitmap[{len(chars)}][{GRID_SIZE * 2}] = {{")
    for i, c in enumerate(chars):
        bm = make_bitmap(c)
        hex_str = ", ".join(f"0x{b:02X}" for b in bm)
        lines.append(f"    {{ {hex_str} }},  /* {i:3d}: {c} U+{ord(c):04X} */")
    lines.append("};")
    lines.append("")

    # 二分查找函数
    lines.append("/* 根据 Unicode 码点查找字模索引 (二分查找), 未找到返回 -1 */")
    lines.append("static int font_cn_lookup(uint16_t unicode) {")
    lines.append(f"    int lo = 0, hi = {len(chars)} - 1;")
    lines.append("    while (lo <= hi) {")
    lines.append("        int mid = (lo + hi) / 2;")
    lines.append("        uint16_t u = s_font_cn_map[mid].unicode;")
    lines.append("        if (u == unicode) return (int)s_font_cn_map[mid].index;")
    lines.append("        if (u < unicode) lo = mid + 1; else hi = mid - 1;")
    lines.append("    }")
    lines.append("    return -1;")
    lines.append("}")
    lines.append("")
    lines.append("#endif /* FONT_CN_H */")

    # 写入文件
    with open(OUTPUT, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    print(f"\n已生成: {OUTPUT}")
    print(f"字库大小: {len(chars)} 字符 × 32 bytes = {len(chars) * 32} bytes")


if __name__ == "__main__":
    main()
