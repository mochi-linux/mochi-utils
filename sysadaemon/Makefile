CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2
LDFLAGS ?=
DESTDIR ?=

SYSDAEMON_DIR = /System/Library/CoreServices/SystemDaemon
SERVICES_DIR = /System/Library/CoreServices/Services
STARTUP_DIR = /System/Library/CoreServices/Startup

TARGET = SysDaemon
SRCS = sysdaemon.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

install: $(TARGET)
	install -d $(DESTDIR)$(SYSDAEMON_DIR)
	install -d $(DESTDIR)$(SERVICES_DIR)
	install -d $(DESTDIR)$(STARTUP_DIR)
	install -m 755 $(TARGET) $(DESTDIR)$(SYSDAEMON_DIR)/$(TARGET)

.PHONY: all clean install
