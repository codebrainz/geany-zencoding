/*
 * zen-module.c
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
 * This file contains a Python module that gets loaded by the Geany plugin
 * directly.  It provides access to the Scintilla buffer of the current
 * document to Zen Coding in Python-land.
 */

#include <Python.h>
#include <structmember.h>
#include <stdarg.h>
#include <geanyplugin.h>
#include "zen-editor.h"


extern GeanyPlugin		*geany_plugin;
extern GeanyData		*geany_data;
extern GeanyFunctions	*geany_functions;


/*#define ZEN_EDITOR_DEBUG 1*/

static void _debug_print(const char *file, int line, const char *function, const char *fmt, ...)
{
	va_list ap;
	char *format = NULL;

	if (function != NULL)
		format = g_strdup_printf("%s:%d: %s(): %s\n", file, line, function, fmt);
	else
		format = g_strdup_printf("%s:%d: %s\n", file, line, fmt);

	va_start(ap, fmt);
	g_vfprintf(stderr, format, ap);
	va_end(ap);

	g_free(format);
}
#ifdef ZEN_EDITOR_DEBUG
#define dbg(fmt, ...) _debug_print(__FILE__, __LINE__, NULL, fmt, ##__VA_ARGS__)
#define dbgf(func, fmt, ...) _debug_print(__FILE__, __LINE__, func, fmt, ##__VA_ARGS__)
#else
#define dbg(fmt, ...)
#define dbgf(func, fmt, ...)
#endif


typedef struct
{
	PyObject_HEAD

	PyObject *active_profile;
	PyObject *context;

	gchar *caret_placeholder;

} ZenEditor;


static GeanyDocument *
ZenEditor_get_context(ZenEditor *self)
{
	GeanyDocument *doc;

	dbgf("ZenEditor_get_context", "called");

	if (self->context != NULL)
	{
		doc = (GeanyDocument *) PyLong_AsVoidPtr(self->context);
		if (DOC_VALID(doc))
			return doc;
	}

	return NULL;
}


static ScintillaObject *
ZenEditor_get_scintilla(ZenEditor *self)
{
	GeanyDocument *doc;

	dbgf("ZenEditor_get_scintilla", "called");

	doc = ZenEditor_get_context(self);
	if (doc == NULL || doc->editor == NULL || doc->editor->sci == NULL)
		return NULL;

	return doc->editor->sci;
}


static PyObject *
ZenEditor_get_selection_range(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	ScintillaObject *sci;

	dbgf("ZenEditor_get_selection_range", "called");

	if ((sci = ZenEditor_get_scintilla(self)) == NULL)
	{
		Py_RETURN_NONE;
	}
	else
	{
		result = Py_BuildValue("(ii)",
					sci_get_selection_start(sci),
					sci_get_selection_end(sci));
		return result;
	}
}


static PyObject *
ZenEditor_create_selection(ZenEditor *self, PyObject *args)
{
	gint sel_start, sel_end;
	ScintillaObject *sci;

	dbgf("ZenEditor_create_selection", "called");

	if ((sci = ZenEditor_get_scintilla(self)) == NULL)
		Py_RETURN_NONE;

	if (PyArg_ParseTuple(args, "ii", &sel_start, &sel_end))
	{
		if (sel_end == -1)
			sci_set_current_position(sci, sel_start, TRUE);
		else
		{
			sci_set_selection_start(sci, sel_start);
			sci_set_selection_end(sci, sel_end);
		}
	}

	Py_RETURN_NONE;
}


static PyObject *
ZenEditor_get_current_line_range(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	gint line, line_start, line_end;
	ScintillaObject *sci;

	dbgf("ZenEditor_current_line_range", "called");

	if ((sci = ZenEditor_get_scintilla(self)) == NULL)
		Py_RETURN_NONE;

	line = sci_get_current_line(sci);
	line_start = sci_get_position_from_line(sci, line);
	line_end = sci_get_line_end_position(sci, line);

	result = Py_BuildValue("ii", line_start, line_end);
	return result;
}


static PyObject *
ZenEditor_get_caret_pos(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	gint pos;
	ScintillaObject *sci;

	dbgf("ZenEditor_get_caret_pos", "called");

	if ((sci = ZenEditor_get_scintilla(self)) == NULL)
		Py_RETURN_NONE;

	pos = sci_get_current_position(sci);
	result = Py_BuildValue("i", pos);

	return result;
}


