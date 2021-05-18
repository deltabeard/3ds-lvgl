/**
 * Copyright (c) 2021 Mahyar Koshkouei
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 * THIS SOFTWARE IS PROVIDED 'AS-IS', WITHOUT ANY EXPRESS OR IMPLIED WARRANTY.
 * IN NO EVENT WILL THE AUTHORS BE HELD LIABLE FOR ANY DAMAGES ARISING FROM THE
 * USE OF THIS SOFTWARE.
 */

#ifdef __3DS__
#include <3ds.h>
#else
#define LV_USE_LOG 1
#include <SDL.h>
#endif

#ifdef _WIN32
#include <direct.h>
#include <dirent_port.h>
#define chdir _chdir
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <lvgl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 3DS screens are portrait, so framebuffer must be rotated by 90 degrees. */
#ifndef GSP_SCREEN_WIDTH_TOP
#define GSP_SCREEN_WIDTH_TOP  240
#define GSP_SCREEN_WIDTH_BOT  240
#define GSP_SCREEN_HEIGHT_TOP 400
#define GSP_SCREEN_HEIGHT_BOT 320
#endif

#define SCREEN_PIXELS_TOP (GSP_SCREEN_WIDTH_TOP * GSP_SCREEN_HEIGHT_TOP)
#define SCREEN_PIXELS_BOT (GSP_SCREEN_WIDTH_BOT * GSP_SCREEN_HEIGHT_BOT)

static bool quit = false;

static void show_error_msg(const char *msg, lv_disp_t *disp);
static void recreate_filepicker(void *p);

/* Declerations for platform specific functions. */
static void *init_system(lv_color_t **fb_top_1, uint32_t *fb_top_1_px,
			 lv_color_t **fb_top_2, uint32_t *fb_top_2_px,
			 lv_color_t **fb_bot_1, uint32_t *fb_bot_1_px,
			 lv_color_t **fb_bot_2, uint32_t *fb_bot_2_px);
static void handle_events(void *ctx);
static void render_present(void *ctx);
static void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p);
static void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p);
static bool read_pointer(struct _lv_indev_drv_t *indev_drv,
			 lv_indev_data_t *data);
static void exit_system(void *ctx);

#define print_fatal(x)                                                         \
	fprintf(stderr, "FATAL: %s+%d: %s - %s\n", __func__, __LINE__, x,      \
		strerror(errno))
#define print_debug(x)                                                         \
	fprintf(stderr, "DEBUG: %s+%d: %s\n", __func__, __LINE__, x)

#if defined(__3DS__)
#define LOOP_CHK() aptMainLoop()
static void *init_system(lv_color_t **fb_top_1, uint32_t *fb_top_1_px,
			 lv_color_t **fb_top_2, uint32_t *fb_top_2_px,
			 lv_color_t **fb_bot_1, uint32_t *fb_bot_1_px,
			 lv_color_t **fb_bot_2, uint32_t *fb_bot_2_px)
{
	void *c = NULL;
	u16 width, height;

	/* This stops scandir() from working. */
	// consoleDebugInit(debugDevice_SVC);

	gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, true);
	gfxSetDoubleBuffering(GFX_TOP, true);
	gfxSetDoubleBuffering(GFX_BOTTOM, true);

	*fb_top_1 =
	    (lv_color_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height);
	*fb_top_1_px = width * height;
	/* Get second buffer for screen. */
	gfxScreenSwapBuffers(GFX_TOP, false);
	*fb_top_2 =
	    (lv_color_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height);
	*fb_top_2_px = width * height;

	*fb_bot_1 = (lv_color_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT,
						    &width, &height);
	*fb_bot_1_px = width * height;
	/* Get second buffer for screen. */
	gfxScreenSwapBuffers(GFX_BOTTOM, false);
	*fb_bot_2 = (lv_color_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT,
						    &width, &height);
	*fb_bot_2_px = width * height;

	c = c - 1;

out:
	return c;
}

static void handle_events(void *ctx)
{
	u32 kDown;

	/* Scan all the inputs. */
	hidScanInput();
	kDown = hidKeysDown();

	return;
}

static void render_present(void *ctx)
{
	gfxFlushBuffers();
	gfxScreenSwapBuffers(GFX_TOP, false);
	gfxScreenSwapBuffers(GFX_BOTTOM, false);

	/* Wait for VBlank */
	gspWaitForVBlank();
}

