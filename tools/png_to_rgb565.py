#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PNG → RGB565 C 数组取模脚本
用法: python png_to_rgb565.py <图标.png> <数组名> <尺寸> [输出文件]
功能:
  - 自动裁剪透明留白 (getbbox)
  - 等比缩放后居中放入 <尺寸>x<尺寸> 方格
  - 透明像素填 0xF81F(品红) 作透明标志色
  - 结果同时打印到控制台并写入 <数组名>.txt
示例: python png_to_rgb565.py temp.png icon_temp 20
"""
import sys
from PIL import Image

TRANSPARENT = 0xF81F  # 透明标志色(品红)


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)

    path = sys.argv[1]
    name = sys.argv[2]
    target = int(sys.argv[3])
    out_file = sys.argv[4] if len(sys.argv) > 4 else (name + ".txt")

    im = Image.open(path).convert("RGBA")
    bbox = im.getbbox()
    if bbox:
        im = im.crop(bbox)
    im.thumbnail((target, target), Image.LANCZOS)
    canvas = Image.new("RGBA", (target, target), (0, 0, 0, 0))
    canvas.paste(im, ((target - im.width) // 2, (target - im.height) // 2), im)
    w, h = canvas.size

    lines = []
    lines.append(f"/* {name}: {w}x{h} RGB565 高字节在前 透明色=0x{TRANSPARENT:04X} */")
    lines.append(f"static const uint16_t {name}[{w * h}] = {{")
    vals = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = canvas.getpixel((x, y))
            vals.append(TRANSPARENT if a < 128 else rgb565(r, g, b))
    for i in range(0, len(vals), 8):
        lines.append("    " + ", ".join(f"0x{v:04X}" for v in vals[i:i + 8]) + ",")
    lines.append("};")

    text = "\n".join(lines)
    with open(out_file, "w", encoding="utf-8") as f:
        f.write(text + "\n")
    print(text)
    print(f"\n[已写入 {out_file}]")


if __name__ == "__main__":
    main()
