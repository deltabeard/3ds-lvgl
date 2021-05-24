/**
 * Copyright (c) 2021 Mahyar Koshkouei
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 * THIS SOFTWARE IS PROVIDED 'AS-IS', WITHOUT ANY EXPRESS OR IMPLIED WARRANTY.
 * IN NO EVENT WILL THE AUTHORS BE HELD LIABLE FOR ANY DAMAGES ARISING FROM THE
 * USE OF THIS SOFTWARE.
 */

#include <lvgl.h>
#include <platform.h>

#if defined(__3DS__)
# include <3ds.h>

int exit_requested(void *ctx)
{
	(void) ctx;
	return !aptMainLoop();
}

void *init_system(void)
{
	void *c = NULL;
	u16 width, height;

	/* This stops scandir() from working. */
	// consoleDebugInit(debugDevice_SVC);

	gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);

	c = c - 1;

out:
	return c;
}

void handle_events(void *ctx)
{
	u32 kDown;

	/* Scan all the inputs. */
	hidScanInput();
	kDown = hidKeysDown();

	return;
}

void render_present(void *ctx)
{
	gfxFlushBuffers();
	gfxScreenSwapBuffers(GFX_TOP, false);
	gfxScreenSwapBuffers(GFX_BOTTOM, false);

	/* Wait for VBlank */
	gspWaitForVBlank();
}

static void draw_pixels(lv_color_t *restrict dst, const lv_color_t *restrict src, const lv_area_t *area, u16 w)
{
	for (lv_coord_t y = area->y1; y <= area->y2; y++)
	{
		lv_color_t *dst_p = dst + (w * y) + area->x1;
		size_t len = (area->x2 - area->x1) + 1;
		size_t sz = len * sizeof(lv_color_t);

		memcpy(dst_p, src, sz);
		src += len;
	}
}

void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct system_ctx *c = disp_drv->user_data;
	u16 w;
	lv_color_t *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, NULL);
	draw_pixels(fb, color_p, area, w);
	lv_disp_flush_ready(disp_drv);
}

void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct system_ctx *c = disp_drv->user_data;
	u16 w;
	lv_color_t *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, NULL);
	draw_pixels(fb, color_p, area, w);
	lv_disp_flush_ready(disp_drv);
}

bool read_pointer(struct _lv_indev_drv_t *indev_drv,
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

void exit_system(void *ctx)
{
	gfxExit();
	ctx = NULL;
	return;
}

#else
#include <SDL.h>

int exit_requested(void *ctx)
{
	(void) ctx;
	return SDL_QuitRequested();
}

struct system_ctx
{
	SDL_Window *win_top;
	SDL_Window *win_bot;
};

void *init_system(void)
{
	struct system_ctx *c;
	int win_top_x, win_top_y, win_top_border;

	c = SDL_calloc(1, sizeof(struct system_ctx));
	SDL_assert_always(c != NULL);

	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
	SDL_Init(SDL_INIT_EVERYTHING);

	c->win_top = SDL_CreateWindow("3DS Top Screen",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		NATURAL_SCREEN_WIDTH_TOP, NATURAL_SCREEN_HEIGHT_TOP, 0);
	SDL_assert_always(c->win_top != NULL);

	/* Align the bottom window to the top window. */
	SDL_GetWindowPosition(c->win_top, &win_top_x, &win_top_y);
	SDL_GetWindowBordersSize(c->win_top, &win_top_border, NULL, NULL, NULL);
	win_top_x += (GSP_SCREEN_HEIGHT_TOP - GSP_SCREEN_HEIGHT_BOT) / 2;
	win_top_y += GSP_SCREEN_WIDTH_TOP + win_top_border;

	c->win_bot = SDL_CreateWindow("3DS Bottom Screen", win_top_x, win_top_y,
				      NATURAL_SCREEN_WIDTH_BOT,
				      NATURAL_SCREEN_HEIGHT_BOT, 0);
	SDL_assert_always(c->win_bot != NULL);

	return c;
}

void handle_events(void *ctx)
{
	SDL_Event e;

	while (SDL_PollEvent(&e))
	{
	}
	return;
}

void render_present(void *ctx)
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

void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct system_ctx *c = disp_drv->user_data;
	SDL_Surface *surf_top = SDL_GetWindowSurface(c->win_top);
	flush_cb(surf_top, area, color_p);
	lv_disp_flush_ready(disp_drv);
}

void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct system_ctx *c = disp_drv->user_data;
	SDL_Surface *surf_bot = SDL_GetWindowSurface(c->win_bot);
	flush_cb(surf_bot, area, color_p);
	lv_disp_flush_ready(disp_drv);
}

bool read_pointer(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
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
void exit_system(void *ctx)
{
	struct system_ctx *c = ctx;

	SDL_DestroyWindow(c->win_top);
	SDL_DestroyWindow(c->win_bot);
	SDL_Quit();

	return;
}

#endif
