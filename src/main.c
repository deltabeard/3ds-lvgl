/**
 * Copyright (c) 2021 Mahyar Koshkouei
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 * THIS SOFTWARE IS PROVIDED 'AS-IS', WITHOUT ANY EXPRESS OR IMPLIED WARRANTY.
 * IN NO EVENT WILL THE AUTHORS BE HELD LIABLE FOR ANY DAMAGES ARISING FROM THE
 * USE OF THIS SOFTWARE.
 */

#ifdef __3DS__
# include <3ds.h>
#else
# define LV_USE_LOG 1
# include <SDL.h>
#endif

#ifdef _WIN32
# include <direct.h>
# include <dirent_port.h>
# define chdir _chdir
#else
# include <dirent.h>
# include <unistd.h>
#endif

#include <errno.h>
#include <lvgl.h>

#ifndef GSP_SCREEN_WIDTH_TOP
# define GSP_SCREEN_WIDTH_TOP	240
# define GSP_SCREEN_WIDTH_BOT	240
# define GSP_SCREEN_HEIGHT_TOP	400
# define GSP_SCREEN_HEIGHT_BOT	320
#endif

#define SCREEN_PIXELS_TOP (GSP_SCREEN_WIDTH_TOP * GSP_SCREEN_HEIGHT_TOP)
#define SCREEN_PIXELS_BOT (GSP_SCREEN_WIDTH_BOT * GSP_SCREEN_HEIGHT_BOT)

/**
 * Initialise platform specific functions.
 *
 * \returns	Private context, or NULL on error.
 */
static void *init_system(void);
static void handle_events(void *ctx);
static void render_present(void *ctx);
static void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
	lv_color_t *color_p);
static void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
	lv_color_t *color_p);
static bool read_pointer(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void exit_system(void *ctx);


#if defined(__3DS__)
# define LOOP_CHK() aptMainLoop()
static void *init_system(void **fb_top, void **fb_bot)
{
	gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);

	*fb_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	*fb_bot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

	return 0;
}
static void handle_events(void *ctx)
{
	return;
}
static void exit_system(void *ctx, void **fb_top, void **fb_bot)
{
	gfxExit();
	return 0;
}

#else
# define LOOP_CHK() (SDL_QuitRequested() == SDL_FALSE)
struct system_ctx {
	SDL_Window *win_top;
	SDL_Window *win_bot;
	SDL_Surface *surf_top;
	SDL_Surface *surf_bot;
};

