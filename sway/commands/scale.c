#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/tree/workspace.h"
#include "sway/output.h"
#include "sway/tree/view.h"
#include "sway/tree/arrange.h"
#include <wlr/types/wlr_fractional_scale_v1.h>

#define MIN_SCALE 0.2f
#define MAX_SCALE 4.0f

static void apply_scale(struct sway_container *container, float scale) {
	view_set_content_scale(container->view, scale);
}

static void reset_scale(struct sway_container *container) {
	view_reset_content_scale(container->view);
}

/*
 *  We can set the scale, modify or reset it.
 *  scale_content <exact number|increment number|reset>
 */
struct cmd_results *cmd_scale_content(int argc, char **argv) {
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there's no outputs connected.");
	}
	struct sway_container *container = config->handler_context.container;
	if (!container || !container->view) {
		return cmd_results_new(CMD_INVALID, "Need a window to run scale_content");
	}

	struct cmd_results *error;
	if ((error = checkarg(argc, "scale_content", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	int fail = 0;
	if (strcasecmp(argv[0], "exact") == 0) {
		if (argc < 2) {
			fail = 1;
		} else {
			double number = strtod(argv[1], NULL);
			if (number < MIN_SCALE) {
				number = MIN_SCALE;
			} else if (number > MAX_SCALE) {
				number = MAX_SCALE;
			}
			apply_scale(container, number);
		}
	} else if (strcasecmp(argv[0], "increment") == 0 || strcasecmp(argv[0], "incr") == 0) {
		if (argc < 2) {
			fail = 1;
		} else {
			double number = strtod(argv[1], NULL);
			if (!view_is_content_scaled(container->view)) {
				number += 1.0;
			} else {
				number += view_get_content_scale(container->view);
			}
			if (number < MIN_SCALE) {
				number = MIN_SCALE;
			} else if (number > MAX_SCALE) {
				number = MAX_SCALE;
			}
			apply_scale(container, number);
		}
	} else if (strcasecmp(argv[0], "reset") == 0) {
		reset_scale(container);
	} else {
		fail = 1;
	}

	if (fail) {
		const char usage[] = "Expected 'scale_content <exact number|increment number|reset>'";
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
