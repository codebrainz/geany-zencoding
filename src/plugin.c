#include "zen-engine.h"

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <geanyplugin.h>


GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;

#define ZENCODING_ICON	ZEN_ENGINE_ICONS_PATH "/zencoding.png"
#define EXPAND_ICON		ZEN_ENGINE_ICONS_PATH "/expand.png"
#define WRAP_ICON		ZEN_ENGINE_ICONS_PATH "/wrap.png"

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
	const gchar*	doc_type;
	ZenEngine*		zen;
}
plugin;


static void init_config(struct ZenCodingPlugin *plugin);


static void on_expand_abbreviation(GtkMenuItem *menuitem, struct ZenCodingPlugin *plugin)
{
	int current_line, current_pos, line_pos, rel_pos, ret_abbr_pos = 0;
	int insert_start, insert_end;
	char *line, *ret_abbr = NULL, *text;
	GeanyDocument *doc;

	doc = document_get_current();
	if (doc == NULL || doc->editor == NULL || doc->editor->sci == NULL)
		return;

	current_line = sci_get_current_line(doc->editor->sci);
	line = sci_get_line(doc->editor->sci, current_line);

	current_pos = sci_get_current_position(doc->editor->sci);
	line_pos = sci_get_position_from_line(doc->editor->sci, current_line);
	rel_pos = current_pos - line_pos;

	if (line == NULL)
		return;

	if (strlen(line) == 0)
	{
		g_free(line);
		return;
	}

	text = zen_engine_expand_abbreviation(plugin->zen, line, rel_pos, &ret_abbr, &ret_abbr_pos);
	if (text != NULL)
	{
		insert_start = line_pos + ret_abbr_pos;
		insert_end = insert_start + strlen(ret_abbr);

		sci_set_selection_start(doc->editor->sci, insert_start);
		sci_set_selection_end(doc->editor->sci, insert_end);
		sci_replace_sel(doc->editor->sci, text);

		g_free(ret_abbr);
		g_free(text);
	}
	else
	{
		ui_set_statusbar(FALSE,
			_("Zen Coding: Unable to expand abbreviation near: %s"),
			g_strstrip(line));
	}

	g_free(line);
}


static void handle_wrap_abbreviation(struct ZenCodingPlugin *plugin, const gchar *abbr)
{
	char *text;
	GeanyDocument *doc = document_get_current();

	if (doc == NULL || doc->editor == NULL || doc->editor->sci == NULL)
		return;

	gint sel_start = sci_get_selection_start(doc->editor->sci);
	gint sel_end = sci_get_selection_end(doc->editor->sci);
	gchar *sel_text = sci_get_selection_contents(doc->editor->sci);

	/*
	g_debug("Abbreviation entered: %s", abbr);
	g_debug("Selected text starting at '%d', ending at '%d'", sel_start, sel_end);
	g_debug("Actual text: %s", sel_text);
	*/

	text = zen_engine_wrap_with_abbreviation(plugin->zen, abbr, sel_text);

	if (text != NULL)
	{
		sci_replace_sel(doc->editor->sci, text);
		g_free(text);
	}
	else
		ui_set_statusbar(FALSE, "Unable to wrap with abbreviation '%s'", abbr);

	g_free(sel_text);
}


#if GTK_CHECK_VERSION(2, 18, 0)

static void on_info_bar_response(GtkInfoBar *info_bar, guint response_id,
	struct ZenCodingPlugin *plugin)
{
	GtkWidget *parent, *grandparent;
	GtkWidget *nb;
	GtkEntry *entry;
	GeanyDocument *doc;

	if (response_id == GTK_RESPONSE_ACCEPT)
	{
		entry = GTK_ENTRY(g_object_get_data(G_OBJECT(info_bar), "entry"));
		handle_wrap_abbreviation(plugin, gtk_entry_get_text(entry));
	}

	parent = gtk_widget_get_parent(GTK_WIDGET(info_bar)); /* our vbox */
	grandparent = gtk_widget_get_parent(GTK_WIDGET(parent)); /* hpaned or devhelp main notebook */

	nb = gtk_widget_ref(geany->main_widgets->notebook);
	gtk_container_remove(GTK_CONTAINER(parent), nb);
	gtk_container_remove(GTK_CONTAINER(grandparent), GTK_WIDGET(parent));

	gtk_container_add(GTK_CONTAINER(grandparent), nb);
	gtk_widget_show_all(nb);
	gtk_widget_unref(nb);

	doc = document_get_current();
	if (doc != NULL && doc->editor != NULL && doc->editor->sci != NULL)
		gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
}

