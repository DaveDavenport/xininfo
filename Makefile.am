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

EXTRA_DIST=\
		   doc/xininfo.markdown

xininfo_SOURCES=\
    source/xininfo.c

dist_man1_MANS=\
			doc/xininfo.1

.PHONY: indent
indent: $(xininfo_SOURCES)
	uncrustify -c $(top_srcdir)/data/uncrustify.cfg --replace $^

.PHONY: update-manpage
update-manpage:
	ronn --roff --pipe $(top_srcdir)/doc/xininfo.markdown > $(top_srcdir)/doc/xininfo.1
