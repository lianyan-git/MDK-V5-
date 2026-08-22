/* ============================================
 * SGL UI Designer - Auto Generated Code
 * Project: 压力传感器界面
 * Screen: 240x135
 * Color Depth: 16bit
 * Generated: 22/8/2026 下午10:41:10
 * ============================================ */

#include "sgl.h"

/* USER CODE BEGIN includes */
/* 用户可在此处添加额外的 #include、全局变量声明等，重新生成代码时本区域内容会被保留 */
/* USER CODE END includes */

/* ============================================
 * 字体字模声明（字模 C 文件由设计器自动生成到 fonts/ 子目录）
 * ============================================ */
extern const sgl_font_t sgl_font_HFSharp_2_ttf_34_bpp4;
extern const sgl_font_t sgl_font_HFSharp_2_ttf_9_bpp4;
extern const sgl_font_t sgl_font_HFSharp_2_ttf_11_bpp4;
extern const sgl_font_t sgl_font_WenQuanZhengHei_1_ttf_16_bpp4;
extern const sgl_font_t sgl_font_WenQuanZhengHei_1_ttf_28_bpp4;
extern const sgl_font_t sgl_font_WenQuanZhengHei_1_ttf_12_bpp4;
extern const sgl_font_t sgl_font_WenQuanZhengHei_1_ttf_30_bpp4;
extern const sgl_font_t sgl_font_WenQuanZhengHei_1_ttf_18_bpp4;
extern const sgl_font_t sgl_font_WenQuanZhengHei_1_ttf_10_bpp4;
extern const sgl_font_t sgl_font_WenQuanZhengHei_1_ttf_15_bpp4;
extern const sgl_font_t sgl_font_WenQuanZhengHei_1_ttf_13_bpp4;

/* === Page: 初始化页面 === */
void ui_page_page1_create(void)
{
    sgl_obj_t *page_page1 = sgl_screen_act();
    sgl_page_set_color(page_page1, sgl_rgb(30, 30, 46));

    /* 柱状条 */
    sgl_obj_t *bar1 = sgl_bar_create(page_page1);
    sgl_obj_set_pos(bar1, 30, 95);
    sgl_obj_set_size(bar1, 180, 10);
    sgl_bar_set_value(bar1, 20);
    sgl_bar_set_direct(bar1, 0);
    sgl_bar_set_fill_color(bar1, sgl_rgb(255, 159, 28));
    sgl_bar_set_track_color(bar1, sgl_rgb(49, 50, 68));
    sgl_bar_set_border_color(bar1, sgl_rgb(235, 235, 235));
    sgl_bar_set_border_width(bar1, 0);
    sgl_bar_set_radius(bar1, 4);
    sgl_bar_set_alpha(bar1, 255);

    /* 标签 */
    sgl_obj_t *label1 = sgl_label_create(page_page1);
    sgl_obj_set_pos(label1, 0, 18);
    sgl_obj_set_size(label1, 240, 47);
    sgl_label_set_font(label1, &sgl_font_HFSharp_2_ttf_34_bpp4);
    sgl_label_set_text(label1, "QIMINGXING");
    sgl_label_set_text_color(label1, sgl_rgb(255, 255, 255));
    sgl_label_set_text_align(label1, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label1, 0);
    sgl_label_set_text_offset(label1, 0);
    sgl_label_set_alpha(label1, 255);

    /* 标签 */
    sgl_obj_t *label2 = sgl_label_create(page_page1);
    sgl_obj_set_pos(label2, 72, 109);
    sgl_obj_set_size(label2, 96, 10);
    sgl_label_set_font(label2, &sgl_font_HFSharp_2_ttf_9_bpp4);
    sgl_label_set_text(label2, "SYSTEM READY 100%");
    sgl_label_set_text_color(label2, sgl_rgb(245, 166, 35));
    sgl_label_set_text_align(label2, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label2, 0);
    sgl_label_set_text_offset(label2, 3);
    sgl_label_set_alpha(label2, 255);

    /* 标签 */
    sgl_obj_t *label3 = sgl_label_create(page_page1);
    sgl_obj_set_pos(label3, 34, 60);
    sgl_obj_set_size(label3, 172, 24);
    sgl_label_set_font(label3, &sgl_font_HFSharp_2_ttf_11_bpp4);
    sgl_label_set_text(label3, "Drying Control System");
    sgl_label_set_text_color(label3, sgl_rgb(127, 132, 156));
    sgl_label_set_text_align(label3, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label3, 0);
    sgl_label_set_text_offset(label3, 0);
    sgl_label_set_alpha(label3, 255);

}

