CC     = cc
CFLAGS = -O2 -Wall -I/usr/X11/include
LDLIBS = -lX11 -L/usr/X11/lib

all: x11bench

x11bench: x11bench.c
	$(CC) $(CFLAGS) -o x11bench x11bench.c $(LDLIBS)

# Build with -std=c89 -pedantic to make sure the source stays old-school.
check: x11bench.c
	$(CC) -std=c89 -pedantic -Wall -c x11bench.c && rm -f x11bench.o

clean:
	rm -f x11bench x11bench.o

.PHONY: all check clean
