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
#include <regex.h>
#include <geanyplugin.h>
#include "zen-editor.h"


extern GeanyPlugin		*geany_plugin;
extern GeanyData		*geany_data;
extern GeanyFunctions	*geany_functions;


#ifdef ZEN_EDITOR_DEBUG
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
#define dbg(...) _debug_print(__FILE__, __LINE__, NULL, __VA_ARGS__)
#define dbgf(...) _debug_print(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define print_called() _debug_print(__FILE__, __LINE__, __FUNCTION__, "%s", "called")
#else
#define dbg(...)
#define dbgf(...)
#define print_called()
#endif


#define py_return_none_if_null(v) \
{ \
	if ((v) == NULL) \
	{ \
		if (PyErr_Occurred()) \
		{ \
			PyErr_Print(); \
			PyErr_Clear(); \
		} \
		Py_RETURN_NONE; \
	} \
}


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

	print_called();

	if (self->context == NULL)
		return NULL;

	doc = (GeanyDocument *) PyLong_AsVoidPtr(self->context);
	return DOC_VALID(doc) ? doc : NULL;
}


static ScintillaObject *
ZenEditor_get_scintilla(ZenEditor *self)
{
	GeanyDocument *doc;

	print_called();

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

	print_called();
	py_return_none_if_null(sci = ZenEditor_get_scintilla(self));

	result = Py_BuildValue("(ii)",
				sci_get_selection_start(sci),
				sci_get_selection_end(sci));
	py_return_none_if_null(result);

	return result;
}


static PyObject *
ZenEditor_create_selection(ZenEditor *self, PyObject *args)
{
	gint sel_start = -1, sel_end = -1;
	ScintillaObject *sci;

	print_called();
	py_return_none_if_null(sci = ZenEditor_get_scintilla(self));

	if (PyArg_ParseTuple(args, "i|i", &sel_start, &sel_end))
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

	print_called();
	py_return_none_if_null(sci = ZenEditor_get_scintilla(self));

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

	print_called();
	py_return_none_if_null(sci = ZenEditor_get_scintilla(self));

	pos = sci_get_current_position(sci);
	result = Py_BuildValue("i", pos);

	return result;
}


static PyObject *
ZenEditor_set_caret_pos(ZenEditor *self, PyObject *args)
{
	gint pos;
	ScintillaObject *sci;

	print_called();
	py_return_none_if_null(sci = ZenEditor_get_scintilla(self));

	if (PyArg_ParseTuple(args, "i", &pos))
		sci_set_current_position(sci, pos, TRUE);

	Py_RETURN_NONE;
}


static PyObject *
ZenEditor_get_current_line(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	gchar *line;
	ScintillaObject *sci;


	print_called();
	py_return_none_if_null(sci = ZenEditor_get_scintilla(self));

	line = sci_get_line(sci, sci_get_current_line(sci));
	result = Py_BuildValue("s", line);
	g_free(line);

	return result;
}


/*
 * Removes caret placeholder from string and puts the position where the first
 * placeholder was in the location pointed to by first_pos.  The value for
 * first_pos will be -1 if there were no caret placeholders in the text.
 * Returns a newly allocated string containing the placeholder-free text.
 */
