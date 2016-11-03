# Specify automake version.
AUTOMAKE_OPTIONS  = 1.11.3

##
# Rofi the program
##
bin_PROGRAMS=xininfo

LIBS=\
	@xcb_LIBS@

AM_CFLAGS=\
	@EXTRA_CFLAGS@\
	@xcb_CFLAGS@\
    -DMANPAGE_PATH="\"$(mandir)/\""\
    -I$(top_builddir)/\
	-Werror=missing-prototypes

xininfo_SOURCES=\
    source/xininfo.c

.PHONY: indent
indent: $(xininfo_SOURCES)
	uncrustify -c $(top_srcdir)/data/uncrustify.cfg --replace $^

