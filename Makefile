.POSIX:

VERSION = 0.0.1
PREFIX = /usr/local

CC = g++
PKG_CONFIG = pkg-config

SRC = cbase16.cpp
BLDFLAGS = `$(PKG_CONFIG) --cflags --libs yaml-cpp libgit2`

all: cbase16

cbase16:
	$(CC) $(SRC) $(BLDFLAGS) -fopenmp -o $@
	chmod 755 $@

clean:
	rm -f cbase16
