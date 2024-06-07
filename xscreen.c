#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <png.h>
#include <webp/encode.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <webp/types.h>


enum {
	ROOT,
	FOCUSED,
	RECTANGLE
};

enum {
	PNG,
	WEBP
};

struct screenshot {
	char *filename;
	int width;
	int height;
	int format;
	int quality;
	int window_type;
	bool freeze;
	bool stdoutput;
	int delay;
	unsigned char *data;
};


static Display *disp;


static int
strend(const char *s, const char *t)
{
	size_t ns = strlen(s), nt = strlen(t);
	return nt <= ns && strcmp(s + ns - nt, t) == 0;
}

static void
display_help(void)
{
	printf("Usage: By default, takes a screenshot of the root window\n "
	    "      and saves it in a PNG file in the current directory.\n\n"
	    "    -u           Focused window.\n"
	    "    -s           Rectangle selection.\n"
	    "    -z           Freeze display during rectangle selection.\n"
	    "    -w [quality] Save to a WebP file, quality ranging from 0 to 100 (lossless).\n"
	    "    -d [delay]   Delay in seconds.\n"
	    "    -t           Print to standard output.\n"
	    "    -f [path]    Path + optional filename location.\n");
	exit(EXIT_SUCCESS);
}

static void
check_args(int argc, char **argv, struct screenshot *shot)
{
	int opt;
	while ((opt = getopt(argc, argv, "f:d:w:huszt")) != -1) {
		switch (opt) {
		case 'f':
			shot->filename = malloc(strlen(optarg) + 1);
			if (shot->filename == NULL)
				errx(1, "malloc failure");
			strcpy(shot->filename, optarg);
			break;
		case 'd':
			shot->delay = strtol(optarg, NULL, 0);
			if (shot->delay < 0)
				shot->delay = 0;
			break;
		case 'h':
			display_help();
			break;
		case 'u':
			shot->window_type = FOCUSED;
			break;
		case 's':
			shot->window_type = RECTANGLE;
			break;
		case 'z':
			shot->freeze = true;
			break;
		case 't':
			shot->stdoutput = true;
			break;
		case 'w':
			shot->format = WEBP;
			shot->quality = strtol(optarg, NULL, 0);
			if (shot->quality < 0)
				shot->quality = 0;
			else if (shot->quality > 100)
				shot->quality = 100;
			break;
		}
	}
}

static char *
make_default_filename(int format)
{
	size_t len;
	time_t cur_time = time(NULL);
	struct tm *date = localtime(&cur_time);

	/* get length of formatted string before malloc */
	len = snprintf(NULL, 0, "screenshot_%d-%02d-%02d-%02d-%02d-%02d",
	    date->tm_year + 1900, date->tm_mon + 1, date->tm_mday,
	    date->tm_hour, date->tm_min, date->tm_sec);

	char *filename = malloc(len + 5);
	if (filename == NULL)
		errx(1, "malloc failure");

	sprintf(filename, "screenshot_%d-%02d-%02d-%02d-%02d-%02d",
	    date->tm_year + 1900, date->tm_mon + 1, date->tm_mday,
	    date->tm_hour, date->tm_min, date->tm_sec);

	if (format == PNG)
		strcat(filename, ".png");
	else if (format == WEBP)
		strcat(filename, ".webp");

	return filename;
}

static Window
get_root_window(void)
{
	return DefaultRootWindow(disp);
}

static Window
get_toplevel_parent(Window win)
{
	Window parent, root, *children;
	unsigned int num_children;
	int status;

	while (1) {
		status = XQueryTree(disp, win, &root, &parent,
			&children, &num_children);
		if (status == 0)
			errx(1, "XQueryTree error");
		if (children)
			XFree(children);
		if (win == root || parent == root)
			return win;
		else
			win = parent;
	}
}

static Window
get_focused_window(void)
{
	Window focused_win;
	int revert_to;

	XGetInputFocus(disp, &focused_win, &revert_to);
	if (focused_win == None)
		errx(1, "No window is being focused.");
	focused_win = get_toplevel_parent(focused_win);

	return focused_win;
}

