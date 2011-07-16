geany_zencoding_defines		=	-DZEN_MODULE_PATH="\"$(libdir)/geany\"" \
								-DZEN_PROFILES_PATH="\"$(datadir)/geany/zencoding/profiles\"" \
								-DZEN_ICONS_PATH="\"$(datadir)/geany/zencoding/icons\""

geanyplugin_LTLIBRARIES		=	zencoding.la
geanyplugindir				=	$(libdir)/geany
zencoding_la_LDFLAGS		=	 -module -avoid-version -Wl,--export-dynamic
zencoding_la_CPPFLAGS		=	@gtk_CFLAGS@ @geany_CFLAGS@ @PYTHON_CPPFLAGS@ \
								$(geany_zencoding_defines)
zencoding_la_LIBADD			=	@gtk_LIBS@ @geany_LIBS@ @PYTHON_LDFLAGS@ \
								@PYTHON_EXTRA_LIBS@ @PYTHON_EXTRA_LDFLAGS@
zencoding_la_SOURCES		=	plugin.c \
								zen-controller.c zen-controller.h \
								zen-editor.c zen-editor.h
