#include "sway/tree/scene.h"
#include "sway/output.h"

bool render_workspace_build_state(struct sway_output *output,
		struct wlr_output_state *state, const struct sway_scene_output_state_options *options) {
	bool ret = sway_scene_output_build_state(output->scene_output, state, options);
	return ret;
}
