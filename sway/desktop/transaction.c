#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/types/wlr_buffer.h>
#include "sway/config.h"
#include "sway/scene_descriptor.h"
#include "sway/desktop/idle_inhibit_v1.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "sway/tree/layout.h"
#include "list.h"
#include "log.h"

struct sway_transaction {
	struct wl_event_source *timer;
	list_t *instructions;   // struct sway_transaction_instruction *
	size_t num_waiting;
	size_t num_configures;
	struct timespec commit_time;
};

struct sway_transaction_instruction {
	struct sway_transaction *transaction;
	struct sway_node *node;
	union {
		struct sway_output_state output_state;
		struct sway_workspace_state workspace_state;
		struct sway_container_state container_state;
	};
	uint32_t serial;
	bool server_request;
	bool waiting;
};

static struct sway_transaction *transaction_create(void) {
	struct sway_transaction *transaction =
		calloc(1, sizeof(struct sway_transaction));
	if (!sway_assert(transaction, "Unable to allocate transaction")) {
		return NULL;
	}
	transaction->instructions = create_list();
	return transaction;
}

static void transaction_destroy(struct sway_transaction *transaction) {
	// Free instructions
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_node *node = instruction->node;
		node->ntxnrefs--;
		if (node->instruction == instruction) {
			node->instruction = NULL;
		}
		if (node->destroying && node->ntxnrefs == 0) {
			switch (node->type) {
			case N_ROOT:
				sway_assert(false, "Never reached");
				break;
			case N_OUTPUT:
				output_destroy(node->sway_output);
				break;
			case N_WORKSPACE:
				workspace_destroy(node->sway_workspace);
				break;
			case N_CONTAINER:
				container_destroy(node->sway_container);
				break;
			}
		}
		free(instruction);
	}
	list_free(transaction->instructions);

	if (transaction->timer) {
		wl_event_source_remove(transaction->timer);
	}
	free(transaction);
}

static void copy_output_state(struct sway_output *output,
		struct sway_transaction_instruction *instruction) {
	struct sway_output_state *state = &instruction->output_state;
	if (state->workspaces) {
		state->workspaces->length = 0;
	} else {
		state->workspaces = create_list();
	}
	list_cat(state->workspaces, output->workspaces);

	state->active_workspace = output_get_active_workspace(output);
}

static void copy_workspace_state(struct sway_workspace *ws,
		struct sway_transaction_instruction *instruction) {
	struct sway_workspace_state *state = &instruction->workspace_state;

	state->fullscreen = ws->fullscreen;
	state->x = ws->x;
	state->y = ws->y;
	state->width = ws->width;
	state->height = ws->height;

	if (state->floating) {
		state->floating->length = 0;
	} else {
		state->floating = create_list();
	}
	if (state->tiling) {
		state->tiling->length = 0;
	} else {
		state->tiling = create_list();
	}
	list_cat(state->floating, ws->floating);
	list_cat(state->tiling, ws->tiling);

	struct sway_seat *seat = input_manager_current_seat();
	state->focused = seat_get_focus(seat) == &ws->node;

	// Set focused_inactive_child to the direct tiling child
	struct sway_container *focus = seat_get_focus_inactive_tiling(seat, ws);
	if (focus) {
		while (focus->pending.parent) {
			focus = focus->pending.parent;
		}
	}
	state->focused_inactive_child = focus;
}

static void copy_container_state(struct sway_container *container,
		struct sway_transaction_instruction *instruction) {
	struct sway_container_state *state = &instruction->container_state;

	if (state->children) {
		list_free(state->children);
	}

	memcpy(state, &container->pending, sizeof(struct sway_container_state));

	if (!container->view) {
		// We store a copy of the child list to avoid having it mutated after
		// we copy the state.
		state->children = create_list();
		list_cat(state->children, container->pending.children);
	} else {
		state->children = NULL;
	}

	struct sway_seat *seat = input_manager_current_seat();
	state->focused = seat_get_focus(seat) == &container->node;

	if (!container->view) {
		struct sway_node *focus =
			seat_get_active_tiling_child(seat, &container->node);
		state->focused_inactive_child = focus ? focus->sway_container : NULL;
	}
}

static void transaction_add_node(struct sway_transaction *transaction,
		struct sway_node *node, bool server_request) {
	struct sway_transaction_instruction *instruction = NULL;

	// Check if we have an instruction for this node already, in which case we
	// update that instead of creating a new one.
	if (node->ntxnrefs > 0) {
		for (int idx = 0; idx < transaction->instructions->length; idx++) {
			struct sway_transaction_instruction *other =
				transaction->instructions->items[idx];
			if (other->node == node) {
				instruction = other;
				break;
			}
		}
	}

	if (!instruction) {
		instruction = calloc(1, sizeof(struct sway_transaction_instruction));
		if (!sway_assert(instruction, "Unable to allocate instruction")) {
			return;
		}
		instruction->transaction = transaction;
		instruction->node = node;
		instruction->server_request = server_request;

		list_add(transaction->instructions, instruction);
		node->ntxnrefs++;
	} else if (server_request) {
		instruction->server_request = true;
	}

	switch (node->type) {
	case N_ROOT:
		break;
	case N_OUTPUT:
		copy_output_state(node->sway_output, instruction);
		break;
	case N_WORKSPACE:
		copy_workspace_state(node->sway_workspace, instruction);
		break;
	case N_CONTAINER:
		copy_container_state(node->sway_container, instruction);
		break;
	}
}

