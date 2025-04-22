#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/edges.h>
#include "sway/commands.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "log.h"
#include "util.h"

#define AXIS_HORIZONTAL (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)
#define AXIS_VERTICAL   (WLR_EDGE_TOP | WLR_EDGE_BOTTOM)

static uint32_t parse_resize_axis(const char *axis) {
	if (strcasecmp(axis, "width") == 0 || strcasecmp(axis, "horizontal") == 0) {
		return AXIS_HORIZONTAL;
	}
	if (strcasecmp(axis, "height") == 0 || strcasecmp(axis, "vertical") == 0) {
		return AXIS_VERTICAL;
	}
	if (strcasecmp(axis, "up") == 0) {
		return WLR_EDGE_TOP;
	}
	if (strcasecmp(axis, "down") == 0) {
		return WLR_EDGE_BOTTOM;
	}
	if (strcasecmp(axis, "left") == 0) {
		return WLR_EDGE_LEFT;
	}
	if (strcasecmp(axis, "right") == 0) {
		return WLR_EDGE_RIGHT;
	}
	return WLR_EDGE_NONE;
}

static bool is_horizontal(uint32_t axis) {
	return axis & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
}

/**
 * Implement `resize <grow|shrink>` for a floating container.
 */
static struct cmd_results *resize_adjust_floating(uint32_t axis,
		struct movement_amount *amount) {
	struct sway_container *con = config->handler_context.container;
	int grow_width = 0, grow_height = 0;

	if (is_horizontal(axis)) {
		grow_width = amount->amount;
	} else {
		grow_height = amount->amount;
	}

	// Make sure we're not adjusting beyond floating min/max size
	int min_width, max_width, min_height, max_height;
	floating_calculate_constraints(&min_width, &max_width,
			&min_height, &max_height);
	if (con->pending.width + grow_width < min_width) {
		grow_width = min_width - con->pending.width;
	} else if (con->pending.width + grow_width > max_width) {
		grow_width = max_width - con->pending.width;
	}
	if (con->pending.height + grow_height < min_height) {
		grow_height = min_height - con->pending.height;
	} else if (con->pending.height + grow_height > max_height) {
		grow_height = max_height - con->pending.height;
	}
	int grow_x = 0, grow_y = 0;

	if (axis == AXIS_HORIZONTAL) {
		grow_x = -grow_width / 2;
	} else if (axis == AXIS_VERTICAL) {
		grow_y = -grow_height / 2;
	} else if (axis == WLR_EDGE_TOP) {
		grow_y = -grow_height;
	} else if (axis == WLR_EDGE_LEFT) {
		grow_x = -grow_width;
	}
	if (grow_width == 0 && grow_height == 0) {
		return cmd_results_new(CMD_INVALID, "Cannot resize any further");
	}
	con->pending.x += grow_x;
	con->pending.y += grow_y;
	con->pending.width += grow_width;
	con->pending.height += grow_height;

	con->pending.content_x += grow_x;
	con->pending.content_y += grow_y;
	con->pending.content_width += grow_width;
	con->pending.content_height += grow_height;

	arrange_container(con);

	return cmd_results_new(CMD_SUCCESS, NULL);
}

/**
 * Implement `resize <grow|shrink>` for a tiled container.
 */
