CC ?= gcc
CFLAGS ?= -Wall -O2
LDFLAGS ?=
PREFIX ?= /usr
DESTDIR ?=
BINDIR ?= $(PREFIX)/bin

SRCS = src/adminctl.c src/adduser.c src/passwd.c src/login.c src/sha256.c src/accman.c
OBJS = $(SRCS:.c=.o)
TARGET = adminctl

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 4755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/adduser
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/passwd
	ln -sf $(TARGET) $(DESTDIR)$(BINDIR)/login

.PHONY: all clean install