static void apply_output_state(struct sway_output *output,
		struct sway_output_state *state) {
	list_free(output->current.workspaces);
	memcpy(&output->current, state, sizeof(struct sway_output_state));
}

static void apply_workspace_state(struct sway_workspace *ws,
		struct sway_workspace_state *state) {
	list_free(ws->current.floating);
	list_free(ws->current.tiling);
	memcpy(&ws->current, state, sizeof(struct sway_workspace_state));
}

static void apply_container_state(struct sway_container *container,
		struct sway_container_state *state) {
	struct sway_view *view = container->view;
	// There are separate children lists for each instruction state, the
	// container's current state and the container's pending state
	// (ie. con->children). The list itself needs to be freed here.
	// Any child containers which are being deleted will be cleaned up in
	// transaction_destroy().
	list_free(container->current.children);

	memcpy(&container->current, state, sizeof(struct sway_container_state));

	if (view) {
		if (view->saved_surface_tree) {
			if (!container->node.destroying || container->node.ntxnrefs == 1) {
				view_remove_saved_buffer(view);
			}
		}

		// If the view hasn't responded to the configure, center it within
		// the container. This is important for fullscreen views which
		// refuse to resize to the size of the output.
		if (view->surface) {
			view_center_and_clip_surface(view);
		}
	}
}

static void arrange_title_bar(struct sway_container *con,
		int x, int y, int width, int height) {
	container_update(con);

	bool has_title_bar = height > 0;
	sway_scene_node_set_enabled(&con->title_bar.tree->node, has_title_bar);
	if (!has_title_bar) {
		return;
	}

	sway_scene_node_set_position(&con->title_bar.tree->node, x, y);

	con->title_width = width;
	container_arrange_title_bar(con);
}

static void disable_container(struct sway_container *con) {
	if (con->view) {
		sway_scene_node_reparent(&con->view->scene_tree->node, con->content_tree);
	} else {
		for (int i = 0; i < con->current.children->length; i++) {
			struct sway_container *child = con->current.children->items[i];

			sway_scene_node_reparent(&child->scene_tree->node, con->content_tree);

			disable_container(child);
		}
	}
}

static void arrange_container(struct sway_container *con,
		double width, double height, bool title_bar, int gaps);

