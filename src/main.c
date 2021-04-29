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
# include <SDL.h>
#endif

#include <lvgl.h>

#ifndef GSP_SCREEN_WIDTH_TOP
# define GSP_SCREEN_WIDTH_TOP	240
# define GSP_SCREEN_WIDTH_BOT	240
# define GSP_SCREEN_HEIGHT_TOP	400
# define GSP_SCREEN_HEIGHT_BOT	320
#endif

#define SCREEN_PIXELS_TOP (GSP_SCREEN_WIDTH_TOP * GSP_SCREEN_HEIGHT_TOP)
#define SCREEN_PIXELS_BOT (GSP_SCREEN_WIDTH_BOT * GSP_SCREEN_HEIGHT_BOT)

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
static int exit_system(void *ctx, void **fb_top, void **fb_bot)
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

static void *init_system(void **fb_top, void **fb_bot)
{
	struct system_ctx *c;

	c = SDL_malloc(sizeof(struct system_ctx));
	if(c == NULL)
		goto out;

	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
	SDL_Init(SDL_INIT_EVERYTHING);

	c->win_top = SDL_CreateWindow("3DS Top Screen",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		GSP_SCREEN_WIDTH_TOP, GSP_SCREEN_HEIGHT_TOP, 0);
	c->win_bot = SDL_CreateWindow("3DS Bottom Screen",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		GSP_SCREEN_WIDTH_TOP, GSP_SCREEN_HEIGHT_TOP, 0);
	c->surf_top = SDL_GetWindowSurface(c->win_top);
	c->surf_bot = SDL_GetWindowSurface(c->win_bot);

	/* If windows or surfaces couldn't be created, free the context and
	 * indicate an error by returning NULL. */
	if(c->win_top == NULL || c->win_bot == NULL ||
		c->surf_top == NULL || c->surf_bot == NULL)
	{
		SDL_free(c);
		c = NULL;
	}

out:
	return c;
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
void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
		lv_color_t *color_p)
{
	int32_t x, y;
	struct system_ctx *c = disp_drv->user_data;
	lv_color_t *p = c->surf_top->pixels;

	for(y = area->y1; y <= area->y2; y++)
	{
		for(x = area->x1; x <= area->x2; x++)
		{
			p[y*x] = *color_p;
			color_p++;
		}
	}

	lv_disp_flush_ready(disp_drv);
	return;
}
void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
		lv_color_t *color_p)
{
	int32_t x, y;
	struct system_ctx *c = disp_drv->user_data;
	lv_color_t *p = c->surf_bot->pixels;

	for(y = area->y1; y <= area->y2; y++)
	{
		for(x = area->x1; x <= area->x2; x++)
		{
			p[y*x] = *color_p;
			color_p++;
		}
	}

	lv_disp_flush_ready(disp_drv);
	return;
}
static int exit_system(void *ctx, void **fb_top, void **fb_bot)
{
	struct system_ctx *c = ctx;

	(void) fb_top;
	(void) fb_bot;

	SDL_DestroyWindow(c->win_top);
	SDL_DestroyWindow(c->win_bot);
	SDL_Quit();

	return 0;
}
#endif

int main(int argc, char *argv[])
{
	void *ctx;
	lv_disp_buf_t lv_disp_buf_top, lv_disp_buf_bot;
	lv_disp_drv_t lv_disp_drv_top, lv_disp_drv_bot;
	lv_disp_t *lv_disp_top, *lv_disp_bot;
	void *fb_top, *fb_bot;

	(void) argc;
	(void) argv;

	ctx = init_system(&fb_top, &fb_bot);

	/* Initialise LVGL. */
	lv_init();
	lv_disp_buf_init(&lv_disp_buf_top, fb_top, NULL, SCREEN_PIXELS_TOP);
	lv_disp_buf_init(&lv_disp_buf_bot, fb_bot, NULL, SCREEN_PIXELS_BOT);

	lv_disp_drv_init(&lv_disp_drv_top);
	lv_disp_drv_top.buffer = &lv_disp_buf_top;
	lv_disp_drv_top.flush_cb = flush_top_cb;
	lv_disp_drv_top.user_data = ctx;
	lv_disp_top = lv_disp_drv_register(&lv_disp_drv_top);

	lv_disp_drv_init(&lv_disp_drv_bot);
	lv_disp_drv_bot.buffer = &lv_disp_buf_bot;
	lv_disp_drv_bot.flush_cb = flush_bot_cb;
	lv_disp_drv_bot.user_data = ctx;
	lv_disp_top = lv_disp_drv_register(&lv_disp_drv_bot);

	lv_disp_drv_init(&lv_disp_drv_top);
	lv_disp_drv_init(&lv_disp_drv_bot);

	lv_obj_create(NULL, NULL);

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
#else
		struct system_ctx *c = ctx;
		SDL_UpdateWindowSurface(c->win_top);
		SDL_UpdateWindowSurface(c->win_bot);
		SDL_Delay(10);
#endif
	}

	exit_system(ctx, &fb_top, &fb_bot);
	return 0;
}
