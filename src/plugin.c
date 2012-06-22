/*
 * plugin.c
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
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <geanyplugin.h>

#include "zen-controller.h"
#include "zen-editor.h"


GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;

#ifndef ZEN_MODULE_PATH
#define ZEN_MODULE_PATH "/usr/local/lib/geany"
#endif

#ifndef ZEN_PROFILES_PATH
#define ZEN_PROFILES_PATH "/usr/local/share/geany/zencoding/profiles"
#endif

#ifndef ZEN_ICONS_PATH
#define ZEN_ICONS_PATH "/usr/local/share/geany/zencoding/icons"
#endif


#define ZENCODING_ICON	ZEN_ICONS_PATH "/zencoding.png"
#define EXPAND_ICON		ZEN_ICONS_PATH "/expand.png"
#define WRAP_ICON		ZEN_ICONS_PATH "/wrap.png"


PLUGIN_VERSION_CHECK(200)

PLUGIN_SET_INFO(_("Zen Coding"),
				_("Zen Coding plugin for Geany"),
				"1.0",
				"Matthew Brush <mbrush@codebrainz.ca>");


static struct ZenCodingPlugin
{
	GtkWidget*		main_menu_item;
	gchar*			config_dir;
	GFileMonitor*	monitor;
	GFile*			settings_file;
	const gchar*	active_profile;
	ZenController*	zen_controller;
}
plugin;


enum
{
	ACTION_EXPAND_ABBR,
	ACTION_EXPAND_ABBR_TAB,
	ACTION_MATCH_IN,
	ACTION_MATCH_OUT,
	ACTION_WRAP_ABBR,
	ACTION_PREV_POINT,
	ACTION_NEXT_POINT,
	ACTION_INSERT_FORMAT_NEWLINE,
	ACTION_SELECT_LINE,
	ACTION_GO_TO_MATCHING_PAIR,
	ACTION_MERGE_LINES,
	ACTION_TOGGLE_COMMENTS,
	ACTION_SPLIT_JOIN_TAG,
	ACTION_REMOVE_TAG,
	/*ACTION_ENCODE_DECODE_BASE64,*/
	ACTION_INCREMENT_NUMBER_BY_1,
	ACTION_INCREMENT_NUMBER_BY_10,
	ACTION_INCREMENT_NUMBER_BY_01,
	ACTION_DECREMENT_NUMBER_BY_1,
	ACTION_DECREMENT_NUMBER_BY_10,
	ACTION_DECREMENT_NUMBER_BY_01,
	ACTION_EVALUATE_MATH_EXPRESSION,
	ACTION_LAST
};


typedef struct _ZenCodingAction ZenCodingAction;

