#include <wlr/types/wlr_cursor.h>
#include <wlr/util/edges.h>
#include "sway/commands.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

struct seatop_resize_tiling_event {
	struct sway_container *con;    // container to resize

	enum wlr_edges edge;
	enum wlr_edges edge_x, edge_y;
	double ref_lx, ref_ly;         // cursor's x/y at start of op
	double con_orig_width;       // width of the container at start
	double con_orig_height;      // height of the container at start
};

static void handle_button(struct sway_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wl_pointer_button_state state) {
	struct seatop_resize_tiling_event *e = seat->seatop_data;

	if (seat->cursor->pressed_button_count == 0) {
		if (e->con) {
			enum sway_container_layout layout = layout_get_type(e->con->pending.workspace);
			struct sway_container *parent = e->con->pending.parent;
			if (layout == L_HORIZ) {
				parent->pending.width = e->con->pending.width;
				parent->free_size = true;
				for (int i = 0; i < parent->pending.children->length; ++i) {
					struct sway_container *child = parent->pending.children->items[i];
					child->pending.width = parent->pending.width;
					child->free_size = true;
				}
			} else {
				parent->pending.height = e->con->pending.height;
				parent->free_size = true;
				for (int i = 0; i < parent->pending.children->length; ++i) {
					struct sway_container *child = parent->pending.children->items[i];
					child->pending.height = parent->pending.height;
					child->free_size = true;
				}
			}
			e->con->free_size = true;
			container_set_resizing(e->con, false);
			arrange_workspace(e->con->pending.workspace);
		}
		transaction_commit_dirty();
		seatop_begin_default(seat);
	}
}

static void handle_pointer_motion(struct sway_seat *seat, uint32_t time_msec) {
	struct seatop_resize_tiling_event *e = seat->seatop_data;
	int amount_x = 0;
	int amount_y = 0;
	int moved_x = seat->cursor->cursor->x - e->ref_lx;
	int moved_y = seat->cursor->cursor->y - e->ref_ly;

	if (e->edge_x) {
		if (e->edge_x & WLR_EDGE_LEFT) {
			amount_x = e->con_orig_width - moved_x;
		} else if (e->edge_x & WLR_EDGE_RIGHT) {
			amount_x = e->con_orig_width + moved_x;
		}
		e->con->pending.width = amount_x;
	}
	if (e->edge_y) {
		if (e->edge_y & WLR_EDGE_TOP) {
			amount_y = e->con_orig_height - moved_y;
		} else if (e->edge_y & WLR_EDGE_BOTTOM) {
			amount_y = e->con_orig_height + moved_y;
		}
		e->con->pending.height = amount_y;
	}
	transaction_commit_dirty();
}

static void handle_unref(struct sway_seat *seat, struct sway_container *con) {
	struct seatop_resize_tiling_event *e = seat->seatop_data;
	if (e->con == con) {
		seatop_begin_default(seat);
	}
}

static const struct sway_seatop_impl seatop_impl = {
	.button = handle_button,
	.pointer_motion = handle_pointer_motion,
	.unref = handle_unref,
};

void seatop_begin_resize_tiling(struct sway_seat *seat,
		struct sway_container *con, enum wlr_edges edge) {
	seatop_end(seat);

	struct seatop_resize_tiling_event *e =
		calloc(1, sizeof(struct seatop_resize_tiling_event));
	if (!e) {
		return;
	}
	e->con = con;
	e->edge = edge;

	e->ref_lx = seat->cursor->cursor->x;
	e->ref_ly = seat->cursor->cursor->y;

	container_set_resizing(e->con, true);

	if (edge & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)) {
		e->edge_x = edge & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
		e->con_orig_width = e->con->pending.width;
	}
	if (edge & (WLR_EDGE_TOP | WLR_EDGE_BOTTOM)) {
		e->edge_y = edge & (WLR_EDGE_TOP | WLR_EDGE_BOTTOM);
		e->con_orig_height = e->con->pending.height;
	}

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;

	transaction_commit_dirty();
	wlr_seat_pointer_notify_clear_focus(seat->wlr_seat);
}
