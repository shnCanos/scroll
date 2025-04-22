# scroll

[scroll](https://github.com/dawsers/scroll) is a Wayland compositor forked
from [sway](https://github.com/swaywm/sway). The main difference is *scroll*
only supports one layout, a scrolling layout similar to
[PaperWM](https://github.com/paperwm/PaperWM), [niri](https://github.com/YaLTeR/niri)
or [hyprscroller](https://github.com/dawsers/hyprscroller).

*scroll* works very similarly to *hyprscroller*, and it is also mostly
compatible with *sway* configurations aside from the window layout. It
supports some added features:

* Content scaling: The content of individual Wayland windows can be scaled
  independently of the general output scale.

* Overview and Jump modes: You can see an overview of the desktop and work
  with the windows at that scale. Jump allows you to move to any window with
  just some key presses, like easymotion in some editors.

* Workspace scaling: Apart from overview, you can scale the workspace to any
  scale, and continue working.

* Trackpad/Mouse scrolling: You can use the trackpad or mouse dragging to
  navigate/scroll the workspace windows.

* Portrait and Landscape monitor support: The layout works and adapts to both
  portrait or landscape monitors. You can define the layout orientation per
  output (monitor).

## Documentation

The documentation is evolving. Currently, this README is the main source of
information to learn about differences between *sway/i3* and *scroll*. For
people unfamiliar with *i3* or *sway*, it is advised to read their
documentation, as compatibility is very high.

[Documentation for i3](https://i3wm.org/docs/userguide.html)

[Documentation for sway](https://github.com/swaywm/sway/wiki)

[scroll TUTORIAL](https://github.com/dawsers/scroll/TUTORIAL.md)

[Example configuration](https://github.com/dawsers/scroll/config.in)

## Building and Installing

There are still no packages or releases for *scroll*. But you can build and then
start it from a tty.

*scroll* is a stable fork of *sway*; the build tree is basically the
same except for the main executable, called *scroll*. *swaymsg*, *swaynag*
etc. keep their names.

### Requirements

[sway compiling instructions](https://github.com/swaywm/sway#compiling-from-source)
apply to *scroll*.


## Configuration

*scroll* is very compatible with *i3* and *sway*, but because it uses a
scrolling layout, to use its extra features you will need to spend some time
configuring key bindings and options. *scroll* includes an example configuration
file that will usually be installed in `/etc/sway/config`. Copy it to
`~/.config/sway/config` and make your changes.


## Commands and Quirks Specific to *scroll*

### Layout types

You can set the layout type per output (monitor). There are two types:
"horizontal" and "vertical". If the monitor is in landscape orientation, the
default will be "horizontal", and if it is in portrait mode, it will be
"vertical". But you can force any mode per output, like this:

``` config
output DP-2 {
    ...
    layout_type horizontal
    ...
}
```

The *horizontal* layout will create columns of windows, and the "vertical"
layout will create rows of windows. You can still add any number of columns or
rows, and any number of windows to each row/column.

### Modes

You can set the mode with the `set_mode <h|v>` command. *scroll* works in any
of two modes that can be changed at any moment.

1. "horizontal (h)" mode: In horizontal layouts, it creates a new column per new
   window. In vertical layouts, it adds the new window to the current row.
   For horizontal layouts, `cyclesize h` and `setsize h` affect the width of
   the active column. `align` aligns the active column according to the
   argument received. `fit_size h` fits the selected columns to the width of
   the monitor. For vertical layouts, those commands affect the active window
   in the active row.

2. "vertical (v)" mode: In horizontal layouts, it adds the new window to the
   active column. In vertical layouts, it adds the new window to a new row.
   For horizontal layouts, `cyclesize v` and `setsize v` affect the height of
   the active window. `align` aligns the active window within its column
   according to the argument received. `fit_size v` fits the selected column
   windows to the height of the monitor. For vertical layouts, those commands
   affect the active row.


### Mode Modifiers

Modes and mode modifiers are per workspace.

At window creation time, *scroll* can apply several modifiers to the
current working mode (*h/v*). `set_mode` supports extra arguments:

``` config
set_mode [<h|v|t> <after|before|end|beg> <focus|nofocus> <center_horiz|nocenter_horiz> <center_vert|nocenter_vert> <reorder_auto|noreorder_auto>]
```

1. `<h|v|t>`: set horizontal, vertical, or toggle the current mode.
2. `position`: It is one of `after` (default), `before`, `end`, `beg`.
This parameter decides the position of new windows: *after* the current one
(default value), *before* the current one, at the *end* of the row/column, or
at the *beginning* of the row/column.
3. `focus`: One of `focus` (default) or `nofocus`. When creating a new window,
this parameter decides whether it will get focus or not.
4. Reorder automatic mode: `reorder_auto` (default) or `noreorder_auto`. By
default, *scroll* will reorder windows every time you change focus, move or
create new windows. But sometimes you want to keep the current window in a
certain position, for example when using `align`. `align` turns reordering
mode to `noreorder_auto`, and the window will keep its position regardless of
what you do, until you set `reorder_auto` again.
5. `center_horiz/nocenter_horiz`: It will keep the active column centered
(or not) on the screen. The default value is the one in your configuration.
4. `center_vert/nocenter_vert`: It will keep the active window centered
(or not) in its column. The default value is the one in your configuration.

You can skip any number of parameters when calling the dispatcher, and their
order doesn't matter.


### Focus

Focus also supports `beginning` and `end`. Those arguments move you to the end
or beginning of a column/row depending on the current mode.


### Window Movement

*scroll* adds movement options specific to the tiled scrolling layout.

``` config
move left|right|up|down|beginning|end [nomode]
```

Depending on the mode and layout type, you will be moving columns/rows or
windows. You can move them in any direction, or to the end/beginning of the
row/column.

Aside from that, you can work in `nomode` "mode". With that argument, windows
will move freely. Movement will **only** move the active window, leaving its
column intact, regardless of which *mode* (h/v) you are currently in. For a
horizontal layout, the movement will be like this:

If the window is in some column with other windows, any `left` or `right`
movement will expel it to that side, creating a new column with just that
window. Moving it again will insert it in the column in the direction of
movement, and so on. Moving the window `up` or `down` will simply move it in
its current column. You can still use `beginning/end` to move it to the edges
of the row if it is alone in a column, or the edges of the column if it has
siblings in that column.


### Resizing

``` config
cycle_size <h|v> <next}prev>
```

`cycle_size <h|v>` cycles forward or backward through a number of column
widths (in *horizontal* mode), or window heights (in *vertical* mode). Those widths or
heights are a fraction of the width or height of the monitor, and are
configurable globally and per monitor. However, using the `resize` command, you
can still modify the width or height of any window freely.

`set_size` is similar to their `cycle_size`, but instead of cycling, it allows
the width or height to be set directly.


### Aligning

Columns are generally aligned in automatic mode, always making the active one
visible, and trying to make at least the previously focused one visible too if
it fits the viewport, if not, the one adjacent on the other side. However, you
can always align any column to the *center*, *left* or *right* of the monitor
(in *horizontal* mode), or *up* (top), *down* (bottom) or to the *center* in
*vertical* mode. For example center a column for easier reading, regardless of
what happens to the other columns. If you want to go back to automatic mode,
you need to use the mode modifier `reorder_auto` or call `align reset`.

You can also center a window on your workspace using *middle*, it
will center its column, and also the window within the column.

``` config
align <left|right|center|up|down|middle|reset>
```

### Fitting the Screen

When you have a ultra-wide monitor, one in a vertical position, or the default
column widths or window heights don't fit your workflow, you can use manual
resizing, but it is sometimes slow and tricky.

``` config
fit_size <h|v> <active|visible|all|toend|tobeg> <proportional|equal>
```

`fit_size` allows you to re-fit the columns (*horizontal* mode) or windows
(*vertical* mode) you want to the screen extents. It accepts an argument related to the
columns/windows it will try to fit. The new width/height of each column/window
will be *proportional* to its previous width or height, relative to the other
columns or windows affected, or *equal* for all of them.

1. `active`: It will maximize the active column/window.
2. `visible`: All the currently fully or partially visible columns/windows will
   be resized to fit the screen.
3. `all`: All the columns in the row or windows in the column will be resized
   to fit.
4. `toend`: All the columns or windows from the focused one to the end of the
   row/column will be affected.
5. `tobeg` or `tobeginning`: All the columns/windows from the focused one to
   the beginning of the row/column will now fit the screen.


### Workspace Scaling Commands: Overview

In *scroll* you can work at any scale. The workspace can be scaled using

``` config
scale_workspace <exact number|increment number|reset|overview>
```

This command will scale the workspace to an exact scale, or incrementally by a
delta value. If you want a useful automatic scale, use the `overview` argument
which will fit all the windows of the workspace in the current viewport.

Note that You will still be able to interact with the windows normally (change
focus, move windows, create or destroy them, type in them etc.). Use it as a
way to see where things are and move the active focus, or reposition windows.

Example configuration:

``` config
# Scaling
    # Workspace
    # Mod + Shift + comma/period will incremmentally scale the workspace
    # down/up. Using Mod + Shift and the mouse scrollwheel will do the same.
    bindsym $mod+Shift+comma scale_workspace incr -0.05
    bindsym --whole-window $mod+Shift+button4 scale_workspace incr -0.05
    bindsym $mod+Shift+period scale_workspace incr 0.05
    bindsym --whole-window $mod+Shift+button5 scale_workspace incr 0.05
    bindsym $mod+Shift+Ctrl+period scale_workspace reset

    # Overview
    # Mod + Tab or a lateral mouse button will toggle overview.
    bindsym --no-repeat $mod+tab scale_workspace overview
    bindsym --whole-window button8 scale_workspace overview
```


### Jump

`jump` provides a shortcut-based quick focus mode for any window on
the active workspaces, similar to [vim-easymotion](https://github.com/easymotion/vim-easymotion)
and variations.

It shows all the windows on your monitors' active workspaces in overview, and
waits for a combination of key presses (overlaid on each window) to quickly
change focus to the selected window. Pressing any key that is not on the list or
a combination that doesn't exist, exits jump mode without changes.

Depending on the total number of windows and keys you set on your list, you
will have to press more or less keys. Each window has its full trigger combination
on the overlaid label.

You can call `jump` from any mode: overview or normal mode.


### Content Scaling

*scroll* lets you scale the content of a window independently of the current
monitor scale or fractional scale. You can have several copies of the same
application with different scales. This works well for Wayland windows, and
only partially for XWayland windows.

Add these key bindings to your config:

``` config
    # Content
    # Mod + period/comma scale the active window's content incremmentally.
    # Mod + scroll wheel do the same.
    bindsym $mod+comma scale_content incr -0.05
    bindsym --whole-window $mod+button4 scale_content incr -0.05
    bindsym $mod+period scale_content incr 0.05
    bindsym --whole-window $mod+button5 scale_content incr 0.05
    bindsym $mod+Ctrl+period scale_content reset
```


### Touchpad and Mouse Drag Scrolling

The default for scrolling is swiping with three fingers to scroll left, right,
up or down.

When swiping vertically (a column), *scroll* will scroll the column
that contains the mouse pointer. This allows you to scroll columns that are
not the active one if your configuration is set to focus following the mouse.

You can also use the mouse (Mod + dragging with the center button pressed) to
scroll.


### Tips for Using Marks

*scroll* supports sway's mark based navigation. I use these scripts and
key bindings:

``` config
    # Marks
    bindsym $mod+m exec sway-mark-toggle.sh
    bindsym $mod+Shift+m exec sway-mark-remove.sh
    bindsym $mod+apostrophe exec sway-mark-switch.sh
```

`sway-mark-toggle.sh`


``` bash
#!/bin/bash
    
marks=($(swaymsg -t get_tree | jq -c 'recurse(.nodes[]?) | recurse(.floating_nodes[]?) | select(.focused==true) | {marks} | .[]' | jq -r '.[]'))

generate_marks() {
    for mark in "${marks[@]}"; do
        echo "$mark"
    done
}

mark=$( (generate_marks) | rofi -p "Toggle a mark" -dmenu)
if [[ -z $mark ]]; then
    exit
fi
swaymsg "mark --add --toggle" "$mark"
```

`sway-mark-remove.sh`

``` bash
#!/bin/bash
    
marks=($(swaymsg -t get_tree | jq -c 'recurse(.nodes[]?) | recurse(.floating_nodes[]?) | select(.focused==true) | {marks} | .[]' | jq -r '.[]'))

generate_marks() {
    for mark in "${marks[@]}"; do
        echo "$mark"
    done
}

remove_marks() {
    echo $marks
    for mark in "${marks[@]}"; do
        swaymsg unmark "$mark"
    done
}

mark=$( (generate_marks) | rofi -p "Remove a mark (leave empty to clear all)" -dmenu)
if [[ -z $mark ]]; then
    remove_marks
    exit
fi
swaymsg unmark "$mark"
```

`sway-mark-switch.sh`

``` bash
#!/bin/bash

marks=($(swaymsg -t get_marks | jq -r '.[]'))

generate_marks() {
    for mark in "${marks[@]}"; do
        echo "$mark"
    done
}

mark=$( (generate_marks) | rofi -p "Switch to mark" -dmenu)
[[ -z $mark ]] && exit

swaymsg "[con_mark=\b$mark\b]" focus
```


## Options Specific to Scroll

Aside from i3/sway options, *scroll* supports a few additional ones to manage
its layout/resizing/jump etc.

### Layout Options

The following options are supported globally and per output:

`layout_default_width`: default is `0.5`. Set the default width for new
columns ("horizontal" layout) or windows ("vertical" layouts).

`layout_default_height`: default is `1.0`. Set the default height for new
windows ("horizontal" layouts) or rows ("vertical" layouts).

`layout_widths`: it is an array of floating point values. The default
is `[0.33333333, 0.5, 0.66666667, 1.0]`. These are the fractions `cycle_size`
will use when resizing the width of columns/windows.

`layout_heights`: it is an array of floating point values. The default
is `[0.33333333, 0.5, 0.66666667, 1.0]`. These are the fractions `cycle_size`
will use when resizing the height of windows/rows.


### Per Output Options

`layout_type`: `<horizontal|vertical>`. You can set it per output, or let
*scroll* decide depending on the aspect ratio of your monitor.

Aside from that, you can also set layout options per output, as in:

``` config
output DP-2 {
    background #000000 solid_color
    scale 1
    resolution 1920x1080
    position 0 0
    layout_type horizontal
    layout_default_width 0.5
    layout_default_height 1.0
    layout_widths [0.33333333 0.5 0.666666667 1.0]
    layout_heights [0.33333333 0.5 0.666666667 1.0]
}
```

### Jump Options

`jump_labels_color`: default is  `#159E3080`. Color of the jump labels.

`jump_labels_background`: default is  `#00000000`. Color of the background of
the jump labels.

`jump_labels_scale`: default is `0.5`. Scale of the label within the window.

`jump_labels_keys`: default is `1234`. Keys that will be used to generate
possible jump labels.

### General Options

`fullscreen_movefocus`: default is `true`. If `true`, changind focus while in
full screen mode will keep full screen status for the new window.


### Gesture Options

`gesture_scroll_enable`: default is `true`. Enables the trackapad scrolling gesture.

`gesture_scroll_fingers`: default is `3`. Number of fingers to use for the
scrolling gesture.

`gesture_scroll_sentitivity`: default is `1.0`. Increase if you want more
sensitivity.


## IPC

*scroll* adds IPC events you can use to create a module for your favorite
desktop bar.

See `include/ipc.h` for `IPG_GET_SCROLLER` and  `IPC_EVENT_SCROLLER`

You can get data for mode/mode modifiers, overview and scale mode.

``` c
json_object *ipc_json_describe_scroller(struct sway_workspace *workspace) {
	if (!(sway_assert(workspace, "Workspace must not be null"))) {
		return NULL;
	}
	json_object *object = json_object_new_object();

	json_object_object_add(object, "workspace", json_object_new_string(workspace->name));
	json_object_object_add(object, "overview", json_object_new_boolean(layout_overview_enabled(workspace)));
	json_object_object_add(object, "scaled", json_object_new_boolean(layout_scale_enabled(workspace)));
	json_object_object_add(object, "scale", json_object_new_double(layout_scale_get(workspace)));

	enum sway_container_layout layout = layout_modifiers_get_mode(workspace);
	json_object_object_add(object, "mode",
		json_object_new_string(layout == L_HORIZ ? "horizontal" : "vertical"));

	enum sway_layout_insert insert = layout_modifiers_get_insert(workspace);
	switch (insert) {
	case INSERT_BEFORE:
		json_object_object_add(object, "insert", json_object_new_string("before"));
		break;
	case INSERT_AFTER:
		json_object_object_add(object, "insert", json_object_new_string("after"));
		break;
	case INSERT_BEGINNING:
		json_object_object_add(object, "insert", json_object_new_string("beginning"));
		break;
	case INSERT_END:
		json_object_object_add(object, "insert", json_object_new_string("end"));
		break;
	}
	bool focus = layout_modifiers_get_focus(workspace);
	json_object_object_add(object, "focus", json_object_new_boolean(focus));
	bool center_horizontal = layout_modifiers_get_center_horizontal(workspace);
	json_object_object_add(object, "center_horizontal", json_object_new_boolean(center_horizontal));
	bool center_vertical = layout_modifiers_get_center_vertical(workspace);
	json_object_object_add(object, "center_vertical", json_object_new_boolean(center_vertical));
	enum sway_layout_reorder reorder = layout_modifiers_get_reorder(workspace);
	json_object_object_add(object, "reorder",
		json_object_new_string(reorder == REORDER_AUTO ? "auto" : "lazy"));

	return object;
}

void ipc_event_scroller(const char *change, struct sway_workspace *workspace) {
	if (!ipc_has_event_listeners(IPC_EVENT_SCROLLER)) {
		return;
	}
	sway_log(SWAY_DEBUG, "Sending scroller event");

	json_object *json = json_object_new_object();
	json_object_object_add(json, "change", json_object_new_string(change));
	json_object_object_add(json, "scroller", ipc_json_describe_scroller(workspace));

	const char *json_string = json_object_to_json_string(json);
	ipc_send_event(json_string, IPC_EVENT_SCROLLER);
	json_object_put(json);
}
```