static struct _ZenCodingAction
{
	const gchar *name;
	const gchar *blurb;
	guint key;
	GdkModifierType mod;
}
actions[ACTION_LAST] = {

	{ "expand_abbreviation", _("Expand Abbreviation"), GDK_e, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "expand_abbreviation_with_tab", _("Expand Abbreviation with Tab"), GDK_T, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "match_pair_inward", _("Match Tag Inward"), GDK_L, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "match_pair_outward", _("Match Tag Outward"), GDK_R, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "wrap_with_abbreviation", _("Wrap with Abbreviation"), GDK_q, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "prev_edit_point", _("Previous Edit Point"), GDK_p, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "next_edit_point", _("Next Edit Point"), GDK_n, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "insert_formatted_newline", _("Insert Formatted Newline"), GDK_l, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "select_line", _("Select Line"), GDK_s, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "go_to_matching_pair", _("Go to Matching Pair"), GDK_m, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "merge_lines", _("Merge Lines"), GDK_b, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "toggle_comment", _("Toggle Comment"), GDK_c, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "split_join_tag", _("Split or Join Tag"), GDK_j, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	{ "remove_tag", _("Remove Tag"), GDK_r, GDK_SHIFT_MASK | GDK_CONTROL_MASK },
	/*{ "encode_decode_base64", _("Encode/Decode to/from Base64"), GDK_6, GDK_SHIFT_MASK | GDK_CONTROL_MASK },*/
	{ "increment_number_by_1", _("Increment Number by 1"), 0, 0 },
	{ "increment_number_by_10", _("Increment Number by 10"), 0, 0 },
	{ "increment_number_by_01", _("Increment Number by 0.1"), 0, 0 },
	{ "decrement_number_by_1", _("Decrement Number by 1"), 0, 0 },
	{ "decrement_number_by_10", _("Decrement Number by 10"), 0, 0 },
	{ "decrement_number_by_01", _("Decrement Number by 0.1"), 0, 0 },
	{ "evaluate_math_expression", _("Evaluate Math Expression"), 0, 0 }

};


static void action_activate(guint key_id)
{
	ZenCodingAction action;

	g_return_if_fail(key_id >= 0 && key_id < ACTION_LAST);

	action = actions[key_id];
	zen_controller_run_action(plugin.zen_controller, action.name);
	ui_set_statusbar(FALSE, "Zen Coding: Running '%s' action", action.blurb);
}


static void on_action_item_activate(GObject *object, gpointer id_ptr)
{
	gint id = GPOINTER_TO_INT(id_ptr);
	ZenCodingAction action;

	g_return_if_fail(id >= 0 && id < ACTION_LAST);

	action = actions[id];
	zen_controller_run_action(plugin.zen_controller, action.name);
	ui_set_statusbar(FALSE, "Zen Coding: Running '%s' action", action.blurb);
}


static void initialize_actions(GtkMenu *menu)
{
	gint i;
	GeanyKeyGroup *group;
	ZenCodingAction action;
	GtkWidget *item;

	group = plugin_set_key_group(geany_plugin, "zencoding", ACTION_LAST, NULL);

	for (i = 0; i < ACTION_LAST; i++)
	{
		action = actions[i];

		item = gtk_menu_item_new_with_label(action.blurb);
		g_signal_connect(item, "activate", G_CALLBACK(on_action_item_activate),
			GINT_TO_POINTER(i));
		keybindings_set_item(group, i, action_activate, action.key, action.mod,
			action.name, action.blurb, item);
		gtk_menu_append(menu, item);
		gtk_widget_show(item);
	}
}


static void init_config(struct ZenCodingPlugin *plugin);


static void on_profile_toggled(GtkCheckMenuItem *item, const gchar *profile_name)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
	{
		zen_controller_set_active_profile(plugin.zen_controller, profile_name);
		ui_set_statusbar(TRUE, _("Zen Coding: Selected profile '%s'"),
			gtk_menu_item_get_label(GTK_MENU_ITEM(item)));
	}
}


static void on_settings_activate(GtkMenuItem *item, struct ZenCodingPlugin *plugin)
{
	gchar *fn;

	fn = g_build_filename(plugin->config_dir, "zencoding", "zen_settings.py", NULL);

	if (g_file_test(fn, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
		(void) document_open_file(fn, FALSE, NULL, NULL);

	g_free(fn);
}


static void build_zc_menu(struct ZenCodingPlugin *plugin)
{
	GtkWidget *img;
	GtkWidget *item, *menu, *pmenu;
	GDir *dir;
	GSList *group = NULL;
	const gchar *ent;
	gchar *name;

	menu = gtk_menu_new();

	plugin->main_menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Zen Coding"));
	img = gtk_image_new_from_file(ZENCODING_ICON);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(plugin->main_menu_item), img);
	ui_add_document_sensitive(plugin->main_menu_item);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(plugin->main_menu_item), menu);

	initialize_actions(GTK_MENU(menu));

	item = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_image_menu_item_new_with_label(_("Set Profile"));
	img = gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	pmenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), pmenu);
	gtk_menu_append(GTK_MENU(menu), item);

	/* Geany default profile */
	item = gtk_radio_menu_item_new_with_label(group, _("Default"));
	g_object_set_data(G_OBJECT(item), "profile_name", "geany_profile");
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	/* TODO: store/read this from file from last run */
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	gtk_menu_append(GTK_MENU(pmenu), item);
	g_signal_connect(item, "toggled", G_CALLBACK(on_profile_toggled), "geany_profile");

	/* Built in profiles */
	item = gtk_radio_menu_item_new_with_label(group, _("Plain"));
	g_object_set_data(G_OBJECT(item), "profile_name", "plain");
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
	gtk_menu_append(GTK_MENU(pmenu), item);
	g_signal_connect(item, "toggled", G_CALLBACK(on_profile_toggled), "plain");

	item = gtk_radio_menu_item_new_with_label(group, _("HTML"));
	g_object_set_data(G_OBJECT(item), "profile_name", "html");
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
	gtk_menu_append(GTK_MENU(pmenu), item);
	g_signal_connect(item, "toggled", G_CALLBACK(on_profile_toggled), "html");

	item = gtk_radio_menu_item_new_with_label(group, _("XHTML"));
	g_object_set_data(G_OBJECT(item), "profile_name", "xhtml");
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
	gtk_menu_append(GTK_MENU(pmenu), item);
	g_signal_connect(item, "toggled", G_CALLBACK(on_profile_toggled), "xhtml");

	item = gtk_radio_menu_item_new_with_label(group, _("XML"));
	g_object_set_data(G_OBJECT(item), "profile_name", "xml");
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
	gtk_menu_append(GTK_MENU(pmenu), item);
	g_signal_connect(item, "toggled", G_CALLBACK(on_profile_toggled), "xml");