/* === Page: 主界面 === */
void ui_page_page2_create(void)
{
    sgl_obj_t *page_page2 = sgl_screen_act();
    sgl_page_set_color(page_page2, sgl_rgb(30, 30, 46));

    /* 矩形 */
    sgl_obj_t *rect1 = sgl_rect_create(page_page2);
    sgl_obj_set_pos(rect1, 2, 2);
    sgl_obj_set_size(rect1, 117, 42);
    sgl_rect_set_color(rect1, sgl_rgb(37, 37, 56));
    sgl_rect_set_border_color(rect1, sgl_rgb(49, 50, 68));
    sgl_rect_set_border_width(rect1, 1);
    sgl_rect_set_radius(rect1, 5);
    sgl_rect_set_alpha(rect1, 255);

    /* 矩形 */
    sgl_obj_t *rect2 = sgl_rect_create(page_page2);
    sgl_obj_set_pos(rect2, 121, 2);
    sgl_obj_set_size(rect2, 117, 42);
    sgl_rect_set_color(rect2, sgl_rgb(37, 37, 56));
    sgl_rect_set_border_color(rect2, sgl_rgb(49, 50, 68));
    sgl_rect_set_border_width(rect2, 1);
    sgl_rect_set_radius(rect2, 5);
    sgl_rect_set_alpha(rect2, 255);

    /* 矩形 */
    sgl_obj_t *rect3 = sgl_rect_create(page_page2);
    sgl_obj_set_pos(rect3, 2, 46);
    sgl_obj_set_size(rect3, 117, 42);
    sgl_rect_set_color(rect3, sgl_rgb(37, 37, 56));
    sgl_rect_set_border_color(rect3, sgl_rgb(49, 50, 68));
    sgl_rect_set_border_width(rect3, 1);
    sgl_rect_set_radius(rect3, 5);
    sgl_rect_set_alpha(rect3, 255);

    /* 矩形 */
    sgl_obj_t *rect4 = sgl_rect_create(page_page2);
    sgl_obj_set_pos(rect4, 121, 46);
    sgl_obj_set_size(rect4, 117, 42);
    sgl_rect_set_color(rect4, sgl_rgb(37, 37, 56));
    sgl_rect_set_border_color(rect4, sgl_rgb(49, 50, 68));
    sgl_rect_set_border_width(rect4, 1);
    sgl_rect_set_radius(rect4, 5);
    sgl_rect_set_alpha(rect4, 255);

    /* 矩形 */
    sgl_obj_t *rect5 = sgl_rect_create(page_page2);
    sgl_obj_set_pos(rect5, 2, 90);
    sgl_obj_set_size(rect5, 236, 43);
    sgl_rect_set_color(rect5, sgl_rgb(37, 37, 56));
    sgl_rect_set_border_color(rect5, sgl_rgb(49, 50, 68));
    sgl_rect_set_border_width(rect5, 1);
    sgl_rect_set_radius(rect5, 5);
    sgl_rect_set_alpha(rect5, 255);

    /* 基础图片 */
    sgl_obj_t *img1 = sgl_img_create(rect1);
    sgl_obj_set_pos(img1, 4, 6);
    sgl_obj_set_size(img1, 30, 30);
    sgl_img_set_alpha(img1, 255);

    /* 基础图片 */
    sgl_obj_t *img2 = sgl_img_create(rect2);
    sgl_obj_set_pos(img2, 4, 6);
    sgl_obj_set_size(img2, 30, 30);
    sgl_img_set_alpha(img2, 255);

    /* 基础图片 */
    sgl_obj_t *img3 = sgl_img_create(rect3);
    sgl_obj_set_pos(img3, 4, 6);
    sgl_obj_set_size(img3, 30, 30);
    sgl_img_set_alpha(img3, 255);

    /* 基础图片 */
    sgl_obj_t *img4 = sgl_img_create(rect4);
    sgl_obj_set_pos(img4, 4, 6);
    sgl_obj_set_size(img4, 30, 30);
    sgl_img_set_alpha(img4, 255);

    /* 基础图片 */
    sgl_obj_t *img5 = sgl_img_create(rect5);
    sgl_obj_set_pos(img5, 4, 6);
    sgl_obj_set_size(img5, 30, 30);
    sgl_img_set_alpha(img5, 255);

    /* 标签 */
    sgl_obj_t *label4 = sgl_label_create(rect1);
    sgl_obj_set_pos(label4, 41, 9);
    sgl_obj_set_size(label4, 72, 24);
    sgl_label_set_font(label4, &sgl_font_WenQuanZhengHei_1_ttf_16_bpp4);
    sgl_label_set_text(label4, "25 ℃");
    sgl_label_set_text_color(label4, sgl_rgb(245, 166, 35));
    sgl_label_set_text_align(label4, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label4, 0);
    sgl_label_set_text_offset(label4, 0);
    sgl_label_set_alpha(label4, 255);

    /* 标签 */
    sgl_obj_t *label8 = sgl_label_create(rect5);
    sgl_obj_set_pos(label8, 41, 6);
    sgl_obj_set_size(label8, 147, 30);
    sgl_label_set_font(label8, &sgl_font_WenQuanZhengHei_1_ttf_28_bpp4);
    sgl_label_set_text(label8, "02:00:00");
    sgl_label_set_text_color(label8, sgl_rgb(255, 255, 255));
    sgl_label_set_text_align(label8, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label8, 0);
    sgl_label_set_text_offset(label8, 0);
    sgl_label_set_alpha(label8, 255);

    /* 标签 */
    sgl_obj_t *label11 = sgl_label_create(rect2);
    sgl_obj_set_pos(label11, 40, 9);
    sgl_obj_set_size(label11, 72, 24);
    sgl_label_set_font(label11, &sgl_font_WenQuanZhengHei_1_ttf_16_bpp4);
    sgl_label_set_text(label11, "50 %");
    sgl_label_set_text_color(label11, sgl_rgb(78, 205, 196));
    sgl_label_set_text_align(label11, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label11, 0);
    sgl_label_set_text_offset(label11, 0);
    sgl_label_set_alpha(label11, 255);

    /* 标签 */
    sgl_obj_t *label12 = sgl_label_create(rect3);
    sgl_obj_set_pos(label12, 40, 9);
    sgl_obj_set_size(label12, 72, 24);
    sgl_label_set_font(label12, &sgl_font_WenQuanZhengHei_1_ttf_16_bpp4);
    sgl_label_set_text(label12, "0 G");
    sgl_label_set_text_color(label12, sgl_rgb(167, 139, 250));
    sgl_label_set_text_align(label12, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label12, 0);
    sgl_label_set_text_offset(label12, 0);
    sgl_label_set_alpha(label12, 255);

    /* 标签 */
    sgl_obj_t *label13 = sgl_label_create(rect4);
    sgl_obj_set_pos(label13, 40, 9);
    sgl_obj_set_size(label13, 72, 24);
    sgl_label_set_font(label13, &sgl_font_WenQuanZhengHei_1_ttf_16_bpp4);
    sgl_label_set_text(label13, "25 ℃");
    sgl_label_set_text_color(label13, sgl_rgb(255, 68, 68));
    sgl_label_set_text_align(label13, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label13, 0);
    sgl_label_set_text_offset(label13, 0);
    sgl_label_set_alpha(label13, 255);

    /* LED指示灯 */
    sgl_obj_t *led1 = sgl_led_create(rect5);
    sgl_obj_set_pos(led1, 195, 11);
    sgl_obj_set_size(led1, 20, 20);
    sgl_led_set_status(led1, false);
    sgl_led_set_on_color(led1, sgl_rgb(255, 0, 0));
    sgl_led_set_off_color(led1, sgl_rgb(0, 0, 0));
    sgl_led_set_bg_color(led1, sgl_rgb(103, 107, 142));
    sgl_led_set_radius(led1, 10);
    sgl_led_set_alpha(led1, 255);

}

