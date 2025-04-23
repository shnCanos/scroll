scroll(1)

# NAME

scroll - An i3-compatible Wayland compositor with a scrolling layout

# SYNOPSIS

*scroll* [options...] [command]

# OPTIONS

*-h, --help*
	Show help message and quit.

*-c, --config* <config>
	Specifies a config file.

*-C, --validate*
	Check the validity of the config file, then exit.

*-d, --debug*
	Enables full logging, including debug information.

*-v, --version*
	Show the version number and quit.

*-V, --verbose*
	Enables more verbose logging.

*--get-socketpath*
	Gets the IPC socket path and prints it, then exits.

# DESCRIPTION

sway was created to fill the need of an i3-like window manager for Wayland. The
upstream i3 developers have no intention of porting i3 to Wayland, and projects
proposed by others ended up as vaporware. Many thanks to the i3 folks for
providing such a great piece of software, so good that your users would rather
write an entirely new window manager from scratch that behaved _exactly_ like i3
rather than switch to something else.

sway is feature complete. So scroll was forked from it, to replace i3 default
layouts with a scrolling desktop, and add new features.

You can run scroll directly from a tty, or via a Wayland-compatible login manager.

# CONFIGURATION

scroll searches for a config file in the following locations, in this order:

. ~/.scroll/config
. $XDG_CONFIG_HOME/scroll/config (suggested location)
. ~/.i3/config
. $XDG_CONFIG_HOME/i3/config
. /etc/scroll/config
. /etc/i3/config

If unset, $XDG_CONFIG_HOME defaults to *~/.config*.

An error is raised when no config file is found. The recommended default
configuration is usually installed to */etc/scroll/config*; you are encouraged to
copy this to *~/.config/scroll/config* and edit it from there.

For information on the config file format, see *scroll*(5).

# IPC COMMANDS

Though *scrollmsg*(1) is generally preferred, you may run *scroll* _command_ to
send _command_ to the running instance of scroll. You can also issue commands
with *i3-msg*(1) or even with *i3*(1).

# ENVIRONMENT

The following environment variables have an effect on sway:

_SCROLLSOCK_
	Specifies the path to the sway IPC socket.

_XKB\_DEFAULT\_RULES_, _XKB\_DEFAULT\_MODEL_, _XKB\_DEFAULT\_LAYOUT_,
_XKB\_DEFAULT\_VARIANT_, _XKB\_DEFAULT\_OPTIONS_
	Configures the xkb keyboard settings. See *xkeyboard-config*(7). The
	preferred way to configure the keyboard is via the configuration file, see
	*scroll-input*(5).

The following environment variables are set by sway:

_DISPLAY_
	If compiled with Xwayland support and Xwayland is not disabled by the
	config, this will be set to the name of the X display used for Xwayland.

_SWAYSOCK_
	For compatibility with sway, specifies the path to the sway IPC socket.

_I3SOCK_
	For compatibility with i3, specifies the path to the sway IPC socket.

_SCROLLSOCK_
	Specifies the path to the sway IPC socket.

_WAYLAND_DISPLAY_
	Specifies the name of the Wayland display that sway is running on.

_XCURSOR_SIZE_
	Specifies the configured cursor size.

_XCURSOR_THEME_
	Specifies the configured cursor theme.

# AUTHORS

This sway fork is maintained by dawsers, who is assisted by other open
source contributors. For more information about scroll development, see
<https://github.com/dawsers/scroll>.

# SEE ALSO

*scroll*(5) *scrollmsg*(1) *scroll-input*(5) *scroll-output*(5) *scroll-bar*(5)
*scroll-ipc*(7)
