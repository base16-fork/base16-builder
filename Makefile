.POSIX:

VERSION = 0.5.2
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

PKG_CONFIG = pkg-config

BCXXFLAGS = $(CFLAGS) $(CXXFLAGS) --std=c++20 -fopenmp
BLDFLAGS = `$(PKG_CONFIG) --cflags --libs yaml-cpp libgit2`
SRC = cbase16.cpp

all: cbase16

options:
	@echo cbase16 build options:
	@echo "CPPFLAGS = $(BCXXFLAGS)"
	@echo "LDFLAGS  = $(BLDFLAGS)"
	@echo "CXX      = $(CXX)"

cbase16: options
	$(CXX) $(SRC) $(BLDFLAGS) $(BCXXFLAGS) -o $@

clean:
	rm -f cbase16

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f cbase16 $(DESTDIR)$(PREFIX)/bin
	chmod 775 $(DESTDIR)$(PREFIX)/bin/cbase16
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < cbase16.1 > $(DESTDIR)$(MANPREFIX)/man1/cbase16.1
	chmod 664 $(DESTDIR)$(MANPREFIX)/man1/cbase16.1
	mkdir -p $(DESTDIR)$(PREFIX)/share/bash-completion/completions
	mkdir -p $(DESTDIR)$(PREFIX)/share/zsh/site-functions
	cp -f completion/bash/cbase16.bash $(DESTDIR)$(PREFIX)/share/bash-completion/completions/cbase16
	cp -f completion/zsh/_cbase16 $(DESTDIR)$(PREFIX)/share/zsh/site-functions

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/cbase16
	rm -f $(DESTDIR)$(MANPREFIX)/man1/cbase16.1
	rm -f $(DESTDIR)$(PREFIX)/share/bash-completion/completions/cbase16
	rm -f $(DESTDIR)$(PREFIX)/share/zsh/site-functions/_cbase16

.PHONY: all options cbase16 clean install uninstall
