zencoding_sources			=	__init__.py \
								editor.py \
								file.py
zencoding_objects			=	$(zencoding_sources:.py=.pyc)
zencodingdir				=	$(libdir)/geany/zencoding/interface
zencoding_DATA				=	$(zencoding_sources) \
								$(zencoding_objects)
CLEANFILES					=	$(zencoding_objects)

%.pyc: %.py
	$(AM_V_GEN)$(PYTHON) -c "import py_compile; py_compile.compile('$<')"
