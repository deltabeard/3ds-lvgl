#include <lvgl.h>

/* 3DS screens are portrait, so framebuffer must be rotated by 90 degrees. */
#ifndef GSP_SCREEN_WIDTH_TOP
#define GSP_SCREEN_WIDTH_TOP  240
#define GSP_SCREEN_WIDTH_BOT  240
#define GSP_SCREEN_HEIGHT_TOP 400
#define GSP_SCREEN_HEIGHT_BOT 320
#endif

#define SCREEN_PIXELS_TOP (GSP_SCREEN_WIDTH_TOP * GSP_SCREEN_HEIGHT_TOP)
#define SCREEN_PIXELS_BOT (GSP_SCREEN_WIDTH_BOT * GSP_SCREEN_HEIGHT_BOT)

/* Declerations for platform specific functions. */
void *init_system(lv_color_t **fb_top_1, uint32_t *fb_top_1_px,
			 lv_color_t **fb_top_2, uint32_t *fb_top_2_px,
			 lv_color_t **fb_bot_1, uint32_t *fb_bot_1_px,
			 lv_color_t **fb_bot_2, uint32_t *fb_bot_2_px);
void handle_events(void *ctx);
void render_present(void *ctx);
void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p);
void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p);
bool read_pointer(struct _lv_indev_drv_t *indev_drv,
			 lv_indev_data_t *data);
void exit_system(void *ctx);
int exit_requested(void *ctx);
