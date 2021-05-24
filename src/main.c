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

#ifdef _MSC_VER
#include <direct.h>
#include <dirent_port.h>
#define chdir _chdir
#else
#include <dirent.h>
#include <unistd.h>
#endif

 #include <noto_sans_14_common.h>

#include <errno.h>
#include <lvgl.h>
#include <platform.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_PX_SIZE (GSP_SCREEN_WIDTH_TOP * 64)
static bool quit = false;

static void show_error_msg(const char *msg, lv_disp_t *disp);
static void recreate_filepicker(void *p);

#define print_fatal(x)                                                         \
	fprintf(stderr, "FATAL: %s+%d: %s - %s\n", __func__, __LINE__, x,      \
		strerror(errno))
#define print_debug(x)                                                         \
	fprintf(stderr, "DEBUG: %s+%d: %s\n", __func__, __LINE__, x)

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
	lv_obj_set_state(mbox1, LV_STATE_DISABLED);
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
#ifdef _MSC_VER
	char buf[] = "C:\\";
#else
	char buf[32];
#endif

	list = lv_obj_get_user_data(btn);
	disp = lv_obj_get_user_data(list);

	if (event != LV_EVENT_CLICKED)
		return;

	/* Check if we are in the filesystem root directory. */
	if(getcwd(buf, sizeof(buf)) != NULL &&
#ifdef _MSC_VER
			buf[2] == '\\'
#else
			strcmp(buf, "/") == 0
#endif
		)
	{
		/* If already in the root directory, don't go up a directory. */
		return;
	}

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
		char err_txt[512] = "";
		char buf[PATH_MAX];

		snprintf(err_txt, sizeof(err_txt),
			 "Unable to scan '%s':\n%s",
			 getcwd(buf, sizeof(buf)) != NULL ? buf : "directory",
			 strerror(errno));

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
		else if(namelist[e]->d_type == DT_LNK)
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
		lv_label_set_long_mode(label, LV_LABEL_LONG_CROP);
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
	const char *lorem_ipsum = "بنی‌آدم اعضای یک پیکرند\n"
		"که در آفرينش ز یک گوهرند\n"
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
	lv_label_set_long_mode(label1, LV_LABEL_LONG_BREAK);
	lv_cont_set_fit(top_cont, LV_FIT_NONE);
	lv_obj_set_size(top_cont, hor, ver);
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

		lv_obj_set_state(toolbar, LV_STATE_DISABLED);

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
	static lv_color_t top_buf[BUF_PX_SIZE];
	static lv_color_t bot_buf[BUF_PX_SIZE];

	(void)argc;
	(void)argv;

	ctx = init_system();
	if (ctx == NULL)
	{
		print_fatal("Unable to initialise system");
		goto err;
	}

	print_debug("System initialised");

	/* Initialise LVGL. */
	lv_init();
	lv_log_register_print_cb(print_cb);

	lv_disp_buf_init(&lv_disp_buf_top, top_buf, NULL, BUF_PX_SIZE);
	lv_disp_buf_init(&lv_disp_buf_bot, bot_buf, NULL, BUF_PX_SIZE);

	lv_disp_drv_init(&lv_disp_drv_top);
	lv_disp_drv_top.buffer = &lv_disp_buf_top;
	lv_disp_drv_top.flush_cb = flush_top_cb;
	lv_disp_drv_top.user_data = ctx;
#ifdef __3DS__
	lv_disp_drv_top.rotated = LV_DISP_ROT_270;
	lv_disp_drv_top.sw_rotate = 1;
	lv_disp_drv_top.hor_res = GSP_SCREEN_WIDTH_TOP;
	lv_disp_drv_top.ver_res = GSP_SCREEN_HEIGHT_TOP;
#else
	lv_disp_drv_top.rotated = 0;
	lv_disp_drv_top.sw_rotate = 0;
	lv_disp_drv_top.hor_res = NATURAL_SCREEN_WIDTH_TOP;
	lv_disp_drv_top.ver_res = NATURAL_SCREEN_HEIGHT_TOP;
#endif
	lv_disp_top = lv_disp_drv_register(&lv_disp_drv_top);

	lv_disp_drv_init(&lv_disp_drv_bot);
	lv_disp_drv_bot.buffer = &lv_disp_buf_bot;
	lv_disp_drv_bot.flush_cb = flush_bot_cb;
	lv_disp_drv_bot.user_data = ctx;
#ifdef __3DS__
	lv_disp_drv_bot.rotated = LV_DISP_ROT_270;
	lv_disp_drv_bot.sw_rotate = 1;
	lv_disp_drv_bot.hor_res = GSP_SCREEN_WIDTH_BOT;
	lv_disp_drv_bot.ver_res = GSP_SCREEN_HEIGHT_BOT;
#else
	lv_disp_drv_bot.rotated = 0;
	lv_disp_drv_bot.sw_rotate = 0;
	lv_disp_drv_bot.hor_res = NATURAL_SCREEN_WIDTH_BOT;
	lv_disp_drv_bot.ver_res = NATURAL_SCREEN_HEIGHT_BOT;
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

	while (exit_requested(ctx) == 0 && quit == false)
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