// Workspace stores in x, y the logical coordinate that will be applied to a node
// with x, y = 0, and it includes gaps_out. So if we want to place
// a container on the leftmost possible position, we should use gaps_in, and it will
// include both gaps, out and the container's in. Workspace also stores its width
// and height, and they are the effective, available space: viewport resolution
// minus 2 * gaps_out.
static double compute_active_offset(struct sway_workspace *workspace,
		enum sway_container_layout layout, list_t *children,
		int active_idx, int width, int height, int gaps) {
	struct sway_container *active = children->items[active_idx];
	// Offsets may be wrong, so consider only the active position and all the
	// widths and order are valid. Also, when the workspace is scaled, offsets
	// and sizes are not.
	double scale = layout_scale_enabled(workspace) ? layout_scale_get(workspace) : 1.0;
	if (layout == L_HORIZ) {
		bool center = layout_modifiers_get_center_horizontal(workspace);
		if (center) {
			return workspace->x + 0.5 * (width - scale * active->current.width);
		}
		// Center row if space available
        double lwidth = 0, rwidth = 0;
        for (int i = 0; i < active_idx; ++i) {
			struct sway_container *con = children->items[i];
            lwidth += scale * (con->current.width + 2 * gaps);
        }
        for (int i = active_idx; i < children->length; ++i) {
			struct sway_container *con = children->items[i];
            rwidth += scale * (con->current.width + 2 * gaps);
        }
        double twidth = lwidth + rwidth;
        if (twidth <= width) {
            double start = 0.5 * (width - twidth);
            return workspace->x + start + lwidth + scale * gaps;
        }

		double a_x = active->pending.x;
		double a_w = active->pending.width;
		if (a_x < workspace->x) {
			// active starts outside on the left
			// set it on the left edge
			return workspace->x + scale * gaps;
		} else if (a_x + scale * a_w > workspace->x + width) {
			// active overflows to the right, move to end of viewport
			return workspace->x + width - scale * (a_w + gaps);
		} else {
			// Active is inside the viewport
			// If Active is the first container, it should be on the left edge,
			// and if it is the last one, on the right (because we know total
			// container width is wider than the viewport, otherwise it would have
			// been centered earlier).
			if (active_idx == 0) {
				return workspace->x + scale * gaps;
			} else if (active_idx == children->length - 1) {
				return workspace->x + width - scale * (a_w + gaps);
			}
			// If any of the windows next to it on its right or left are
			// in the viewport, keep the current position.
			struct sway_container *prev = active_idx > 0 ? children->items[active_idx - 1] : NULL;
			struct sway_container *next = active_idx < children->length - 1 ? children->items[active_idx + 1] : NULL;
			bool keep_current = false;
			// Note that previous and next positions may be wrong because we could be moving the active container
			if (prev) {
				double prev_x = a_x - scale * (prev->pending.width + 2 * gaps);
				if (prev_x - scale * gaps >= workspace->x && prev_x + scale * (prev->pending.width + gaps) <= workspace->x + width) {
					keep_current = true;
				}
			}
			if (!keep_current && next) {
				double next_x = a_x + scale * (a_w + 2 * gaps);
				if (next_x - scale * gaps >= workspace->x && next_x + scale * (next->pending.width + gaps) <= workspace->x + width) {
					keep_current = true;
				}
			}
			if (!keep_current) {
				// If not:
				// We try to fit the column next to it on the right if it fits
				// completely, otherwise the one on the left. If none of them fit,
				// we leave it as it is.
				if (next) {
					if (scale * (a_w + 3 * gaps + next->pending.width) <= width) {
						// set next at the right edge of the viewport
						return workspace->x + width - scale * (a_w + 3 * gaps + next->pending.width);
					} else if (prev) {
						if (scale * (prev->pending.width + 3 * gaps + a_w) <= width) {
							// set previous at the left edge of the viewport
							return workspace->x + scale * (prev->pending.width + 3 * gaps);
						} else {
							// none of them fit, leave active as it is
							return a_x;
						}
					} else {
						// nothing on the left, move active to left edge of viewport
						return workspace->x + scale * gaps;
					}
				} else if (prev) {
					if (scale * (prev->pending.width + 3 * gaps + a_w) <= width) {
						// set previous at the left edge of the viewport
						return workspace->x + scale * (prev->pending.width + 3 * gaps);
					} else {
						// it doesn't fit and nothing on the right, move active to right edge of viewport
						return workspace->x + width - scale * (a_w + gaps);
					}
				} else {
					// nothing on the right or left, the window is in a correct position
					return a_x;
				}
			} else {
				// the window is in a correct position
				return a_x;
			}
		}
	} else {
		bool center = layout_modifiers_get_center_vertical(workspace);
		if (center) {
			return workspace->y + 0.5 * (height - scale * active->pending.height);
		}
		// Center row if space available
        double lheight = 0, rheight = 0;
        for (int i = 0; i < active_idx; ++i) {
			struct sway_container *con = children->items[i];
            lheight += scale * (con->current.height + 2 * gaps);
        }
        for (int i = active_idx; i < children->length; ++i) {
			struct sway_container *con = children->items[i];
            rheight += scale * (con->current.height + 2 * gaps);
        }
        double theight = lheight + rheight;
        if (theight <= height) {
            double start = 0.5 * (height - theight);
            return workspace->y + start + lheight + scale * gaps;
        }

		double a_y = active->pending.y;
		double a_h = active->pending.height;
		if (a_y < workspace->y) {
			// active starts outside on the left
			// set it on the left edge
			return workspace->y + scale * gaps;
		} else if (a_y + scale * a_h > workspace->y + height) {
			// active overflows to the right, move to end of viewport
			return workspace->y + height - scale * (a_h + gaps);
		} else {
			// Active is inside the viewport
			// If Active is the first container, it should be on the top edge,
			// and if it is the last one, on the bottom (because we know total
			// container height is higher than the viewport, otherwise it would have
			// been centered earlier).
			if (active_idx == 0) {
				return workspace->y + scale * gaps;
			} else if (active_idx == children->length - 1) {
				return workspace->y + height - scale * (a_h + gaps);
			}
			// If any of the windows next to it on its right or left are
			// in the viewport, keep the current position.
			struct sway_container *prev = active_idx > 0 ? children->items[active_idx - 1] : NULL;
			struct sway_container *next = active_idx < children->length - 1 ? children->items[active_idx + 1] : NULL;
			bool keep_current = false;
			// Note that previous and next positions may be wrong because we could be moving the active container
			if (prev) {
				double prev_y = a_y - scale * (prev->pending.height + 2 * gaps);
				if (prev_y - scale * gaps >= workspace->y && prev_y + scale * (prev->pending.height + gaps) <= workspace->y + height) {
					keep_current = true;
				}
			}
			if (!keep_current && next) {
				double next_y = a_y + scale * (a_h + 2 * gaps);
				if (next_y - scale * gaps >= workspace->y && next_y + scale * (next->pending.height + gaps) <= workspace->y + height) {
					keep_current = true;
				}
			}
			if (!keep_current) {
				// If not:
				// We try to fit the column next to it on the right if it fits
				// completely, otherwise the one on the left. If none of them fit,
				// we leave it as it is.
				if (next) {
					if (scale * (a_h + 3 * gaps + next->pending.height) <= height) {
						// set next at the right edge of the viewport
						return workspace->y + height - scale * (a_h + 3 * gaps + next->pending.height);
					} else if (prev) {
						if (scale * (prev->pending.height + 3 * gaps + a_h) <= 0.0) {
							// set previous at the left edge of the viewport
							return workspace->y + scale * (prev->pending.height + 3 * gaps);
						} else {
							// none of them fit, leave active as it is
							return a_y;
						}
					} else {
						// nothing on the left, move active to left edge of viewport
						return workspace->y + scale * gaps;
					}
				} else if (prev) {
					if (scale * (prev->pending.height + 3 * gaps + a_h) <= height) {
						// set previous at the left edge of the viewport
						return workspace->y + scale * (prev->pending.height + 3 * gaps);
					} else {
						// it doesn't fit and nothing on the right, move active to right edge of viewport
						return workspace->y + height - scale * (a_h + gaps);
					}
				} else {
					// nothing on the right or left, the window is in a correct position
					return a_y;
				}
			} else {
				// the window is in a correct position
				return a_y;
			}
		}
	}
}

