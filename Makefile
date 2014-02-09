QUIET?=@
CFLAGS?=-Wall -Wextra -O3 -g 
VERSION?=0.14.2

PROGRAM=xininfo


PREFIX?=$(DESTDIR)/usr
BIN_DIR?=$(PREFIX)/bin


SOURCE_DIR=source
CONFIG_DIR=config
DOC_DIR=doc
BUILD_DIR=build

SOURCES=$(wildcard $(SOURCE_DIR)/*.c $(CONFIG_DIR)/*.c )
OBJECTS=$(SOURCES:%.c=$(BUILD_DIR)/%.o)
HEADERS=$(wildcard include/*.h)
OTHERS=Makefile LICENSE

INSTALL_PROGRAM=$(BIN_DIR)/$(PROGRAM)


DIST_TARGET=$(BUILD_DIR)/$(PROGRAM)-$(VERSION).tar.xz


CFLAGS+=-std=c99
CFLAGS+=-Iinclude/
CFLAGS+=-DVERSION="\"$(VERSION)\""

LDADD=-lm
# Check deps.
ifeq (${DEBUG},1)
CFLAGS+=-DTIMING=1 -g3
LDADD+=-lrt
endif

CLANG=$(shell which clang)

ifneq (${CLANG},${EMPTY})
    $(info Using clang compiler: ${CLANG})
    CC=${CLANG}
endif


##
# Check dependencies
##
PKG_CONFIG?=$(shell which pkg-config)
ifeq (${PKG_CONFIG},${EMPTY})
$(error Failed to find pkg-config. Please install pkg-config)
endif

CFLAGS+=$(shell ${PKG_CONFIG} --cflags x11 xinerama )
LDADD+=$(shell ${PKG_CONFIG} --libs x11 xinerama )

ifeq (${LDADD},${EMPTY})
$(error Failed to find the required dependencies: x11, xinerama )
endif


all: $(BUILD_DIR)/$(PROGRAM)

$(BUILD_DIR):
	$(info Creating build dir)
	$(QUIET)mkdir -p $@
	$(QUIET)mkdir -p $@/$(SOURCE_DIR)
	$(QUIET)mkdir -p $@/$(CONFIG_DIR)

# Objects depend on header files and makefile too.
$(BUILD_DIR)/%.o: %.c Makefile $(HEADERS) | $(BUILD_DIR)
	$(info Compiling $< -> $@)
	$(QUIET) $(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/$(PROGRAM): $(OBJECTS)
	$(info Linking   $@)
	$(QUIET)$(CC) -o $@ $^  $(LDADD) $(LDFLAGS)

$(INSTALL_PROGRAM): $(BUILD_DIR)/$(PROGRAM)
	$(info Install   $^ -> $@ )
	$(QUIET)install -Dm 755 $^ $@ 

install: $(INSTALL_PROGRAM)

clean:
	$(info Clean build dir)
	$(QUIET)rm -rf $(BUILD_DIR)


indent:
	@astyle --style=linux -S -C -D -N -H -L -W3 -f $(SOURCES) $(HEADERS)

dist: $(DIST_TARGET)

$(BUILD_DIR)/$(PROGRAM)-$(VERSION): $(SOURCES) $(HEADERS) $(OTHERS) | $(BUILD_DIR)
	$(info Create release directory)
	$(QUIET)mkdir -p $@
	$(QUIET)cp -a --parents $^ $@


$(DIST_TARGET): $(BUILD_DIR)/$(PROGRAM)-$(VERSION) 
	$(info Creating release tarball: $@)
	$(QUIET) tar -C $(BUILD_DIR) -cavvJf $@ $(PROGRAM)-$(VERSION)