static struct cmd_results *resize_adjust_tiled(uint32_t axis,
		struct movement_amount *amount) {
	struct sway_container *current = config->handler_context.container;
	enum sway_container_layout layout = layout_get_type(config->handler_context.workspace);
	bool horizontal = is_horizontal(axis);
	if ((layout == L_HORIZ && horizontal) || (layout == L_VERT && !horizontal)) {
		if (current->pending.parent) {
			// Choose parent if not at workspace level yet
			current = current->pending.parent;
		}
	}

	if (container_is_scratchpad_hidden_or_child(current)) {
		return cmd_results_new(CMD_FAILURE, "Cannot resize a hidden scratchpad container");
	}

	if (amount->unit == MOVEMENT_UNIT_DEFAULT) {
		amount->unit = MOVEMENT_UNIT_PPT;
	}
	bool fail = true;
	if (amount->unit == MOVEMENT_UNIT_PPT) {
		float pct = amount->amount / 100.0f;
		if (horizontal) {
			double new_width = current->width_fraction + pct;
			if (new_width <= 1.0 && new_width >= 0.05) {
				fail = false;
				current->width_fraction = new_width;
				current->pending.width = current->pending.workspace->width * current->width_fraction;
				current->free_size = true;
			}
			if (layout == L_HORIZ) {
				// If it has children, propagate its width_fraction, overwriting whatever they had
				for (int i = 0; i < current->pending.children->length; ++i) {
					struct sway_container *con = current->pending.children->items[i];
					con->width_fraction = current->width_fraction;
					con->pending.width = con->pending.workspace->width * con->width_fraction;
					con->free_size = true;
				}
			}
		} else {
			double new_height = current->height_fraction + pct;
			if (new_height <= 1.0 && new_height >= 0.05) {
				fail = false;
				current->height_fraction = new_height;
				current->pending.height = current->pending.workspace->height * current->height_fraction;
				current->free_size = true;
			}
			if (layout == L_VERT) {
				// If it has children, propagate its height_fraction, overwriting whatever they had
				for (int i = 0; i < current->pending.children->length; ++i) {
					struct sway_container *con = current->pending.children->items[i];
					con->height_fraction = current->height_fraction;
					con->pending.height = con->pending.workspace->height * con->height_fraction;
					con->free_size = true;
				}
			}
		}
	} else {
		if (horizontal) {
			double new_width = current->pending.width + amount->amount;
			if (new_width <= current->pending.workspace->width && new_width >= MIN_SANE_W) {
				fail = false;
				current->pending.width = new_width;
				current->free_size = true;
			}
			if (layout == L_HORIZ) {
				// If it has children, propagate its width, overwriting whatever they had
				for (int i = 0; i < current->pending.children->length; ++i) {
					struct sway_container *con = current->pending.children->items[i];
					con->pending.width = new_width;
					con->free_size = true;
				}
			}
		} else {
			double new_height = current->pending.height + amount->amount;
			if (new_height <= current->pending.workspace->height && new_height >= MIN_SANE_H) {
				fail = false;
				current->pending.height = new_height;
				current->free_size = true;
			}
			if (layout == L_VERT) {
				// If it has children, propagate its height_fraction, overwriting whatever they had
				for (int i = 0; i < current->pending.children->length; ++i) {
					struct sway_container *con = current->pending.children->items[i];
					con->pending.height = new_height;
					con->free_size = true;
				}
			}
		}
	}
	if (fail) {
		return cmd_results_new(CMD_INVALID, "Cannot resize any further");
	}
	if (current->pending.parent) {
		arrange_container(current->pending.parent);
	} else {
		arrange_workspace(current->pending.workspace);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

/**
 * Implement `resize set` for a tiled container.
 */
static struct cmd_results *resize_set_tiled(struct sway_container *con,
		struct movement_amount *width, struct movement_amount *height) {

	if (container_is_scratchpad_hidden_or_child(con)) {
		return cmd_results_new(CMD_FAILURE, "Cannot resize a hidden scratchpad container");
	}

	struct sway_workspace *workspace = con->pending.workspace;
	enum sway_container_layout layout = layout_get_type(workspace);

	bool fail = true;
	if (width->amount) {
		if (layout == L_HORIZ && con->pending.parent) {
			con = con->pending.parent;
		}
		if (width->unit == MOVEMENT_UNIT_PPT ||
				width->unit == MOVEMENT_UNIT_DEFAULT) {
			if (width->amount <= 1.0 && width->amount >= 0.05) {
				fail = false;
				con->width_fraction = width->amount;
				con->pending.width = workspace->width * con->width_fraction;
				con->free_size = true;
				// If it has children, propagate its width, overwriting whatever they had
				for (int i = 0; i < con->pending.children->length; ++i) {
					struct sway_container *child = con->pending.children->items[i];
					child->width_fraction = con->width_fraction;
					child->pending.width = workspace->width * child->width_fraction;
					child->free_size = true;
				}
			}
		} else {
			if (width->amount <= workspace->width && width->amount >= MIN_SANE_W) {
				fail = false;
				con->pending.width = width->amount;
				con->free_size = true;
				// If it has children, propagate its width, overwriting whatever they had
				for (int i = 0; i < con->pending.children->length; ++i) {
					struct sway_container *child = con->pending.children->items[i];
					child->pending.width = width->amount;
					child->free_size = true;
				}
			}
		}
	}
	if (height->amount) {
		if (layout == L_VERT && con->pending.parent) {
			con = con->pending.parent;
		}
		if (height->unit == MOVEMENT_UNIT_PPT ||
				height->unit == MOVEMENT_UNIT_DEFAULT) {
			if (height->amount <= 1.0 && height->amount >= 0.05) {
				fail = false;
				con->height_fraction = height->amount;
				con->pending.height = con->pending.workspace->height * con->height_fraction;
				con->free_size = true;
				// If it has children, propagate its height, overwriting whatever they had
				for (int i = 0; i < con->pending.children->length; ++i) {
					struct sway_container *child = con->pending.children->items[i];
					child->height_fraction = con->height_fraction;
					child->pending.height = workspace->height * child->height_fraction;
					child->free_size = true;
				}
			}
		} else {
			if (height->amount <= workspace->height && height->amount >= MIN_SANE_H) {
				fail = false;
				con->pending.height = height->amount;
				con->free_size = true;
				// If it has children, propagate its width, overwriting whatever they had
				for (int i = 0; i < con->pending.children->length; ++i) {
					struct sway_container *child = con->pending.children->items[i];
					child->pending.height = height->amount;
					child->free_size = true;
				}
			}
		}
	}
	if (fail) {
		return cmd_results_new(CMD_INVALID, "Invalid size");
	}
	if (con->pending.parent) {
		arrange_container(con->pending.parent);
	} else {
		arrange_workspace(con->pending.workspace);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

/**
 * Implement `resize set` for a floating container.
 */
static struct cmd_results *resize_set_floating(struct sway_container *con,
		struct movement_amount *width, struct movement_amount *height) {
	int min_width, max_width, min_height, max_height, grow_width = 0, grow_height = 0;
	floating_calculate_constraints(&min_width, &max_width,
			&min_height, &max_height);

	if (width->amount) {
		switch (width->unit) {
		case MOVEMENT_UNIT_PPT:
			if (container_is_scratchpad_hidden(con)) {
				return cmd_results_new(CMD_FAILURE,
						"Cannot resize a hidden scratchpad container by ppt");
			}
			// Convert to px
			width->amount = con->pending.workspace->width * width->amount / 100;
			width->unit = MOVEMENT_UNIT_PX;
			// Falls through
		case MOVEMENT_UNIT_PX:
		case MOVEMENT_UNIT_DEFAULT:
			width->amount = fmax(min_width, fmin(width->amount, max_width));
			grow_width = width->amount - con->pending.width;
			con->pending.x -= grow_width / 2;
			con->pending.width = width->amount;
			break;
		case MOVEMENT_UNIT_INVALID:
			sway_assert(false, "invalid width unit");
			break;
		}
	}

	if (height->amount) {
		switch (height->unit) {
		case MOVEMENT_UNIT_PPT:
			if (container_is_scratchpad_hidden(con)) {
				return cmd_results_new(CMD_FAILURE,
						"Cannot resize a hidden scratchpad container by ppt");
			}
			// Convert to px
			height->amount = con->pending.workspace->height * height->amount / 100;
			height->unit = MOVEMENT_UNIT_PX;
			// Falls through
		case MOVEMENT_UNIT_PX:
		case MOVEMENT_UNIT_DEFAULT:
			height->amount = fmax(min_height, fmin(height->amount, max_height));
			grow_height = height->amount - con->pending.height;
			con->pending.y -= grow_height / 2;
			con->pending.height = height->amount;
			break;
		case MOVEMENT_UNIT_INVALID:
			sway_assert(false, "invalid height unit");
			break;
		}
	}

	con->pending.content_x -= grow_width / 2;
	con->pending.content_y -= grow_height / 2;
	con->pending.content_width += grow_width;
	con->pending.content_height += grow_height;

	arrange_container(con);

	return cmd_results_new(CMD_SUCCESS, NULL);
}

/**
 * resize set <args>
 *
 * args: [width] <width> [px|ppt]
 *     : height <height> [px|ppt]
 *     : [width] <width> [px|ppt] [height] <height> [px|ppt]
 */
static struct cmd_results *cmd_resize_set(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "resize", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	const char usage[] = "Expected 'resize set [width] <width> [px|ppt]' or "
		"'resize set height <height> [px|ppt]' or "
		"'resize set [width] <width> [px|ppt] [height] <height> [px|ppt]'";

	// Width
	struct movement_amount width = {0};
	if (argc >= 2 && !strcmp(argv[0], "width") && strcmp(argv[1], "height")) {
		argc--; argv++;
	}
	if (strcmp(argv[0], "height")) {
		int num_consumed_args = parse_movement_amount(argc, argv, &width);
		argc -= num_consumed_args;
		argv += num_consumed_args;
		if (width.unit == MOVEMENT_UNIT_INVALID) {
			return cmd_results_new(CMD_INVALID, "%s", usage);
		}
	}

	// Height
	struct movement_amount height = {0};
	if (argc) {
		if (argc >= 2 && !strcmp(argv[0], "height")) {
			argc--; argv++;
		}
		int num_consumed_args = parse_movement_amount(argc, argv, &height);
		if (argc > num_consumed_args) {
			return cmd_results_new(CMD_INVALID, "%s", usage);
		}
		if (width.unit == MOVEMENT_UNIT_INVALID) {
			return cmd_results_new(CMD_INVALID, "%s", usage);
		}
	}

	// If 0, don't resize that dimension
	struct sway_container *con = config->handler_context.container;
	if (width.amount <= 0) {
		width.amount = con->pending.width;
	}
	if (height.amount <= 0) {
		height.amount = con->pending.height;
	}

	if (container_is_floating(con)) {
		return resize_set_floating(con, &width, &height);
	}
	return resize_set_tiled(con, &width, &height);
}

/**
 * resize <grow|shrink> <args>
 *
 * args: <direction>
 * args: <direction> <amount> <unit>
 * args: <direction> <amount> <unit> or <amount> <other_unit>
 */
static struct cmd_results *cmd_resize_adjust(int argc, char **argv,
		int multiplier) {
	const char usage[] = "Expected 'resize grow|shrink <direction> "
		"[<amount> px|ppt [or <amount> px|ppt]]'";
	uint32_t axis = parse_resize_axis(*argv);
	if (axis == WLR_EDGE_NONE) {
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}
	--argc; ++argv;

	// First amount
	struct movement_amount first_amount;
	if (argc) {
		int num_consumed_args = parse_movement_amount(argc, argv, &first_amount);
		argc -= num_consumed_args;
		argv += num_consumed_args;
		if (first_amount.unit == MOVEMENT_UNIT_INVALID) {
			return cmd_results_new(CMD_INVALID, "%s", usage);
		}
	} else {
		first_amount.amount = 10;
		first_amount.unit = MOVEMENT_UNIT_DEFAULT;
	}

	// "or"
	if (argc) {
		if (strcmp(*argv, "or") != 0) {
			return cmd_results_new(CMD_INVALID, "%s", usage);
		}
		--argc; ++argv;
	}

	// Second amount
	struct movement_amount second_amount;
	if (argc) {
		int num_consumed_args = parse_movement_amount(argc, argv, &second_amount);
		if (argc > num_consumed_args) {
			return cmd_results_new(CMD_INVALID, "%s", usage);
		}
		if (second_amount.unit == MOVEMENT_UNIT_INVALID) {
			return cmd_results_new(CMD_INVALID, "%s", usage);
		}
	} else {
		second_amount.amount = 0;
		second_amount.unit = MOVEMENT_UNIT_INVALID;
	}

	first_amount.amount *= multiplier;
	second_amount.amount *= multiplier;

	struct sway_container *con = config->handler_context.container;
	if (container_is_floating(con)) {
		// Floating containers can only resize in px. Choose an amount which
		// uses px, with fallback to an amount that specified no unit.
		if (first_amount.unit == MOVEMENT_UNIT_PX) {
			return resize_adjust_floating(axis, &first_amount);
		} else if (second_amount.unit == MOVEMENT_UNIT_PX) {
			return resize_adjust_floating(axis, &second_amount);
		} else if (first_amount.unit == MOVEMENT_UNIT_DEFAULT) {
			return resize_adjust_floating(axis, &first_amount);
		} else if (second_amount.unit == MOVEMENT_UNIT_DEFAULT) {
			return resize_adjust_floating(axis, &second_amount);
		} else {
			return cmd_results_new(CMD_INVALID,
					"Floating containers cannot use ppt measurements");
		}
	}

	// For tiling, prefer ppt -> default -> px
	if (first_amount.unit == MOVEMENT_UNIT_PPT) {
		return resize_adjust_tiled(axis, &first_amount);
	} else if (second_amount.unit == MOVEMENT_UNIT_PPT) {
		return resize_adjust_tiled(axis, &second_amount);
	} else if (first_amount.unit == MOVEMENT_UNIT_DEFAULT) {
		return resize_adjust_tiled(axis, &first_amount);
	} else if (second_amount.unit == MOVEMENT_UNIT_DEFAULT) {
		return resize_adjust_tiled(axis, &second_amount);
	} else {
		return resize_adjust_tiled(axis, &first_amount);
	}
}

struct cmd_results *cmd_resize(int argc, char **argv) {
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there's no outputs connected.");
	}
	struct sway_container *current = config->handler_context.container;
	if (!current) {
		return cmd_results_new(CMD_INVALID, "Cannot resize nothing");
	}

	struct cmd_results *error;
	if ((error = checkarg(argc, "resize", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	if (strcasecmp(argv[0], "set") == 0) {
		return cmd_resize_set(argc - 1, &argv[1]);
	}
	if (strcasecmp(argv[0], "grow") == 0) {
		return cmd_resize_adjust(argc - 1, &argv[1], 1);
	}
	if (strcasecmp(argv[0], "shrink") == 0) {
		return cmd_resize_adjust(argc - 1, &argv[1], -1);
	}

	const char usage[] = "Expected 'resize <set|shrink|grow> "
		"<width|height|up|down|left|right> [<amount>] [px|ppt]'";

	return cmd_results_new(CMD_INVALID, "%s", usage);
}
