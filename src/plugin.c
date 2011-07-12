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




static void init_config(struct ZenCodingPlugin *plugin);


static void on_expand_abbreviation(GtkMenuItem *menuitem, struct ZenCodingPlugin *plugin)
{
	zen_controller_run_action(plugin->zen_controller, "expand_abbreviation");
}

static void on_expand_wrap(GtkMenuItem *menuitem, struct ZenCodingPlugin *plugin)
{
	zen_controller_run_action(plugin->zen_controller, "wrap_with_abbreviation");
}


static void on_profile_toggled(GtkCheckMenuItem *item, const gchar *profile_name)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
	{
		zen_controller_set_active_profile(plugin.zen_controller, profile_name);
		ui_set_statusbar(TRUE, _("Zen Coding: Selected profile '%s'"),
			gtk_menu_item_get_label(GTK_MENU_ITEM(item)));
	}
}


static void on_expand_abbreviation_keybinding(guint key_id)
{
	on_expand_abbreviation(NULL, &plugin);
}


static void on_expand_wrap_keybinding(guint key_id)
{
	on_expand_wrap(NULL, &plugin);
}


static void on_settings_activate(GtkMenuItem *item, struct ZenCodingPlugin *plugin)
{
	gchar *fn;

	fn = g_build_filename(plugin->config_dir, "zencoding", "zen_settings.py", NULL);

	if (g_file_test(fn, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
		(void) document_open_file(fn, FALSE, NULL, NULL);

	g_free(fn);
}


static void build_zc_menu(GeanyKeyGroup *kg, struct ZenCodingPlugin *plugin)
{
	GtkWidget *img;
	GtkWidget *item, *menu, *pmenu;
	GDir *dir;
	GSList *group = NULL;
	const gchar *ent;
	gchar *name;

	menu = gtk_menu_new();

	plugin->main_menu_item = gtk_image_menu_item_new_with_label(_("Zen Coding"));
	img = gtk_image_new_from_file(ZENCODING_ICON);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(plugin->main_menu_item), img);
	ui_add_document_sensitive(plugin->main_menu_item);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(plugin->main_menu_item), menu);

	item = gtk_image_menu_item_new_with_label(_("Expand Abbreviation"));
	img = gtk_image_new_from_file(EXPAND_ICON);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect(item, "activate", G_CALLBACK(on_expand_abbreviation), plugin);
	gtk_menu_append(GTK_MENU(menu), item);
	keybindings_set_item(kg, 0,
		(GeanyKeyCallback) on_expand_abbreviation_keybinding,
		GDK_e, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "expand_abbreviation",
		_("Expand Abbreviation"),
		item);

	item = gtk_image_menu_item_new_with_label(_("Wrap with Abbreviation"));
	img = gtk_image_new_from_file(WRAP_ICON);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect(item, "activate", G_CALLBACK(on_expand_wrap), plugin);
	gtk_menu_append(GTK_MENU(menu), item);
	keybindings_set_item(kg, 1,
		(GeanyKeyCallback) on_expand_wrap_keybinding,
		GDK_q, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "wrap_abbreviation",
		_("Wrap with Abbreviation"),
		item);

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

	item = gtk_image_menu_item_new_with_label(_("Open Zen Coding Settings File"));
	img = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect(item, "activate", G_CALLBACK(on_settings_activate), plugin);
	gtk_menu_append(GTK_MENU(menu), item);

	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), plugin->main_menu_item);

	gtk_widget_show_all(plugin->main_menu_item);
}


static void on_monitor_changed(GFileMonitor *monitor, GFile *file,
	GFile *other_file, GFileMonitorEvent event_type, struct ZenCodingPlugin *plugin)
{
	if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
		event_type == G_FILE_MONITOR_EVENT_CREATED)
	{
		if (plugin->zen_controller != NULL)
			zen_controller_free(plugin->zen_controller);

		plugin->zen_controller = zen_controller_new(plugin->config_dir);

		if (plugin->zen_controller == NULL)
			g_warning(_("Failed re-initializing Zen Coding after settings change detected"));
		else
			ui_set_statusbar(TRUE, _("Zen Coding: Re-initialized after settings change detected"));
	}
	else if (event_type == G_FILE_MONITOR_EVENT_DELETED ||
		event_type == G_FILE_MONITOR_EVENT_MOVED)
	{
		init_config(plugin);
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
			(g_str_has_suffix(src_fn, ".py") || g_str_has_suffix(src_fn, ".so")))
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
		g_signal_connect(plugin->monitor, "changed", G_CALLBACK(on_monitor_changed), plugin);

	g_free(tmp);
	g_free(sys_path);
}


void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *kg;

	/*
	if (!g_thread_supported())
		g_thread_init(NULL);
	*/

	memset(&plugin, 0, sizeof(struct ZenCodingPlugin));

	kg = plugin_set_key_group(geany_plugin, "zencoding", 2, NULL);

	build_zc_menu(kg, &plugin);

	init_config(&plugin);

	plugin.zen_controller = zen_controller_new(plugin.config_dir);

	zen_controller_set_active_profile(plugin.zen_controller, "xhtml");

	g_print("Zen Coding Plugin - Version Information\n"
			"---------------------------------------\n"
			"  GTK+ Version: %d.%d.%d\n"
			"  GLib Version: %d.%d.%d\n"
			"  Python Version: %s\n",
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			glib_major_version, glib_minor_version, glib_micro_version,
			Py_GetVersion());
}


void plugin_cleanup(void)
{
	gtk_widget_destroy(plugin.main_menu_item);
	g_free(plugin.config_dir);
	g_object_unref(plugin.settings_file);
	g_object_unref(plugin.monitor);
	zen_controller_free(plugin.zen_controller);
}
