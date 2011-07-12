SUBDIRS = actions filters interface parser


zencoding_sources			=	__init__.py \
								html_matcher.py \
								resources.py \
								utils.py \
								zen_settings.py
zencoding_objects			=	$(zencoding_sources:.py=.pyc)
zencodingdir				=	$(libdir)/geany/zencoding
zencoding_DATA				=	$(zencoding_sources) \
								$(zencoding_objects)
CLEANFILES					=	$(zencoding_objects)

%.pyc: %.py
	$(AM_V_GEN)$(PYTHON) -c "import py_compile; py_compile.compile('$<')"

