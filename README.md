# x11bench

A small benchmark of X11 drawing performance for old and new X servers.

Each test issues 1000 drawing operations to the server, waits for it to
finish (`XSync`), and prints the total time, the per-operation time and
the operations-per-second rate on the console:

| test | operation |
|------|-----------|
| line       | `XDrawLine` across the full window |
| rectangle  | `XFillRectangle`, half the window |
| circle     | `XFillArc` (full circle) inscribed in the window |
| bit blit   | `XCopyArea` of a checkerboard pixmap, full window |

The checkerboard pattern stops the server optimising the blit into a
solid fill.

Written in C89 using only X11R5 functionality, and it draws at the
native bit depth of the default screen, so it runs on old
workstations.

## Build

    make        # build x11bench
    make check  # verify the source is still C89-clean (-std=c89 -pedantic)

## Usage

    x11bench [-d display] [-w width] [-h height]

Defaults: display from `$DISPLAY`, 512x512 test window (64–4096).

## Notes

- X11 requests are asynchronous; the timer starts before the 1000 calls
  and stops after `XSync`, so the number is real server-side drawing
  time, not just the cost of queueing protocol packets.
- A window the size of the test is opened so you can watch the drawing.