static void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct system_ctx *c = disp_drv->user_data;
	lv_disp_flush_ready(disp_drv);
}

static void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct system_ctx *c = disp_drv->user_data;
	lv_disp_flush_ready(disp_drv);
}

static bool read_pointer(struct _lv_indev_drv_t *indev_drv,
			 lv_indev_data_t *data)
{
	static touchPosition touch = {0};
	u32 kRepeat;

	kRepeat = hidKeysHeld();

	if (kRepeat & KEY_TOUCH)
		hidTouchRead(&touch);

	data->point.x = touch.px;
	data->point.y = touch.py;
	data->state =
	    (kRepeat & KEY_TOUCH) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
	return false;
}

static void exit_system(void *ctx)
{
	gfxExit();
	ctx = NULL;
	return;
}

#else
#define LOOP_CHK() (SDL_QuitRequested() == SDL_FALSE)
struct system_ctx
{
	SDL_Window *win_top;
	SDL_Window *win_bot;
	SDL_Surface *surf_top_1, *surf_top_2;
	SDL_Surface *surf_bot_1, *surf_bot_2;
};

static void *init_system(lv_color_t **fb_top_1, uint32_t *fb_top_1_px,
			 lv_color_t **fb_top_2, uint32_t *fb_top_2_px,
			 lv_color_t **fb_bot_1, uint32_t *fb_bot_1_px,
			 lv_color_t **fb_bot_2, uint32_t *fb_bot_2_px)
{
	struct system_ctx *c;
	int win_top_x, win_top_y, win_top_border;

	c = SDL_calloc(1, sizeof(struct system_ctx));
	SDL_assert_always(c != NULL);

	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
	SDL_Init(SDL_INIT_EVERYTHING);

	c->win_top = SDL_CreateWindow("3DS Top Screen", SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED, GSP_SCREEN_WIDTH_TOP,
			GSP_SCREEN_HEIGHT_TOP, 0);
	SDL_assert_always(c->win_top != NULL);

	/* Align the bottom window to the top window. */
	SDL_GetWindowPosition(c->win_top, &win_top_x, &win_top_y);
	SDL_GetWindowBordersSize(c->win_top, &win_top_border, NULL, NULL, NULL);
	win_top_x += (GSP_SCREEN_HEIGHT_TOP - GSP_SCREEN_HEIGHT_BOT) / 2;
	win_top_y += GSP_SCREEN_WIDTH_TOP + win_top_border;

	c->win_bot = SDL_CreateWindow("3DS Bottom Screen", win_top_x, win_top_y,
			    GSP_SCREEN_WIDTH_BOT, GSP_SCREEN_HEIGHT_BOT, 0);
	SDL_assert_always(c->win_bot != NULL);

	//c->surf_top = SDL_GetWindowSurface(c->win_top);
	
	c->surf_top_1 = SDL_CreateRGBSurfaceWithFormat(0, GSP_SCREEN_WIDTH_TOP,
			GSP_SCREEN_HEIGHT_TOP, 16, SDL_PIXELFORMAT_RGB565);
	c->surf_top_2 = SDL_CreateRGBSurfaceWithFormat(0, GSP_SCREEN_WIDTH_TOP,
			GSP_SCREEN_HEIGHT_TOP, 16, SDL_PIXELFORMAT_RGB565);
	c->surf_bot_1 = SDL_CreateRGBSurfaceWithFormat(0, GSP_SCREEN_WIDTH_BOT,
			GSP_SCREEN_HEIGHT_BOT, 16, SDL_PIXELFORMAT_RGB565);
	c->surf_bot_2 = SDL_CreateRGBSurfaceWithFormat(0, GSP_SCREEN_WIDTH_BOT,
			GSP_SCREEN_HEIGHT_BOT, 16, SDL_PIXELFORMAT_RGB565);
	SDL_assert_always(c->surf_top_1 != NULL);
	SDL_assert_always(c->surf_top_2 != NULL);
	SDL_assert_always(c->surf_bot_1 != NULL);
	SDL_assert_always(c->surf_bot_2 != NULL);

	*fb_top_1 = c->surf_top_1->pixels;
	*fb_top_2 = c->surf_top_2->pixels;
	*fb_top_1_px = GSP_SCREEN_WIDTH_TOP * GSP_SCREEN_HEIGHT_TOP * 2;
	*fb_top_2_px = GSP_SCREEN_WIDTH_TOP * GSP_SCREEN_HEIGHT_TOP * 2;

	*fb_bot_1 = c->surf_bot_1->pixels;
	*fb_bot_2 = c->surf_bot_2->pixels;
	*fb_bot_1_px = GSP_SCREEN_WIDTH_BOT * GSP_SCREEN_HEIGHT_BOT * 2;
	*fb_bot_2_px = GSP_SCREEN_WIDTH_BOT * GSP_SCREEN_HEIGHT_BOT * 2;

	return c;
}

