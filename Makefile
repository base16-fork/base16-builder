.POSIX:

VERSION = 0.0.1
PREFIX = /usr/local

PKG_CONFIG = pkg-config

BCXXFLAGS = $(CFLAGS) $(CXXFLAGS) --std=gnu++17 -fopenmp
BLDFLAGS = `$(PKG_CONFIG) --cflags --libs yaml-cpp libgit2` \
	   "-lboost_filesystem"
SRC = cbase16.cpp

all: cbase16

options:
	@echo cbase16 build options:
	@echo "CPPFLAGS = $(BCXXFLAGS)"
	@echo "LDFLAGS  = $(BLDFLAGS)"
	@echo "CXX      = $(CXX)"
cbase16: options
	$(CXX) $(SRC) $(BLDFLAGS) $(BCXXFLAGS) -o $@
	chmod 755 $@

clean:
	rm -f cbase16

.PHONY: all options cbase16 clean