#if 0
	dir = g_dir_open(ZEN_PROFILES_PATH, 0, NULL);
	if (dir != NULL)
	{

		while ((ent = g_dir_read_name(dir)) != NULL)
		{
			/* TODO: add error handling */
			gchar *p = g_build_filename(ZEN_PROFILES_PATH, ent, NULL);
			GKeyFile *kf = g_key_file_new();
			g_key_file_load_from_file(kf, p, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
			if (g_key_file_has_group(kf, "profile") && g_key_file_has_key(kf, "profile", "name", NULL))
			{
				name = g_key_file_get_string(kf, "profile", "name", NULL);
				item = gtk_radio_menu_item_new_with_label(group, name);
				g_object_set_data_full(G_OBJECT(item), "profile_name", name, g_free);
				group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
				gtk_menu_append(GTK_MENU(pmenu), item);
				g_signal_connect(item, "toggled", G_CALLBACK(on_profile_toggled), name);
			}
		}
		g_dir_close(dir);
	}
#endif

	item = gtk_image_menu_item_new_with_label(_("Open Zen Coding Settings File"));
	img = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect(item, "activate", G_CALLBACK(on_settings_activate), plugin);
	gtk_menu_append(GTK_MENU(menu), item);

	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), plugin->main_menu_item);

	gtk_widget_show_all(plugin->main_menu_item);
}


static void reload_zen_coding_notice(const gchar *msg)
{
	GtkWidget *dialog;
	static gboolean is_showing = FALSE;

	g_return_if_fail(msg != NULL);

	if (!is_showing) /* prevent showing multiple times */
	{
		is_showing = TRUE;
		dialog = gtk_message_dialog_new(
					GTK_WINDOW(geany->main_widgets->window),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_OK,
					"%s", msg);

		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		is_showing = FALSE;
	}
}


static void on_monitor_changed(GFileMonitor *monitor, GFile *file,
	GFile *other_file, GFileMonitorEvent event_type, struct ZenCodingPlugin *plugin)
{
	if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
	{
		reload_zen_coding_notice(
				"The Zen Coding settings file has changed, you need to "
				"reload the Zen Coding plugin or restart Geany for the "
				"settings to take effect.");
	}
	else if (event_type == G_FILE_MONITOR_EVENT_DELETED ||
		event_type == G_FILE_MONITOR_EVENT_MOVED)
	{
		reload_zen_coding_notice(
			"The Zen Coding settings file has been deleted.  It will be "
			"recreated when you reload the Zen Coding plugin or restart "
			"Geany.");
	}
}


static gboolean copy_file(const gchar *src, const gchar *dst)
{
	gboolean result = TRUE;
	gint ch;
	FILE *fpsrc, *fpdst;

	fpsrc = fopen(src, "rb");
	if (fpsrc == NULL)
		return FALSE;

	fpdst = fopen(dst, "wb");
	if (fpdst == NULL)
		return FALSE;

	while (!feof(fpsrc))
	{
		ch = getc(fpsrc);
		if (ferror(fpsrc))
		{
			result = FALSE;
			break;
		}
		else
		{
			if (!feof(fpsrc))
				putc(ch, fpdst);
			if (ferror(fpdst))
			{
				result = FALSE;
				break;
			}
		}
	}

	fclose(fpsrc);
	fclose(fpdst);

	return result;
}


