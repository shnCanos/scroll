#include <strings.h>
#include "sway/commands.h"
#include "sway/tree/layout.h"

struct cmd_results *cmd_selection(int argc, char **argv) {
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there are no outputs connected.");
	}
	struct cmd_results *error;
	if ((error = checkarg(argc, "selection", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "toggle") == 0) {
		struct sway_container *current = config->handler_context.container;
		if (!current) {
			return cmd_results_new(CMD_INVALID, "Need a container to select");
		}
		layout_selection_toggle(current);
	} else if (strcasecmp(argv[0], "workspace") == 0) {
		struct sway_workspace *workspace = config->handler_context.workspace;
		if (!workspace) {
			return cmd_results_new(CMD_INVALID, "Need a workspace to select");
		}
		layout_selection_workspace(workspace);
	} else if (strcasecmp(argv[0], "reset") == 0) {
		layout_selection_reset();
	} else if (strcasecmp(argv[0], "move") == 0) {
		struct sway_workspace *workspace = config->handler_context.workspace;
		if (!workspace) {
			return cmd_results_new(CMD_INVALID, "Need a workspace to move the selection to");
		}
		if (!layout_selection_move(workspace)) {
			return cmd_results_new(CMD_INVALID, "Need a selection to move");
		}
	} else {
		return cmd_results_new(CMD_INVALID, "Unknown argument %s, expected 'selection <toggle|workspace|reset|move>'", argv[0]);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