static PyObject *
ZenEditor_set_caret_pos(ZenEditor *self, PyObject *args)
{
	gint pos;
	ScintillaObject *sci;

	dbgf("ZenEditor_set_caret_pos", "called");

	if ((sci = ZenEditor_get_scintilla(self)) == NULL)
		Py_RETURN_NONE;

	if (PyArg_ParseTuple(args, "i", &pos))
		sci_set_current_position(sci, pos, TRUE);

	Py_RETURN_NONE;
}


static PyObject *
ZenEditor_get_current_line(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	gchar *text;
	ScintillaObject *sci;

	dbgf("ZenEditor_get_current_line", "called");

	if ((sci = ZenEditor_get_scintilla(self)) == NULL)
		Py_RETURN_NONE;

	text = sci_get_line(sci, sci_get_current_line(sci));
	result = Py_BuildValue("s", text);
	g_free(text);

	return result;
}


static gchar *
ZenEditor_replace_caret_placeholder(const gchar *placeholder,
	const gchar *text, gint *first_pos)
{
	gint placeholder_pos, len;
	gchar *new_text, **arr;

	dbgf("ZenEditor_replace_caret_placeholder", "called");

	arr = g_strsplit(text, placeholder, 0);
	len = g_strv_length(arr);

	if (len >= 2)
	{
		placeholder_pos = strlen(arr[0]);
		new_text = g_strjoinv(NULL, arr);
	}
	else
	{
		placeholder_pos = -1;
		new_text = g_strdup(arr[0]);
	}

	g_strfreev(arr);

	if (first_pos != NULL)
		*first_pos = placeholder_pos;

	return new_text;
}


static PyObject *
ZenEditor_replace_content(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	gint sel_start, sel_end, ph_pos;
	gchar *text, *ph, *tmp;
	ScintillaObject *sci;

	dbgf("ZenEditor_replace_content", "called");

	if ((sci = ZenEditor_get_scintilla(self)) == NULL)
		Py_RETURN_NONE;

	/* TODO: handle case of missing args, not -1 anymore */
	if (PyArg_ParseTuple(args, "sii", &text, &sel_start, &sel_end))
	{
		tmp = ZenEditor_replace_caret_placeholder(self->caret_placeholder, text, &ph_pos);

		if (sel_start == -1 && sel_end == -1)
		{
			/* replace whole editor content */
			sci_set_text(sci, tmp);
		}
		else if (sel_start != -1 && sel_end == -1)
		{
			/* insert text at sel_start */
			sci_insert_text(sci, sel_start, tmp);
		}
		else if (sel_start != -1 && sel_end != -1)
		{
			/* replace from sel_start to sel_end */
			sci_set_selection_start(sci, sel_start);
			sci_set_selection_end(sci, sel_end);
			sci_replace_sel(sci, tmp);
		}
		else
		{
			g_free(tmp);
			Py_RETURN_NONE;
		}

		/* Insert cursor at placeholder, if found */
		if (ph_pos > -1)
			sci_set_current_position(sci, sel_start+ph_pos, TRUE);

		g_free(tmp);
	}
	else
	{
		if (PyErr_Occurred())
			PyErr_Print();
	}

	Py_RETURN_NONE;
}


static PyObject *
ZenEditor_get_content(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	gchar *text;
	ScintillaObject *sci;

	dbgf("ZenEditor_get_content", "called");

	if ((sci = ZenEditor_get_scintilla(self)) == NULL)
		Py_RETURN_NONE;

	text = sci_get_contents(sci, sci_get_length(sci) + 1);
	if (text != NULL)
	{
		result = PyString_FromString(text);
		g_free(text);
		if (result == NULL)
		{
			if (PyErr_Occurred())
				PyErr_Print();
			Py_RETURN_NONE;
		}
		else
			return result;
	}

	Py_RETURN_NONE;
}


static PyObject *
ZenEditor_get_selection(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	gchar *text;
	ScintillaObject *sci;

	dbgf("ZenEditor_get_selection", "called");

	if ((sci = ZenEditor_get_scintilla(self)) == NULL)
		Py_RETURN_NONE;

	text = sci_get_selection_contents(sci);
	result = Py_BuildValue("s", text);
	g_free(text);

	return result;
}