static void arrange_children(enum sway_container_layout layout, list_t *children,
		struct sway_container *active, struct sway_scene_tree *content,
		int width, int height, int gaps) {

	if (children->length == 0) {
		return;
	}

	int active_idx = list_find(children, active);
	if (active_idx == -1) {
		active_idx = 0;
		active = children->items[active_idx];
	}
	struct sway_workspace *workspace = active->pending.workspace;
	float scale = layout_scale_enabled(workspace) ? layout_scale_get(workspace) : 1.0f;
	double offset;
	if (workspace->gesture.scrolling || layout_modifiers_get_reorder(workspace) == REORDER_LAZY) {
		offset = layout == L_HORIZ ? active->pending.x : active->pending.y;
	} else {
		offset = compute_active_offset(workspace, layout, children, active_idx,
			workspace->width, workspace->height, gaps);
	}

	if (layout == L_VERT) {
		double off = offset - workspace->y;
		for (int i = active_idx; i < children->length; ++i) {
			struct sway_container *child = children->items[i];
			struct sway_container *parent = child->pending.parent;
			double cheight = child->pending.height;
			sway_scene_node_set_enabled(&child->border.tree->node, true);
			sway_scene_node_set_position(&child->scene_tree->node, 0, round(off));
			double delta = child->pending.content_y - child->pending.y;
			child->pending.y = off + workspace->y;
			if (child->view) {
				child->pending.content_y = child->pending.y + delta;
			}
			if (parent) {
				delta = parent->pending.x - child->pending.x;
				child->pending.x = parent->pending.x;
				if (child->view) {
					child->pending.content_x += delta;
				}
			}
			sway_scene_node_reparent(&child->scene_tree->node, content);
			arrange_container(child, child->pending.width, cheight, true, gaps);
			off += scale * (cheight + 2 * gaps);
		}
		off = offset - workspace->y;
		for (int i = active_idx - 1; i >= 0; i--) {
			struct sway_container *child = children->items[i];
			struct sway_container *parent = child->pending.parent;
			double cheight = child->pending.height;
			off -= scale * (cheight + 2 * gaps);
			double delta = child->pending.content_y - child->pending.y;
			child->pending.y = off + workspace->y;
			if (child->view) {
				child->pending.content_y = child->pending.y + delta;
			}
			if (parent) {
				delta = parent->pending.x - child->pending.x;
				child->pending.x = parent->pending.x;
				if (child->view) {
					child->pending.content_x += delta;
				}
			}
			sway_scene_node_set_enabled(&child->border.tree->node, true);
			sway_scene_node_set_position(&child->scene_tree->node, 0, round(off));
			sway_scene_node_reparent(&child->scene_tree->node, content);
			arrange_container(child, child->pending.width, cheight, true, gaps);
		}
	} else if (layout == L_HORIZ) {
		double off = offset - workspace->x;
		for (int i = active_idx; i < children->length; ++i) {
			struct sway_container *child = children->items[i];
			struct sway_container *parent = child->pending.parent;
			int cwidth = child->pending.width;
			sway_scene_node_set_enabled(&child->border.tree->node, true);
			sway_scene_node_set_position(&child->scene_tree->node, round(off), 0);
			// Update child for next iteration. Transactions don't re-arrange
			// the layout (arrange.c:apply_xxx()), so we need to set it here,
			// otherwise the next call will have the positions wrong and the
			// offset won't be optimal.
			double delta = child->pending.content_x - child->pending.x;
			child->pending.x = off + workspace->x;
			if (child->view) {
				child->pending.content_x = child->pending.x + delta;
			}
			if (parent) {
				delta = parent->pending.y - child->pending.y;
				child->pending.y = parent->pending.y;
				if (child->view) {
					child->pending.content_y += delta;
				}
			}
			sway_scene_node_reparent(&child->scene_tree->node, content);
			arrange_container(child, cwidth, child->pending.height, true, gaps);
			off += scale * (cwidth + 2 * gaps);
		}
		off = offset - workspace->x;
		for (int i = active_idx - 1; i >= 0; i--) {
			struct sway_container *child = children->items[i];
			struct sway_container *parent = child->pending.parent;
			int cwidth = child->pending.width;
			off -= scale * (cwidth + 2 * gaps);
			double delta = child->pending.content_x - child->pending.x;
			child->pending.x = off + workspace->x;
			if (child->view) {
				child->pending.content_x = child->pending.x + delta;
			}
			if (parent) {
				delta = parent->pending.y - child->pending.y;
				child->pending.y = parent->pending.y;
				if (child->view) {
					child->pending.content_y += delta;
				}
			}
			sway_scene_node_set_enabled(&child->border.tree->node, true);
			sway_scene_node_set_position(&child->scene_tree->node, round(off), 0);
			sway_scene_node_reparent(&child->scene_tree->node, content);
			arrange_container(child, cwidth, child->pending.height, true, gaps);
		}
	} else {
		sway_assert(false, "unreachable");
	}
}

