Zen Coding plugin for Geany
===========================

**From the [Zen Coding website](https://code.google.com/p/zen-coding):**

[Zen Coding](https://code.google.com/p/zen-coding) is an editor plugin for
high-speed HTML, XML, XSL (or any other structured code format) coding and
editing. The core of this plugin is a powerful abbreviation engine which
allows you to expand expressions—similar to CSS selectors—into HTML code.
For example:

	div#page>div.logo+ul#navigation>li*5>a

...can be expanded into:

```html
<div id="page">
        <div class="logo"></div>
        <ul id="navigation">
                <li><a href=""></a></li>
                <li><a href=""></a></li>
                <li><a href=""></a></li>
                <li><a href=""></a></li>
                <li><a href=""></a></li>
        </ul>
</div>
```

[Read more about current Zen Coding syntax](https://code.google.com/p/zen-coding/wiki/ZenHTMLSelectorsEn)

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
home directory, typically `~/.config/geany/plugins/zencoding/profiles`.  These
are just standard config/ini files with the group `[profile]` and having the
key `name` as well as zero or more Zen Coding profile dictionary keys/values.

### Settings

You can edit the Zen Coding settings module by choosing the menu item
`Tools->Zen Coding->Open Zen Coding settings file`.  This will open the module
used to control the Zen Coding engine.  You can also just open and edit this
file from it's location on disk, typically
`~/.config/geany/plugins/zencoding/zencoding/zen_settings.py`.  The plugin
will detect when this file has been updated and it will reload itself.

#### Reset Settings

You can reset the settings file by just deleting it from your home directory,
typically `~/.config/geany/plugins/zencoding/zencoding/zen_settings.py`.

Dependencies
------------

* Geany plugin development package (including GTK+/GLib)
* Python 2.6+ development package

Hacking
-------

There's a few parts to the Zen Coding plugin.  The first, obviously, is the
actual Zen Coding Python code.  You can easily upgrade the Zen Coding portion
of the plugin by either replacing the relevant Zen Coding Python modules,
typically in `~/.config/geany/plugins/zencoding/zencoding` or deleting those
package files and replacing the system-wide Zen Coding package files, typically
in `/usr/local/lib/geany/zencoding`.  The second part is the `ZenEngine` Python
class which is found in `engine.py`.  This class is designed for easy access
from the C code which interacts with it.  The third part is a small helper
library, `libzencoding`, which glues the Python `ZenEngine` class into C-land
using the Python C API.  Lastly, the fourth part is `plugin.c` which is the
actual Geany plugin that uses the `ZenEngine` glue and controls interaction
from the Geany interface to the `libzencoding` library.

Bugs
----

* No support for changing doc_type.
* Only system-wide profiles currently loaded.
* Need better icons.
