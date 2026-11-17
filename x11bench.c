/*
 * x11bench - a small benchmark of X11 drawing performance.
 *
 * Measures 1000 timed operations each for four basic tests:
 * line drawing, filled rectangles, filled circles and bit blits.
 *
 * The program is written in C89 and uses only X11R5 functionality,
 * so it should build and run on old workstations.  All drawing is
 * done at the native (default) bit depth of the display.
 *
 * usage: x11bench [-d display] [-w width] [-h height]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <X11/Xlib.h>

#define NUM_OPS        1000000
#define DEFAULT_WIDTH  512
#define DEFAULT_HEIGHT 512
#define MIN_SIZE       64
#define MAX_SIZE       4096
#define CHECKER_SQ     16   /* checkerboard square size in the blit source */

/* Random state for LCG */
static unsigned int random_state = 0x12345678ULL;

/* C89-compatible random number generator (LCG) */
static unsigned int my_random(void)
{
    random_state = (random_state * 1103515245ULL + 12345ULL) & 0x7fffffffULL;
    return random_state;
}

/* Random integer in range [min, max] inclusive */
static int random_range(int min, int max)
{
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    return min + (my_random() % (max - min + 1));
}

struct test_ops {
    const char *name;
    void (*op)(Display *dpy, Window win, GC gc, Window src,
               int width, int height);
};

static long elapsed_us(const struct timeval *start, const struct timeval *end)
{
    long sec = (long) (end->tv_sec - start->tv_sec);
    long usec = (long) (end->tv_usec - start->tv_usec);

    if (usec < 0) {
        usec += 1000000L;
        sec -= 1;
    }
    return sec * 1000000L + usec;
}

static void op_line(Display *dpy, Window win, GC gc, Window src,
                    int width, int height)
{
    (void) src;
    int x1 = random_range(0, width - 1);
    int y1 = random_range(0, height - 1);
    int x2 = random_range(x1 + 1, width);
    int y2 = random_range(y1 + 1, height);
    XDrawLine(dpy, win, gc, x1, y1, x2, y2);
}

static void op_rect(Display *dpy, Window win, GC gc, Window src,
                    int width, int height)
{
    (void) src;
    int x = random_range(0, width - 1);
    int y = random_range(0, height - 1);
    int w = random_range(MIN_SIZE, width / 2);
    int h = random_range(MIN_SIZE, height / 2);
    XFillRectangle(dpy, win, gc, x, y, w, h);
}

static void op_arc(Display *dpy, Window win, GC gc, Window src,
                   int width, int height)
{
    int d = random_range(MIN_SIZE, (width < height ? width : height) / 2);
    int x = random_range(0, width - 2 * d);
    int y = random_range(0, height - 2 * d);

    (void) src;
    XFillArc(dpy, win, gc, x, y, 2 * d, 2 * d, 0, 360 * 64);
}

static void op_blit(Display *dpy, Window win, GC gc, Window src,
                    int width, int height)
{
    int src_x = random_range(0, width - 1);
    int src_y = random_range(0, height - 1);
    int src_w = random_range(MIN_SIZE, width / 2);
    int src_h = random_range(MIN_SIZE, height / 2);
    int dest_x = random_range(0, width - src_w);
    int dest_y = random_range(0, height - src_h);
    XCopyArea(dpy, src, win, gc, src_x, src_y, src_w, src_h, dest_x, dest_y);
}

static const struct test_ops tests[] = {
    { "line (XDrawLine)",      op_line },
    { "rectangle (XFillRect)", op_rect },
    { "circle (XFillArc)",     op_arc  },
    { "bit blit (XCopyArea)",  op_blit }
};

#define NUM_TESTS ((int) (sizeof(tests) / sizeof(tests[0])))

static void usage(void)
{
    fprintf(stderr, "usage: x11bench [-d display] [-w width] [-h height]\n");
}

int main(int argc, char **argv)
{
    const char *dpy_name = 0;
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    Display *dpy;
    int screen;
    int depth;
    XSetWindowAttributes attrs;
    Window win;
    GC gc;
    Window src;
    int i, j;
    int cx, cy;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
            dpy_name = argv[++i];
        else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc)
            width = atoi(argv[++i]);
        else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc)
            height = atoi(argv[++i]);
        else {
            usage();
            return EXIT_FAILURE;
        }
    }

    if (width < MIN_SIZE || width > MAX_SIZE ||
        height < MIN_SIZE || height > MAX_SIZE) {
        fprintf(stderr, "x11bench: size must be between %d and %d\n",
                MIN_SIZE, MAX_SIZE);
        return EXIT_FAILURE;
    }

    dpy = XOpenDisplay(dpy_name);
    if (dpy == 0) {
        fprintf(stderr, "x11bench: cannot open display \"%s\"\n",
                XDisplayName(dpy_name));
        return EXIT_FAILURE;
    }
    screen = DefaultScreen(dpy);
    depth = DefaultDepth(dpy, screen);

    attrs.background_pixel = BlackPixel(dpy, screen);
    attrs.border_pixel = attrs.background_pixel;
    win = XCreateWindow(dpy, RootWindow(dpy, screen), 0, 0,
                        width, height, 0, depth, InputOutput,
                        DefaultVisual(dpy, screen),
                        CWBackPixel | CWBorderPixel, &attrs);
    XMapWindow(dpy, win);

    gc = XCreateGC(dpy, win, 0, 0);
    XSetForeground(dpy, gc, WhitePixel(dpy, screen));

    /* Offscreen blit source, filled with a checkerboard so the server
     * cannot optimise the copy away as a solid fill. */
    src = XCreatePixmap(dpy, win, width, height, depth);
    for (cy = 0; cy < height; cy += CHECKER_SQ)
        for (cx = 0; cx < width; cx += CHECKER_SQ) {
            if (((cx / CHECKER_SQ) + (cy / CHECKER_SQ)) & 1)
                XSetForeground(dpy, gc, WhitePixel(dpy, screen));
            else
                XSetForeground(dpy, gc, BlackPixel(dpy, screen));
            XFillRectangle(dpy, src, gc, cx, cy, CHECKER_SQ, CHECKER_SQ);
        }
    XSetForeground(dpy, gc, WhitePixel(dpy, screen));

    printf("x11bench: %s screen %d, %d bits/pixel\n",
           XDisplayName(dpy_name), screen, depth);
    printf("%d operations per test, %dx%d test window\n\n",
           NUM_OPS, width, height);
    printf("%-20s %12s %12s %12s\n",
           "test", "total (ms)", "per-op (us)", "rate (ops/s)");

    for (i = 0; i < NUM_TESTS; i++) {
        struct timeval t0;
        struct timeval t1;
        long total_us;

        XClearWindow(dpy, win);
        XSync(dpy, False);
        gettimeofday(&t0, 0);
        for (j = 0; j < NUM_OPS; j++)
            tests[i].op(dpy, win, gc, src, width, height);
        /* The calls above only queue requests; wait for the server
         * to finish drawing before stopping the clock. */
        XSync(dpy, False);
        gettimeofday(&t1, 0);

        total_us = elapsed_us(&t0, &t1);
        if (total_us <= 0)
            total_us = 1;
        printf("%-20s %12.3f %12.2f %12.0f\n",
               tests[i].name,
               total_us / 1000.0,
               total_us / (double) NUM_OPS,
               (double) NUM_OPS * 1000000.0 / (double) total_us);
    }

    XFreePixmap(dpy, src);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return EXIT_SUCCESS;
}