static Window
get_selected_rectangle(bool freeze)
{
	int x = 0, y = 0, rx = 0, ry = 0;
	int width = 0, height = 0, rw = 0, rh = 0;
	XEvent e;
	XGCValues gcval;
	Window root = DefaultRootWindow(disp);

	XGrabPointer(disp, root, False,
	    ButtonMotionMask | ButtonPressMask | ButtonReleaseMask,
	    GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

	gcval.foreground = XWhitePixel(disp, 0);
	gcval.function = GXxor;
	gcval.background = XBlackPixel(disp, 0);
	gcval.plane_mask = gcval.background ^ gcval.foreground;
	gcval.subwindow_mode = IncludeInferiors;

	GC gc = XCreateGC(disp, root,
	    GCFunction | GCForeground | GCBackground | GCSubwindowMode, &gcval);

	while (1) {
		XNextEvent(disp, &e);

		switch (e.type) {
		case ButtonPress:
			x = e.xbutton.x;
			y = e.xbutton.y;
			if (freeze)
				XGrabServer(disp);
			break;
		case ButtonRelease:
			width = e.xbutton.x - x;
			height = e.xbutton.y - y;

			if (width < 0) {
				width *= -1;
				x -= width;
			}
			if (height < 0) {
				height *= -1;
				y -= height;
			}
			break;
		case MotionNotify:
			XDrawRectangle(disp, root, gc, rx, ry, rw, rh);

			rx = x - 1;
			ry = y - 1;
			rw = e.xmotion.x - rx + 1;
			rh = e.xmotion.y - ry + 1;

			if (rw < 0) {
				rx += rw;
				rw = 0 - rw;
			}
			if (rh < 0) {
				ry += rh;
				rh = 0 - rh;
			}

			XDrawRectangle(disp, root, gc, rx, ry, rw, rh);
			XFlush(disp);
			break;
		}

		if (e.type == ButtonRelease)
			break;
	}

	XDrawRectangle(disp, root, gc, rx, ry, rw, rh);
	if (freeze)
		XUngrabServer(disp);
	XUngrabPointer(disp, CurrentTime);
	XFreeGC(disp, gc);
	XFlush(disp);

	return XCreateSimpleWindow(disp, root, x, y, width, height, 0, 0, 0);
}

static void
delay(int delay_sec)
{
	int i;
	for (i = delay_sec; i > 0; i--) {
		printf("%d... ", i);
		fflush(stdout);
		sleep(1);
	}
	if (delay_sec)
		printf("\n");
}

static void
capture(Window win, struct screenshot *shot)
{
	int sr, sg;
	XWindowAttributes attr;
	XImage *ximg;

	XGetWindowAttributes(disp, win, &attr);

	shot->width = attr.width;
	shot->height = attr.height;

	if (shot->window_type == RECTANGLE) {
		ximg = XGetImage(disp, DefaultRootWindow(disp), attr.x, attr.y,
		    shot->width, shot->height, AllPlanes, ZPixmap);
	} else {
		ximg = XGetImage(disp, win, 0, 0, shot->width, shot->height,
		    AllPlanes, ZPixmap);
	}

	shot->data = malloc(shot->width * shot->height * 3);
	if (shot->data == NULL)
		errx(1, "malloc failure");

	switch (ximg->bits_per_pixel) {
	case 16:
		sr = 11;
		sg = 5;
		break;
	case 24:
	case 32:
		sr = 16;
		sg = 8;
		break;
	default:
		errx(1, "unsupported bpp: %d", ximg->bits_per_pixel);
	}

	for (int x = 0; x < shot->width; x++) {
		for (int y = 0; y < shot->height ; y++) {
			unsigned long xpix = XGetPixel(ximg, x, y);
			size_t pix_pos = (x + shot->width * y) * 3;

			shot->data[pix_pos] = (xpix & ximg->red_mask) >> sr;
			shot->data[pix_pos+1] = (xpix & ximg->green_mask) >> sg;
			shot->data[pix_pos+2] = xpix & ximg->blue_mask;
		}
	}

	XDestroyImage(ximg);
}

static void
save_png(struct screenshot shot)
{
	png_bytep row_pointers[shot.height];
	png_structp png;
	FILE *fp;

	if (!shot.stdoutput) {
		fp = fopen(shot.filename, "wb");
		if (!fp) abort();
	} else {
		fp = stdout;
	}

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) abort();

	png_infop info = png_create_info_struct(png);
	if (!info) abort();

	if (setjmp(png_jmpbuf(png))) abort();

	png_init_io(png, fp);

	png_set_IHDR(
		png,
		info,
		shot.width, shot.height,
		8,
		PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);
	png_write_info(png, info);

	for (int y = 0; y < shot.height; y++) {
		row_pointers[y] = (shot.data + y * shot.width * 3);
	}
	png_write_image(png, row_pointers);
	png_write_end(png, NULL);

	if (!shot.stdoutput)
		fclose(fp);

	png_destroy_write_struct(&png, &info);
}

static void
save_webp(struct screenshot shot)
{
	size_t len;
	uint8_t* output;
	FILE *fp;

	if (!shot.stdoutput) {
		fp = fopen(shot.filename, "wb");
		if (!fp) abort();
	} else {
		fp = stdout;
	}

	if (shot.quality == 100) {
		len = WebPEncodeLosslessRGB(shot.data, shot.width, shot.height,
		    shot.width * 3, &output);
	} else {
		len = WebPEncodeRGB(shot.data, shot.width, shot.height, shot.width * 3,
		    shot.quality, &output);
	}

	fwrite(output, sizeof output[0], len, fp);
	if (!shot.stdoutput)
		fclose(fp);

	WebPFree(output);
}

static void
save_image(struct screenshot shot)
{
	if (shot.format == PNG)
		save_png(shot);
	else if (shot.format == WEBP)
		save_webp(shot);
}

int
main(int argc, char **argv)
{
	disp = XOpenDisplay(NULL);
	if (disp == NULL)
		errx(1, "Failed opening DISPLAY.");
	Window win;

	struct screenshot shot;
	shot.format = PNG;
	shot.quality = 100;
	shot.window_type = ROOT;
	shot.delay = 0;
	shot.freeze = false;
	shot.stdoutput = false;
	shot.filename = NULL;

	check_args(argc, argv, &shot);

	if (!shot.stdoutput) {
		if (shot.filename == NULL) {
			shot.filename = make_default_filename(shot.format);
		} else if (strend(shot.filename, "/")) {
			char *tmpdefault = make_default_filename(shot.format);
			char *tmpalloc = realloc(shot.filename,
				strlen(shot.filename) + strlen(tmpdefault) + 1);
			if (tmpalloc)
				shot.filename = tmpalloc;
			else
				errx(1, "failure during memory reallocation");
			strcat(shot.filename, tmpdefault);
			free(tmpdefault);
		}
	}

	if (shot.window_type == RECTANGLE) {
		win = get_selected_rectangle(shot.freeze);
		delay(shot.delay);
	} else if (shot.window_type == FOCUSED) {
		delay(shot.delay);
		win = get_focused_window();
	} else {
		delay(shot.delay);
		win = get_root_window();
	}

	capture(win, &shot);
	save_image(shot);

	free(shot.filename);
	free(shot.data);
	XCloseDisplay(disp);

	return EXIT_SUCCESS;
}
