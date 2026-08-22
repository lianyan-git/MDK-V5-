/* source/include/sgl_font.h
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: https://sgl-docs.readthedocs.io
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

#ifndef __SGL_FONT_H__
#define __SGL_FONT_H__

#include <sgl_cfgfix.h>
#include <stddef.h>
#include <sgl_list.h>
#include <sgl_types.h>
#include <sgl_core.h>

/* declare all font */

#if CONFIG_SGL_FONT_SONG23
extern const sgl_font_t song23;
#endif

#if CONFIG_SGL_FONT_CONSOLAS14
extern const sgl_font_t consolas14;
#endif

#if CONFIG_SGL_FONT_CONSOLAS23
extern const sgl_font_t consolas23;
#endif

#if CONFIG_SGL_FONT_CONSOLAS24
extern const sgl_font_t consolas24;
#endif

#if CONFIG_SGL_FONT_CONSOLAS32
extern const sgl_font_t consolas32;
#endif

#if CONFIG_SGL_FONT_KAI33
extern const sgl_font_t kai33;
#endif

#if CONFIG_SGL_FONT_CONSOLAS24_COMPRESS
extern const sgl_font_t consolas24_compress;
#endif

#endif // !__SGL_FONT_H__
