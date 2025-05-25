# scroll

[scroll](https://github.com/dawsers/scroll) is a Wayland compositor forked
from [sway](https://github.com/swaywm/sway). The main difference is *scroll*
only supports one layout, a scrolling layout similar to
[PaperWM](https://github.com/paperwm/PaperWM), [niri](https://github.com/YaLTeR/niri)
or [hyprscroller](https://github.com/dawsers/hyprscroller).

[video.mp4](https://github.com/user-attachments/assets/d39f2e91-1258-437f-8ea3-60dd44bb4d3b)

*scroll* works very similarly to *hyprscroller*, and it is also mostly
compatible with *sway* configurations aside from the window layout. It
supports some added features:

* Animations: *scroll* supports very customizable animations.

* Content scaling: The content of individual Wayland windows can be scaled
  independently of the general output scale.

* Overview and Jump modes: You can see an overview of the desktop and work
  with the windows at that scale. Jump allows you to move to any window with
  just some key presses, like easymotion in some editors. There is also a jump
  mode to preview and switch to any available workspace.

* Workspace scaling: Apart from overview, you can scale the workspace to any
  scale, and continue working.

* Trackpad/Mouse scrolling: You can use the trackpad or mouse dragging to
  navigate/scroll the workspace windows.

* Portrait and Landscape monitor support: The layout works and adapts to both
  portrait or landscape monitors. You can define the layout orientation per
  output (monitor).

## Documentation

This README explains the basic differences between *sway/i3* and *scroll*. For
people unfamiliar with *i3* or *sway*, it is advised to read their
documentation, as compatibility is very high.

[Documentation for i3](https://i3wm.org/docs/userguide.html)

[Documentation for sway](https://github.com/swaywm/sway/wiki)

[scroll TUTORIAL](https://github.com/dawsers/scroll/blob/master/TUTORIAL.md)

[Example configuration](https://github.com/dawsers/scroll/blob/master/config.in)

You can also read *scroll's* man pages for details on the commands. They are
mostly up to date.

``` bash
man 5 scroll
man 1 scroll
man 1 scrollmsg
man 7 scroll-ipc
man scroll-output
man scroll-bar
man scrollnag
```

## Building and Installing

*scroll* is a stable fork of *sway*; the build tree is basically the
same and the executables are renamed to *scroll*, "scrollmsg", "scrollnag" and
"scrollbar".

### Arch Linux

If you are using Arch Linux, there is an AUR package you can install:

`sway-scroll-git`

``` sh
paru -S sway-scroll-git
```

Now prepare a configuration file `~/.config/scroll/config` using the provided
example (`/etc/scroll/config`), and you can start *scroll* from a tty.


### Requirements

If you want to compile *scroll* yourself, [sway compiling instructions](https://github.com/swaywm/sway#compiling-from-source)
apply to *scroll*.


## Configuration

*scroll* is very compatible with *i3* and *sway*, but because it uses a
scrolling layout, to use its extra features you will need to spend some time
configuring key bindings and options. *scroll* includes an example configuration
file that will usually be installed in `/etc/scroll/config`. Copy it to
`~/.config/scroll/config` and make your changes.


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

The command `layout_transpose` allows you to change from one type of layout to
the other at runtime. For example, you want to move your current workspace
from a landscape monitor to one in portrait mode: `move` the workspace and then
call `layout_transpose`; it will change your existing layout from a row of
columns to a column of rows, keeping all your windows in the same relative
positions. You can undo it by calling `layout_transpose` again.

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

There are also two special `jump` modes:

`jump workspaces` will show you a preview of all the available workspaces on
their respective monitor. You can use this mode to preview and quickly jump
to any workspace.

`jump container` will show you all the windows in the active column
(horizontal layout) or all the windows in the active row (vertical layout), so
you can quickly jump to any of them. It is a good substitute for tabs, because
you also see the content of the windows.

``` config
    bindsym --no-repeat $mod+slash jump
    bindsym --no-repeat $mod+Shift+slash jump container
    bindsym --no-repeat $mod+Ctrl+slash jump workspaces
```


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

### Animations

*scroll* supports very customizable animations using N-order Bezier curves.
You can use specific animation curves for each operation, and each curve is
composed of two additional curves. One controls the timing for the animation
of the changing variable, and another an offset for the non-changing one.
This means you can animate the speed of the effect, like in most compositors,
but you can also animate the offset that isn't changing. For example, when
a window is moving, you can animate the coordinate that is moving, but also
define an oscillation for the coordinate that doesn't change.

Using N-order Bezier curves, the curves can be practically anything. 

There are two optional curves to define:

- `var` defines the timing/position for the main animated variable, and should
always start at (0, 0) and end at (1, 1). You only define the points in-between.
(0, 0) is `time = 0` and initial `x` position. (1, 1) is `time = 1` (end of
animation) and `x` at the final animation position.

- `off` defines the positional offset for the variable that is static (for
example, if moving on the x direction, `var` defines the timing of the x
coordinate and `off` the offset for y). This curve starts at (0, 0) (initial
`x` and `y` positions) and ends at (1, 0) (final `x` position, and final
offset for `y` which is 0 because that is the variable that doesn't move. To
avoid mistakes, you only define the points in-between.

[This](https://nurbscalculator.in/) page has a Bezier curve design applet you
can use to customize any curve. Don't forget to select `Bezier` as the type of
the curve instead of the default `NURBS`. Here, you will need to add the
origin and end points to be able to design the curve, but then you don't
add them in your config.

If you want some simpler (order 3) curves for the `var` curve, you can copy them
from [here](https://www.cssportal.com/css-cubic-bezier-generator/). You can
copy them directly, as they don't include the initial and last points either.

Read the man pages and the section of this document on *Animation Options* for
details, and try these example curves in your `config` to see it in action:

``` config
animations {
    default yes 300 var 3 [ 0.215 0.61 0.355 1 ]
    window_open yes 300 var 3 [ 0 0 1 1 ]
    window_move yes 300 var 3 [ 0.215 0.61 0.355 1 ] off 0.05 6 [0 0.6 0.4 0 1 0 0.4 -0.6 1 -0.6]
    window_size yes 300 var 3 [ -0.35 0 0 0.5 ]
}
```


### Pins

*scroll* supports *pinning* a tiled top level container to either edge of the
workspace. This may be useful when you have a very wide monitor, or you want
to keep a column visible at all times. You may want to have some documentation
or terminal always visible.

The command is `pin <beginning|end>`.

It will *pin* the active top level container. For horizontal layouts, it will
pin it to the left (`beginning`) or right (`end`) edge of the monitor. For
vertical layouts, to the top (`beginning`) or bottom (`end`) edge.

`pin` works as a toggle, and there can only be one pin per workspace. The
logic is as follows when you call `pin`:

1. If the current container is already pinned: if you call `pin` with the same
   argument of the current pin, it will be unset and the container freed from
   its pin. If the argument is different, it will move the pinned container to
   the other position.
2. If the current container is not pinned yet: it will replace the pinned
   container, if any.

### Window Selection/Moving

The command `selection` manages window/container selections. You can select
several windows or containers at a time, even in different workspaces and/or
from overview mode. Those windows will change the border color to the one
specified in the option `client.selected`.

Use `selection toggle` to select/deselect a window (in window mode)
or a full container (in top-level mode).

If you want to clear a selection, instead of "toggling" each window/container,
you can call `selection reset`, which will clear all the selected items.

Once you have made a selection, you can move those windows to a different
workspace or location in the same workspace using `selection move`.
The selection order and column/window configuration will be maintained.

If your new location has a different layout type (for example, vertical
instead of horizontal), your containers and windows will adapt, transposing
their positions to better fit the new destination.

`selection workspace` will add every window of the current workspace
to the selection. You can use this when you want to move one workspace to a
different one, but keeping windows positions and sizes. Use
`selection workspace`, and then `selection move` where you want the windows
to appear.

`selection move` uses the current mode modifier to locate the new containers.
So you can place the new containers `before`, `after`, at the beginning
(`beg`) or `end` depending on the current mode.

```
selection <toggle|reset|workspace|move>
```


### Trails and Trailmarks

Trails and Trailmarks are a concept borrowed from [trailblazer.nvim](https://github.com/LeonHeidelbach/trailblazer.nvim).

A **trailmark** is like an anonymous mark on a window, and a **trail** is a
collection of trailmarks. You can have as many trails as you want, and as many
trailmarks as you want in any trail. Each window can be in as many trails
as you want, too.

Creating your first trailmark (`trailmark toggle`) will create a trail. From
then on, every trailmark you create will be assigned to that trail. You can
navigate back (`trailmark prev`) and forth (`trailmark next`) within the
collection of trailmarks contained in the trail.

To create a new trail, use `trail new`. With `trail prev` and `trail next`
you can navigate trails, changing the active one. The active trail will be
the one used for the trailmark command (`toggle`, `next`, and `prev`).

Clear all the trailmarks of the active trail using `trail clear`, or delete
the trail from the list with `trail delete`.

`trail to_selection` creates a selection list from the trailmarks in the active
trail. You can use that selection for example to move all the windows to a new
workspace using `selection move`.

*scroll* generates IPC signals for trail/trailmark events. See the man page
or the example implementation in *scrollbar* if you want to use these signals
to display information on your desktop bar.

There is also a reference implementation for the *scroll* modules in
[gtkshell](https://github.com/dawsers/gtkshell).

Read the example config for an example on how to set bindings for the trail
and trailmark commands.

``` config
mode "trailmark" {
    bindsym bracketright trailmark next
    bindsym bracketleft trailmark prev
    bindsym semicolon trailmark toggle; mode default
    bindsym Escape mode "default"
}
bindsym $mod+semicolon mode "trailmark"

mode "trail" {
    bindsym bracketright trail next
    bindsym bracketleft trail prev
    bindsym semicolon trail new; mode default
    bindsym d trail delete; mode default
    bindsym c trail clear; mode default
    bindsym insert trail to_selection; mode default
    bindsym Escape mode "default"
}
bindsym $mod+Shift+semicolon mode "trail"
```


### Tips for Using Marks

*scroll* also supports sway's mark based navigation. I use these scripts and
key bindings:

``` config
    # Marks
    bindsym $mod+m exec scroll-mark-toggle.sh
    bindsym $mod+Shift+m exec scroll-mark-remove.sh
    bindsym $mod+apostrophe exec scroll-mark-switch.sh
```

`scroll-mark-toggle.sh`


``` bash
#!/bin/bash
    
marks=($(scrollmsg -t get_tree | jq -c 'recurse(.nodes[]?) | recurse(.floating_nodes[]?) | select(.focused==true) | {marks} | .[]' | jq -r '.[]'))

generate_marks() {
    for mark in "${marks[@]}"; do
        echo "$mark"
    done
}

mark=$( (generate_marks) | rofi -p "Toggle a mark" -dmenu)
if [[ -z $mark ]]; then
    exit
fi
scrollmsg "mark --add --toggle" "$mark"
```

`scroll-mark-remove.sh`

``` bash
#!/bin/bash
    
marks=($(scrollmsg -t get_tree | jq -c 'recurse(.nodes[]?) | recurse(.floating_nodes[]?) | select(.focused==true) | {marks} | .[]' | jq -r '.[]'))

generate_marks() {
    for mark in "${marks[@]}"; do
        echo "$mark"
    done
}

remove_marks() {
    echo $marks
    for mark in "${marks[@]}"; do
        scrollmsg unmark "$mark"
    done
}

mark=$( (generate_marks) | rofi -p "Remove a mark (leave empty to clear all)" -dmenu)
if [[ -z $mark ]]; then
    remove_marks
    exit
fi
scrollmsg unmark "$mark"
```

`scroll-mark-switch.sh`

``` bash
#!/bin/bash

marks=($(scrollmsg -t get_marks | jq -r '.[]'))

generate_marks() {
    for mark in "${marks[@]}"; do
        echo "$mark"
    done
}

mark=$( (generate_marks) | rofi -p "Switch to mark" -dmenu)
[[ -z $mark ]] && exit

scrollmsg "[con_mark=\b$mark\b]" focus
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

`fullscreen_movefocus`: default is `true`. If `true`, changing focus while in
full screen mode will keep full screen status for the new window. You can use
overview/jump while in full screen mode to move to other window, making it
full screen too.


### Gesture Options

`gesture_scroll_enable`: default is `true`. Enables the trackapad scrolling gesture.

`gesture_scroll_fingers`: default is `3`. Number of fingers to use for the
scrolling gesture.

`gesture_scroll_sentitivity`: default is `1.0`. Increase if you want more
sensitivity.

### Animation Options

You can define a block in your `config` file for animations. The block is
called `animations`. Within that block there are several options allowed:

`enabled`: (boolean) default is `yes`. Enables/disables animations globally.

`frequency_ms`: (integer) default is `16`. Number of milliseconds for each
animation step. The default is approximately the refresh rate of a monitor
that works at 60 Hz. If you need smoother animations, reduce this value. Use
with caution to avoid crashes or performance issues. The default should work
for most cases unless you have a very high refresh rate monitor.

`default`: defines the default animation curve. Follows the format explained
below. default is `yes 300 var 3 [ 0.215 0.61 0.355 1 ]`

`window_move`: defines the curve for windows movement (`move` command). By
default it is not defined, so it uses the `default` curve unless you add one to
your config.

`window_open`: defines the curve for windows opening. By default it is not
defined, so it uses the `default` curve unless you add one to your config.

`window_size`: defines the curve for windows resizing (`cycle_size`,
`set_size`, `fit_size`, `resize` commands). By default it is not defined, so
it uses the `default` curve unless you add one to your config.

Format of an animation curve:

- The first field is `enabled`. It can be `yes` or `no`. It enables or
  disables animations for that operation. If set to `no`, you don't need to
  define the curve. If set to `yes`, you can set the `duration` if you
  want to use the `default` curve, or define the curves using the following
  options.

- `duration_ms`. Duration of the animation in milliseconds.

- `var`. Defines the animation curve for the variable changing in the
  operation/command. The fields are `order` and `control_points`. `order`
  defines the order of the Bezier curve that follows, and `control_points` is
  an array defining its control points except for the first, that is `(0, 0)`,
  and the last, `(1, 1)`. The number of values in the array will be `2 * (order - 1)`.
  A Bezier curve of `order` needs `order + 1` control points to be
  defined, but we set the first and last to `(0, 0)` and `(1, 1)`.
  We use 2D Bezier curves, so the array needs `2 * (order -1)` numbers in it.

- `off`. Defines the animation curve for the variable that doesn't change in the
  operation/command. The fields are `scale` `order` and `control_points`.
  `scale` is the fraction of the workspace size used to scale the curve. As
  the curve is defined from `(0, 0)` to `(1, 0)`, we need a parameter to scale
  the offset value. This parameter does that. For example, using a small parameter
  like `0.05`, creates offsets of an order of `5%` of the size of the workspace.
  `order` and `control_points` work as in the `var` case. But this time, the
  points you will not include are: `(0, 0)` (first) and `(1, 0)` (last).

Example:

``` config
animations {
    default yes 300 var 3 [ 0.215 0.61 0.355 1 ]
    window_open yes 300 var 3 [ 0 0 1 1 ]
    window_move yes 300 var 3 [ 0.215 0.61 0.355 1 ] off 0.05 6 [0 0.6 0.4 0 1 0 0.4 -0.6 1 -0.6]
    window_size yes 300 var 3 [ -0.35 0 0 0.5 ]
}
```


## IPC

*scroll* adds IPC events you can use to create a module for your favorite
desktop bar.

See `include/ipc.h` for `IPG_GET_SCROLLER`, `IPC_EVENT_SCROLLER`,
`IPG_GET_TRAILS` and `IPC_EVENT_TRAILS`.

You can get data for mode/mode modifiers, overview and scale mode as well as
trails and whether a view has an active trailmark.

For anyone interested in creating modules for popular desktop bars, there is a
reference implementation for the *scroll* modules in
[gtkshell](https://github.com/dawsers/gtkshell).


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

json_object *ipc_json_describe_trails() {
	json_object *object = json_object_new_object();

	json_object_object_add(object, "length", json_object_new_int(layout_trails_length()));
	json_object_object_add(object, "active", json_object_new_int(layout_trails_active()));
	json_object_object_add(object, "trail_length", json_object_new_int(layout_trails_active_length()));

	return object;
}

void ipc_event_trails() {
	if (!ipc_has_event_listeners(IPC_EVENT_TRAILS)) {
		return;
	}
	sway_log(SWAY_DEBUG, "Sending trails event");

	json_object *json = json_object_new_object();
	json_object_object_add(json, "trails", ipc_json_describe_trails());

	const char *json_string = json_object_to_json_string(json);
	ipc_send_event(json_string, IPC_EVENT_TRAILS);
	json_object_put(json);
}

static void ipc_json_describe_view(struct sway_container *c, json_object *object) {
...
	json_object_object_add(object, "trailmark", json_object_new_boolean(layout_trails_trailmarked(c->view)));
}
```
