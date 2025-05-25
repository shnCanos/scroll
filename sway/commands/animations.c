#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "util.h"
#include "log.h"

// must be in order for the bsearch
static const struct cmd_handler animations_config_handlers[] = {
	{ "default", animations_cmd_default },
	{ "enabled", animations_cmd_enabled },
	{ "frequency_ms", animations_cmd_frequency },
	{ "window_move", animations_cmd_window_move },
	{ "window_open", animations_cmd_window_open },
	{ "window_size", animations_cmd_window_size },
};

struct cmd_results *animations_cmd_enabled(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "enabled", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	config->animations.enabled = parse_boolean(argv[0], config->animations.enabled);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *animations_cmd_frequency(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "frequency_ms", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	config->animations.frequency_ms = strtol(argv[0], NULL, 10);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

static struct cmd_results *parse_animation_curve(int argc, char **argv, struct sway_animation_curve **curve) {
	bool enabled = parse_boolean(argv[0], true);
	if (argc == 1) {
		destroy_animation_curve(*curve);
		*curve = create_animation_curve(enabled, 0, 0, NULL, 0.0, 0, NULL);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
	uint32_t duration = strtol(argv[1], NULL, 10);
	int arg = 2;
	uint32_t var_order = 0, off_order = 0;
	list_t *var_list = NULL, *off_list = NULL;
	double off_scale = 0.0;
	bool error = false;
	while (arg < argc) {
		if (strcasecmp(argv[arg], "var") == 0) {
			++arg;
			var_order = strtol(argv[arg++], NULL, 10);
			var_list = parse_double_array(argv[arg++]);
			if (!var_list) {
				error = true;
			}
		} else if (strcasecmp(argv[arg], "off") == 0) {
			++arg;
			off_scale = strtod(argv[arg++], NULL);
			off_order = strtol(argv[arg++], NULL, 10);
			off_list = parse_double_array(argv[arg++]);
			if (!off_list) {
				error = true;
			}
		} else {
			++arg;
		}
	}
	if (var_list || off_list) {
		destroy_animation_curve(*curve);
		*curve = create_animation_curve(enabled, duration, var_order, var_list, off_scale, off_order, off_list);
		if (var_list) {
			list_free_items_and_destroy(var_list);
		}
		if (off_list) {
			list_free_items_and_destroy(off_list);
		}
		return cmd_results_new(CMD_SUCCESS, NULL);
	} else if (error) {
		return cmd_results_new(CMD_FAILURE, "Error parsing animations array");
	} else {
		destroy_animation_curve(*curve);
		*curve = create_animation_curve(enabled, duration, 0, NULL, 0.0, 0, NULL);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
}

struct cmd_results *animations_cmd_default(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "default", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	return parse_animation_curve(argc, argv, &config->animations.anim_default);
}

struct cmd_results *animations_cmd_window_open(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "window_open", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	return parse_animation_curve(argc, argv, &config->animations.window_open);
}

struct cmd_results *animations_cmd_window_move(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "window_move", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	return parse_animation_curve(argc, argv, &config->animations.window_move);
}

struct cmd_results *animations_cmd_window_size(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "window_size", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	return parse_animation_curve(argc, argv, &config->animations.window_size);
}

struct cmd_results *cmd_animations(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "animations", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	sway_log(SWAY_DEBUG, "entering animations block: %s", argv[0]);

	struct cmd_results *res;

	if (find_handler(argv[0], animations_config_handlers,
			sizeof(animations_config_handlers))) {
		if (config->reading) {
			res = config_subcommand(argv, argc,
				animations_config_handlers, sizeof(animations_config_handlers));
		} else {
			res = cmd_results_new(CMD_FAILURE,
				"Can only be used in config file.");
		}
	} else {
		res = cmd_results_new(CMD_FAILURE,
			"Unknown animations subcommand.");
	}

	return res;
}
