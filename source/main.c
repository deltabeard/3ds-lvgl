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

#ifndef GSP_SCREEN_WIDTH_TOP
# define GSP_SCREEN_WIDTH_TOP	240
# define GSP_SCREEN_WIDTH_BOT	240
# define GSP_SCREEN_HEIGHT_TOP	400
# define GSP_SCREEN_HEIGHT_BOT	320
#endif

#if defined(__3DS__)
# define LOOP_FUNCTION() aptMainLoop()
static int init_system(void **fb_top, void **fb_bot)
{
	gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);

	*fb_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	*fb_bot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

	return 0;
}
static int exit_system(void **fb_top, void **fb_bot)
{
	gfxExit();
	return 0;
}

#else
# define LOOP_CHK() (SDL_QuitRequested() == SDL_FALSE)
static SDL_Window *win_top, *win_bot;
static SDL_Surface *surf_top, *surf_bot;
static int init_system(void **fb_top, void **fb_bot)
{
	SDL_Init(SDL_INIT_EVERYTHING);
	win_top = SDL_CreateWindow("3DS Top Screen",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		GSP_SCREEN_WIDTH_TOP, GSP_SCREEN_HEIGHT_TOP, 0);
	win_bot = SDL_CreateWindow("3DS Bottom Screen",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		GSP_SCREEN_WIDTH_TOP, GSP_SCREEN_HEIGHT_TOP, 0);
	surf_top = SDL_GetWindowSurface(win_top);
	surf_bot = SDL_GetWindowSurface(win_bot);
	return 0;
}
static int exit_system(void **fb_top, void **fb_bot)
{
	SDL_Quit();
	return 0;
}
#endif

int main(int argc, char *argv[])
{
	void *fb_top;
	void *fb_bot;

	(void) argc;
	(void) argv;

	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
	init_system(&fb_top, &fb_bot);

	while(LOOP_CHK())
	{
		SDL_Delay(10);
#if 0
		u32 input;

		hidScanInput();
		input = hidKeysDown();

		if(input & KEY_START)
			break;

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
#endif
	}

	exit_system(&fb_top, &fb_bot);
	return 0;
}
