#ifndef _SCROLL_RENDER_H
#define _SCROLL_RENDER_H

#include <stdbool.h>

struct sway_workspace;
struct sway_output;
struct sway_scene_tree;
struct sway_scene_output;
struct wlr_output_state;
struct sway_scene_output_state_options;

/**
 * Render workspace and populate the given output state.
 */
bool render_workspace_build_state(struct sway_output *output,
		struct wlr_output_state *state, const struct sway_scene_output_state_options *options);


#endif // _SCROLL_RENDER_H
