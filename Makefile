CC ?= $(CROSS_COMPILE)gcc

DAEMON = time-manager
DAEMON_OBJ  =  time-utils.o $(DAEMON).o
INSTALLED_HEADERS = time-utils.h

CFLAGS += -Wall -Wno-unused-result

INC_FLAG += $(shell pkg-config --cflags --libs libsystemd) -I. -O2
LIB_FLAG += $(shell pkg-config  --libs libsystemd)

DESTDIR ?= /
SBINDIR ?= /usr/sbin
INCLUDEDIR ?= /usr/include
LIBDIR ?= /usr/lib

all: $(DAEMON)

%.o: %.c
	$(CC) -fpic -c $< $(CFLAGS) $(INC_FLAG) -o $@

$(DAEMON): $(DAEMON_OBJ)
	$(CXX) $^ $(LDFLAGS) $(LIB_FLAG) -o $@

clean:
	rm -f $(DAEMON) *.o

install:
		install -m 0755 -d $(DESTDIR)$(SBINDIR)
		install -m 0755 time-manager $(DESTDIR)$(SBINDIR)
		install -m 0755 -d $(DESTDIR)$(INCLUDEDIR)
		install -m 0644 $(INSTALLED_HEADERS) $(DESTDIR)$(INCLUDEDIR)