/* === Page: 页面 3 === */
void ui_page_page3_create(void)
{
    sgl_obj_t *page_page3 = sgl_screen_act();
    sgl_page_set_color(page_page3, sgl_rgb(30, 30, 46));

    /* 标签 */
    sgl_obj_t *label14 = sgl_label_create(page_page3);
    sgl_obj_set_pos(label14, 60, 0);
    sgl_obj_set_size(label14, 120, 20);
    sgl_label_set_font(label14, &sgl_font_WenQuanZhengHei_1_ttf_12_bpp4);
    sgl_label_set_text(label14, "烘干温度设置");
    sgl_label_set_text_color(label14, sgl_rgb(245, 166, 35));
    sgl_label_set_text_align(label14, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label14, 0);
    sgl_label_set_text_offset(label14, 0);
    sgl_label_set_alpha(label14, 255);

    /* 直线 */
    sgl_obj_t *line2 = sgl_line_create(page_page3);
    sgl_obj_set_pos(line2, 0, 20);
    sgl_obj_set_size(line2, 239, 0);
    sgl_line_set_color(line2, sgl_rgb(49, 50, 68));
    sgl_line_set_width(line2, 1);
    sgl_line_set_dashed(line2, 0);
    sgl_line_set_pos(line2, 0, 20, 239, 20);
    sgl_line_set_alpha(line2, 255);

    /* 按钮 */
    sgl_obj_t *button7 = sgl_button_create(page_page3);
    sgl_obj_set_pos(button7, 0, 115);
    sgl_obj_set_size(button7, 240, 20);
    sgl_button_set_font(button7, &sgl_font_WenQuanZhengHei_1_ttf_12_bpp4);
    sgl_button_set_text(button7, "退出");
    sgl_button_set_color(button7, sgl_rgb(49, 50, 68));
    sgl_button_set_text_color(button7, sgl_rgb(245, 166, 35));
    sgl_button_set_border_color(button7, sgl_rgb(49, 50, 68));
    sgl_button_set_border_width(button7, 0);
    sgl_button_set_radius(button7, 0);
    sgl_button_set_text_align(button7, SGL_ALIGN_CENTER);
    sgl_button_set_alpha(button7, 255);

    /* 按钮 */
    sgl_obj_t *button8 = sgl_button_create(page_page3);
    sgl_obj_set_pos(button8, 159, 63);
    sgl_obj_set_size(button8, 30, 30);
    sgl_button_set_font(button8, &sgl_font_WenQuanZhengHei_1_ttf_30_bpp4);
    sgl_button_set_text(button8, "+");
    sgl_button_set_color(button8, sgl_rgb(100, 100, 109));
    sgl_button_set_text_color(button8, sgl_rgb(245, 166, 35));
    sgl_button_set_border_color(button8, sgl_rgb(49, 50, 68));
    sgl_button_set_border_width(button8, 1);
    sgl_button_set_radius(button8, 4);
    sgl_button_set_text_align(button8, SGL_ALIGN_CENTER);
    sgl_button_set_alpha(button8, 255);

    /* 标签 */
    sgl_obj_t *label22 = sgl_label_create(page_page3);
    sgl_obj_set_pos(label22, 98, 66);
    sgl_obj_set_size(label22, 40, 24);
    sgl_label_set_font(label22, &sgl_font_WenQuanZhengHei_1_ttf_18_bpp4);
    sgl_label_set_text(label22, "70℃");
    sgl_label_set_text_color(label22, sgl_rgb(245, 166, 35));
    sgl_label_set_text_align(label22, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label22, 0);
    sgl_label_set_text_offset(label22, 0);
    sgl_label_set_alpha(label22, 255);

    /* 按钮 */
    sgl_obj_t *button9 = sgl_button_create(page_page3);
    sgl_obj_set_pos(button9, 49, 63);
    sgl_obj_set_size(button9, 30, 30);
    sgl_button_set_font(button9, &sgl_font_WenQuanZhengHei_1_ttf_30_bpp4);
    sgl_button_set_text(button9, "-");
    sgl_button_set_color(button9, sgl_rgb(100, 100, 109));
    sgl_button_set_text_color(button9, sgl_rgb(245, 166, 35));
    sgl_button_set_border_color(button9, sgl_rgb(49, 50, 68));
    sgl_button_set_border_width(button9, 1);
    sgl_button_set_radius(button9, 4);
    sgl_button_set_text_align(button9, SGL_ALIGN_CENTER);
    sgl_button_set_alpha(button9, 255);

    /* 标签 */
    sgl_obj_t *label23 = sgl_label_create(page_page3);
    sgl_obj_set_pos(label23, 79, 33);
    sgl_obj_set_size(label23, 43, 24);
    sgl_label_set_font(label23, &sgl_font_WenQuanZhengHei_1_ttf_10_bpp4);
    sgl_label_set_text(label23, "当前温度");
    sgl_label_set_text_color(label23, sgl_rgb(96, 97, 113));
    sgl_label_set_text_align(label23, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label23, 0);
    sgl_label_set_text_offset(label23, 0);
    sgl_label_set_alpha(label23, 255);

    /* 标签 */
    sgl_obj_t *label24 = sgl_label_create(page_page3);
    sgl_obj_set_pos(label24, 125, 33);
    sgl_obj_set_size(label24, 39, 24);
    sgl_label_set_font(label24, &sgl_font_WenQuanZhengHei_1_ttf_15_bpp4);
    sgl_label_set_text(label24, "60℃");
    sgl_label_set_text_color(label24, sgl_rgb(245, 166, 35));
    sgl_label_set_text_align(label24, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label24, 0);
    sgl_label_set_text_offset(label24, 0);
    sgl_label_set_alpha(label24, 255);

}

