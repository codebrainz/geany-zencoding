/*
 * zen-controller.c
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

/*
 * This file initializes the Python interpreter, sets up the Python path,
 * imports the requred modules, and provides a way for the Geany plugin to
 * run Zen Coding actions by means of the zen_controller_run_action()
 * function.
 */

#include <Python.h>
#include <limits.h>
#include <geanyplugin.h>
#include "zen-controller.h"
#include "zen-editor.h"


extern GeanyPlugin		*geany_plugin;
extern GeanyData		*geany_data;
extern GeanyFunctions	*geany_functions;


ZenController *zen_controller_new(const char *zendir)
{
	ZenController *result;
	char zen_path[PATH_MAX + 20] = { 0 };
	PyObject *module, *cls;

	result = malloc(sizeof(ZenController));
	result->editor = NULL;
	result->run_action = NULL;
	result->set_context = NULL;
	result->set_active_profile = NULL;

	if (!Py_IsInitialized())
		Py_Initialize();

	PyRun_SimpleString("import sys");
	snprintf(zen_path, PATH_MAX + 20 - 1, "sys.path.append('%s')", zendir);
	PyRun_SimpleString(zen_path);

	module = PyImport_ImportModule("zencoding");
	if (module == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		free(result);
		return NULL;
	}

	result->run_action = PyObject_GetAttrString(module, "run_action");
	if (result->run_action == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(module);
		free(result);
		return NULL;
	}

	if (!PyCallable_Check(result->run_action))
	{
		Py_XDECREF(result->run_action);
		Py_XDECREF(module);
		free(result);
		return NULL;
	}

	Py_XDECREF(module);


	module = zen_editor_module_init();
	if (module == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(result->run_action);
		free(result);
		return NULL;
	}

	cls = PyObject_GetAttrString(module, "ZenEditor");
	if (cls == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(module);
		Py_XDECREF(result->run_action);
		free(result);
		return NULL;
	}
	Py_XDECREF(module);

	result->editor = PyObject_CallObject(cls, NULL);
	/*result->editor = PyInstance_New(cls, NULL, NULL);*/
	if (result->editor == NULL)
	{
		//fprintf(stderr, "%s: %d: Here\n", __FILE__, __LINE__);
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(cls);
		Py_XDECREF(result->run_action);
		free(result);
		return NULL;
	}

	Py_XDECREF(cls);

	result->set_context = PyObject_GetAttrString(result->editor, "set_context");
	if (result->set_context == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(result->editor);
		Py_XDECREF(result->run_action);
		free(result);
		return NULL;
	}
	else if (!PyCallable_Check(result->set_context))
	{
		Py_XDECREF(result->editor);
		Py_XDECREF(result->run_action);
		Py_XDECREF(result->set_context);
		free(result);
		return NULL;
	}

	result->set_active_profile = PyObject_GetAttrString(result->editor, "set_profile_name");
	if (result->set_active_profile == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(result->editor);
		Py_XDECREF(result->run_action);
		Py_XDECREF(result->set_context);
		free(result);
		return NULL;
	}
	else if (!PyCallable_Check(result->set_active_profile))
	{
		Py_XDECREF(result->editor);
		Py_XDECREF(result->run_action);
		Py_XDECREF(result->set_context);
		Py_XDECREF(result->set_active_profile);
		free(result);
		return NULL;
	}

	return result;
}

void zen_controller_free(ZenController *zen)
{
	Py_XDECREF(zen->editor);
	Py_XDECREF(zen->run_action);
	free(zen);
}


void zen_controller_set_active_profile(ZenController *zen, const char *profile)
{
	PyObject *args, *result;

	args = Py_BuildValue("(s)", profile);
	if (args == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		g_warning("Unable to build arguments for set_profile_name().");
		return;
	}

	result = PyObject_CallObject(zen->set_active_profile, args);
	if (result == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(args);
		g_warning("Unable to call set_profile_name().");
		return;
	}
	Py_XDECREF(args);
	Py_XDECREF(result);
}


void zen_controller_run_action(ZenController *zen, const char *action_name)
{
	PyObject *addr, *result;
	GeanyDocument *doc;

	ui_set_statusbar(TRUE, _("Zen Coding: Running '%s' action"), action_name);

	doc = document_get_current();
	if (!DOC_VALID(doc))
	{
		g_warning("No valid document detected.");
		return;
	}

	addr = PyLong_FromVoidPtr((void *) doc);
	if (addr == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		g_warning("Unable to convert document pointer to Python object.");
		return;
	}

	result = PyObject_CallMethod(zen->editor, "set_context", "O", addr);
	Py_DECREF(addr);
	if (result == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		g_warning("Unable to call set_context() function.");
		return;
	}

	/* FIXME - shouldn't need extra args */
	result = PyObject_CallFunction(zen->run_action, "sO", action_name, zen->editor);
	if (result == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		g_warning("Call to run_action() failed.");
		return;
	}

	Py_XDECREF(result);
}
