/* source/fonts/sgl_font_HFSharp_2_ttf_9_bpp4.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: docs directory
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sgl_core.h>
#include <sgl_font.h>

static const uint8_t font_bitmap[] = {
    /* U+0020 */
    /* U+0025 */
    0x2a, 0xb1, 0x01, 0x6a, 0xc5, 0x81, 0x07, 0x88,
    0x00, 0x01, 0x97, 0x70, 0x28, 0x89, 0xd3, 0x10,
    0x3a, 0xc1,

    /* U+0030 */
    0x08, 0x9c, 0x20, 0x4f, 0x0a, 0xc0, 0x8f, 0x08,
    0xf0, 0x7f, 0x08, 0xf0, 0x4f, 0x09, 0xb0, 0x09,
    0x8b, 0x20,

    /* U+0031 */
    0x00, 0x70, 0x09, 0xf0, 0x05, 0xf0, 0x05, 0xf0,
    0x05, 0xf0, 0x07, 0xf2,

    /* U+0041 */
    0x00, 0x29, 0x00, 0x00, 0x0a, 0xf4, 0x00, 0x01,
    0xca, 0xa0, 0x00, 0x7b, 0x9f, 0x00, 0x0d, 0x20,
    0xf5, 0x05, 0xe0, 0x0d, 0xc0,

    /* U+0044 */
    0x6f, 0x79, 0xa0, 0x4f, 0x20, 0xca, 0x4f, 0x20,
    0x9e, 0x4f, 0x20, 0x9d, 0x4f, 0x20, 0xd8, 0x6f,
    0x79, 0xa0,

    /* U+0045 */
    0x6f, 0x7b, 0x94, 0xf2, 0x04, 0x4f, 0x23, 0x14,
    0xf7, 0xe2, 0x4f, 0x20, 0x26, 0xf7, 0x99,

    /* U+004D */
    0x6f, 0x60, 0x0b, 0xf0, 0x4f, 0xc0, 0x2f, 0xe0,
    0x4e, 0xf3, 0x8e, 0xe0, 0x4b, 0xda, 0xe9, 0xe0,
    0x4b, 0x6f, 0x98, 0xe0, 0x6d, 0x1f, 0x3a, 0xf0,

    /* U+0052 */
    0x6f, 0x8e, 0x40, 0x4f, 0x2c, 0xa0, 0x4f, 0x3e,
    0x40, 0x4f, 0xf8, 0x00, 0x4f, 0x7f, 0x30, 0x6f,
    0x49, 0xe2,

    /* U+0053 */
    0x1b, 0x7c, 0x36, 0xe0, 0x11, 0x2e, 0xb1, 0x00,
    0x1b, 0xf2, 0x40, 0x0f, 0x66, 0xc8, 0xb0,

    /* U+0054 */
    0xa9, 0xec, 0xb8, 0x20, 0xc9, 0x02, 0x00, 0xc9,
    0x00, 0x00, 0xc9, 0x00, 0x00, 0xc9, 0x00, 0x00,
    0xeb, 0x00,

    /* U+0059 */
    0x4f, 0x60, 0xd6, 0x0a, 0xd3, 0xc0, 0x01, 0xfe,
    0x30, 0x00, 0x9d, 0x00, 0x00, 0x9d, 0x00, 0x00,
    0xbe, 0x00
};


static const sgl_font_table_t font_table[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 43, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 94, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 18, .adv_w = 87, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 36, .adv_w = 61, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 104, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 69, .adv_w = 103, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 87, .adv_w = 83, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 102, .adv_w = 122, .box_w = 8, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 96, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 144, .adv_w = 78, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 93, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 177, .adv_w = 99, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0}
};

static const uint16_t unicode_list_0[] = {
    0x0, 0x5, 0x10, 0x11, 0x21, 0x24, 0x25, 0x2d,
    0x32, 0x33, 0x34, 0x39
};

static const sgl_font_unicode_t font_unicode[] = {
    { .offset = 0x20, .len = 12, .list = unicode_list_0, .tab_offset = 1, }
};

const sgl_font_t sgl_font_HFSharp_2_ttf_9_bpp4 = {
    .bitmap = font_bitmap,
    .table = font_table,
    .font_table_size = SGL_ARRAY_SIZE(font_table),
    .font_height = 6,
    .base_line = 0,
    .bpp = 4,
    .compress = 0,
    .unicode = font_unicode,
    .unicode_num = SGL_ARRAY_SIZE(font_unicode),
};