static PyObject *
ZenEditor_get_file_path(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	GeanyDocument *doc;

	dbgf("ZenEditor_get_file_path", "called");

	if ((doc = ZenEditor_get_context(self)) == NULL)
		Py_RETURN_NONE;

	if (doc->file_name == NULL)
		Py_RETURN_NONE;

	result = Py_BuildValue("s", doc->file_name);
	return result;
}


static PyObject *
ZenEditor_prompt(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	GtkWidget *dialog, *input, *content_area, *vbox;
	gchar *abbr = NULL, *title = NULL;

	dbgf("ZenEditor_prompt", "called");

	if (!PyArg_ParseTuple(args, "s", &title) || title == NULL)
		title = "Enter Abbreviation";

	dialog = gtk_dialog_new_with_buttons(title,
				GTK_WINDOW(geany_data->main_widgets->window),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_REJECT,
				GTK_STOCK_OK,
				GTK_RESPONSE_ACCEPT,
				NULL);

	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 300, -1);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	input = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(input), TRUE);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), input, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_container_add(GTK_CONTAINER(content_area), vbox);

	gtk_widget_show_all(vbox);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		abbr = g_strdup(gtk_entry_get_text(GTK_ENTRY(input)));

	gtk_widget_destroy(dialog);

	if (abbr == NULL)
		Py_RETURN_NONE;
	else if (strlen(abbr) == 0)
	{
		g_free(abbr);
		Py_RETURN_NONE;
	}
	else
		return PyString_FromString(abbr);
}


static PyObject *
ZenEditor_set_context(ZenEditor *self, PyObject *args)
{
	PyObject *context = NULL, *tmp;

	dbgf("ZenEditor_set_context", "called");

	if (PyArg_ParseTuple(args, "O", &context))
	{
		if (context != NULL)
		{
			if (DOC_VALID((GeanyDocument *) PyLong_AsVoidPtr(context)))
			{
				tmp = self->context;
				Py_INCREF(context);
				self->context = context;
				Py_XDECREF(tmp);
			}
		}
	}

	Py_RETURN_NONE;
}


static PyObject *
ZenEditor_get_profile_name(ZenEditor *self, PyObject *args)
{
	dbgf("ZenEditor_get_profile_name", "called");
	return PyString_FromString("html");
}


static PyObject *
ZenEditor_set_profile_name(ZenEditor *self, PyObject *args)
{
	PyObject *profile = NULL, *tmp;

	dbgf("ZenEditor_set_profile_name", "called");

	if (PyArg_ParseTuple(args, "s", &profile))
	{
		if (profile != NULL)
		{
			tmp = self->active_profile;
			Py_INCREF(profile);
			self->active_profile = profile;
			Py_XDECREF(tmp);

			Py_RETURN_TRUE;
		}
	}
	Py_RETURN_FALSE;
}


static PyObject *
ZenEditor_get_syntax(ZenEditor *self, PyObject *args)
{
	dbgf("ZenEditor_get_syntax", "called");
	return PyString_FromString("html");
}


static void
ZenEditor_dealloc(ZenEditor *self)
{
	dbgf("ZenEditor_dealloc", "called");
	Py_XDECREF(self->active_profile);
	Py_XDECREF(self->context);
	g_free(self->caret_placeholder);
	self->ob_type->tp_free((PyObject *) self);
}


static PyObject *
ZenEditor_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	ZenEditor *self;

	dbgf("ZenEditor_new", "called");

	self = (ZenEditor *)type->tp_alloc(type, 0);
	if (self != NULL)
	{
		self->active_profile = PyString_FromString("xhtml");
		if (self->active_profile == NULL)
		{
			Py_XDECREF(self);
			return NULL;
		}

		self->context = PyLong_FromVoidPtr((void *)NULL);
		if (self->context == NULL)
		{
			Py_XDECREF(self);
			return NULL;
		}
	}
	return (PyObject *) self;
}