static void *init_system(void)
{
	struct system_ctx *c;

	c = SDL_calloc(1, sizeof(struct system_ctx));
	if(c == NULL)
		goto out;

	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
	SDL_Init(SDL_INIT_EVERYTHING);

	c->win_top = SDL_CreateWindow("3DS Top Screen",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		GSP_SCREEN_HEIGHT_TOP, GSP_SCREEN_WIDTH_TOP, 0);
	if(c->win_top == NULL)
		goto err;

	c->win_bot = SDL_CreateWindow("3DS Bottom Screen",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		GSP_SCREEN_HEIGHT_BOT, GSP_SCREEN_WIDTH_BOT, 0);
	if(c->win_bot == NULL)
		goto err;

	c->surf_top = SDL_GetWindowSurface(c->win_top);
	if(c->surf_top == NULL)
		goto err;

	c->surf_bot = SDL_GetWindowSurface(c->win_bot);
	if(c->surf_bot == NULL)
		goto err;

out:
	return c;

err:
	if(c == NULL)
		goto out;

	if(c->surf_top != NULL)
		SDL_FreeSurface(c->surf_top);

	if(c->surf_bot != NULL)
		SDL_FreeSurface(c->surf_bot);

	if(c->win_top != NULL)
		SDL_DestroyWindow(c->win_top);

	if(c->win_bot != NULL)
		SDL_DestroyWindow(c->win_bot);

	SDL_free(c);
	goto out;
}
static void handle_events(void *ctx)
{
	SDL_Event e;

	while(SDL_PollEvent(&e))
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
};
static void flush_cb(SDL_Surface *dst, const lv_area_t *area,
	lv_color_t *color_p)
{
	SDL_Surface *src;
	SDL_Rect dstrect;

	dstrect.x = area->x1;
	dstrect.y = area->y1;
	dstrect.w = area->x2 - area->x1 + 1;
	dstrect.h = area->y2 - area->y1 + 1;
	src = SDL_CreateRGBSurfaceWithFormatFrom(color_p,
		dstrect.w, dstrect.h, 16, dstrect.w * sizeof(lv_color_t),
		SDL_PIXELFORMAT_RGB565);

	SDL_BlitSurface(src, NULL, dst, &dstrect);
	SDL_FreeSurface(src);
}
static void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
		lv_color_t *color_p)
{
	struct system_ctx *c = disp_drv->user_data;
	flush_cb(c->surf_top, area, color_p);
	lv_disp_flush_ready(disp_drv);
}
static void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
		lv_color_t *color_p)
{
	struct system_ctx *c = disp_drv->user_data;
	flush_cb(c->surf_bot, area, color_p);
	lv_disp_flush_ready(disp_drv);
}
bool read_pointer(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
	struct system_ctx  *c = indev_drv->user_data;

	if(indev_drv->type == LV_INDEV_TYPE_POINTER)
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
		if(win_focused_id != win_bot_id)
			return false;

		btnmask = SDL_GetMouseState(&x, &y);

		data->point.x = x;
		data->point.y = y;
		data->state = (btnmask & SDL_BUTTON(SDL_BUTTON_LEFT))
			? LV_INDEV_STATE_PR
			: LV_INDEV_STATE_REL;
	}
	else if(indev_drv->type == LV_INDEV_TYPE_KEYPAD)
	{
		const Uint8 *state = SDL_GetKeyboardState(NULL);
		SDL_Keymod mod = SDL_GetModState();

		if(state[SDL_SCANCODE_DOWN])
			data->key = LV_KEY_DOWN;
		else if(state[SDL_SCANCODE_UP])
			data->key = LV_KEY_UP;
		else if(state[SDL_SCANCODE_RIGHT])
			data->key = LV_KEY_RIGHT;
		else if(state[SDL_SCANCODE_LEFT])
			data->key = LV_KEY_LEFT;
		else if(state[SDL_SCANCODE_RETURN])
			data->key = LV_KEY_ENTER;
		else if(state[SDL_SCANCODE_BACKSPACE])
			data->key = LV_KEY_BACKSPACE;
		else if(state[SDL_SCANCODE_ESCAPE])
			data->key = LV_KEY_ESC;
		else if(state[SDL_SCANCODE_DELETE])
			data->key = LV_KEY_DEL;
		else if(state[SDL_SCANCODE_TAB] && (mod & KMOD_LSHIFT))
			data->key = LV_KEY_PREV;
		else if(state[SDL_SCANCODE_TAB])
			data->key = LV_KEY_NEXT;
		else if(state[SDL_SCANCODE_HOME])
			data->key = LV_KEY_HOME;
		else if(state[SDL_SCANCODE_END])
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

static void recreate_filepicker(void *p);

static void btnev_chdir(lv_obj_t *btn, lv_event_t event)
{
	lv_obj_t *label;
	const char *dir;
	lv_obj_t *list;

	label = lv_obj_get_child(btn, NULL);
	dir = lv_label_get_text(label);
	list = lv_obj_get_user_data(btn);

	if(event != LV_EVENT_CLICKED)
		return;

	if(chdir(dir) != 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"Changing directory to %s failed", dir);
		return;
	}

	SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION,
		"Changing directory to %s", dir);
	lv_async_call(recreate_filepicker, list);

	return;
}

static void recreate_filepicker(void *p)
{
	lv_obj_t *list = p;
	lv_obj_t *list_btn = NULL, *img = NULL, *label = NULL;
	struct dirent **namelist;
	int entries;
	int w;

	/* Clean file list */
	{
		lv_obj_t *scrl = lv_page_get_scrollable(list);
		lv_fit_t scrl_fitl = lv_cont_get_fit_left(scrl);
		lv_fit_t scrl_fitr = lv_cont_get_fit_right(scrl);
		lv_fit_t scrl_fitt = lv_cont_get_fit_top(scrl);
		lv_fit_t scrl_fitb = lv_cont_get_fit_bottom(scrl);
		lv_layout_t scrl_layout = lv_cont_get_layout(scrl);

		/* Do not re-fit all the buttons on the removable of each button. */
		lv_cont_set_fit(scrl, LV_FIT_NONE);
		lv_cont_set_layout(scrl, LV_LAYOUT_OFF);
		lv_page_clean(list);
		lv_cont_set_fit4(scrl, scrl_fitl, scrl_fitr, scrl_fitr, scrl_fitb);
		lv_cont_set_layout(scrl, scrl_layout);
		w = lv_obj_get_width_fit(scrl);
	}

	entries = scandir(".", &namelist, NULL, alphasort);
	if(entries == -1)
	{
		lv_obj_t *l;
		char err_txt[512] = "Unknown";

		SDL_snprintf(err_txt, sizeof(err_txt),
			"Unable to scan directory: %s", strerror(errno));

		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "%s", err_txt);
		l = lv_label_create(list, NULL);
		lv_label_set_text(l, err_txt);
		return;
	}

	for(int e = 0; e < entries; e++)
	{
		lv_event_cb_t event_cb;
		const char *symbol;

		/* Ignore "current directory" file. */
		if(SDL_strcmp(namelist[e]->d_name, ".") == 0)
			continue;

		switch(namelist[e]->d_type)
		{
		case DT_DIR:
			event_cb = btnev_chdir;
			symbol = LV_SYMBOL_DIRECTORY;
			break;

		default:
			event_cb = NULL;
			symbol = LV_SYMBOL_FILE;
			break;
		}

		list_btn = lv_btn_create(list, list_btn);
		lv_page_glue_obj(list_btn, true);
		lv_btn_set_layout(list_btn, LV_LAYOUT_ROW_MID);
		lv_obj_set_width(list_btn, w);
		lv_obj_set_height(list_btn, lv_dpx(70));

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
		//lv_group_add_obj(ui_ctx->groups[SCREEN_OPEN_FILE], list_btn);

		free(namelist[e]);
	}

	free(namelist);
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
	lv_obj_set_size(tabview, GSP_SCREEN_HEIGHT_BOT, GSP_SCREEN_WIDTH_BOT);

	tab_file = lv_tabview_add_tab(tabview, LV_SYMBOL_DIRECTORY);
	tab_playlist = lv_tabview_add_tab(tabview, LV_SYMBOL_LIST);
	tab_control = lv_tabview_add_tab(tabview, LV_SYMBOL_PLAY);
	tab_settings = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS);
	tab_system = lv_tabview_add_tab(tabview, LV_SYMBOL_POWER);

	/* Create file picker. */
	{
		lv_obj_t *list = lv_page_create(tab_file, NULL);
		lv_coord_t cw = lv_obj_get_width(tab_file) - 10;
		lv_coord_t ch = lv_obj_get_height(tab_file) - 10;
		lv_obj_set_size(list, cw, ch);
		lv_theme_apply(list, LV_THEME_LIST);
		lv_page_set_scrl_layout(list, LV_LAYOUT_COLUMN_MID);
		lv_page_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
		lv_async_call(recreate_filepicker, list);
	}

	return;
}

