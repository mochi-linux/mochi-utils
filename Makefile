-include config.mk

SUBDIRS = adminctl sysadaemon tty

CC ?= gcc
CFLAGS ?= -Wall -O2
LDFLAGS ?=
SYSROOT ?=
PREFIX ?= /usr
DESTDIR ?=

export CC CFLAGS LDFLAGS SYSROOT PREFIX DESTDIR

.PHONY: all clean install $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@echo "===> Building $@"
	@$(MAKE) -C $@ CC="$(CC)" SYSROOT="$(SYSROOT)" PREFIX="$(PREFIX)" DESTDIR="$(DESTDIR)"

clean:
	@for dir in $(SUBDIRS); do \
		echo "===> Cleaning $$dir"; \
		$(MAKE) -C $$dir clean; \
	done

install:
	@for dir in $(SUBDIRS); do \
		echo "===> Installing $$dir"; \
		$(MAKE) -C $$dir install CC="$(CC)" SYSROOT="$(SYSROOT)" PREFIX="$(PREFIX)" DESTDIR="$(DESTDIR)"; \
	done
