# Specify automake version.
AUTOMAKE_OPTIONS  = 1.11.3

##
# Rofi the program
##
bin_PROGRAMS=xininfo

LIBS=\
    @xft_LIBS@\
    @x11_LIBS@\
    @xinerama_LIBS@\
    @xext_LIBS@\
	@xrandr_LIBS@

AM_CFLAGS=\
	@EXTRA_CFLAGS@\
    @xft_CFLAGS@\
    @x11_CFLAGS@\
    @xinerama_CFLAGS@\
    @xext_CFLAGS@\
	@xrandr_CFLAGS@\
    -DMANPAGE_PATH="\"$(mandir)/\""\
    -I$(top_builddir)/\
	-Werror=missing-prototypes

xininfo_SOURCES=\
    source/xininfo.c