static void arrange_container(struct sway_container *con,
		double dwidth, double dheight, bool title_bar, int gaps) {
	// this container might have previously been in the scratchpad,
	// make sure it's enabled for viewing
	sway_scene_node_set_enabled(&con->scene_tree->node, true);

	if (con->output_handler) {
		sway_scene_buffer_set_dest_size(con->output_handler, round(dwidth), round(dheight));
	}

	if (con->view) {
		struct sway_workspace *workspace = con->pending.workspace;
		float scale = layout_scale_enabled(workspace) ? layout_scale_get(workspace) : 1.0f;
		int border_top = round(container_titlebar_height() * scale);
		int border_width = round(con->current.border_thickness * scale);
		int width = round(scale * dwidth);
		int height = round(scale * dheight);

		if (title_bar && con->current.border != B_NORMAL) {
			sway_scene_node_set_enabled(&con->title_bar.tree->node, false);
			sway_scene_node_set_enabled(&con->border.top->node, true);
		} else {
			sway_scene_node_set_enabled(&con->border.top->node, false);
		}

		if (con->current.border == B_NORMAL) {
			if (title_bar) {
				arrange_title_bar(con, 0, 0, width, border_top);
			} else {
				border_top = 0;
				// should be handled by the parent container
			}
		} else if (con->current.border == B_PIXEL) {
			container_update(con);
			border_top = title_bar && con->current.border_top ? border_width : 0;
		} else if (con->current.border == B_NONE) {
			container_update(con);
			border_top = 0;
			border_width = 0;
		} else if (con->current.border == B_CSD) {
			border_top = 0;
			border_width = 0;
		} else {
			sway_assert(false, "unreachable");
		}

		int border_bottom = con->current.border_bottom ? border_width : 0;
		int border_left = con->current.border_left ? border_width : 0;
		int border_right = con->current.border_right ? border_width : 0;
		int vert_border_height = MAX(0, height - border_top - border_bottom);

		sway_scene_rect_set_size(con->border.top, width, border_top);
		sway_scene_rect_set_size(con->border.bottom, width, border_bottom);
		sway_scene_rect_set_size(con->border.left,
			border_left, vert_border_height);
		sway_scene_rect_set_size(con->border.right,
			border_right, vert_border_height);

		sway_scene_node_set_position(&con->border.top->node, 0, 0);
		sway_scene_node_set_position(&con->border.bottom->node,
			0, height - border_bottom);
		sway_scene_node_set_position(&con->border.left->node,
			0, border_top);
		sway_scene_node_set_position(&con->border.right->node,
			width - border_right, border_top);

		// make sure to reparent, it's possible that the client just came out of
		// fullscreen mode where the parent of the surface is not the container
		sway_scene_node_reparent(&con->view->scene_tree->node, con->content_tree);
		sway_scene_node_set_position(&con->view->scene_tree->node,
			border_left, border_top);
	} else {
		// make sure to disable the title bar if the parent is not managing it
		if (title_bar) {
			sway_scene_node_set_enabled(&con->title_bar.tree->node, false);
		}

		arrange_children(con->current.layout, con->current.children,
			con->current.focused_inactive_child, con->content_tree,
			round(dwidth), round(dheight), gaps);
	}
}

static int container_get_gaps(struct sway_container *con) {
	struct sway_workspace *ws = con->current.workspace;
	return ws->gaps_inner;
}

static void arrange_fullscreen(struct sway_scene_tree *tree,
		struct sway_container *fs, struct sway_workspace *ws,
		int width, int height) {
	struct sway_scene_node *fs_node;
	if (fs->view) {
		fs_node = &fs->view->scene_tree->node;

		// if we only care about the view, disable any decorations
		sway_scene_node_set_enabled(&fs->scene_tree->node, false);
	} else {
		fs_node = &fs->scene_tree->node;
		arrange_container(fs, width, height, true, container_get_gaps(fs));
	}

	sway_scene_node_reparent(fs_node, tree);
	sway_scene_node_lower_to_bottom(fs_node);
	sway_scene_node_set_position(fs_node, 0, 0);
}

static void arrange_workspace_floating(struct sway_workspace *ws) {
	for (int i = 0; i < ws->current.floating->length; i++) {
		struct sway_container *floater = ws->current.floating->items[i];
		struct sway_scene_tree *layer = root->layers.floating;

		if (floater->current.fullscreen_mode != FULLSCREEN_NONE) {
			continue;
		}

		if (root->fullscreen_global) {
			if (container_is_transient_for(floater, root->fullscreen_global)) {
				layer = root->layers.fullscreen_global;
			}
		} else {
			for (int i = 0; i < root->outputs->length; i++) {
				struct sway_output *output = root->outputs->items[i];
				struct sway_workspace *active = output->current.active_workspace;

				if (active && active->fullscreen &&
						container_is_transient_for(floater, active->fullscreen)) {
					layer = root->layers.fullscreen;
				}
			}
		}

		sway_scene_node_reparent(&floater->scene_tree->node, layer);
		sway_scene_node_set_position(&floater->scene_tree->node,
			floater->current.x, floater->current.y);
		sway_scene_node_set_enabled(&floater->scene_tree->node, true);

		arrange_container(floater, floater->current.width, floater->current.height,
			true, ws->gaps_inner);
	}
}