/* TODO: make this handle the Devhelp plugin's main notebook */
static char *do_wrap_abbreviation(struct ZenCodingPlugin *plugin)
{
	GtkWidget *info_bar, *input, *content_area;
	GtkWidget *vbox, *parent;
	GtkWidget *nb;

	info_bar = gtk_info_bar_new_with_buttons(
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_REJECT,
					GTK_STOCK_OK,
					GTK_RESPONSE_ACCEPT,
					NULL);

	content_area = gtk_info_bar_get_content_area(GTK_INFO_BAR(info_bar));

	input = gtk_entry_new();
	gtk_widget_show(input);
	gtk_entry_set_activates_default(GTK_ENTRY(input), TRUE);
	gtk_container_add(GTK_CONTAINER(content_area), input);

	g_object_set_data(G_OBJECT(info_bar), "entry", GTK_ENTRY(input));
	g_signal_connect(info_bar, "response", G_CALLBACK(on_info_bar_response), plugin);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	parent = gtk_widget_get_parent(geany->main_widgets->notebook);

	gtk_box_pack_start(GTK_BOX(vbox), info_bar, FALSE, TRUE, 0);

	nb = gtk_widget_ref(geany->main_widgets->notebook);
	gtk_container_remove(GTK_CONTAINER(parent), geany->main_widgets->notebook);
	gtk_container_add(GTK_CONTAINER(parent), vbox);
	gtk_box_pack_start(GTK_BOX(vbox), nb, TRUE, TRUE, 0);
	gtk_widget_unref(nb);

	gtk_info_bar_set_default_response(GTK_INFO_BAR(info_bar), GTK_RESPONSE_ACCEPT);
	gtk_widget_show(info_bar);
	gtk_window_set_focus(GTK_WINDOW(geany->main_widgets->window), input);
}

#else

static char *do_wrap_abbreviation(struct ZenCodingPlugin *plugin)
{
	GtkWidget *dialog, *input, *content_area, *vbox;
	gchar *abbr = NULL;
	GeanyDocument *doc;

	dialog = gtk_dialog_new_with_buttons("Enter Abbreviation",
				GTK_WINDOW(geany->main_widgets->window),
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
	{
		handle_wrap_abbreviation(plugin, gtk_entry_get_text(GTK_ENTRY(input)));

		doc = document_get_current();
		if (doc != NULL && doc->editor != NULL && doc->editor->sci != NULL)
			gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
	}

	gtk_widget_destroy(dialog);

	return abbr;
}

#endif /* GTK_CHECK_VERSION(2, 18, 0) - for GtkInfo support */

static void on_expand_wrap(GtkMenuItem *menuitem, struct ZenCodingPlugin *plugin)
{
	do_wrap_abbreviation(plugin);
}


static void on_profile_toggled(GtkCheckMenuItem *item, const gchar *profile_name)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
	{
		zen_engine_set_active_profile(plugin.zen, profile_name);
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

	dir = g_dir_open(ZEN_ENGINE_PROFILES_PATH, 0, NULL);
	if (dir != NULL)
	{

		while ((ent = g_dir_read_name(dir)) != NULL)
		{
			/* TODO: add error handling */
			gchar *p = g_build_filename(ZEN_ENGINE_PROFILES_PATH, ent, NULL);
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
		if (plugin->zen != NULL)
			zen_engine_free(plugin->zen);

		plugin->zen = zen_engine_new(plugin->doc_type, plugin->config_dir,
						plugin->active_profile, ZEN_ENGINE_PROFILES_PATH);

		if (plugin->zen == NULL)
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


static void init_config(struct ZenCodingPlugin *plugin)
{
	gint i;
	gchar *tmp, *settings, *sys_path, *user_path, *code = NULL;
	gchar *mods[7] = {
		"__init__.py",
		"engine.py",
		"html_matcher.py",
		"htmlparser.py",
		"stparser.py",
		"zen_core.py",
		"zen_settings.py"
	};

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

	for (i = 0; i < 7; i++)
	{
		sys_path = g_build_filename(ZEN_ENGINE_MODULE_PATH, "zencoding", mods[i], NULL);
		user_path = g_build_filename(tmp, mods[i], NULL);

		if (!g_file_test(user_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
		{
			if (g_file_get_contents(sys_path, &code, NULL, NULL))
			{
				g_file_set_contents(user_path, code, -1, NULL);
				g_free(code);
				code = NULL;
			}
		}

		g_free(sys_path);
		g_free(user_path);
	}

	settings = g_build_filename(tmp, "zen_settings.py", NULL);
	plugin->settings_file = g_file_new_for_path(settings);
	g_free(settings);

	plugin->monitor = g_file_monitor_file(plugin->settings_file,
						G_FILE_MONITOR_NONE, NULL, NULL);
	if (plugin->monitor != NULL)
		g_signal_connect(plugin->monitor, "changed", G_CALLBACK(on_monitor_changed), plugin);

	g_free(tmp);
}


void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *kg;

	memset(&plugin, 0, sizeof(struct ZenCodingPlugin));

	kg = plugin_set_key_group(geany_plugin, "zencoding", 2, NULL);

	build_zc_menu(kg, &plugin);

	init_config(&plugin);

	plugin.active_profile = "geany_profile";
	plugin.doc_type = "html"; /* fixme */

	plugin.zen = zen_engine_new(plugin.doc_type, ZEN_ENGINE_MODULE_PATH,
					plugin.active_profile, ZEN_ENGINE_PROFILES_PATH);

	if (plugin.zen == NULL)
		g_warning(_("Failed initializing Zen Coding engine"));
}


void plugin_cleanup(void)
{
	gtk_widget_destroy(plugin.main_menu_item);
	g_free(plugin.config_dir);
	g_object_unref(plugin.settings_file);
	g_object_unref(plugin.monitor);
	zen_engine_free(plugin.zen);
}
