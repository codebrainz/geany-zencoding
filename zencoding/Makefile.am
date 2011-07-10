zencoding_flags				=	-fq
zencoding_sources			=	__init__.py \
								engine.py \
								html_matcher.py \
								htmlparser.py \
								stparser.py \
								zen_core.py \
								zen_settings.py
zencoding_objects			=	$(zencoding_sources:.py=.pyc)
zencodingdir				=	$(libdir)/geany/zencoding
zencoding_DATA				=	$(zencoding_sources) \
								$(zencoding_objects)
CLEANFILES					=	$(zencoding_objects) engine.py

%.pyc: %.py
	$(AM_V_GEN)$(PY_COMPILEFILES) $(zencoding_flags) $<

engine.py: engine.py.in
	$(AM_V_GEN)$(SED) \
		-e 's|@DEFAULT_ZENDIR[@]|$(libdir)/geany|g' \
		-e 's|@DEFAULT_DOC_TYPE[@]|html|g' \
		-e 's|@DEFAULT_PROFILE[@]|geany_profile|g' \
		-e 's|@DEFAULT_PROFILE_DIR[@]|$(datadir)/geany/zencoding/profiles|g' \
		$< > $@
