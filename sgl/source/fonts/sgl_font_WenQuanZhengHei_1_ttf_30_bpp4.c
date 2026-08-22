/* source/fonts/sgl_font_WenQuanZhengHei_1_ttf_30_bpp4.c
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
    /* U+002B */
    0x00, 0x00, 0x00, 0x0d, 0xd0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0xcf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb,
    0xcf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00,

    /* U+002D */
    0xee, 0xee, 0xee, 0x5f, 0xff, 0xff, 0xf6
};


static const sgl_font_table_t font_table[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 288, .box_w = 16, .box_h = 16, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 135, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 128, .adv_w = 144, .box_w = 7, .box_h = 2, .ofs_x = 0, .ofs_y = 7}
};

static const sgl_font_unicode_t font_unicode[] = {
    { .offset = 0x2b, .len = 3, .list = NULL, .tab_offset = 1, }
};

const sgl_font_t sgl_font_WenQuanZhengHei_1_ttf_30_bpp4 = {
    .bitmap = font_bitmap,
    .table = font_table,
    .font_table_size = SGL_ARRAY_SIZE(font_table),
    .font_height = 16,
    .base_line = -1,
    .bpp = 4,
    .compress = 0,
    .unicode = font_unicode,
    .unicode_num = SGL_ARRAY_SIZE(font_unicode),
};