static void arrange_workspace_tiling(struct sway_workspace *ws,
		int width, int height) {
	if (ws->current.tiling->length == 0) {
		return;
	}
	if (layout_overview_enabled(ws)) {
		layout_overview_recompute_scale(ws, ws->gaps_inner);
	}
	arrange_children(layout_get_type(ws), ws->current.tiling,
		ws->current.focused_inactive_child, ws->layers.tiling,
		width, height, ws->gaps_inner);
}

static void disable_workspace(struct sway_workspace *ws) {
	// if any containers were just moved to a disabled workspace it will
	// have the parent of the old workspace. Move the workspace so that it won't
	// be shown.
	for (int i = 0; i < ws->current.tiling->length; i++) {
		struct sway_container *child = ws->current.tiling->items[i];

		sway_scene_node_reparent(&child->scene_tree->node, ws->layers.tiling);
		disable_container(child);
	}

	for (int i = 0; i < ws->current.floating->length; i++) {
		struct sway_container *floater = ws->current.floating->items[i];
		sway_scene_node_reparent(&floater->scene_tree->node, root->layers.floating);
		disable_container(floater);
		sway_scene_node_set_enabled(&floater->scene_tree->node, false);
	}
}

static void arrange_output(struct sway_output *output, int width, int height) {
	for (int i = 0; i < output->current.workspaces->length; i++) {
		struct sway_workspace *child = output->current.workspaces->items[i];

		bool activated = output->current.active_workspace == child && output->wlr_output->enabled;

		sway_scene_node_reparent(&child->layers.tiling->node, output->layers.tiling);
		sway_scene_node_reparent(&child->layers.fullscreen->node, output->layers.fullscreen);

		for (int i = 0; i < child->current.floating->length; i++) {
			struct sway_container *floater = child->current.floating->items[i];
			sway_scene_node_reparent(&floater->scene_tree->node, root->layers.floating);
			sway_scene_node_set_enabled(&floater->scene_tree->node, activated);
		}

		if (activated) {
			struct sway_container *fs = child->current.fullscreen;
			sway_scene_node_set_enabled(&child->layers.tiling->node, !fs);
			sway_scene_node_set_enabled(&child->layers.fullscreen->node, fs);

			arrange_workspace_floating(child);

			sway_scene_node_set_enabled(&output->layers.shell_background->node, !fs);
			sway_scene_node_set_enabled(&output->layers.shell_bottom->node, !fs);
			sway_scene_node_set_enabled(&output->layers.fullscreen->node, fs);

			if (fs) {
				sway_scene_rect_set_size(output->fullscreen_background, width, height);

				arrange_fullscreen(child->layers.fullscreen, fs, child,
					width, height);
			} else {
				struct wlr_box *area = &output->usable_area;
				struct side_gaps *gaps = &child->current_gaps;

				sway_scene_node_set_position(&child->layers.tiling->node,
					gaps->left + area->x, gaps->top + area->y);

				arrange_workspace_tiling(child,
					area->width - gaps->left - gaps->right,
					area->height - gaps->top - gaps->bottom);
			}
		} else {
			sway_scene_node_set_enabled(&child->layers.tiling->node, false);
			sway_scene_node_set_enabled(&child->layers.fullscreen->node, false);

			disable_workspace(child);
		}
	}
}

void arrange_popups(struct sway_scene_tree *popups) {
	struct sway_scene_node *node;
	wl_list_for_each(node, &popups->children, link) {
		struct sway_popup_desc *popup = scene_descriptor_try_get(node,
			SWAY_SCENE_DESC_POPUP);

		if (popup) {
			int lx, ly;
			sway_scene_node_coords(popup->relative, &lx, &ly);
			sway_scene_node_set_position(node, lx, ly);
		}
	}
}

