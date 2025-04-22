#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/tree/workspace.h"
#include "util.h"

struct cmd_results *cmd_jump_labels_color(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "jump_labels_color", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	uint32_t color;
	parse_color(argv[0], &color);
	color_to_rgba(config->jump_labels_color, color);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_jump_labels_background(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "jump_labels_background", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	uint32_t color;
	parse_color(argv[0], &color);
	color_to_rgba(config->jump_labels_background, color);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_jump_labels_scale(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "jump_labels_scale", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	config->jump_labels_scale = fmin(fmax(strtod(argv[0], NULL), 0.1), 1.0);

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_jump_labels_keys(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "jump_labels_keys", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	free(config->jump_labels_keys);
	config->jump_labels_keys = strdup(argv[0]);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

/*
 *  jump
 */
struct cmd_results *cmd_jump(int argc, char **argv) {
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there are no outputs connected.");
	}

	layout_jump();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