static void handle_events(void *ctx)
{
	SDL_Event e;

	while (SDL_PollEvent(&e))
	{
	}
	return;
}

static void render_present(void *ctx)
{
	struct system_ctx *c = ctx;
	SDL_UpdateWindowSurface(c->win_top);
	SDL_UpdateWindowSurface(c->win_bot);
	SDL_Delay(5);
	return;
}

static void flush_cb(SDL_Surface *dst, const lv_area_t *area,
		     lv_color_t *color_p)
{
	SDL_Surface *src;
	SDL_Rect dstrect;

	dstrect.x = area->x1;
	dstrect.y = area->y1;
	dstrect.w = area->x2 - area->x1 + 1;
	dstrect.h = area->y2 - area->y1 + 1;
	src = SDL_CreateRGBSurfaceWithFormatFrom(
	    color_p, dstrect.w, dstrect.h, 16, dstrect.w * sizeof(lv_color_t),
	    SDL_PIXELFORMAT_RGB565);

	SDL_BlitSurface(src, NULL, dst, &dstrect);
	SDL_FreeSurface(src);
}

static void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct system_ctx *c = disp_drv->user_data;
	SDL_Surface *surf_top = SDL_GetWindowSurface(c->win_top);
	flush_cb(surf_top, area, color_p);
	lv_disp_flush_ready(disp_drv);
}

static void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct system_ctx *c = disp_drv->user_data;
	SDL_Surface *surf_bot = SDL_GetWindowSurface(c->win_bot);
	flush_cb(surf_bot, area, color_p);
	lv_disp_flush_ready(disp_drv);
}

static bool read_pointer(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
	struct system_ctx *c = indev_drv->user_data;

	if (indev_drv->type == LV_INDEV_TYPE_POINTER)
	{
		int x, y;
		Uint32 btnmask;
		SDL_Window *win_active;
		Uint32 win_bot_id;
		Uint32 win_focused_id;

		win_active = SDL_GetMouseFocus();
		win_bot_id = SDL_GetWindowID(c->win_bot);
		win_focused_id = SDL_GetWindowID(win_active);

		/* Only accept touch input on bottom screen. */
		if (win_focused_id != win_bot_id)
			return false;

		btnmask = SDL_GetMouseState(&x, &y);

		data->point.x = x;
		data->point.y = y;
		data->state = (btnmask & SDL_BUTTON(SDL_BUTTON_LEFT))
				  ? LV_INDEV_STATE_PR
				  : LV_INDEV_STATE_REL;
	}
	else if (indev_drv->type == LV_INDEV_TYPE_KEYPAD)
	{
		const Uint8 *state = SDL_GetKeyboardState(NULL);
		SDL_Keymod mod = SDL_GetModState();

		if (state[SDL_SCANCODE_DOWN])
			data->key = LV_KEY_DOWN;
		else if (state[SDL_SCANCODE_UP])
			data->key = LV_KEY_UP;
		else if (state[SDL_SCANCODE_RIGHT])
			data->key = LV_KEY_RIGHT;
		else if (state[SDL_SCANCODE_LEFT])
			data->key = LV_KEY_LEFT;
		else if (state[SDL_SCANCODE_RETURN])
			data->key = LV_KEY_ENTER;
		else if (state[SDL_SCANCODE_BACKSPACE])
			data->key = LV_KEY_BACKSPACE;
		else if (state[SDL_SCANCODE_ESCAPE])
			data->key = LV_KEY_ESC;
		else if (state[SDL_SCANCODE_DELETE])
			data->key = LV_KEY_DEL;
		else if (state[SDL_SCANCODE_TAB] && (mod & KMOD_LSHIFT))
			data->key = LV_KEY_PREV;
		else if (state[SDL_SCANCODE_TAB])
			data->key = LV_KEY_NEXT;
		else if (state[SDL_SCANCODE_HOME])
			data->key = LV_KEY_HOME;
		else if (state[SDL_SCANCODE_END])
			data->key = LV_KEY_END;
		else
		{
			data->state = LV_INDEV_STATE_REL;
			return false;
		}

		data->state = LV_INDEV_STATE_PR;
	}
	return false;
}
static void exit_system(void *ctx)
{
	struct system_ctx *c = ctx;

	SDL_DestroyWindow(c->win_top);
	SDL_DestroyWindow(c->win_bot);
	SDL_Quit();

	return;
}
#endif