static void arrange_root(struct sway_root *root) {
	struct sway_container *fs = root->fullscreen_global;

	sway_scene_node_set_enabled(&root->layers.shell_background->node, !fs);
	sway_scene_node_set_enabled(&root->layers.shell_bottom->node, !fs);
	sway_scene_node_set_enabled(&root->layers.tiling->node, !fs);
	sway_scene_node_set_enabled(&root->layers.floating->node, !fs);
	sway_scene_node_set_enabled(&root->layers.shell_top->node, !fs);
	sway_scene_node_set_enabled(&root->layers.fullscreen->node, !fs);

	// hide all contents in the scratchpad
	for (int i = 0; i < root->scratchpad->length; i++) {
		struct sway_container *con = root->scratchpad->items[i];

		// When a container is moved to a scratchpad, it's possible that it
		// was moved into a floating container as part of the same transaction.
		// In this case, we need to make sure we reparent all the container's
		// children so that disabling the container will disable all descendants.
		if (!con->view) for (int ii = 0; ii < con->current.children->length; ii++) {
			struct sway_container *child = con->current.children->items[ii];
			sway_scene_node_reparent(&child->scene_tree->node, con->content_tree);
		}

		sway_scene_node_set_enabled(&con->scene_tree->node, false);
	}

	if (fs) {
		for (int i = 0; i < root->outputs->length; i++) {
			struct sway_output *output = root->outputs->items[i];
			struct sway_workspace *ws = output->current.active_workspace;

			sway_scene_output_set_position(output->scene_output, output->lx, output->ly);

			if (ws) {
				arrange_workspace_floating(ws);
			}
		}

		arrange_fullscreen(root->layers.fullscreen_global, fs, NULL,
			root->width, root->height);
	} else {
		for (int i = 0; i < root->outputs->length; i++) {
			struct sway_output *output = root->outputs->items[i];

			sway_scene_output_set_position(output->scene_output, output->lx, output->ly);

			sway_scene_node_reparent(&output->layers.shell_background->node, root->layers.shell_background);
			sway_scene_node_reparent(&output->layers.shell_bottom->node, root->layers.shell_bottom);
			sway_scene_node_reparent(&output->layers.tiling->node, root->layers.tiling);
			sway_scene_node_reparent(&output->layers.shell_top->node, root->layers.shell_top);
			sway_scene_node_reparent(&output->layers.shell_overlay->node, root->layers.shell_overlay);
			sway_scene_node_reparent(&output->layers.fullscreen->node, root->layers.fullscreen);
			sway_scene_node_reparent(&output->layers.session_lock->node, root->layers.session_lock);

			sway_scene_node_set_position(&output->layers.shell_background->node, output->lx, output->ly);
			sway_scene_node_set_position(&output->layers.shell_bottom->node, output->lx, output->ly);
			sway_scene_node_set_position(&output->layers.tiling->node, output->lx, output->ly);
			sway_scene_node_set_position(&output->layers.fullscreen->node, output->lx, output->ly);
			sway_scene_node_set_position(&output->layers.shell_top->node, output->lx, output->ly);
			sway_scene_node_set_position(&output->layers.shell_overlay->node, output->lx, output->ly);
			sway_scene_node_set_position(&output->layers.session_lock->node, output->lx, output->ly);

			arrange_output(output, output->width, output->height);
		}
	}

	arrange_popups(root->layers.popup);
}

/**
 * Apply a transaction to the "current" state of the tree.
 */
static void transaction_apply(struct sway_transaction *transaction) {
	sway_log(SWAY_DEBUG, "Applying transaction %p", transaction);
	if (debug.txn_timings) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		struct timespec *commit = &transaction->commit_time;
		float ms = (now.tv_sec - commit->tv_sec) * 1000 +
			(now.tv_nsec - commit->tv_nsec) / 1000000.0;
		sway_log(SWAY_DEBUG, "Transaction %p: %.1fms waiting "
				"(%.1f frames if 60Hz)", transaction, ms, ms / (1000.0f / 60));
	}

	// Apply the instruction state to the node's current state
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_node *node = instruction->node;

		switch (node->type) {
		case N_ROOT:
			break;
		case N_OUTPUT:
			apply_output_state(node->sway_output, &instruction->output_state);
			break;
		case N_WORKSPACE:
			apply_workspace_state(node->sway_workspace,
					&instruction->workspace_state);
			break;
		case N_CONTAINER:
			apply_container_state(node->sway_container,
					&instruction->container_state);
			break;
		}

		node->instruction = NULL;
	}
}

static void transaction_commit_pending(void);

static void transaction_progress(void) {
	if (!server.queued_transaction) {
		return;
	}
	if (server.queued_transaction->num_waiting > 0) {
		return;
	}
	transaction_apply(server.queued_transaction);
	arrange_root(root);
	cursor_rebase_all();
	transaction_destroy(server.queued_transaction);
	server.queued_transaction = NULL;

	if (!server.pending_transaction) {
		sway_idle_inhibit_v1_check_active();
		return;
	}

	transaction_commit_pending();
}

static int handle_timeout(void *data) {
	struct sway_transaction *transaction = data;
	sway_log(SWAY_DEBUG, "Transaction %p timed out (%zi waiting)",
			transaction, transaction->num_waiting);
	transaction->num_waiting = 0;
	transaction_progress();
	return 0;
}

static bool should_configure(struct sway_node *node,
		struct sway_transaction_instruction *instruction) {
	if (!node_is_view(node)) {
		return false;
	}
	if (node->destroying) {
		return false;
	}
	if (!instruction->server_request) {
		return false;
	}
	struct sway_container_state *cstate = &node->sway_container->current;
	struct sway_container_state *istate = &instruction->container_state;
#if WLR_HAS_XWAYLAND
	// Xwayland views are position-aware and need to be reconfigured
	// when their position changes.
	if (node->sway_container->view->type == SWAY_VIEW_XWAYLAND) {
		// Sway logical coordinates are doubles, but they get truncated to
		// integers when sent to Xwayland through `xcb_configure_window`.
		// X11 apps will not respond to duplicate configure requests (from their
		// truncated point of view) and cause transactions to time out.
		if ((int)cstate->content_x != (int)istate->content_x ||
				(int)cstate->content_y != (int)istate->content_y) {
			return true;
		}
	}
#endif
	if (cstate->content_width == istate->content_width &&
			cstate->content_height == istate->content_height) {
		return false;
	}
	return true;
}