/* === Page: 页面 4 === */
void ui_page_page4_create(void)
{
    sgl_obj_t *page_page4 = sgl_screen_act();
    sgl_page_set_color(page_page4, sgl_rgb(30, 30, 46));

    /* 标签 */
    sgl_obj_t *label18 = sgl_label_create(page_page4);
    sgl_obj_set_pos(label18, 60, 0);
    sgl_obj_set_size(label18, 120, 20);
    sgl_label_set_font(label18, &sgl_font_WenQuanZhengHei_1_ttf_13_bpp4);
    sgl_label_set_text(label18, "加热器设置");
    sgl_label_set_text_color(label18, sgl_rgb(255, 68, 68));
    sgl_label_set_text_align(label18, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label18, 0);
    sgl_label_set_text_offset(label18, 0);
    sgl_label_set_alpha(label18, 255);

    /* 直线 */
    sgl_obj_t *line3 = sgl_line_create(page_page4);
    sgl_obj_set_pos(line3, 0, 20);
    sgl_obj_set_size(line3, 239, 0);
    sgl_line_set_color(line3, sgl_rgb(49, 50, 68));
    sgl_line_set_width(line3, 1);
    sgl_line_set_dashed(line3, 0);
    sgl_line_set_pos(line3, 0, 20, 239, 20);
    sgl_line_set_alpha(line3, 255);

    /* 按钮 */
    sgl_obj_t *button4 = sgl_button_create(page_page4);
    sgl_obj_set_pos(button4, 49, 63);
    sgl_obj_set_size(button4, 30, 30);
    sgl_button_set_font(button4, &sgl_font_WenQuanZhengHei_1_ttf_30_bpp4);
    sgl_button_set_text(button4, "-");
    sgl_button_set_color(button4, sgl_rgb(100, 100, 109));
    sgl_button_set_text_color(button4, sgl_rgb(255, 68, 68));
    sgl_button_set_border_color(button4, sgl_rgb(49, 50, 68));
    sgl_button_set_border_width(button4, 1);
    sgl_button_set_radius(button4, 4);
    sgl_button_set_text_align(button4, SGL_ALIGN_CENTER);
    sgl_button_set_alpha(button4, 255);

    /* 标签 */
    sgl_obj_t *label19 = sgl_label_create(page_page4);
    sgl_obj_set_pos(label19, 98, 66);
    sgl_obj_set_size(label19, 40, 24);
    sgl_label_set_font(label19, &sgl_font_WenQuanZhengHei_1_ttf_18_bpp4);
    sgl_label_set_text(label19, "70℃");
    sgl_label_set_text_color(label19, sgl_rgb(255, 68, 68));
    sgl_label_set_text_align(label19, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label19, 0);
    sgl_label_set_text_offset(label19, 0);
    sgl_label_set_alpha(label19, 255);

    /* 按钮 */
    sgl_obj_t *button5 = sgl_button_create(page_page4);
    sgl_obj_set_pos(button5, 159, 63);
    sgl_obj_set_size(button5, 30, 30);
    sgl_button_set_font(button5, &sgl_font_WenQuanZhengHei_1_ttf_30_bpp4);
    sgl_button_set_text(button5, "+");
    sgl_button_set_color(button5, sgl_rgb(100, 100, 109));
    sgl_button_set_text_color(button5, sgl_rgb(255, 68, 68));
    sgl_button_set_border_color(button5, sgl_rgb(49, 50, 68));
    sgl_button_set_border_width(button5, 1);
    sgl_button_set_radius(button5, 4);
    sgl_button_set_text_align(button5, SGL_ALIGN_CENTER);
    sgl_button_set_alpha(button5, 255);

    /* 按钮 */
    sgl_obj_t *button6 = sgl_button_create(page_page4);
    sgl_obj_set_pos(button6, 0, 115);
    sgl_obj_set_size(button6, 240, 20);
    sgl_button_set_font(button6, &sgl_font_WenQuanZhengHei_1_ttf_12_bpp4);
    sgl_button_set_text(button6, "退出");
    sgl_button_set_color(button6, sgl_rgb(49, 50, 68));
    sgl_button_set_text_color(button6, sgl_rgb(255, 0, 0));
    sgl_button_set_border_color(button6, sgl_rgb(49, 50, 68));
    sgl_button_set_border_width(button6, 0);
    sgl_button_set_radius(button6, 0);
    sgl_button_set_text_align(button6, SGL_ALIGN_CENTER);
    sgl_button_set_alpha(button6, 255);

    /* 标签 */
    sgl_obj_t *label25 = sgl_label_create(page_page4);
    sgl_obj_set_pos(label25, 79, 33);
    sgl_obj_set_size(label25, 43, 24);
    sgl_label_set_font(label25, &sgl_font_WenQuanZhengHei_1_ttf_10_bpp4);
    sgl_label_set_text(label25, "当前温度");
    sgl_label_set_text_color(label25, sgl_rgb(96, 97, 113));
    sgl_label_set_text_align(label25, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label25, 0);
    sgl_label_set_text_offset(label25, 0);
    sgl_label_set_alpha(label25, 255);

    /* 标签 */
    sgl_obj_t *label26 = sgl_label_create(page_page4);
    sgl_obj_set_pos(label26, 125, 33);
    sgl_obj_set_size(label26, 39, 24);
    sgl_label_set_font(label26, &sgl_font_WenQuanZhengHei_1_ttf_15_bpp4);
    sgl_label_set_text(label26, "60℃");
    sgl_label_set_text_color(label26, sgl_rgb(255, 68, 68));
    sgl_label_set_text_align(label26, SGL_ALIGN_CENTER);
    sgl_label_set_radius(label26, 0);
    sgl_label_set_text_offset(label26, 0);
    sgl_label_set_alpha(label26, 255);

}

/* === Page: 页面 5 === */
void ui_page_page5_create(void)
{
    sgl_obj_t *page_page5 = sgl_screen_act();
    sgl_page_set_color(page_page5, sgl_rgb(255, 255, 255));

}

/* === UI Initialization === */
void ui_init(void)
{
    ui_page_page1_create();
    ui_page_page2_create();
    ui_page_page3_create();
    ui_page_page4_create();
    ui_page_page5_create();
}

/* USER CODE BEGIN functions */
/* 用户可在此处添加自定义函数实现，重新生成代码时本区域内容会被保留 */
/* USER CODE END functions */
