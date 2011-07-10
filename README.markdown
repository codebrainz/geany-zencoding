Zen Coding plugin for Geany
===========================

Installation
------------

Check out the sources from GitHub:

	$ mkdir -p ~/src
	$ cd ~/src
	$ git clone git://github.com/codebrainz/geany-zencoding.git

Then change to the Geany Zen Coding plugin directory and generate the required
build system files:

	$ cd geany-zencoding
	$ ./autogen.sh

Now configure, compile and install the build system:

	$ ./configure --prefix=$(dirname $(dirname $(which geany))) # this is Geany's prefix
	$ make
	$ make install # maybe as root depending on prefix

After that you can run geany and use the plugin:

	$ geany -v

Activating the Plugin
---------------------

Open the Plugin Manager by selecting `Tools->Plugin Manager`.  Find the Zen
Coding plugin in the list and check the box next to it.

De-Activating the Plugin
------------------------

Open the Plugin Manager by selecting `Tools->Plugin Manager`.  Find the Zen
Coding plugin in the list and uncheck the box next to it.

Using the Plugin
----------------

The first thing you will probably want to do is change the keybindings for
the Zen Coding plugin from their defaults.  To do this, go to
`Edit->Preferences` and then select the `Key Bindings` tab.  Find the
`Zen Coding` group of keybindings and change either of the two keybindings to
your preference.

_Note:_ I would welcome suggestions for key bindings to use which do not
conflict with Geany's default keybindings.  The current defaults are
`Ctrl+Shift+e` for `Expand Abbreviation` and `Ctrl+Shift+q` for `Wrap with
Abbreviation`.

### Expand Abbreviation:

To use this, type a Zen Coding abbreviation and then activate the keybinding
for `Expand Abbreviation`.  The abbreviation will be replaced with the expanded
text if it was valid.  You can also activate this action by selecting the menu
item `Tools->Zen Coding->Expand Abbreviation`.

### Wrap with Abbreviation

To use this, select text to be wrapped and then activate the keybinding for
`Wrap with Abbreviation`.  You will be prompted for an abbreviation and then the
selected text will be wrapped with the expanded text of the supplied
abbreviation.  You can also activate this action by selecting the menu item
`Tools->Zen Coding->Wrap with Abbreviation`.

### Profiles

You can select the profile you wish to use by selecting a profile under
`Tools->Zen Coding->Profile`.  Custom profiles can be installed under your
home directory, typically `~/.config/geanyplugins/zencoding/profiles`.  These
are just standard config/ini files with the group `[profile]` and having the
key `name` as well as zero or more Zen Coding profile dictionary keys/values.

### Settings

You can edit the Zen Coding settings module by choosing the menu item
`Tools->Zen Coding->Open Zen Coding settings file`.  This will open the module
used to control the Zen Coding engine.  You can also just open and edit this
file from it's location on disk, typically
`~/.config/geany/plugins/zencoding/zen_settings.py`.

#### Reset Settings

You can reset the settings file by just deleting it from your home directory,
typically `~/.config/geany/plugins/zencoding/zen_settings.py`.

Dependencies
------------

* Geany plugin development package (including GTK+/GLib)
* Python 2.6+