static void btnev_quit(lv_obj_t *btn, lv_event_t event)
{
	(void)btn;

	if (event != LV_EVENT_CLICKED)
		return;

	quit = true;
	return;
}

static void mboxen_del(lv_obj_t *mbox, lv_event_t event)
{
	lv_obj_t *bg;

	if (event != LV_EVENT_CLICKED)
		return;

	if (lv_msgbox_get_active_btn(mbox) == LV_BTNMATRIX_BTN_NONE)
		return;

	bg = lv_obj_get_parent(mbox);
	lv_obj_del_async(bg);
}

/**
 * Displays an error message box.
 * The application must continue to function correctly.
 */
static void show_error_msg(const char *msg, lv_disp_t *disp)
{
	static const char *btns[] = {"OK", ""};
	lv_obj_t *scr, *bg, *mbox1;
	lv_coord_t ver, hor;

	lv_disp_set_default(disp);
	scr = lv_scr_act();

	ver = lv_disp_get_ver_res(disp);
	hor = lv_disp_get_hor_res(disp);

	bg = lv_obj_create(scr, NULL);
	lv_obj_set_size(bg, hor, ver);
	lv_obj_set_style_local_border_width(bg, LV_OBJ_PART_MAIN,
					    LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_radius(bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT,
				      0);
	lv_obj_set_style_local_bg_color(bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT,
					LV_COLOR_BLACK);
	lv_obj_set_style_local_bg_opa(bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT,
				      80);

	mbox1 = lv_msgbox_create(bg, NULL);
	lv_msgbox_set_text(mbox1, msg);
	lv_msgbox_add_btns(mbox1, btns);
	lv_obj_set_event_cb(mbox1, mboxen_del);
	lv_obj_set_width(mbox1, hor - 32);
	lv_obj_set_height(mbox1, ver - 32);
	lv_obj_align_mid(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
}

static void btnev_chdir(lv_obj_t *btn, lv_event_t event)
{
	lv_obj_t *label;
	const char *dir;
	lv_obj_t *list;
	lv_disp_t *disp;

	label = lv_obj_get_child(btn, NULL);
	dir = lv_label_get_text(label);
	list = lv_obj_get_user_data(btn);
	disp = lv_obj_get_user_data(list);

	if (event != LV_EVENT_CLICKED)
		return;

	if (chdir(dir) != 0)
	{
		char err_txt[256] = "";
		snprintf(err_txt, sizeof(err_txt),
			 "Changing directory to '%s' failed:\n"
			 "%s",
			 dir, strerror(errno));
		show_error_msg(err_txt, disp);
		return;
	}

	lv_async_call(recreate_filepicker, list);

	return;
}

static void btnev_updir(lv_obj_t *btn, lv_event_t event)
{
	lv_obj_t *list;
	lv_disp_t *disp;

	list = lv_obj_get_user_data(btn);
	disp = lv_obj_get_user_data(list);

	if (event != LV_EVENT_CLICKED)
		return;

	if (chdir("..") != 0)
	{
		char err_txt[256] = "";
		snprintf(err_txt, sizeof(err_txt),
			 "Unable to go up a directory:\n"
			 "%s",
			 strerror(errno));
		show_error_msg(err_txt, disp);
		return;
	}

	lv_async_call(recreate_filepicker, list);

	return;
}

static const char *get_filename_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if(dot == NULL)
	    return "";

    return dot + 1;
}

static void recreate_filepicker(void *p)
{
	lv_obj_t *list = p;
	lv_obj_t *list_btn = NULL, *img = NULL, *label = NULL;
	struct dirent **namelist;
	int entries;
	int w;
	lv_disp_t *disp;

	disp = lv_obj_get_user_data(list);
	entries = scandir(".", &namelist, NULL, alphasort);
	if (entries == -1)
	{
		char err_txt[256] = "";

		snprintf(err_txt, sizeof(err_txt),
			 "Unable to scan directory: %s", strerror(errno));

		show_error_msg(err_txt, disp);

		/* Attempt to recover by going to root directory. */
		chdir("/");

		/* Don't clean file list on error. */
		return;
	}

	/* Clean file list */
	{
		lv_obj_t *scrl = lv_page_get_scrollable(list);
		lv_fit_t scrl_fitl = lv_cont_get_fit_left(scrl);
		lv_fit_t scrl_fitr = lv_cont_get_fit_right(scrl);
		lv_fit_t scrl_fitt = lv_cont_get_fit_top(scrl);
		lv_fit_t scrl_fitb = lv_cont_get_fit_bottom(scrl);
		lv_layout_t scrl_layout = lv_cont_get_layout(scrl);

		/* Do not re-fit all the buttons on the removable of each
button. */
		lv_cont_set_fit(scrl, LV_FIT_NONE);
		lv_cont_set_layout(scrl, LV_LAYOUT_OFF);
		lv_page_clean(list);
		lv_cont_set_fit4(scrl, scrl_fitl, scrl_fitr, scrl_fitr,
				 scrl_fitb);
		lv_cont_set_layout(scrl, scrl_layout);
		w = lv_obj_get_width_fit(scrl);
	}

	for (int e = 0; e < entries; e++)
	{
		lv_event_cb_t event_cb = NULL;
		const char *symbol = LV_SYMBOL_FILE;
		const char *compat_fileext[] = {
			"wav", "flac", "mp3", "mp2", "ogg", "opus"
		};

		/* Ignore "current directory" file. */
		if (strcmp(namelist[e]->d_name, ".") == 0)
			continue;

		/* Ignore "up directory" file, since the 3DS does not generate
		 * this automatically. */
		if (strcmp(namelist[e]->d_name, "..") == 0)
			continue;

		if(namelist[e]->d_type == DT_DIR)
		{
			event_cb = btnev_chdir;
			symbol = LV_SYMBOL_DIRECTORY;
		}
		else
		{
			const unsigned exts_n =
				sizeof(compat_fileext)/sizeof(*compat_fileext);
			const char *ext = get_filename_ext(namelist[e]->d_name);

			for(unsigned ext_n = 0; ext_n < exts_n; ext_n++)
			{
				if(strcmp(ext, compat_fileext[ext_n]) != 0)
					continue;

				symbol = LV_SYMBOL_AUDIO;
				break;
			}
		}

		list_btn = lv_btn_create(list, list_btn);
		lv_page_glue_obj(list_btn, true);
		lv_btn_set_layout(list_btn, LV_LAYOUT_ROW_MID);
		lv_obj_set_width(list_btn, w);

		img = lv_img_create(list_btn, img);
		lv_img_set_src(img, symbol);
		lv_obj_set_click(img, false);

		label = lv_label_create(list_btn, label);
		lv_obj_set_width(label, w - (lv_obj_get_width_margin(img) * 4));
		lv_label_set_text(label, namelist[e]->d_name);
		lv_label_set_long_mode(label, LV_LABEL_LONG_SROLL_CIRC);
		lv_obj_set_click(label, false);

		lv_obj_set_event_cb(list_btn, event_cb);
		lv_obj_set_user_data(list_btn, list);
		lv_theme_apply(list_btn, LV_THEME_LIST_BTN);
		// lv_group_add_obj(ui_ctx->groups[SCREEN_OPEN_FILE], list_btn);

		free(namelist[e]);
	}

	free(namelist);
}

static void create_top_ui(lv_disp_t *top_disp)
{
	const char *lorem_ipsum = "\nبنی‌آدم اعضای یک پیکرند"
		"\nکه در آفرينش ز یک گوهرند"
		"چو عضوى به‌درد آورَد روزگار\n"
		"دگر عضوها را نمانَد قرار\n"
		"تو کز محنت دیگران بی‌غمی\n"
		"نشاید که نامت نهند آدمی\n";
	lv_obj_t *label1, *top_cont;
	lv_coord_t ver, hor;

	/* Select bottom screen. */
	lv_disp_set_default(top_disp);
	ver = lv_disp_get_ver_res(top_disp);
	hor = lv_disp_get_hor_res(top_disp);

	top_cont = lv_cont_create(lv_scr_act(), NULL);
	label1 = lv_label_create(top_cont, NULL);
	lv_label_set_text(label1, lorem_ipsum);
	lv_cont_set_fit(top_cont, LV_FIT_TIGHT);
	lv_cont_set_layout(top_cont, LV_LAYOUT_COLUMN_MID);
}

static void create_bottom_ui(lv_disp_t *bottom_disp)
{
	lv_obj_t *tabview;
	lv_obj_t *tab_file, *tab_playlist, *tab_control, *tab_settings,
	    *tab_system;

	/* Select bottom screen. */
	lv_disp_set_default(bottom_disp);

	/* Create tabview with main options. */
	tabview = lv_tabview_create(lv_scr_act(), NULL);

	tab_file = lv_tabview_add_tab(tabview, LV_SYMBOL_DIRECTORY);
	tab_playlist = lv_tabview_add_tab(tabview, LV_SYMBOL_LIST);
	tab_control = lv_tabview_add_tab(tabview, LV_SYMBOL_PLAY);
	tab_settings = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS);
	tab_system = lv_tabview_add_tab(tabview, LV_SYMBOL_POWER);

	/* Create file picker. */
	{
		lv_obj_t *list, *toolbar;
		lv_obj_t *up_btn, *up_btn_lbl;
		lv_coord_t cw = lv_obj_get_width(tab_file);
		lv_coord_t ch = lv_obj_get_height(tab_file);
		lv_coord_t toolbar_h = 32;
		lv_obj_t *file_scrl = lv_page_get_scrollable(tab_file);

		lv_cont_set_layout(file_scrl, LV_LAYOUT_ROW_TOP);

		list = lv_page_create(tab_file, NULL);
		toolbar = lv_cont_create(tab_file, NULL);

		up_btn = lv_btn_create(toolbar, NULL);
		up_btn_lbl = lv_label_create(up_btn, NULL);
		lv_label_set_text(up_btn_lbl, LV_SYMBOL_UP);
		lv_cont_set_fit(toolbar, LV_FIT_NONE);
		lv_cont_set_layout(toolbar, LV_LAYOUT_ROW_TOP);
		lv_obj_set_size(toolbar, toolbar_h, ch);
		lv_obj_set_size(up_btn, toolbar_h, toolbar_h);
		lv_obj_set_event_cb(up_btn, btnev_updir);
		lv_obj_set_user_data(up_btn, list);

		lv_obj_set_style_local_pad_all(toolbar, LV_CONT_PART_MAIN,
					       LV_STATE_DEFAULT, 0);
		lv_obj_set_style_local_pad_all(up_btn, LV_BTN_PART_MAIN,
					       LV_STATE_DEFAULT, 0);
		lv_obj_set_style_local_radius(up_btn, LV_BTN_PART_MAIN,
				LV_STATE_DEFAULT, 0);

		lv_obj_set_size(list, cw - toolbar_h, ch);
		lv_theme_apply(list, LV_THEME_LIST);
		lv_page_set_scrl_layout(list, LV_LAYOUT_COLUMN_MID);
		lv_page_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

		/* Store target display in user data for the file list. */
		lv_obj_set_user_data(list, bottom_disp);
		lv_async_call(recreate_filepicker, list);
	}

	/* Populate system tab. */
	{
		lv_obj_t *quit_btn, *quit_lbl;
		quit_btn = lv_btn_create(tab_system, NULL);
		lv_obj_set_event_cb(quit_btn, btnev_quit);
		lv_obj_align(quit_btn, tab_system, LV_ALIGN_IN_TOP_MID, 0, 0);
		quit_lbl = lv_label_create(quit_btn, NULL);
		lv_label_set_text(quit_lbl, "Quit");
	}

	return;
}

