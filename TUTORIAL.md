# Tutorial

[scroll](https://github.com/dawsers/scroll) is a Wayland compositor forked from
[sway](https://github.com/swaywm/sway). It creates a window layout similar to
[PaperWM](https://github.com/paperwm/PaperWM).

Consult the [README](https://github.com/dawsers/scroll/blob/master/README.md)
for more details beyond this quick tutorial.

## Rows and Columns

*scroll* supports multiple workspaces per monitor. Each workspace is a
*row*: a scrollable set of columns, where each *column* may contain one or
more windows.

Your workspace is not limited by the size of the monitor; windows will have a
standard size and will scroll in and out of the view as they gain or lose focus.
This is an important difference with respect to other tiling window managers,
you always control the size of your windows, and if they are too big and not
in focus, they will just scroll out of view. There are ways to view all
windows at the same time, through overview mode, but more on that later.

There are two working modes that can be changed at any time:

1. *horizontal*: it is the default. For each new window you open, it creates a new
   column containing only that window.
2. *vertical*: a new window will be placed in the current *column*,
   right below the active window.

If your monitor is in landscape mode (the usual one), you will probably work
mostly in *horizontal* mode. If you are using a monitor in portrait mode, *scroll*
will adapt and instead of having a row with columns of windows, you will have
a column with rows of windows. You can configure each monitor independently through
available options.


## Window Sizes

There are options to define the default width and height of windows. These
sizes are usually a fraction of the available monitor space so it is easier to
fit an integer number of windows fully on screen at a time. If you use a regular
monitor, you may use fractions like `0.5` or `0.333333` to fit 2 or 3
windows. If you have an ultra-wide monitor, you may decide to use smaller
fractions to fit more windows in the viewport at a time.

It is convenient to define complementary fractions like `0.6666667`, `0.75` etc.,
so for example you can fit a `0.333333` and a `0.666667` window for the full
size of the monitor. Depending on the size and resolution of your monitor you
can decide which fractions are best suited for you, and configure them
through options.

You can toggle different window widths and heights in real-time using
`cycle_size`. It will go through your list of selected fractions defined in the
configuration, and resize the active window accordingly.

Of course you can also manually define exactly the size of a window with
`resize` or use `set_size` to directly choose a fraction without having to
cycle through them.

If you want to quickly fit a series of windows in a column or columns in a
row, you can use `fit_size`. It will ignore fractions, and re-scale all the
affected windows to fit in the screen.


## Focusing and Alignment

When you change window focus, *scroll* will always show the active
(focused) window on screen, and intelligently try to fit another adjacent
window if it can. However, in some cases you may want to decide to align a
window manually; `align` does exactly that.


## Overview Mode

When you have too many windows in a workspace, it can be hard to know where
things are. `scale_workspace overview` helps with that by creating a bird's eye view
of the whole workspace.


## Jump Mode

Jump mode takes advantage of overview mode to provide a very quick way to focus on
any window or workspace on your monitors. Similar to [vim-easymotion](https://github.com/easymotion/vim-easymotion),
it overlays a label on each window (or workspace if you use `jump workspaces`.
Pressing all the numbers of a label, changes focus to the selected window or
workspace.

1. Assign a key binding to `jump`, for this tutorial:

``` config
bindsym --no-repeat $mod+slash jump
bindsym --no-repeat $mod+Shift+slash jump workspaces
```

2. Pressing your `mod` key and `/` will show an overview of your windows on
   each monitor.
3. Now press the numbers shown on the overlay for the window you want to change
   focus to.
4. `jump` will exit, and the focus will be on the window you selected.

If instead you press `mod`, `Shift` and `/`, you will see a bird's eye view of
all your workspaces. Pressing a combination of keys, like in the example
above, will take you to the chosen workspace.

## Content Scaling

You can set the scale of the content of any window independently. Add these
bindings and pressing Mod and moving the scroll wheel of you mouse will scale
the content of the active window.

``` config
bindsym --whole-window $mod+button4 scale_content incr -0.05
bindsym --whole-window $mod+button5 scale_content incr 0.05
```

## Workspace Scaling

Apart from overview and jump, you can scale your workspace to an arbitrary
scale value. Add these bindings to your configuration, and pressing Mod +
Shift and moving the scroll wheel of your mouse will scale the workspace.

``` config
bindsym --whole-window $mod+Shift+button4 scale_workspace incr -0.05
bindsym --whole-window $mod+Shift+button5 scale_workspace incr 0.05
```


## Pins

Sometimes you want to keep one window static at all times, for example an
editor with a file you are working on, while letting other windows scroll
in the rest of the available space. These windows may include terminals or
documentation browser windows. You can pin a column to the right or left
(top or bottom) of the monitor, and it will stay at that location until you
call pin again.


## Moving Columns/Windows Around.

To be able to re-organize windows and columns more easily, you can use
`selection toggle` to select and deselect windows. Those windows can be in
any workspace, and can also be selected from overview mode. After you are
done with your selection, go to the place where you want to move them (same
or different workspace), and call `selection move`. All the selected
windows/containers will move to your current location. If you want to select
a full workspace, use `selection workspace`, or `selection reset` to undo a
multiple selection. You can control more precisely where the selection will
end up by changing mode modifiers (after, before, end,...)


## Working in Full Screen Mode

Overview and jump can be very helpful when you work in fullscreen mode. You
can keep all your windows maximized or in full screen mode, and toggle
overview on/off to see your full desktop, and quickly focus or change the
position to a different application.


## Touchpad Gestures

By default, *scroll* supports three and four finger touchpad swipe
gestures to scroll windows, call *overview* and switch workspaces.

You can also scroll windows with the mouse. Press `mod` and the middle mouse
button, and you can drag/scroll columns and windows.

## Example Configuration

*scroll* includes an example [configuration](https://github.com/dawsers/scroll/blob/master/config.in)
with most of the key bindings recommended for a standard setup.