static int
ZenEditor_init(ZenEditor *self, PyObject *args, PyObject *kwds)
{
	PyObject *context = NULL;
	PyObject *profile = NULL;
	PyObject *tmp, *mod, *caret_ph;
	const gchar *ph;
	static gchar *kwlist[] = { "profile", "context", NULL };

	dbgf("ZenEditor_init", "called");

	if (PyArg_ParseTupleAndKeywords(args, kwds, "|OO", kwlist,
			&context, &profile))
	{

		if (context != NULL)
		{
			if (!DOC_VALID((GeanyDocument *) PyLong_AsVoidPtr(context)))
				return -1;

			tmp = self->context;
			Py_INCREF(context);
			self->context = context;
			Py_XDECREF(tmp);
		}

		if (profile != NULL)
		{
			tmp = self->active_profile;
			Py_INCREF(profile);
			self->active_profile = profile;
			Py_XDECREF(tmp);
		}
	}

	mod = PyImport_ImportModule("zencoding.utils");
	if (mod == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		return -1;
	}

	caret_ph = PyObject_GetAttrString(mod, "caret_placeholder");
	if (caret_ph == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(mod);
		return -1;
	}
	Py_XDECREF(mod);

	ph = (const gchar *)PyString_AsString(caret_ph);
	if (ph == NULL)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		Py_XDECREF(caret_ph);
		return -1;
	}

	self->caret_placeholder = g_strstrip(g_strdup(ph));

	return 0;
}


static PyMethodDef ZenEditor_methods[] = {
	{"set_context", (PyCFunction)ZenEditor_set_context, METH_VARARGS},
	{"get_selection_range", (PyCFunction)ZenEditor_get_selection_range, METH_VARARGS},
	{"create_selection", (PyCFunction)ZenEditor_create_selection, METH_VARARGS},
	{"get_current_line_range", (PyCFunction)ZenEditor_get_current_line_range, METH_VARARGS},
	{"get_caret_pos", (PyCFunction)ZenEditor_get_caret_pos, METH_VARARGS},
	{"set_caret_pos", (PyCFunction)ZenEditor_set_caret_pos, METH_VARARGS},
	{"get_current_line", (PyCFunction)ZenEditor_get_current_line, METH_VARARGS},
	{"replace_content", (PyCFunction)ZenEditor_replace_content, METH_VARARGS},
	{"get_content", (PyCFunction)ZenEditor_get_content, METH_VARARGS},
	{"get_syntax", (PyCFunction)ZenEditor_get_syntax, METH_VARARGS},
	{"get_profile_name", (PyCFunction)ZenEditor_get_profile_name, METH_VARARGS},
	{"set_profile_name", (PyCFunction)ZenEditor_set_profile_name, METH_VARARGS},
	{"prompt", (PyCFunction)ZenEditor_prompt, METH_VARARGS},
	{"get_selection", (PyCFunction)ZenEditor_get_selection, METH_VARARGS},
	{"get_file_path", (PyCFunction)ZenEditor_get_file_path, METH_VARARGS},
	{NULL}
};


static PyMemberDef ZenEditor_members[] = {
	{"active_profile", T_STRING, offsetof(ZenEditor, active_profile), 0,
		"The profile used by Zen Coding when performing actions."},
	{"context", T_LONG, offsetof(ZenEditor, context), 0,
		"The current editor context used by Zen Coding when performing actions."},
	{NULL}
};


static PyTypeObject ZenEditorType = {
	PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "geany.ZenEditor",         /*tp_name*/
    sizeof(ZenEditor),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)ZenEditor_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Zen Coding ZenEditor",    /* tp_doc */
    0,		                   /* tp_traverse */
    0,		               	   /* tp_clear */
    0,		                   /* tp_richcompare */
    0,		                   /* tp_weaklistoffset */
    0,		                   /* tp_iter */
    0,		                   /* tp_iternext */
    ZenEditor_methods,         /* tp_methods */
    ZenEditor_members,         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)ZenEditor_init,  /* tp_init */
    0,                         /* tp_alloc */
    ZenEditor_new,             /* tp_new */

};

PyMethodDef Module_methods[] = { { NULL } };


PyObject *zen_editor_module_init(void)
{
	PyObject *m;

	ZenEditorType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&ZenEditorType) < 0)
		return;

	m = Py_InitModule3("geany", Module_methods, "Geany Zen Coding module");

	Py_INCREF(&ZenEditorType);
	PyModule_AddObject(m, "ZenEditor", (PyObject *) &ZenEditorType);

	return m;
}
