/*
 * zen-engine.c
 *
 * Copyright 2011 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <Python.h>
#include <stdio.h>
#include <assert.h>

#include "zen-engine.h"


ZenEngine *zen_engine_new(const char *doc_type, const char *zendir,
							const char *active_profile, const char *profiles_dir)
{
	ZenEngine *zen;
	PyObject *cls, *args, *func, *mod_name;
	char zen_path[PATH_MAX + 20] = { 0 };

	zen = malloc(sizeof(ZenEngine));

	zen->module = NULL;
	zen->instance = NULL;

	if (!Py_IsInitialized())
		Py_Initialize();

	PyRun_SimpleString("import sys");
	snprintf(zen_path, PATH_MAX + 20 - 1, "sys.path.append('%s')", zendir);
	PyRun_SimpleString(zen_path);

	mod_name = PyString_FromString(ZEN_ENGINE_MODULE_NAME);
	if (mod_name == NULL)
		return NULL;

	zen->module = PyImport_Import(mod_name);
	if (zen->module == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(mod_name);
		return NULL;
	}
	Py_XDECREF(mod_name);

	cls = PyObject_GetAttrString(zen->module, "ZenEngine");
	if (cls == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(zen->module);
		return NULL;
	}

	if (doc_type == NULL)
		doc_type = "html";

	args = Py_BuildValue("(ssss)", doc_type, zendir, active_profile, profiles_dir);
	if (args == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(zen->module);
		Py_XDECREF(cls);
		return NULL;
	}

	zen->instance = PyObject_CallObject(cls, args);
	if (zen->instance == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(zen->module);
		Py_XDECREF(cls);
		Py_XDECREF(args);
	}

	Py_XDECREF(cls);
	Py_XDECREF(args);

	return zen;
}


void zen_engine_free(ZenEngine *zen)
{

	if (zen != NULL)
	{
		Py_XDECREF(zen->instance);
		Py_XDECREF(zen->module);
	}

	if (Py_IsInitialized())
		Py_Finalize();

	free(zen);
}


static char *zen_engine_get_string_attribute(ZenEngine *zen, const char *attr_name)
{
	const char *tmp;
	char *ret;
	PyObject *val;
	if ((val = PyObject_GetAttrString(zen->instance, attr_name)) == NULL)
		return NULL;
	tmp = PyString_AsString(val);
	if (tmp == NULL || strlen(tmp) == 0)
	{
		Py_XDECREF(val);
		return NULL;
	}
	ret = strdup(tmp);
	Py_XDECREF(val);
	return ret;
}


static void zen_engine_set_string_attribute(ZenEngine *zen, const char *attr_name,
	const char *attr_value)
{
	char *tmp;
	PyObject *val;

	if ((val = PyString_FromString(attr_value)) == NULL)
		return;
	PyObject_SetAttrString(zen->instance, attr_name, val);
	Py_XDECREF(val);

	tmp = zen_engine_get_string_attribute(zen, attr_name);
	assert(tmp != NULL);
	assert(strcmp(tmp, attr_value) == 0);
	free(tmp);
}


char *zen_engine_get_profiles_dir(ZenEngine *zen)
{
	return zen_engine_get_string_attribute(zen, "profiles_dir");
}


char *zen_engine_get_zen_dir(ZenEngine *zen)
{
	return zen_engine_get_string_attribute(zen, "zendir");
}


char *zen_engine_get_active_profile(ZenEngine *zen)
{
	return zen_engine_get_string_attribute(zen, "active_profile");
}


void zen_engine_set_active_profile(ZenEngine *zen, const char *profile)
{
	zen_engine_set_string_attribute(zen, "active_profile", profile);
}


char *zen_engine_get_doc_type(ZenEngine *zen)
{
	return zen_engine_get_string_attribute(zen, "doc_type");
}


void zen_engine_set_doc_type(ZenEngine *zen, const char *doc_type)
{
	zen_engine_set_string_attribute(zen, "doc_type", doc_type);
}


char *zen_engine_expand_abbreviation(ZenEngine *zen, const char *line, char **abbr)
{
	char *tmp_abbr = NULL, *tmp_text = NULL;
	PyObject *func, *rargs, *pargs, *dt;

	if (zen == NULL || line == NULL)
		return NULL;

	func = PyObject_GetAttrString(zen->instance, "expand_abbreviation");
	if (func == NULL)
		return NULL;
	else if (!PyCallable_Check(func))
	{
		Py_XDECREF(func);
		return NULL;
	}

	pargs = Py_BuildValue("(s)", line);

	if (pargs == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(func);
		return NULL;
	}

	rargs = PyObject_CallObject(func, pargs);
	Py_XDECREF(pargs);

	if (rargs == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(func);
		return NULL;
	}

	if (!PyArg_Parse(rargs, "(ss)", &tmp_abbr, &tmp_text))
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(func);
		Py_XDECREF(rargs);
		return NULL;
	}
	Py_XDECREF(func);
	Py_XDECREF(rargs);

	if (tmp_abbr == NULL || strlen(tmp_abbr) == 0)
		return NULL;

	if (tmp_text == NULL || strlen(tmp_text) == 0)
		return NULL;

	if (abbr != NULL)
		*abbr = strdup(tmp_abbr);

	return strdup(tmp_text);
}


char *zen_engine_wrap_with_abbreviation(ZenEngine *zen, const char *abbr,
											const char *text)
{
	char *tmp_text = NULL;
	PyObject *func, *rargs, *pargs, *dt;

	if (zen == NULL || abbr == NULL || text == NULL)
		return NULL;

	func = PyObject_GetAttrString(zen->instance, "wrap_with_abbreviation");
	if (func == NULL)
		return NULL;
	else if (!PyCallable_Check(func))
	{
		Py_XDECREF(func);
		return NULL;
	}


	pargs = Py_BuildValue("(ss)", abbr, text);
	if (pargs == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(func);
		return NULL;
	}

	rargs = PyObject_CallObject(func, pargs);
	Py_XDECREF(pargs);

	if (rargs == NULL || PyErr_Occurred())
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(func);
		return NULL;
	}

	if (!PyArg_Parse(rargs, "s", &tmp_text))
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(func);
		Py_XDECREF(rargs);
		return NULL;
	}
	Py_XDECREF(func);
	Py_XDECREF(rargs);

	if (tmp_text == NULL || strlen(tmp_text) == 0)
		return NULL;

	return strdup(tmp_text);
}
