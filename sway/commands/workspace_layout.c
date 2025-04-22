#include <string.h>
#include <strings.h>
#include "sway/commands.h"

struct cmd_results *cmd_workspace_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_layout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "default") == 0) {
		config->default_layout = L_NONE;
	} else if (strcasecmp(argv[0], "horizontal") == 0) {
		config->default_layout = L_HORIZ;
	} else if (strcasecmp(argv[0], "vertical") == 0) {
		config->default_layout = L_VERT;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Expected 'workspace_layout <default|horzontal|vertical>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
