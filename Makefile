
APPNAME ?= gravblur
SRCS    = src
SRCNAME ?= $(SRCS)/main.c

$(APPNAME): $(SRCS)/bitmap.h $(SRCS)/gravityblur.h $(SRCS)/pngio.h $(SRCS)/jpegio.h

MFLAGS := -march=native
CFLAGS := -Wall -Wextra -pedantic -O3 $(MFLAGS)
LIBS := -ljpeg -lpng -lm

.PHONY: clean all

all: $(APPNAME)

clean:
	rm -f $(APPNAME)

$(APPNAME): $(SRCNAME)
	$(CC) -DAPPNAME=$(APPNAME) $(CFLAGS) -s -o $@ $< $(LIBS)