static void print_cb(lv_log_level_t level, const char *file, uint32_t line,
	      const char *fn, const char *desc)
{
	const char *pri_str[] = {"TRACE", "INFO", "WARNING",
				 "ERROR", "USER", "NONE"};

	fprintf(stderr, "%s %s +%u %s(): %s\n", pri_str[level], file, line, fn,
		desc);
	return;
}

int main(int argc, char *argv[])
{
	void *ctx;
	int ret = EXIT_FAILURE;
	lv_disp_buf_t lv_disp_buf_top, lv_disp_buf_bot;
	lv_disp_drv_t lv_disp_drv_top, lv_disp_drv_bot;
	lv_disp_t *lv_disp_top, *lv_disp_bot;

	lv_color_t *fb_top_1, *fb_top_2;
	lv_color_t *fb_bot_1, *fb_bot_2;
	uint32_t fb_top_1_px, fb_top_2_px, fb_bot_1_px, fb_bot_2_px;

	(void)argc;
	(void)argv;

	ctx = init_system(&fb_top_1, &fb_top_1_px, &fb_top_2, &fb_top_2_px,
			  &fb_bot_1, &fb_bot_1_px, &fb_bot_2, &fb_bot_2_px);

	if (ctx == NULL)
	{
		print_fatal("Unable to initialise system");
		goto err;
	}

	print_debug("System initialised");

	/* Initialise LVGL. */
	lv_init();
	lv_log_register_print_cb(print_cb);

	lv_disp_buf_init(&lv_disp_buf_top, fb_top_1, fb_top_2, fb_top_1_px);
	lv_disp_buf_init(&lv_disp_buf_bot, fb_bot_1, fb_bot_2, fb_bot_1_px);

	lv_disp_drv_init(&lv_disp_drv_top);
	lv_disp_drv_top.buffer = &lv_disp_buf_top;
	lv_disp_drv_top.flush_cb = flush_top_cb;
	lv_disp_drv_top.user_data = ctx;
#ifdef __3DS__
	lv_disp_drv_top.rotated = LV_DISP_ROT_90;
	lv_disp_drv_top.sw_rotate = 1;
        lv_disp_drv_top.hor_res = GSP_SCREEN_HEIGHT_TOP;
	lv_disp_drv_top.ver_res = GSP_SCREEN_WIDTH_TOP;
#else
	lv_disp_drv_top.rotated = LV_DISP_ROT_90;
	lv_disp_drv_top.sw_rotate = 1;
	lv_disp_drv_top.hor_res = GSP_SCREEN_WIDTH_TOP;
	lv_disp_drv_top.ver_res = GSP_SCREEN_HEIGHT_TOP;
#endif
	lv_disp_top = lv_disp_drv_register(&lv_disp_drv_top);

	lv_disp_drv_init(&lv_disp_drv_bot);
	lv_disp_drv_bot.buffer = &lv_disp_buf_bot;
	lv_disp_drv_bot.flush_cb = flush_bot_cb;
	lv_disp_drv_bot.user_data = ctx;
#ifdef __3DS__
	lv_disp_drv_bot.rotated = LV_DISP_ROT_90;
	lv_disp_drv_bot.sw_rotate = 1;
        lv_disp_drv_bot.hor_res = GSP_SCREEN_HEIGHT_BOT;
	lv_disp_drv_bot.ver_res = GSP_SCREEN_WIDTH_BOT;
#else
	lv_disp_drv_bot.rotated = LV_DISP_ROT_90;
	lv_disp_drv_bot.sw_rotate = 1;
	lv_disp_drv_bot.hor_res = GSP_SCREEN_WIDTH_BOT;
	lv_disp_drv_bot.ver_res = GSP_SCREEN_HEIGHT_BOT;
#endif
	lv_disp_bot = lv_disp_drv_register(&lv_disp_drv_bot);

	lv_disp_drv_init(&lv_disp_drv_top);
	lv_disp_drv_init(&lv_disp_drv_bot);

	/* Initialise UI input drivers. */
	lv_disp_set_default(lv_disp_bot);
	lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = read_pointer;
	indev_drv.user_data = ctx;
	lv_indev_drv_register(&indev_drv);

	create_top_ui(lv_disp_top);
	create_bottom_ui(lv_disp_bot);

	while (LOOP_CHK() && quit == false)
	{
		handle_events(ctx);
		lv_task_handler();
		render_present(ctx);
	}

	exit_system(ctx);

	ret = EXIT_SUCCESS;
	print_debug("Successful exit");

err:
	return ret;
}
