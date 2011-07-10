/*
 * zen-engine.h
 *
 * Copyright 2011 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA.
 *
 */

#ifndef ZEN_ENGINE_H
#define ZEN_ENGINE_H
#ifdef __cplusplus
extern "C" {
#endif


#include <Python.h>


#ifndef ZEN_ENGINE_MODULE_PATH
#define ZEN_ENGINE_MODULE_PATH "/usr/local/lib/zencoding"
#endif

#ifndef ZEN_ENGINE_MODULE_NAME
#define ZEN_ENGINE_MODULE_NAME "zencoding.engine"
#endif

#ifndef ZEN_ENGINE_PROFILES_PATH
#define ZEN_ENGINE_PROFILES_PATH "/usr/local/share/zencoding/profiles"
#endif

#ifndef ZEN_ENGINE_ICONS_PATH
#define ZEN_ENGINE_ICONS_PATH "/usr/local/share/zencoding/icons"
#endif


typedef struct
{
	PyObject *module;
	PyObject *instance;
}
ZenEngine;


ZenEngine*	zen_engine_new						(const char *doc_type,
													const char *zendir,
													const char *active_profile,
													const char *profiles_dir);


void		zen_engine_free						(ZenEngine *zen);


char*		zen_engine_get_profiles_dir			(ZenEngine *zen);
char*		zen_engine_get_zen_dir				(ZenEngine *zen);

char*		zen_engine_get_active_profile		(ZenEngine *zen);

void		zen_engine_set_active_profile		(ZenEngine *zen,
													const char *profile);

char*		zen_engine_get_doc_type				(ZenEngine *zen);
void		zen_engine_set_doc_type				(ZenEngine *zen,
													const char *doc_type);


char*		zen_engine_expand_abbreviation		(ZenEngine *zen,
													const char *line,
													int index,
													char **abbr,
													int *abbr_pos);


char*		zen_engine_wrap_with_abbreviation	(ZenEngine *zen,
													const char *abbr,
													const char *text);


#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* ZEN_ENGINE_H */
