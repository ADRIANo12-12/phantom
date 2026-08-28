TOOLCHAIN_PREFIX := x86_64-linux-gnu-
CC := $(TOOLCHAIN_PREFIX)gcc
CXX := $(TOOLCHAIN_PREFIX)g++
STRIP := $(TOOLCHAIN_PREFIX)strip
AR := $(TOOLCHAIN_PREFIX)ar

PREFIX := $(CURDIR)/out
BINDIR := $(PREFIX)/bin
LIBDIR := $(PREFIX)/lib
SHAREDIR := $(PREFIX)/share

CFLAGS := -O2 -pipe -fno-plt -fexceptions -Wp,-D_FORTIFY_SOURCE=2 -Wformat -Werror=format-security -fstack-clash-protection -fcf-protection -static
CXXFLAGS := $(CFLAGS)
LDFLAGS := -Wl,-O1,--sort-common,--as-needed,-z,relro,-z,now -static

TOOLS := gum superfile nvim btop impala wiremix lazygit

.PHONY: all clean install $(TOOLS)

all: $(TOOLS)

install: all
	mkdir -p $(DESTDIR)/bin
	cp $(BINDIR)/* $(DESTDIR)/bin/
	mkdir -p $(DESTDIR)/share
	cp -r $(SHAREDIR)/* $(DESTDIR)/share/

clean:
	rm -rf $(PREFIX) src/*/build

gum: $(BINDIR)/gum
superfile: $(BINDIR)/superfile
nvim: $(BINDIR)/nvim
btop: $(BINDIR)/btop
impala: $(BINDIR)/impala
wiremix: $(BINDIR)/wiremix
lazygit: $(BINDIR)/lazygit

$(BINDIR)/gum: src/gum/Makefile
	$(MAKE) -C src/gum build
	cp src/gum/build/gum $(BINDIR)/gum
	$(STRIP) $(BINDIR)/gum

$(BINDIR)/superfile: src/superfile/Makefile
	$(MAKE) -C src/superfile build
	cp src/superfile/build/superfile $(BINDIR)/superfile
	$(STRIP) $(BINDIR)/superfile

$(BINDIR)/nvim: src/nvim/build/bin/nvim
	cp src/nvim/build/bin/nvim $(BINDIR)/nvim
	$(STRIP) $(BINDIR)/nvim

$(BINDIR)/btop: src/btop/build/bin/btop
	cp src/btop/build/bin/btop $(BINDIR)/btop
	$(STRIP) $(BINDIR)/btop

$(BINDIR)/impala: src/impala/target/release/impala
	cp src/impala/target/release/impala $(BINDIR)/impala
	$(STRIP) $(BINDIR)/impala

$(BINDIR)/wiremix: src/wiremix/target/release/wiremix
	cp src/wiremix/target/release/wiremix $(BINDIR)/wiremix
	$(STRIP) $(BINDIR)/wiremix

$(BINDIR)/lazygit: src/lazygit/build/lazygit
	cp src/lazygit/build/lazygit $(BINDIR)/lazygit
	$(STRIP) $(BINDIR)/lazygit

src/gum/Makefile: src/gum
src/superfile/Makefile: src/superfile
src/nvim/build/bin/nvim: src/nvim
src/btop/build/bin/btop: src/btop
src/impala/target/release/impala: src/impala
src/wiremix/target/release/wiremix: src/wiremix
src/lazygit/build/lazygit: src/lazygit

src/gum:
	git clone --depth 1 --branch v0.14.1 https://github.com/charmbracelet/gum.git $@

src/superfile:
	git clone --depth 1 --branch v1.11.0 https://github.com/yorukot/superfile.git $@

src/nvim:
	git clone --depth 1 --branch v0.10.1 https://github.com/neovim/neovim.git $@

src/btop:
	git clone --depth 1 --branch v1.3.2 https://github.com/aristocratos/btop.git $@

src/impala:
	git clone --depth 1 --branch v0.3.0 https://github.com/impala-rs/impala.git $@

src/wiremix:
	git clone --depth 1 --branch v0.5.0 https://github.com/impala-rs/wiremix.git $@

src/lazygit:
	git clone --depth 1 --branch v0.44.0 https://github.com/jesseduffield/lazygit.git $@

.PHONY: fetch-sources
fetch-sources: $(addprefix src/,$(TOOLS))

src/gum src/superfile src/nvim src/btop src/impala src/wiremix src/lazygit:
	@true