static gchar *
ZenEditor_replace_caret_placeholder(const gchar *placeholder,
	const gchar *text, gint *first_pos)
{
	gint placeholder_pos, len;
	gchar *new_text, **arr;

	print_called();

	arr = g_strsplit(text, placeholder, 0);
	len = g_strv_length(arr);

	if (len > 1)
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


/*
 * Takes a string containing a range (ie. ${0:1}) and replaces it in the new
 * string with the range end.  The returned string is a copy and should be
 * freed when no longer needed.
 *
 * Example:
 *   'some text here ${10:20} and more'
 * becomes
 *   'some text here 20 and more'
 */
static gchar *
ZenEditor_replace_range(const gchar *text)
{
	gchar *start_str, *end_str;
	gchar *repl;
	regex_t re;
	regmatch_t match[3] = {0};

	if (regcomp(&re, "\\$\\{([0-9\\.]+):([0-9\\.\\-]+)\\}", REG_EXTENDED) != 0)
		return g_strdup(text);

	if (regexec(&re, text, 3, match, 0) == 0)
	{
		start_str = g_strndup(text + match[1].rm_so,
						match[1].rm_eo - match[1].rm_so);
		end_str = g_strndup(text + match[2].rm_so,
						match[2].rm_eo - match[2].rm_so);

		repl = g_malloc0(strlen(text) + 1);

		strncpy(repl, text, match[0].rm_so + 1);
		strncpy(repl + match[0].rm_so, end_str, match[2].rm_eo - match[2].rm_so);
		strncpy(repl + match[0].rm_so + (match[2].rm_eo - match[2].rm_so),
			text + match[0].rm_eo,
			strlen(text) - match[0].rm_eo);

		g_free(start_str);
		g_free(end_str);
		regfree(&re);

		return repl;
	}

	regfree(&re);
	return g_strdup(text);
}


static PyObject *
ZenEditor_replace_content(ZenEditor *self, PyObject *args)
{
	gint sel_start = -1, sel_end = -1, ph_pos;
	gchar *text, *tmp, *tmp2;
	ScintillaObject *sci;

	print_called();
	py_return_none_if_null(sci = ZenEditor_get_scintilla(self));

	if (PyArg_ParseTuple(args, "s|ii", &text, &sel_start, &sel_end))
	{
		tmp = ZenEditor_replace_caret_placeholder(self->caret_placeholder, text, &ph_pos);
		tmp2 = ZenEditor_replace_range(tmp);
		g_free(tmp);

		if (sel_start == -1 && sel_end == -1)
		{
			/* replace whole editor content */
			sci_set_text(sci, tmp2);
		}
		else if (sel_start != -1 && sel_end == -1)
		{
			/* insert text at sel_start */
			sci_insert_text(sci, sel_start, tmp2);
		}
		else if (sel_start != -1 && sel_end != -1)
		{
			/* replace from sel_start to sel_end */
			sci_set_selection_start(sci, sel_start);
			sci_set_selection_end(sci, sel_end);
			sci_replace_sel(sci, tmp2);
		}
		else
		{
			dbgf("Invalid arguments were supplied.");
			g_free(tmp2);
			Py_RETURN_NONE;
		}

		g_free(tmp2);

		/* Move cursor to first placeholder position, if found */
		if (ph_pos > -1)
			sci_set_current_position(sci, sel_start + ph_pos, TRUE);

	}
	else
	{
		if (PyErr_Occurred())
		{
			PyErr_Print();
			PyErr_Clear();
		}
	}

	Py_RETURN_NONE;
}


static PyObject *
ZenEditor_get_content(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	gchar *text;
	ScintillaObject *sci;

	print_called();
	py_return_none_if_null(sci = ZenEditor_get_scintilla(self));

	text = sci_get_contents(sci, sci_get_length(sci) + 1);
	py_return_none_if_null(text);

	result = PyString_FromString(text);
	g_free(text);
	py_return_none_if_null(result);

	return result;
}


static PyObject *
ZenEditor_get_selection(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	gchar *text;
	ScintillaObject *sci;

	print_called();
	py_return_none_if_null(sci = ZenEditor_get_scintilla(self));

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

	print_called();
	py_return_none_if_null(doc = ZenEditor_get_context(self));
	py_return_none_if_null(doc->file_name);

	result = Py_BuildValue("s", doc->file_name);
	return result;
}


static PyObject *
ZenEditor_prompt(ZenEditor *self, PyObject *args)
{
	PyObject *result;
	GtkWidget *dialog, *input, *content_area, *vbox;
	gchar *abbr = NULL;
	const gchar *title = NULL;

	print_called();

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

	py_return_none_if_null(abbr);

	if (strlen(abbr) == 0)
	{
		g_free(abbr);
		Py_RETURN_NONE;
	}

	result = PyString_FromString(abbr);
	g_free(abbr);
	return result;
}


static PyObject *
ZenEditor_set_context(ZenEditor *self, PyObject *args)
{
	PyObject *context = NULL, *tmp;

	print_called();

	if (PyArg_ParseTuple(args, "O", &context))
	{
		py_return_none_if_null(context);

		if (DOC_VALID((GeanyDocument *) PyLong_AsVoidPtr(context)))
		{
			tmp = self->context;
			Py_INCREF(context);
			self->context = context;
			Py_XDECREF(tmp);
		}
	}

	Py_RETURN_NONE;
}


static PyObject *
ZenEditor_get_profile_name(ZenEditor *self, PyObject *args)
{
	PyObject *result;

	print_called();

	if (self->active_profile != NULL && PyString_Check(self->active_profile))
	{
		result = PyString_FromString(PyString_AsString(self->active_profile));
		return result;
	}
	else
		return PyString_FromString("html");
}


static PyObject *
ZenEditor_set_profile_name(ZenEditor *self, PyObject *args)
{
	gchar *profile = NULL;
	PyObject *tmp, *temp_profile = NULL;

	print_called();

	if (PyArg_ParseTuple(args, "s", &profile))
	{
		if (profile != NULL)
		{
			temp_profile = PyString_FromString(profile);

			tmp = self->active_profile;
			Py_INCREF(temp_profile);
			self->active_profile = temp_profile;
			Py_XDECREF(tmp);

			Py_RETURN_TRUE;
		}
	}
	Py_RETURN_FALSE;
}


/*
 * FIXME: Write this in C using GKeyFile */
/* Reads all of the .conf files in the passed in directory, parses them into
 * Zen Coding profiles and then sets each of them up in using
 * zencoding.utils.setup_profile().
 */
static PyObject *
ZenEditor_init_profiles(ZenEditor *self, PyObject *args)
{
	gint result = -1;
	gchar *profiles_dir = NULL, *runcode = NULL;
	static gboolean has_run = 0;
#define RUNFMT																		\
	"import os\n"																	\
	"from ConfigParser import SafeConfigParser\n"									\
	"import zencoding.utils\n"														\
	"for cfg_file in os.listdir('%s'):\n"											\
	"	if cfg_file.endswith('.conf'):\n"											\
	"		cfg_file = os.path.join('%s', cfg_file)\n"								\
	"		p = SafeConfigParser()\n"												\
	"		p.read(cfg_file)\n"														\
	"		if p.has_section('profile') and p.has_option('profile', 'name'):\n"		\
	"			d = {}\n"															\
	"			name = p.get('profile', 'name')\n"									\
	"			if not name: continue\n"											\
	"			if p.has_option('profile', 'tag_case'):\n"							\
	"				d['case'] = p.get('profile', 'tag_case').lower()\n"				\
	"			if p.has_option('profile', 'attr_case'):\n"							\
	"				d['attr_case'] = p.get('profile', 'attr_case').lower()\n"		\
	"			if p.has_option('profile', 'attr_quotes'):\n"						\
	"				d['attr_quotes'] = p.get('profile', 'attr_quotes').lower()\n"	\
	"			if p.has_option('profile', 'tag_nl'):\n"							\
	"				if p.get('profile', 'tag_nl').lower() == 'decide':\n"			\
	"					d['tag_nl'] = 'decide'\n"									\
	"				else:\n"														\
	"					d['tag_nl'] = p.getboolean('profile', 'tag_nl')\n"			\
	"			if p.has_option('profile', 'place_cursor'):\n"						\
	"				d['place_cursor'] = p.getboolean('profile', 'place_cursor')\n"	\
	"			if p.has_option('profile', 'indent'):\n"							\
	"				d['indent'] = p.getboolean('profile', 'indent')\n"				\
	"			if p.has_option('profile', 'self_closing_tag'):\n"					\
	"				if p.get('profile', 'self_closing_tag').lower() == 'xhtml':\n"	\
	"					d['self_closing_tag'] = 'xhtml'\n"							\
	"				else:\n"														\
	"					d['self_closing_tag'] = p.getboolean('profile', \n"			\
	"												'self_closing_tag')\n"			\
	"			d['filters'] = None\n"												\
	"			zencoding.utils.setup_profile(name, d)\n"

	print_called();

	if (has_run)
		Py_RETURN_TRUE;

	if (PyArg_ParseTuple(args, "s", &profiles_dir))
	{
		py_return_none_if_null(profiles_dir);

		runcode = g_strdup_printf(RUNFMT, profiles_dir, profiles_dir);
		result = PyRun_SimpleString(runcode);
		g_free(runcode);
	}

	if (result == 0)
	{
		has_run = TRUE;
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;

#undef RUNFMT
}


static PyObject *
ZenEditor_get_syntax(ZenEditor *self, PyObject *args)
{
	print_called();
	return PyString_FromString("html");
}


static void
ZenEditor_dealloc(ZenEditor *self)
{
	print_called();
	Py_XDECREF(self->active_profile);
	Py_XDECREF(self->context);
	g_free(self->caret_placeholder);
	self->ob_type->tp_free((PyObject *) self);
}


static PyObject *
ZenEditor_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	ZenEditor *self;

	print_called();

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

	print_called();

	self->active_profile = PyString_FromString("xhtml");
	self->context = NULL;

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
	{"init_profiles", (PyCFunction)ZenEditor_init_profiles, METH_VARARGS},
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
		return NULL;

	m = Py_InitModule3("geany", Module_methods, "Geany Zen Coding module");

	Py_INCREF(&ZenEditorType);
	PyModule_AddObject(m, "ZenEditor", (PyObject *) &ZenEditorType);

	return m;
}