void print_SDL_cb(lv_log_level_t level, const char *file, uint32_t line,
	const char *fn, const char *desc)
{
	int p[_LV_LOG_LEVEL_NUM] = { SDL_LOG_PRIORITY_DEBUG,
		SDL_LOG_PRIORITY_INFO,
		SDL_LOG_PRIORITY_WARN,
		SDL_LOG_PRIORITY_ERROR,
		SDL_LOG_PRIORITY_INFO,
		SDL_LOG_PRIORITY_VERBOSE };
	(void)file;
	(void)line;

	SDL_LogMessage(SDL_LOG_CATEGORY_VIDEO, p[level], "%s: %s", fn, desc);
	return;
}

int main(int argc, char *argv[])
{
	void *ctx;
	lv_disp_buf_t lv_disp_buf_top, lv_disp_buf_bot;
	lv_disp_drv_t lv_disp_drv_top, lv_disp_drv_bot;
	lv_disp_t *lv_disp_top, *lv_disp_bot;
#define BUF_TOP_PX_SZ (GSP_SCREEN_WIDTH_TOP * 16)
#define BUF_BOT_PX_SZ (GSP_SCREEN_WIDTH_BOT * 16)
	static lv_color_t fb_top[BUF_TOP_PX_SZ];
	static lv_color_t fb_bot[BUF_BOT_PX_SZ];

	(void) argc;
	(void) argv;

	ctx = init_system();

	/* Initialise LVGL. */
	lv_init();
	//lv_log_register_print_cb(print_SDL_cb);
	lv_disp_buf_init(&lv_disp_buf_top, fb_top, NULL, BUF_TOP_PX_SZ);
	lv_disp_buf_init(&lv_disp_buf_bot, fb_bot, NULL, BUF_BOT_PX_SZ);

	lv_disp_drv_init(&lv_disp_drv_top);
	lv_disp_drv_top.buffer = &lv_disp_buf_top;
	lv_disp_drv_top.flush_cb = flush_top_cb;
	lv_disp_drv_top.user_data = ctx;
	lv_disp_top = lv_disp_drv_register(&lv_disp_drv_top);

	lv_disp_drv_init(&lv_disp_drv_bot);
	lv_disp_drv_bot.buffer = &lv_disp_buf_bot;
	lv_disp_drv_bot.flush_cb = flush_bot_cb;
	lv_disp_drv_bot.user_data = ctx;
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

	{
		lv_obj_t *label1;

		lv_disp_set_default(lv_disp_top);
		label1 = lv_label_create(lv_scr_act(), NULL);
		lv_label_set_text(label1, "Top Screen");
	}

	create_bottom_ui(lv_disp_bot);

	while(LOOP_CHK())
	{
#ifdef __3DS__
		u32 input;

		hidScanInput();
		input = hidKeysDown();

		if(input & KEY_START)
			break;

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
#endif
		lv_task_handler();
		render_present(ctx);
	}

	exit_system(ctx);
	return 0;
}