static void transaction_commit(struct sway_transaction *transaction) {
	sway_log(SWAY_DEBUG, "Transaction %p committing with %i instructions",
			transaction, transaction->instructions->length);
	transaction->num_waiting = 0;
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_node *node = instruction->node;
		bool hidden = node_is_view(node) && !node->destroying &&
			!view_is_visible(node->sway_container->view);
		if (should_configure(node, instruction)) {
			instruction->serial = view_configure(node->sway_container->view,
					instruction->container_state.content_x,
					instruction->container_state.content_y,
					instruction->container_state.content_width,
					instruction->container_state.content_height);
			if (!hidden) {
				instruction->waiting = true;
				++transaction->num_waiting;
			}

			view_send_frame_done(node->sway_container->view);
		}
		if (!hidden && node_is_view(node) &&
				!node->sway_container->view->saved_surface_tree) {
			view_save_buffer(node->sway_container->view);
		}
		node->instruction = instruction;
	}
	transaction->num_configures = transaction->num_waiting;
	if (debug.txn_timings) {
		clock_gettime(CLOCK_MONOTONIC, &transaction->commit_time);
	}
	if (debug.noatomic) {
		transaction->num_waiting = 0;
	} else if (debug.txn_wait) {
		// Force the transaction to time out even if all views are ready.
		// We do this by inflating the waiting counter.
		transaction->num_waiting += 1000000;
	}

	if (transaction->num_waiting) {
		// Set up a timer which the views must respond within
		transaction->timer = wl_event_loop_add_timer(server.wl_event_loop,
				handle_timeout, transaction);
		if (transaction->timer) {
			wl_event_source_timer_update(transaction->timer,
					server.txn_timeout_ms);
		} else {
			sway_log_errno(SWAY_ERROR, "Unable to create transaction timer "
					"(some imperfect frames might be rendered)");
			transaction->num_waiting = 0;
		}
	}
}

static void transaction_commit_pending(void) {
	if (server.queued_transaction) {
		return;
	}
	struct sway_transaction *transaction = server.pending_transaction;
	server.pending_transaction = NULL;
	server.queued_transaction = transaction;
	transaction_commit(transaction);
	transaction_progress();
}

static void set_instruction_ready(
		struct sway_transaction_instruction *instruction) {
	struct sway_transaction *transaction = instruction->transaction;

	if (debug.txn_timings) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		struct timespec *start = &transaction->commit_time;
		float ms = (now.tv_sec - start->tv_sec) * 1000 +
			(now.tv_nsec - start->tv_nsec) / 1000000.0;
		sway_log(SWAY_DEBUG, "Transaction %p: %zi/%zi ready in %.1fms (%s)",
				transaction,
				transaction->num_configures - transaction->num_waiting + 1,
				transaction->num_configures, ms,
				instruction->node->sway_container->title);
	}

	// If the transaction has timed out then its num_waiting will be 0 already.
	if (instruction->waiting && transaction->num_waiting > 0 &&
			--transaction->num_waiting == 0) {
		sway_log(SWAY_DEBUG, "Transaction %p is ready", transaction);
		wl_event_source_timer_update(transaction->timer, 0);
	}

	instruction->node->instruction = NULL;
	transaction_progress();
}

bool transaction_notify_view_ready_by_serial(struct sway_view *view,
		uint32_t serial) {
	struct sway_transaction_instruction *instruction =
		view->container->node.instruction;
	if (instruction != NULL && instruction->serial == serial) {
		set_instruction_ready(instruction);
		return true;
	}
	return false;
}

bool transaction_notify_view_ready_by_geometry(struct sway_view *view,
		double x, double y, int width, int height) {
	struct sway_transaction_instruction *instruction =
		view->container->node.instruction;
	if (instruction != NULL &&
			(int)instruction->container_state.content_x == (int)x &&
			(int)instruction->container_state.content_y == (int)y &&
			instruction->container_state.content_width == width &&
			instruction->container_state.content_height == height) {
		set_instruction_ready(instruction);
		return true;
	}
	return false;
}

static void _transaction_commit_dirty(bool server_request) {
	if (!server.dirty_nodes->length) {
		return;
	}

	if (!server.pending_transaction) {
		server.pending_transaction = transaction_create();
		if (!server.pending_transaction) {
			return;
		}
	}

	for (int i = 0; i < server.dirty_nodes->length; ++i) {
		struct sway_node *node = server.dirty_nodes->items[i];
		transaction_add_node(server.pending_transaction, node, server_request);
		node->dirty = false;
	}
	server.dirty_nodes->length = 0;

	transaction_commit_pending();
}

void transaction_commit_dirty(void) {
	_transaction_commit_dirty(true);
}

void transaction_commit_dirty_client(void) {
	_transaction_commit_dirty(false);
}