static void recursively_copy(const char *src, const char *dst)
{
	GDir *dir;

	char *src_fn, *dst_fn;
	const char *ent;

	dir = g_dir_open(src, 0, NULL);

	while ((ent = g_dir_read_name(dir)) != NULL)
	{
		if (g_str_equal(ent, ".") || g_str_equal(ent, ".."))
			continue;

		src_fn = g_build_filename(src, ent, NULL);
		dst_fn = g_build_filename(dst, ent, NULL);

		if (g_file_test(src_fn, G_FILE_TEST_IS_DIR))
		{
			if (!g_file_test(dst_fn, G_FILE_TEST_IS_DIR))
				g_mkdir_with_parents(dst_fn, 0700);
			recursively_copy(src_fn, dst_fn);
		}
		else if (g_file_test(src_fn, G_FILE_TEST_IS_REGULAR) &&
					g_str_has_suffix(src_fn, ".py"))
		{
			if (!copy_file(src_fn, dst_fn))
				g_warning("An error occurred copy file '%s' to '%s'.", src_fn, dst_fn);
		}

		g_free(src_fn);
		g_free(dst_fn);
	}
}


static void init_config(struct ZenCodingPlugin *plugin)
{
	gint i;
	gchar *tmp, *settings, *sys_path, *user_path, *code = NULL;

	g_free(plugin->config_dir);
	plugin->config_dir = g_build_filename(geany->app->configdir, "plugins", "zencoding", NULL);

	if (plugin->monitor != NULL)
		g_object_unref(plugin->monitor);
	if (plugin->settings_file != NULL)
		g_object_unref(plugin->settings_file);

	tmp = g_build_filename(plugin->config_dir, "zencoding", NULL);

	if (!g_file_test(tmp, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir_with_parents(tmp, 0700) == -1)
		{
			g_debug("Failed making dir '%s'", tmp);
			g_free(tmp);
			return;
		}
	}

	sys_path = g_build_filename(ZEN_MODULE_PATH, "zencoding", NULL);

	recursively_copy(sys_path, tmp);

	settings = g_build_filename(tmp, "zen_settings.py", NULL);
	plugin->settings_file = g_file_new_for_path(settings);
	g_free(settings);

	plugin->monitor = g_file_monitor_file(plugin->settings_file,
						G_FILE_MONITOR_NONE, NULL, NULL);

	if (plugin->monitor != NULL)
	{
		g_signal_connect(plugin->monitor, "changed",
			G_CALLBACK(on_monitor_changed), plugin);
	}

	g_free(tmp);
	g_free(sys_path);
}


#ifdef ZEN_EDITOR_DEBUG
static gchar *python_version(void)
{
	gchar *version;
	gchar *delim;

	version = g_strdup(Py_GetVersion());

	if ((delim = strchr(version, ' ')) == NULL)
		return NULL;

	*delim = '\0';

	return version;
}
#endif


void plugin_init(GeanyData *data)
{
	memset(&plugin, 0, sizeof(struct ZenCodingPlugin));

	build_zc_menu(&plugin);

	init_config(&plugin);

	plugin.zen_controller = zen_controller_new(plugin.config_dir, ZEN_PROFILES_PATH);

	zen_controller_set_active_profile(plugin.zen_controller, "xhtml");

#ifdef ZEN_EDITOR_DEBUG
	gchar *pyversion;
	pyversion = python_version();
	g_print("Zen Coding Plugin - Version Information\n"
			"---------------------------------------\n"
			"  GTK+ Version: %d.%d.%d\n"
			"  GLib Version: %d.%d.%d\n"
			"  Python Version: %s\n",
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			glib_major_version, glib_minor_version, glib_micro_version,
			pyversion);
	g_free(pyversion);
#endif
}


void plugin_cleanup(void)
{
	gtk_widget_destroy(plugin.main_menu_item);
	g_free(plugin.config_dir);
	g_object_unref(plugin.settings_file);
	g_object_unref(plugin.monitor);
	zen_controller_free(plugin.zen_controller);
}
