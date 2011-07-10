noinst_LTLIBRARIES			=	libzencoding.la

geany_zencoding_defines		=	-DZEN_ENGINE_MODULE_PATH="\"$(libdir)/geany\"" \
								-DZEN_ENGINE_MODULE_NAME="\"zencoding.engine\"" \
								-DZEN_ENGINE_PROFILES_PATH="\"$(datadir)/geany/zencoding/profiles\"" \
								-DZEN_ENGINE_ICONS_PATH="\"$(datadir)/geany/zencoding/icons\""

libzencoding_la_CPPFLAGS	=	@PYTHON_CPPFLAGS@ \
								$(geany_zencoding_defines)
libzencoding_la_LIBADD		=	@PYTHON_LDFLAGS@
libzencoding_la_SOURCES		=	zen-engine.c zen-engine.h


geanyplugin_LTLIBRARIES		=	zencoding.la
geanyplugindir				=	$(libdir)/geany
zencoding_la_LDFLAGS		=	 -module -avoid-version
zencoding_la_CPPFLAGS		=	@gtk_CFLAGS@ @geany_CFLAGS@ @PYTHON_CPPFLAGS@ \
								$(geany_zencoding_defines)
zencoding_la_LIBADD			=	@gtk_LIBS@ @geany_LIBS@ @PYTHON_LDFLAGS@ \
								libzencoding.la
zencoding_la_SOURCES		=	plugin.c
