# ========================================================================================
# Makefile for rudi
# ========================================================================================

# ========================================================================================
# Compile flags

CC = gcc-8
COPT = -O2 -march=native -mtune=native
CFLAGS = -Wall -Wextra -std=gnu11 -D_GNU_SOURCE
CFLAGS += -D BUILD_VERSION="\"$(shell git describe --dirty --always)\""	\
		-D BUILD_DATE="\"$(shell date '+%Y-%m-%d_%H:%M:%S')\""

BIN = rpi-toggle-web

# ========================================================================================
# Source files

SRCDIR = src

SRC = $(SRCDIR)/main.c \
		$(SRCDIR)/gpio.c

# ========================================================================================
# External Libraries

PKGCONFIGDIR = libwebsockets/build/
LIBSDIR = libwebsockets/build/include
OBSDIR = libwebsockets/build/lib

LIBS = -lm -pthread -Wl,-Bstatic -lwebsockets -Wl,-Bdynamic $(GSTFLAGS)

CFLAGS += 

# ========================================================================================
# Makerules

all:
	PKG_CONFIG_PATH=$(PKGCONFIGDIR) pkg-config --modversion "libwebsockets = 3.1.0"
	$(CC) $(COPT) $(CFLAGS) $(SRC) -o $(BIN) -I $(LIBSDIR) -L $(OBSDIR) $(LIBS)

debug: COPT = -Og -ggdb -fno-omit-frame-pointer -D__DEBUG
debug: all

clean:
	rm -fv $(BIN)
