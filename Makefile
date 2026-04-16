-include config.mk

SUBDIRS = adminctl sysadaemon tty

CC ?= gcc
CFLAGS ?= -Wall -O2
LDFLAGS ?=
SYSROOT ?=
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
SBINDIR ?= $(PREFIX)/sbin
DESTDIR ?=
# Resolve to absolute path so relative values survive `make -C <subdir>`
ifneq ($(DESTDIR),)
override DESTDIR := $(abspath $(DESTDIR))
endif

export CC CFLAGS LDFLAGS SYSROOT PREFIX BINDIR SBINDIR DESTDIR

.PHONY: all clean install uninstall $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@echo "===> Building $@"
	@$(MAKE) -C $@ CC="$(CC)" SYSROOT="$(SYSROOT)" PREFIX="$(PREFIX)" \
		BINDIR="$(BINDIR)" SBINDIR="$(SBINDIR)" DESTDIR="$(DESTDIR)"

clean:
	@for dir in $(SUBDIRS); do \
		echo "===> Cleaning $$dir"; \
		$(MAKE) -C $$dir clean; \
	done

install:
	@for dir in $(SUBDIRS); do \
		echo "===> Installing $$dir"; \
		$(MAKE) -C $$dir install CC="$(CC)" SYSROOT="$(SYSROOT)" PREFIX="$(PREFIX)" \
			BINDIR="$(BINDIR)" SBINDIR="$(SBINDIR)" DESTDIR="$(DESTDIR)"; \
	done

uninstall:
	@for dir in $(SUBDIRS); do \
		echo "===> Uninstalling $$dir"; \
		$(MAKE) -C $$dir uninstall PREFIX="$(PREFIX)" \
			BINDIR="$(BINDIR)" SBINDIR="$(SBINDIR)" DESTDIR="$(DESTDIR)"; \
	done
