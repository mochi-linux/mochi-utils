CC ?= gcc
CFLAGS ?= -Wall -O2
LDFLAGS ?=
PREFIX ?= /usr
DESTDIR ?=
BINDIR ?= $(PREFIX)/bin
SBINDIR ?= $(PREFIX)/sbin

SRCS = src/networkctl.c src/netif.c src/route.c src/ping.c \
       src/hostname.c src/netstat.c src/dns.c
OBJS = $(SRCS:.c=.o)
TARGET = networkctl

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/ifconfig
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/route
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/ping
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/hostname
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/netstat
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/dns

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(BINDIR)/ifconfig
	rm -f $(DESTDIR)$(BINDIR)/route
	rm -f $(DESTDIR)$(BINDIR)/ping
	rm -f $(DESTDIR)$(BINDIR)/hostname
	rm -f $(DESTDIR)$(BINDIR)/netstat
	rm -f $(DESTDIR)$(BINDIR)/dns

.PHONY: all clean install uninstall
