.POSIX:

VERSION = 0.0.1
PREFIX = /usr/local

CC = g++
PKG_CONFIG = pkg-config

BCFLAGS = $(CFLAGS)
BCPPFLAGS = $(BCFLAGS)
BLDFLAGS = `$(PKG_CONFIG) --cflags --libs yaml-cpp libgit2`
SRC = cbase16.cpp

all: cbase16

options:
	@echo cbase16 build options:
	@echo "CPPFLAGS = $(BCPPFLAGS)"
	@echo "LDFLAGS  = $(BLDFLAGS)"
	@echo "CC       = $(CC)"
cbase16: options
	$(CC) $(SRC) $(BLDFLAGS) -fopenmp -o $@
	chmod 755 $@

clean:
	rm -f cbase16
